#include <Lv2Host.h>
#include <algorithm>
#include "lilv_interface_private.h"
#include <string.h>

enum {kMapNotConnected = -255};

Lv2Host::Lv2Host(float sampleRate, unsigned int maxBlockSize, unsigned int nAudioInputs, unsigned int nAudioOutputs)
{
	setup(sampleRate, maxBlockSize, nAudioInputs, nAudioOutputs);
}

bool Lv2Host::setup(float sampleRate, unsigned int maxBlockSize, unsigned int nAudioInputs, unsigned int nAudioOutputs)
{
	world = LV2Apply_initializeWorld();
	if(!world)
		return false;
	this->maxBlockSize = maxBlockSize;
	this->sampleRate = sampleRate;
	this->nAudioInputs = nAudioInputs;
	this->nAudioOutputs = nAudioOutputs;
	dummyInput.resize(maxBlockSize);
	struct map defaultMap;
	defaultMap.slot = kMapNotConnected;
	defaultMap.channel = 0;
	inputMap.resize(nAudioInputs, defaultMap);
	outputMap.resize(nAudioOutputs, defaultMap);
	symap = symap_new();
	map.handle = symap;
	map.map = (LV2_URID (*)(LV2_URID_Map_Handle, const char *))symap_map;
	mapFeature.URI = LV2_URID__map;
	mapFeature.data = &map;
	unmap.handle = symap;
	unmap.unmap = (const char *(*)(LV2_URID_Unmap_Handle, LV2_URID))symap_unmap;
	unmapFeature.URI = LV2_URID__unmap;
	unmapFeature.data = &unmap;
	featureList.push_back(&mapFeature);
	featureList.push_back(&unmapFeature);
	featureList.push_back(NULL);
	return true;
}

void Lv2Host::cleanup()
{
	for(auto slot : slots)
	{
		LV2Apply_cleanup(slot);
	}
	LV2Apply_cleanupWorld(world);
}

int Lv2Host::add(std::string const& pluginUri)
{
	auto slot = LV2Apply_instantiatePlugin(world, pluginUri.c_str(), sampleRate, featureList.data());
	if(!slot)
		return -1;
	slots.push_back(slot);
	LV2Apply_printPorts(world, slot->plugin);
	// verbose
	unsigned int inAudio, outAudio, inCtl, outCtl;
	LV2Apply_getPortCount(slot, &inAudio, &outAudio, &inCtl, &outCtl);
	printf("Ports: in audio %u, out audio %u, in ctl %u, out ctl %u\n", inAudio, outAudio, inCtl, outCtl);

	auto idx = slots.size() - 1;

	if(idx == 0)
	{
#ifdef ALLOCATE_INPUTS
		// allocate arrays for inputs of the first slot
		for(unsigned int n = 0; n < std::min(slots[idx]->n_audio_in, nAudioInputs); ++n);
		{
			buffers.push_back();
			buffers.last().resize(maxBlockSize);
			slots[idx]->out_bufs[n] = buffers.last().data();
		}
#endif /* ALLOCATE_INPUTS */
		for(unsigned int n = 0; n < inputMap.size(); ++n)
		{
			// automap inputs of first plugin to audio inputs
			if(n < inAudio) {
				inputMap[n].slot = 0;
				inputMap[n].channel = n;
			}
			// and, for good measure, give them a dummyInput
			// buffer, in case they are not connected above, or
			// they are re-routed before the first call to render()
			slots[0]->in_bufs[n] = dummyInput.data();
		}
	} else {
		// connect the outputs of the previous slot to the input of the
		// current one.
		// Somehow handle the case where the channel count differs
		// (currently: drop extra channels, or duplicate)
		// TODO: should mix instead of dropping when prevN > currN
		unsigned int prevNOut = slots[idx-1]->n_audio_out;
		unsigned int currNIn = slots[idx]->n_audio_in;
		for(unsigned int n = 0; n < std::max(currNIn, prevNOut); ++n)
		{
			unsigned int prevN;
			unsigned int currN;
			prevN = std::min(prevNOut - 1, n);
			currN = std::min(currNIn - 1, n);
			slots[idx]->in_bufs[currN] = slots[idx-1]->out_bufs[prevN];
		}
	}
	// map the outputs of the last plugin to the outputs of the host
	for(unsigned int n = 0; n < std::min(outputMap.size(), outAudio); ++n)
	{
		outputMap[n].slot = idx;
		outputMap[n].channel = n;
	}

	// allocate arrays for outputs
	for(unsigned int n = 0; n < slot->n_audio_out; ++n)
	{
		buffers.emplace_back(std::vector<float>(maxBlockSize));
		buffers.back().shrink_to_fit();
		slot->out_bufs[n] = buffers.back().data();
	}

	LV2Apply_connectPorts(slot);
	tmpModifiedSlots.reserve(slots.size());
	return slots.size() - 1;
}

#if 0
bool Lv2Host::connect(unsigned int source, unsigned int dest)
{
	unsigned int inCtl, outCtl;
	unsigned int sourceAudioIn, sourceAudioOut;
	unsigned int destAudioIn, destAudioOut;
	LV2Apply_getPortCount(slots[source], &sourceAudioIn, &sourceAudioOut, &inCtl, &outCtl);
	LV2Apply_getPortCount(slots[dest], &destAudioIn, &destAudioOut, &inCtl, &outCtl);
	LV2Apply_connectPorts(slots[source]);
}
#endif
bool Lv2Host::connect(int sourceSlotNumber, unsigned int sourceChannel, unsigned int destinationSlotNumber, unsigned int destinationChannel)
{
	bool done = false;
	if(-1 == sourceSlotNumber) {
		inputMap[sourceChannel].slot = destinationSlotNumber;
		inputMap[sourceChannel].channel = destinationChannel;
		done = true;
	}
	if (slots.size() == destinationSlotNumber) {
		outputMap[destinationChannel].slot = sourceSlotNumber;
		outputMap[destinationChannel].channel = sourceChannel;
		done = true;
	}
	try {
		if(!done)
		{
			auto& sourceSlot = slots[sourceSlotNumber];
			auto& destinationSlot = slots[destinationSlotNumber];
			destinationSlot->in_bufs[destinationChannel] = sourceSlot->out_bufs[sourceChannel];
			LV2Apply_connectPorts(sourceSlot);
			LV2Apply_connectPorts(destinationSlot);
		}
	} catch (std::exception e) {
		return false;
	}
	return true;
}

bool Lv2Host::disconnect(unsigned int destinationSlotNumber, unsigned int destinationChannel)
{
	try {
		if(-1 == destinationSlotNumber) {
			inputMap[destinationChannel].slot = kMapNotConnected;
		} else if(slots.size() == destinationSlotNumber) {
			outputMap[destinationChannel].slot = kMapNotConnected;
		} else {
			auto& destinationSlot = slots[destinationSlotNumber];
			destinationSlot->in_bufs[destinationChannel] = dummyInput.data();
			LV2Apply_connectPorts(destinationSlot);
		}
	} catch (std::exception e) {
		return false;
	}
	return true;
}

void Lv2Host::bypass(unsigned int slotNumber, bool bypassed)
{
	slots[slotNumber]->bypass = bypassed;
}

void Lv2Host::render(unsigned int nFrames, const float** inputs, float** outputs)
{
	if(slots.size() > 0)
	{
		tmpModifiedSlots.resize(0);
		for(unsigned int n = 0; n < nAudioInputs; ++n)
		{
			int slot = inputMap[n].slot;
			int channel = inputMap[n].channel;
			if(kMapNotConnected == slot)
				continue;
			if(slots.size() == slot)// pass-through, handled below
				continue;
			slots[slot]->in_bufs[channel] = (float*)inputs[n];
			tmpModifiedSlots.push_back(slot);
		}
		for(unsigned int n = 0; n < nAudioOutputs; ++n)
		{
			int slot = outputMap[n].slot;
			int channel = outputMap[n].channel;
			if(kMapNotConnected == slot)
				continue;
			if(-1 == slot) {// pass-through, handled here
				memcpy(outputs[n], inputs[channel],
					sizeof(outputs[n][0]) * nFrames);
				continue;
			}
			slots[slot]->out_bufs[channel] = outputs[n];
			tmpModifiedSlots.push_back(slot);
		}
		auto last = std::unique(tmpModifiedSlots.begin(), tmpModifiedSlots.end());
		tmpModifiedSlots.resize(last - tmpModifiedSlots.begin());
		for(auto n : tmpModifiedSlots)
		{
			LV2Apply_connectPorts(slots[n]);
		}
		for(auto& slot : slots)
		{
			if(!slot->bypass)
				lilv_instance_run(slot->instance, nFrames);
		}
	}
}

int Lv2Host::setPort(unsigned int slotN, unsigned int portN, float value)
{
	if(slots.size() <= slotN)
	{
		return -1;
	}
	if(nullptr == slots[slotN])
	{
		return -2;
	}
	auto slot = slots[slotN];
	if(slot->n_ports <= portN)
	{
		return -3;
	}
	auto port = &slot->ports[portN];
	if(port->type != TYPE_CONTROL)
	{
		return -4;
	}
	if(!port->is_input)
	{
		return -5;
	}
	if(value > port->maxValue)
	{
		value = port->maxValue;
	} else if (value < port->minValue)
	{
		value = port->minValue;
	}
	port->value = value;
	return 0;
}

float Lv2Host::getPortValue(unsigned int slotN, unsigned int portN)
{
	if(slots.size() <= slotN)
	{
		return 0;
	}
	if(nullptr == slots[slotN])
	{
		return 0;
	}
	auto slot = slots[slotN];
	if(slot->n_ports <= portN)
	{
		return 0;
	}
	auto port = &slot->ports[portN];
	if(port->type != TYPE_CONTROL)
	{
		return 0;
	}
	return port->value;
}

int Lv2Host::countPorts(unsigned int slotN)
{
	auto slot = slots[slotN];
	return slot->n_ports;
}

struct portDesc Lv2Host::getPortDesc(unsigned int slotNumber, unsigned int portNumber)
{
	portDesc newPortDesc;
	auto slot = slots[slotNumber];

	newPortDesc.name =  LV2Apply_getPortName(slot, portNumber);
	newPortDesc.type = LV2Apply_getControlPortType(slot,  world, portNumber);
	LV2Apply_getPortRanges(slot, portNumber, &newPortDesc.min, &newPortDesc.max, &newPortDesc.defaultVal);
	newPortDesc.isLogarithmic = LV2Apply_isLogarithmic(slot, world, portNumber);
	newPortDesc.hasStrictBounds= LV2Apply_hasStrictBounds(slot, world, portNumber);

	return newPortDesc;
}

const char* Lv2Host::getPluginName(unsigned int slotN)
{
	auto slot = slots[slotN];
	return LV2Apply_getPluginName(slot);
}
