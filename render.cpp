#include <Bela.h>
#include <Lv2Host.h>
#include <math.h>

Lv2Host gLv2Host;

void Bela_userSettings(BelaInitSettings *settings)
{
	settings->uniformSampleRate = 1;
	settings->interleave = 0;
	settings->analogOutputsPersist = 0;
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
	lv2Chain.emplace_back("http://calf.sourceforge.net/plugins/Flanger");
	lv2Chain.emplace_back("http://calf.sourceforge.net/plugins/Filter");
	lv2Chain.emplace_back("http://calf.sourceforge.net/plugins/Limiter");
	for(auto &name : lv2Chain)
	{
		gLv2Host.add(name);
	}
	if(0 == gLv2Host.count())
	{
		fprintf(stderr, "No plugins were successfully instantiated\n");
		return false;	
	}
	// set effect parameter
	if(int ret = gLv2Host.setPort(0, 7, -0.1))
		fprintf(stderr, "error setting port for flanger: %d\n", ret);
	if(int ret = gLv2Host.setPort(1, 5, 10.f))
		fprintf(stderr, "error setting port for filter: %d\n", ret);
	
	return true;
}

void render(BelaContext* context, void* userData)
{
	if(1)
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
		gLv2Host.setPort(1, 4, count);
	}

	// set inputs and outputs: normally you would just do this
	const float* inputs[context->audioInChannels];
	float* outputs[context->audioOutChannels];
	for(unsigned int ch = 0; ch < context->audioInChannels; ++ch)
		inputs[ch] = (float*)&context->audioIn[context->audioFrames * ch];
	for(unsigned int ch = 0; ch < context->audioOutChannels; ++ch)
		outputs[ch] = &context->audioOut[context->audioFrames * ch];
	if(1)
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
