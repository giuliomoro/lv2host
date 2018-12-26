#include <Lv2Host.h>
#include <algorithm>
#include "lilv_interface_private.h"

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
	auto slot = LV2Apply_instantiatePlugin(world, pluginUri.c_str(), sampleRate);
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

	// allocate arrays for outputs
	for(unsigned int n = 0; n < slot->n_audio_out; ++n)
	{
		buffers.emplace_back(std::vector<float>(maxBlockSize));
		buffers.back().shrink_to_fit();
		slot->out_bufs[n] = buffers.back().data();
	}

	LV2Apply_connectPorts(slot);
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

void Lv2Host::render(unsigned int nFrames, const float** inputs, float** outputs)
{
	if(slots.size() > 0)
	{
		for(unsigned int n = 0; n < nAudioInputs; ++n)
		{
			slots[0]->in_bufs[n] = (float*)inputs[n];
		}
		for(unsigned int n = 0; n < nAudioOutputs; ++n)
		{
			slots.back()->out_bufs[n] = outputs[n];
		}
		LV2Apply_connectPorts(slots[0]);
		if(slots.size() > 1) {
			LV2Apply_connectPorts(slots[slots.size()-1]);
		}
		for(auto& slot : slots)
		{
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
