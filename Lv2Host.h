#include <vector>
#include <string>
#include "lilv_interface.h"
extern "C"
{
#include "symap.h"
}

struct portDesc
{
	const char* name;
	port_type_t type;
	float min;
	float max;
	float defaultVal;
	bool isLogarithmic;
	bool hasStrictBounds;
};
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
	const char* getPluginName(unsigned int slotN);
	/// set the value of a control port
	int setPort(unsigned int slotN, unsigned int port, float value);
	float getPortValue(unsigned int slotN, unsigned int portN);
	int countPorts(unsigned int slotN);
	struct portDesc getPortDesc(unsigned int slotNumber, unsigned int portNumber);

	/**
	 * Create a new audio connection between two slots.
	 * Note that the outputPort and inputPort are indexed between 0 and
	 * the number of output or input audio ports, respectively.
	 */
	bool connect(unsigned int outSlotNumber, unsigned int outputPort, unsigned int inSlotNumber, unsigned int inputPort);
	/**
	 * Disconnect an audio connection between two slots.
	 * Note that the inputPort is indexed between 0 and
	 * the number of input audio ports.
	 */
	bool disconnect(unsigned int inSlotNumber, unsigned int inputPort);
	/**
	 * bypass a slot. You have to manually create connections across the
	 * slot for the signal to go through when it is bypassed.
	 */
	void bypass(unsigned int slotNumber, bool bypassed);
	/** process the effect chain
	 * @param inputs array of pointers to audio input channels (as set by setup())
	 * @param outputs array of pointers to audio output channels (as set by setup())
	 */
	void render(unsigned int nFrames, float const** inputs, float** outputs);
	void cleanup();

private:
	std::vector<LV2Apply*> slots;
	LilvWorld* world;
	Symap* symap;
	LV2_URID_Map map;
	LV2_URID_Unmap unmap;
	LV2_Feature mapFeature;
	LV2_Feature unmapFeature;
	std::vector<const LV2_Feature*> featureList;
	std::vector<std::vector<float>> buffers;
	float sampleRate;
	unsigned int maxBlockSize;
	unsigned int nAudioInputs;
	unsigned int nAudioOutputs;
};
