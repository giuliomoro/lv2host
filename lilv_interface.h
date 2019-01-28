#pragma once

#include <lilv-0/lilv/lilv.h>
#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

typedef enum
{
	kNotControl,
	kToggle,
	kEnumerated,
	kInteger,
	kFloat
} port_type_t;

typedef struct _lv2apply LV2Apply;

LV2Apply* LV2Apply_instantiatePlugin(LilvWorld* world, const char* plugin_uri, float sampleRate);
LilvWorld* LV2Apply_initializeWorld();
void LV2Apply_cleanup(LV2Apply* self);
void LV2Apply_cleanupWorld(LilvWorld* world);
void LV2Apply_printPorts(LilvWorld* world, const LilvPlugin* p);
void LV2Apply_getPortCount(LV2Apply* self, unsigned int* in_audio,
	unsigned int* out_audio, unsigned int* in_ctl, unsigned int* out_ctl);
void LV2Apply_connectPorts(LV2Apply* self);
void LV2Apply_getPortRanges(LV2Apply* self, unsigned int index, float* min, float* max, float* defaultValue);
const char* LV2Apply_getPortName(LV2Apply* self, unsigned int index);
port_type_t LV2Apply_getControlPortType(LV2Apply* self, LilvWorld* world, unsigned int index);
int LV2Apply_getPortIndex(LV2Apply* self, LilvWorld* world, char* symbol);
bool LV2Apply_isLogarithmic(LV2Apply* self, LilvWorld* world, unsigned int index);
bool LV2Apply_hasStrictBounds(LV2Apply* self, LilvWorld* world, unsigned int index);
const char* LV2Apply_getPluginName(LV2Apply* self);


#ifdef __cplusplus
}
#endif /* __cplusplus */
