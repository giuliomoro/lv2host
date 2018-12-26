#include <vector>
#include <string>
#include "lilv_interface.h"

/// an  effect chain
class Lv2Host
{
public:
	Lv2Host() {};
	Lv2Host(float sampleRate, unsigned int maxBlockSize, unsigned int nAudioInputs, unsigned int nAudioOutputs);
	~Lv2Host() { cleanup();};
	bool setup(float sampleRate, unsigned int maxBlockSize, unsigned int nAudioInputs, unsigned int nAudioOutputs);
	int count() { return slots.size();};
	/// add the next plugin in the effect chain
	int add(std::string const& pluginUri);
	/// set the value of a control port
	int setPort(unsigned int slotN, unsigned int port, float value);
	/** process the effect chain
	 * @param inputs array of pointers to audio input channels (as set by setup())
	 * @param outputs array of pointers to audio output channels (as set by setup())
	 */
	void render(unsigned int nFrames, float const** inputs, float** outputs);
	void cleanup();

private:
	std::vector<LV2Apply*> slots;
	LilvWorld* world;
	std::vector<std::vector<float>> buffers;
	float sampleRate;
	unsigned int maxBlockSize;
	unsigned int nAudioInputs;
	unsigned int nAudioOutputs;
};
