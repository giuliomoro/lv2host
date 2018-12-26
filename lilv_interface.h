#pragma once

#include <lilv-0/lilv/lilv.h>
#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */
typedef struct _lv2apply LV2Apply;

LV2Apply* LV2Apply_instantiatePlugin(LilvWorld* world, const char* plugin_uri, float sampleRate);
LilvWorld* LV2Apply_initializeWorld();
void LV2Apply_cleanup(LV2Apply* self);
void LV2Apply_cleanupWorld(LilvWorld* world);
void LV2Apply_printPorts(LilvWorld* world, const LilvPlugin* p);
void LV2Apply_getPortCount(LV2Apply* self, unsigned int* in_audio,
	unsigned int* out_audio, unsigned int* in_ctl, unsigned int* out_ctl);
void LV2Apply_connectPorts(LV2Apply* self);

#ifdef __cplusplus
}
#endif /* __cplusplus */
