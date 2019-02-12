#include <Bela.h>
#include <Lv2Host.h>
#include <Gui.h>
#include <math.h>
#include <vector>
#include <string.h>

Lv2Host gLv2Host;
Gui sliderGui;


std::vector<std::vector<portDesc>> gPortDescriptions;
std::vector<std::vector<int>> gControlPortNum;

float gUpdateInterval = 0.05;

void Bela_userSettings(BelaInitSettings *settings)
{
	settings->uniformSampleRate = 1;
	settings->interleave = 0;
	settings->analogOutputsPersist = 0;
}

void printPortDescription(const portDesc& description)
{
	printf("Port name: %s\n", description.name);
	printf("Port type: %d\n", (int)description.type);
	printf("Port min: %f\n", description.min);
	printf("Port max: %f\n", description.max);
	printf("Port default: %f\n", description.defaultVal);
	printf("Port is logarithmic: %d\n", description.isLogarithmic);
	printf("Port has strict bounds: %d\n", description.hasStrictBounds);
}

void createSliderFromDescription(unsigned int slotNumber, const portDesc& description)
{
	std::string sliderName;
	sliderName = std::string(gLv2Host.getPluginName(slotNumber)) + " " + std::string(description.name);
	float step = 0;
	if(description.type == kToggle || description.type == kEnumerated || description.type == kInteger)
		step = 1;
		
	sliderGui.addSlider(sliderName, description.min, description.max, step, description.defaultVal);
}

//getAllCtlPorts
void generatePortDescriptions()
{
	for(unsigned int slot = 0; slot < gLv2Host.count(); slot++)
	{
		std::vector<portDesc> pluginPortDesc;
		std::vector<int> controlPortNum;

		for(unsigned int port = 0; port < gLv2Host.countPorts(slot); port++)
		{
			portDesc newPortDesc;
			newPortDesc = gLv2Host.getPortDesc(slot, port);
			if(newPortDesc.type != kNotControl)
			{
				
				pluginPortDesc.emplace_back(newPortDesc);
				controlPortNum.emplace_back(port);
				createSliderFromDescription(slot,newPortDesc);
			}
		}
		gPortDescriptions.emplace_back(pluginPortDesc);
		gControlPortNum.emplace_back(controlPortNum);
	}
}

void setControls()
{
	unsigned int portOffset = 0;
	for(unsigned int slot = 0; slot < gControlPortNum.size(); slot++)
	{
		for(unsigned int cPort = 0; cPort < gControlPortNum[slot].size(); cPort ++)
		{
			gLv2Host.setPort(slot, gControlPortNum[slot][cPort], sliderGui.getSliderValue(cPort + portOffset));
		}
		portOffset = gControlPortNum[slot].size();
	}
	
}
	
bool setup(BelaContext* context, void* userData)
{
	
	// these should be initialized by Bela_userSettings above
	if((context->flags & BELA_FLAG_INTERLEAVED) || context->audioSampleRate != context->analogSampleRate)
	{
		fprintf(stderr, "Using Lv2Host requires non-interleaved buffers and uniform sample rate\n");
		return false;
	}
	if(!gLv2Host.setup(context->audioSampleRate, context->audioFrames,
				context->audioInChannels, context->audioOutChannels))
	{
		fprintf(stderr, "Unable to create Lv2 host\n");
		return false;
	}
		
	std::vector<std::string> lv2Chain;
	lv2Chain.emplace_back("http://calf.sourceforge.net/plugins/SidechainGate");
	for(auto &name : lv2Chain)
	{
		gLv2Host.add(name);
	}
	if(0 == gLv2Host.count())
	{
		fprintf(stderr, "No plugins were successfully instantiated\n");
		return false;	
	}

	sliderGui.setup(5432, "gui");
	generatePortDescriptions();
	
	//setControls();
	
	return true;
}

void render(BelaContext* context, void* userData)
{
	if(0)
	{
		// modulate parameter once per block, for testing purposes
		int tot = 5000;
		static int count = 0;
		count++;
		gLv2Host.setPort(0, 6, count/(float)tot * 10);
		if(count == tot)
		{
			count = 0;
		}
	}
	
	// Set control values 
	static unsigned int count = 0;
	for(unsigned int n = 0; n < context->audioFrames; n++) {
				setControls();
		count++;

	}


	// set inputs and outputs: normally you would just do this
	const float* inputs[context->audioInChannels];
	float* outputs[context->audioOutChannels];
	for(unsigned int ch = 0; ch < context->audioInChannels; ++ch)
		inputs[ch] = (float*)&context->audioIn[context->audioFrames * ch];
	for(unsigned int ch = 0; ch < context->audioOutChannels; ++ch)
		outputs[ch] = &context->audioOut[context->audioFrames * ch];
		

	
	if(0)
	{
		// optionally, override audio inputs, for testing purposes
		for(unsigned int n = 0; n < context->audioFrames; ++n)
		{
			static float phase = 0;
			phase += 2 * M_PI * 300 / context->audioSampleRate;
			if(phase > M_PI)
				phase -= 2 * M_PI;
			for(unsigned int ch = 0; ch < context->audioInChannels; ++ch)
			{
				float* in = (float*)&inputs[ch][n];
				*in = 0.1 * (phase * (1 + ch)); // aliased sawtooth!
			}
		}
	}
	// do the actual processing on the buffers specified above
	gLv2Host.render(context->audioFrames, inputs, outputs);
}

void cleanup(BelaContext* context, void* userData) {}
	