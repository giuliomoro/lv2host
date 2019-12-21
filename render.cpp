#include <Bela.h>
#include <vector>
#include "Lv2Host.h"
#include <libraries/Gui/Gui.h>
#include <libraries/OnePole/OnePole.h>
#include <libraries/Scope/Scope.h>

Lv2Host gLv2Host;

Scope scope;

OnePole LpFilters[4];
float gLpFiltF0 = 100; // LP filter frequency
int gControlPins[] = {0, 1, 2, 3};
int gAudioFramesPerAnalogFrame;
int gLedPin = 0;


float gUpdateInterval = 0.05;

float processPot(unsigned int i, float rawVal, float min, float max)
{
	float val = floorf(rawVal * 100.f) / 100;
	val = LpFilters[i].process(val);
	val = map(val, 0.0, 0.82999, min, max); // Potentiometers wired in reverse
	val = constrain(val, min, max);
	return val;
}

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
	lv2Chain.emplace_back("http://calf.sourceforge.net/plugins/SidechainGate");
	lv2Chain.emplace_back("http://calf.sourceforge.net/plugins/Compressor");
	for(auto &name : lv2Chain)
	{
		gLv2Host.add(name);
	}
	if(0 == gLv2Host.count())
	{
		fprintf(stderr, "No plugins were successfully instantiated\n");
		return false;	
	}

	if(context->analogFrames)
		gAudioFramesPerAnalogFrame = context->audioFrames / context->analogFrames;

	// Setup lp filters for controls	
	for(unsigned int i = 0; i < sizeof(gControlPins)/sizeof(*gControlPins); i++)
		LpFilters[i].setup(gLpFiltF0, context->audioSampleRate/context->analogFrames);

	// Gate

	gLv2Host.setPort(0, 6, 1); // Bypass
	gLv2Host.setPort(0, 7, 1); // Input
	gLv2Host.setPort(0, 12, 0.06); // Max Gain Reduction
	gLv2Host.setPort(0, 13, 0.07); // Threshold
	gLv2Host.setPort(0, 14, 10); // Ratio
	gLv2Host.setPort(0, 15, 10); // Attack
	gLv2Host.setPort(0, 16, 200); // Release
	gLv2Host.setPort(0, 17, 1); // Makeup Gain
	gLv2Host.setPort(0, 18, 2.8); // Knee
	gLv2Host.setPort(0, 19, 0); // Detection
	gLv2Host.setPort(0, 20, 1); // Stereo link
	gLv2Host.setPort(0, 22, 9); // S/C mode
	gLv2Host.setPort(0, 23, 250); // F1
	gLv2Host.setPort(0, 24, 5000); // F2
	gLv2Host.setPort(0, 25, 1); // F1 level
	gLv2Host.setPort(0, 26, 1); // F2 level
	gLv2Host.setPort(0, 27, 0); // S/C Listen
	gLv2Host.setPort(0, 31, 0); // S/C Route

	// Compressor

	gLv2Host.setPort(1, 4, 1); // Bypass
	gLv2Host.setPort(1, 5, 1); // Input
	gLv2Host.setPort(1, 10, 0.01); // Threshold
	gLv2Host.setPort(1, 11, 6); // Ratio
	gLv2Host.setPort(1, 12, 1); // Attack
	gLv2Host.setPort(1, 13, 40); // Release
	gLv2Host.setPort(1, 14, 1); // Makeup Gain
	gLv2Host.setPort(1, 15, 5); // Knee
	gLv2Host.setPort(1, 16, 1); // Detection
	gLv2Host.setPort(1, 17, 1); // Stereo Link
	gLv2Host.setPort(1, 19, 0.9); // Mix

	scope.setup(4, context->audioSampleRate);
	
	// Turn LED on
	pinMode(context, 0, gLedPin, OUTPUT); // Set pin as output
	digitalWrite(context, 0, gLedPin, 1); //Turn LED on

	return true;
}

void render(BelaContext* context, void* userData)
{
	static bool pluginsOn[2] = {false, false};

	// Set control values 
	// Pot 0 -- Gate Threshold
	float gateThresholdVal = processPot(0, analogReadNI(context, 0, gControlPins[0]), 0.0, 0.5);
	gLv2Host.setPort(0, 13, gateThresholdVal);

	if(gateThresholdVal == 0.0 && pluginsOn[0]) {
		gLv2Host.setPort(0, 6, 1);
		rt_printf("Gate OFF\n");
		pluginsOn[0] = false;
	} else if (gateThresholdVal > 0.0 && !pluginsOn[0]) {
		gLv2Host.setPort(0, 6, 0);
		rt_printf("Gate ON\n");
		pluginsOn[0] = true;
	}

	// Pot 1 -- Gate Ratio
	float gateRatioVal = processPot(1, analogReadNI(context, 0, gControlPins[1]), 0.0, 10.0);
	gLv2Host.setPort(0, 14, gateRatioVal);
	
	// Pot 2 -- Compressor Drive (threshold)
	float compInputVal = processPot(2, analogReadNI(context, 0, gControlPins[2]), 0.0, 1.0);
	gLv2Host.setPort(1, 10, compInputVal);
	if(compInputVal == 0.0 && pluginsOn[1]) {
		gLv2Host.setPort(1, 4, 1);
		rt_printf("Compressor OFF\n");
		pluginsOn[1] = false;
	} else if (compInputVal > 0.0 && !pluginsOn[1]) {
		gLv2Host.setPort(1, 4, 0);
		rt_printf("Compressor ON\n");
		pluginsOn[1] = true;
	}
	// Pot 3 -- Compressor Release
	float compReleaseVal = processPot(3, analogReadNI(context, 0, gControlPins[3]), 0.01, 1999);
	gLv2Host.setPort(1, 13, compReleaseVal);

	// set inputs and outputs
	const float* inputs[context->audioInChannels];
	float* outputs[context->audioOutChannels];
	for(unsigned int ch = 0; ch < context->audioInChannels; ++ch)
		inputs[ch] = (float*)&context->audioIn[context->audioFrames * ch];
	for(unsigned int ch = 0; ch < context->audioOutChannels; ++ch)
		outputs[ch] = &context->audioOut[context->audioFrames * ch];

	// do the actual processing on the buffers specified above
	gLv2Host.render(context->audioFrames, inputs, outputs);

	float compThres = gLv2Host.getPortValue(1, 10);
	float compGainReduction = gLv2Host.getPortValue(1, 18);

	// Log input, output, compressor's threshold and compressor's gain reduction into scope
	for(unsigned int n = 0; n < context->audioFrames; n++)
		scope.log(audioReadNI(context, n, 0), context->audioOut[context->audioFrames*0+n], compGainReduction, compThres);

}

void cleanup(BelaContext* context, void* userData) {

}

