/*
  Copyright 2007-2016 David Robillard <http://drobilla.net>

  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THIS SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
/*
 * The above copyright notice is from drobillard's lilv/utils/lv2apply.c and
 * lilv/utils/lv2info.c which is where most of the code in the present file
 * comes from.
 *
 * This file is about wrapping lilv in a more approachable and OO way).
 * Additional copyright 2018   github.com/giuliomoro
 */

#include "lilv_interface.h"
#include "lilv_interface_private.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

static LilvNode* applies_to_pred     = NULL;
static LilvNode* control_class       = NULL;
static LilvNode* event_class         = NULL;
static LilvNode* group_pred          = NULL;
static LilvNode* label_pred          = NULL;
static LilvNode* preset_class        = NULL;
static LilvNode* designation_pred    = NULL;
static LilvNode* supports_event_pred = NULL;

#define VERBOSE

static void
print_port(const LilvPlugin* p,
           uint32_t          index,
           float*            mins,
           float*            maxes,
           float*            defaults)
{
	const LilvPort* port = lilv_plugin_get_port_by_index(p, index);

	printf("\n\tPort %d:\n", index);

	if (!port) {
		printf("\t\tERROR: Illegal/nonexistent port\n");
		return;
	}

	bool first = true;

	const LilvNodes* classes = lilv_port_get_classes(p, port);
	printf("\t\tType:        ");
	LILV_FOREACH(nodes, i, classes) {
		const LilvNode* value = lilv_nodes_get(classes, i);
		if (!first) {
			printf("\n\t\t             ");
		}
		printf("%s", lilv_node_as_uri(value));
		first = false;
	}

	if (lilv_port_is_a(p, port, event_class)) {
		LilvNodes* supported = lilv_port_get_value(
			p, port, supports_event_pred);
		if (lilv_nodes_size(supported) > 0) {
			printf("\n\t\tSupported events:\n");
			LILV_FOREACH(nodes, i, supported) {
				const LilvNode* value = lilv_nodes_get(supported, i);
				printf("\t\t\t%s\n", lilv_node_as_uri(value));
			}
		}
		lilv_nodes_free(supported);
	}

	LilvScalePoints* points = lilv_port_get_scale_points(p, port);
	if (points) {
		printf("\n\t\tScale Points:\n");
	}
	LILV_FOREACH(scale_points, i, points) {
		const LilvScalePoint* point = lilv_scale_points_get(points, i);
		printf("\t\t\t%s = \"%s\"\n",
				lilv_node_as_string(lilv_scale_point_get_value(point)),
				lilv_node_as_string(lilv_scale_point_get_label(point)));
	}
	lilv_scale_points_free(points);

	const LilvNode* sym = lilv_port_get_symbol(p, port);
	printf("\n\t\tSymbol:      %s\n", lilv_node_as_string(sym));

	LilvNode* name = lilv_port_get_name(p, port);
	printf("\t\tName:        %s\n", lilv_node_as_string(name));
	lilv_node_free(name);

	LilvNodes* groups = lilv_port_get_value(p, port, group_pred);
	if (lilv_nodes_size(groups) > 0) {
		printf("\t\tGroup:       %s\n",
		       lilv_node_as_string(lilv_nodes_get_first(groups)));
	}
	lilv_nodes_free(groups);

	LilvNodes* designations = lilv_port_get_value(p, port, designation_pred);
	if (lilv_nodes_size(designations) > 0) {
		printf("\t\tDesignation: %s\n",
		       lilv_node_as_string(lilv_nodes_get_first(designations)));
	}
	lilv_nodes_free(designations);

	if (lilv_port_is_a(p, port, control_class)) {
		if (!isnan(mins[index])) {
			printf("\t\tMinimum:     %f\n", mins[index]);
		}
		if (!isnan(maxes[index])) {
			printf("\t\tMaximum:     %f\n", maxes[index]);
		}
		if (!isnan(defaults[index])) {
			printf("\t\tDefault:     %f\n", defaults[index]);
		}
	}

	LilvNodes* properties = lilv_port_get_properties(p, port);
	if (lilv_nodes_size(properties) > 0) {
		printf("\t\tProperties:  ");
	}
	first = true;
	LILV_FOREACH(nodes, i, properties) {
		if (!first) {
			printf("\t\t             ");
		}
		printf("%s\n", lilv_node_as_uri(lilv_nodes_get(properties, i)));
		first = false;
	}
	if (lilv_nodes_size(properties) > 0) {
		printf("\n");
	}
	lilv_nodes_free(properties);
}
/** Clean up all resources. */
void LV2Apply_cleanup(LV2Apply* self)
{
	lilv_instance_deactivate(self->instance);
	lilv_instance_free(self->instance);
	free(self->ports);
	free(self->params);
}

void LV2Apply_cleanupWorld(LilvWorld* world)
{
	lilv_world_free(world);
}

/** Print a fatal error and clean up for exit. */
static void*
fatal(LV2Apply* self, void* status, const char* fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	fprintf(stderr, "error: ");
	vfprintf(stderr, fmt, args);
	va_end(args);
	if(self) LV2Apply_cleanup(self);
	return status;
}
/**
   Create port structures from data (via create_port()) for all ports.
*/
static int
create_ports(LV2Apply* self, LilvWorld* world)
{
	const uint32_t n_ports = lilv_plugin_get_num_ports(self->plugin);

	self->n_ports = n_ports;
	self->ports   = (Port*)calloc(self->n_ports, sizeof(Port));

	/* Get default values for all ports */
	float* minValues = (float*)calloc(n_ports, sizeof(float));
	float* maxValues = (float*)calloc(n_ports, sizeof(float));
	float* values = (float*)calloc(n_ports, sizeof(float));
	lilv_plugin_get_port_ranges_float(self->plugin, minValues, maxValues, values);

	LilvNode* lv2_InputPort          = lilv_new_uri(world, LV2_CORE__InputPort);
	LilvNode* lv2_OutputPort         = lilv_new_uri(world, LV2_CORE__OutputPort);
	LilvNode* lv2_AudioPort          = lilv_new_uri(world, LV2_CORE__AudioPort);
	LilvNode* lv2_ControlPort        = lilv_new_uri(world, LV2_CORE__ControlPort);
	LilvNode* lv2_connectionOptional = lilv_new_uri(world, LV2_CORE__connectionOptional);

	for (uint32_t i = 0; i < n_ports; ++i) {
		Port*           port  = &self->ports[i];
		const LilvPort* lport = lilv_plugin_get_port_by_index(self->plugin, i);

		port->lilv_port = lport;
		port->index     = i;
		port->minValue  = minValues[i];
		port->maxValue  = maxValues[i];
		port->value     = isnan(values[i]) ? 0.0f : values[i];
		port->optional  = lilv_port_has_property(
			self->plugin, lport, lv2_connectionOptional);

		/* Check if port is an input or output */
		if (lilv_port_is_a(self->plugin, lport, lv2_InputPort)) {
			port->is_input = true;
		} else if (!lilv_port_is_a(self->plugin, lport, lv2_OutputPort) &&
		           !port->optional) {
			fprintf(stderr, "Port %d is neither input nor output\n", i);
		}

		/* Check if port is an audio or control port */
		if (lilv_port_is_a(self->plugin, lport, lv2_ControlPort)) {
			port->type = TYPE_CONTROL;
		} else if (lilv_port_is_a(self->plugin, lport, lv2_AudioPort)) {
			port->type = TYPE_AUDIO;
			if (port->is_input) {
				++self->n_audio_in;
			} else {
				++self->n_audio_out;
			}
		} else if (!port->optional) {
			fprintf(stderr, "Port %d has unsupported type\n", i);
		}
	}

	lilv_node_free(lv2_connectionOptional);
	lilv_node_free(lv2_ControlPort);
	lilv_node_free(lv2_AudioPort);
	lilv_node_free(lv2_OutputPort);
	lilv_node_free(lv2_InputPort);
	free(values);

	return 0;
}

LV2Apply* LV2Apply_instantiatePlugin(LilvWorld* world, const char* plugin_uri, float sampleRate, const LV2_Feature** features)
{
	LV2Apply self;
	memset(&self, 0, sizeof(self));

	/* Check that required arguments are given */
	if (!plugin_uri) {
	       return NULL;
       }
	if(strlen(plugin_uri) == 0) {
		return NULL;
	}
	/* Create plugin URI */
	LilvNode* uri = lilv_new_uri(world, plugin_uri);
	if (!uri) {
		return fatal(&self, 0, "Invalid plugin URI <%s>\n", plugin_uri);
	}

	/* Get plugin */
	const LilvPlugins* plugins = lilv_world_get_all_plugins(world);
	const LilvPlugin*  plugin  = lilv_plugins_get_by_uri(plugins, uri);
	lilv_node_free(uri);
	if (!(self.plugin = plugin)) {
		return fatal(&self, 0, "Plugin <%s> not found\n", plugin_uri);
	}

	/* Create port structures */
	if (create_ports(&self, world)) {
		return NULL;
	}

	LilvNode* rt_feature = lilv_new_uri(world,
			"http://lv2plug.in/ns/lv2core#hardRTCapable");
	if(!lilv_plugin_has_feature(plugin, rt_feature))
	{
		fprintf(stderr, "Plugin %s has no `hardRTCapable` flag. It *may* cause problems when running in real-time.\n", plugin_uri);
	
	}
	lilv_node_free(rt_feature);

	/* Set control values */
	for (unsigned i = 0; i < self.n_params; ++i) {
		const Param*    param = &self.params[i];
		LilvNode*       sym   = lilv_new_string(world, param->sym);
		const LilvPort* port  = lilv_plugin_get_port_by_symbol(plugin, sym);
		lilv_node_free(sym);
		if (!port) {
			return fatal(&self, 0, "Unknown port `%s'\n", param->sym);
		}

		self.ports[lilv_port_get_index(plugin, port)].value = param->value;
	}

	/* Prepare arrays for pointers that will hold inputs and outputs */
	self.in_bufs = calloc(self.n_audio_in, sizeof(float*));
	self.out_bufs = calloc(self.n_audio_out, sizeof(float*));

	/* Instantiate plugin */
	self.instance = lilv_plugin_instantiate(
		self.plugin, sampleRate, features);
	if(!self.instance) {
		return fatal(&self, 0, "Unable to instantiate plugin `%s'\n", plugin_uri);
	}
	lilv_instance_activate(self.instance);

	// Success: let's finally allocate memory and copy
	LV2Apply* ret = (LV2Apply*)malloc(sizeof(LV2Apply));
	if(!ret){
		return fatal(&self, 0, "Unable to allocate memory for plugin `%s'\n", plugin_uri);
	}
	memcpy(ret, &self, sizeof(LV2Apply));
	return ret;
}

LilvWorld* LV2Apply_initializeWorld()
{
	/* Create world */
	LilvWorld* world = lilv_world_new();
	if(!world)
	{
		fprintf(stderr, "Initializing LilvWorld failed\n");
		return NULL;
	}
	/* Discover world */
	lilv_world_load_all(world);
	return world;
}

// this also from lv2info.c
#include "lv2/lv2plug.in/ns/ext/port-groups/port-groups.h"
#include "lv2/lv2plug.in/ns/ext/presets/presets.h"
#include "lv2/lv2plug.in/ns/ext/event/event.h"
static void
print_plugin(LilvWorld*        world,
             const LilvPlugin* p)
{
	LilvNode* val = NULL;

	printf("%s\n\n", lilv_node_as_uri(lilv_plugin_get_uri(p)));

	val = lilv_plugin_get_name(p);
	if (val) {
		printf("\tName:              %s\n", lilv_node_as_string(val));
		lilv_node_free(val);
	}

	const LilvPluginClass* pclass      = lilv_plugin_get_class(p);
	const LilvNode*       class_label = lilv_plugin_class_get_label(pclass);
	if (class_label) {
		printf("\tClass:             %s\n", lilv_node_as_string(class_label));
	}

	val = lilv_plugin_get_author_name(p);
	if (val) {
		printf("\tAuthor:            %s\n", lilv_node_as_string(val));
		lilv_node_free(val);
	}

	val = lilv_plugin_get_author_email(p);
	if (val) {
		printf("\tAuthor Email:      %s\n", lilv_node_as_uri(val));
		lilv_node_free(val);
	}

	val = lilv_plugin_get_author_homepage(p);
	if (val) {
		printf("\tAuthor Homepage:   %s\n", lilv_node_as_uri(val));
		lilv_node_free(val);
	}

	if (lilv_plugin_has_latency(p)) {
		uint32_t latency_port = lilv_plugin_get_latency_port_index(p);
		printf("\tHas latency:       yes, reported by port %d\n", latency_port);
	} else {
		printf("\tHas latency:       no\n");
	}

	printf("\tBundle:            %s\n",
	       lilv_node_as_uri(lilv_plugin_get_bundle_uri(p)));

	const LilvNode* binary_uri = lilv_plugin_get_library_uri(p);
	if (binary_uri) {
		printf("\tBinary:            %s\n",
		       lilv_node_as_uri(lilv_plugin_get_library_uri(p)));
	}

	LilvUIs* uis = lilv_plugin_get_uis(p);
	if (lilv_nodes_size(uis) > 0) {
		printf("\tUIs:\n");
		LILV_FOREACH(uis, i, uis) {
			const LilvUI* ui = lilv_uis_get(uis, i);
			printf("\t\t%s\n", lilv_node_as_uri(lilv_ui_get_uri(ui)));

			const char* binary = lilv_node_as_uri(lilv_ui_get_binary_uri(ui));

			const LilvNodes* types = lilv_ui_get_classes(ui);
			LILV_FOREACH(nodes, t, types) {
				printf("\t\t\tClass:  %s\n",
				       lilv_node_as_uri(lilv_nodes_get(types, t)));
			}

			if (binary) {
				printf("\t\t\tBinary: %s\n", binary);
			}

			printf("\t\t\tBundle: %s\n",
			       lilv_node_as_uri(lilv_ui_get_bundle_uri(ui)));
		}
	}
	lilv_uis_free(uis);

	printf("\tData URIs:         ");
	const LilvNodes* data_uris = lilv_plugin_get_data_uris(p);
	bool first = true;
	LILV_FOREACH(nodes, i, data_uris) {
		if (!first) {
			printf("\n\t                   ");
		}
		printf("%s", lilv_node_as_uri(lilv_nodes_get(data_uris, i)));
		first = false;
	}
	printf("\n");

	/* Required Features */

	LilvNodes* features = lilv_plugin_get_required_features(p);
	if (features) {
		printf("\tRequired Features: ");
	}
	first = true;
	LILV_FOREACH(nodes, i, features) {
		if (!first) {
			printf("\n\t                   ");
		}
		printf("%s", lilv_node_as_uri(lilv_nodes_get(features, i)));
		first = false;
	}
	if (features) {
		printf("\n");
	}
	lilv_nodes_free(features);

	/* Optional Features */

	features = lilv_plugin_get_optional_features(p);
	if (features) {
		printf("\tOptional Features: ");
	}
	first = true;
	LILV_FOREACH(nodes, i, features) {
		if (!first) {
			printf("\n\t                   ");
		}
		printf("%s", lilv_node_as_uri(lilv_nodes_get(features, i)));
		first = false;
	}
	if (features) {
		printf("\n");
	}
	lilv_nodes_free(features);

	/* Extension Data */

	LilvNodes* data = lilv_plugin_get_extension_data(p);
	if (data) {
		printf("\tExtension Data:    ");
	}
	first = true;
	LILV_FOREACH(nodes, i, data) {
		if (!first) {
			printf("\n\t                   ");
		}
		printf("%s", lilv_node_as_uri(lilv_nodes_get(data, i)));
		first = false;
	}
	if (data) {
		printf("\n");
	}
	lilv_nodes_free(data);

	/* Presets */

	LilvNodes* presets = lilv_plugin_get_related(p, preset_class);
	if (presets) {
		printf("\tPresets: \n");
	}
	LILV_FOREACH(nodes, i, presets) {
		const LilvNode* preset = lilv_nodes_get(presets, i);
		lilv_world_load_resource(world, preset);
		LilvNodes* titles = lilv_world_find_nodes(
			world, preset, label_pred, NULL);
		if (titles) {
			const LilvNode* title = lilv_nodes_get_first(titles);
			printf("\t         %s\n", lilv_node_as_string(title));
			lilv_nodes_free(titles);
		} else {
			fprintf(stderr, "Preset <%s> has no rdfs:label\n",
			        lilv_node_as_string(lilv_nodes_get(presets, i)));
		}
	}
	lilv_nodes_free(presets);

#ifdef VERBOSE
	/* Ports */

	const uint32_t num_ports = lilv_plugin_get_num_ports(p);
	float* mins     = (float*)calloc(num_ports, sizeof(float));
	float* maxes    = (float*)calloc(num_ports, sizeof(float));
	float* defaults = (float*)calloc(num_ports, sizeof(float));
	lilv_plugin_get_port_ranges_float(p, mins, maxes, defaults);

	for (uint32_t i = 0; i < num_ports; ++i) {
		print_port(p, i, mins, maxes, defaults);
	}

	free(mins);
	free(maxes);
	free(defaults);
#endif /* VERBOSE */
}
void LV2Apply_printPorts(LilvWorld* world, const LilvPlugin* p)
{
	applies_to_pred     = lilv_new_uri(world, LV2_CORE__appliesTo);
	control_class       = lilv_new_uri(world, LILV_URI_CONTROL_PORT);
	event_class         = lilv_new_uri(world, LILV_URI_EVENT_PORT);
	group_pred          = lilv_new_uri(world, LV2_PORT_GROUPS__group);
	label_pred          = lilv_new_uri(world, LILV_NS_RDFS "label");
	preset_class        = lilv_new_uri(world, LV2_PRESETS__Preset);
	designation_pred    = lilv_new_uri(world, LV2_CORE__designation);
	supports_event_pred = lilv_new_uri(world, LV2_EVENT__supportsEvent);

	print_plugin(world, p);

	lilv_node_free(supports_event_pred);
	lilv_node_free(designation_pred);
	lilv_node_free(preset_class);
	lilv_node_free(label_pred);
	lilv_node_free(group_pred);
	lilv_node_free(event_class);
	lilv_node_free(control_class);
	lilv_node_free(applies_to_pred);

}

void LV2Apply_getPortCount(LV2Apply* self, unsigned int* in_audio,
	unsigned int* out_audio, unsigned int* in_ctl, unsigned int* out_ctl)
{
	unsigned int dummy;
	if(in_audio == NULL) in_audio = &dummy;
	if(out_audio == NULL) out_audio = &dummy;
	if(in_ctl == NULL) in_ctl = &dummy;
	if(out_ctl == NULL) in_ctl = &dummy;

	*in_audio = 0;
	*out_audio = 0;
	*in_ctl = 0;
	*out_ctl = 0;
	for (uint32_t p = 0; p < self->n_ports; ++p) {
		if (self->ports[p].type == TYPE_CONTROL) {
			if (self->ports[p].is_input) {
				(*in_ctl)++;
			} else {
				(*out_ctl)++;
			}
		} else {
			if (self->ports[p].is_input) {
				(*in_audio)++;
			} else {
				(*out_audio)++;
			}
		}
	}
	if(*in_audio != self->n_audio_in)
		fprintf(stderr, "Recounting the audio in ports does not match: %d %d\n", *in_audio, self->n_audio_in);
	if(*out_audio != self->n_audio_out)
		fprintf(stderr, "Recounting the audio out ports does not match: %d %d\n", *out_audio, self->n_audio_out);
}
// in_buf must point to self.n_audio_in arrays n_frames long
// out_buf must point to self.n_audio_out arrays n_frames long
void LV2Apply_connectPorts(LV2Apply* self)
{
	/* Connect ports */
	const LilvPlugin* plugin = self->plugin;
	const uint32_t n_ports = lilv_plugin_get_num_ports(plugin);
	float** in_bufs = self->in_bufs;
	float** out_bufs = self->out_bufs;
	for (uint32_t p = 0, i = 0, o = 0; p < n_ports; ++p) {
		if (self->ports[p].type == TYPE_CONTROL) {
			lilv_instance_connect_port(self->instance, p, &self->ports[p].value);
		} else if (self->ports[p].type == TYPE_AUDIO) {
			if (self->ports[p].is_input) {
				lilv_instance_connect_port(self->instance, p, in_bufs[i++]);
			} else {
				lilv_instance_connect_port(self->instance, p, out_bufs[o++]);
			}
		} else {
			lilv_instance_connect_port(self->instance, p, NULL);
		}
	}
}

void LV2Apply_getPortRanges(LV2Apply* self, unsigned int index, float* min, float* max, float* defaultValue)
{
	const LilvPlugin* plugin = self->plugin;
	const uint32_t num_ports = lilv_plugin_get_num_ports(plugin);
	if(index >= num_ports)
		return;
	float* mins     = (float*)calloc(num_ports, sizeof(float));
	float* maxes    = (float*)calloc(num_ports, sizeof(float));
	float* defaults = (float*)calloc(num_ports, sizeof(float));
	lilv_plugin_get_port_ranges_float(plugin, mins, maxes, defaults);

	*min = mins[index];
	*max = maxes[index];
	*defaultValue = defaults[index];

	free(mins);
	free(maxes);
	free(defaults);
}

const char* LV2Apply_getPortName(LV2Apply* self, unsigned int index)
{
	const LilvPlugin* plugin = self->plugin;
	const LilvPort* port = lilv_plugin_get_port_by_index(plugin, index);
	if (!port)
		return NULL;
	LilvNode* name = lilv_port_get_name(plugin, port);
	const char* ret = lilv_node_as_string(name);
	lilv_node_free(name);

	return ret;
}

port_type_t LV2Apply_getControlPortType(LV2Apply* self, LilvWorld* world, unsigned int index)
{

	LilvNode* control_class = lilv_new_uri(world, LILV_URI_CONTROL_PORT);
	LilvNode* input_class = lilv_new_uri(world, LILV_URI_INPUT_PORT);

	LilvNode* enum_property = lilv_new_uri(world, "http://lv2plug.in/ns/lv2core#enumeration");
	LilvNode* int_property= lilv_new_uri(world, "http://lv2plug.in/ns/lv2core#integer");
	LilvNode* toggled_property= lilv_new_uri(world, "http://lv2plug.in/ns/lv2core#toggled");

	const LilvPlugin* plugin = self->plugin;
	const LilvPort* port = lilv_plugin_get_port_by_index(plugin, index);
	if (!port)
		return kNotControl;
	if (!lilv_port_is_a(plugin, port, input_class))
		return kNotControl;
	if (!lilv_port_is_a(plugin, port, control_class))
		return kNotControl;

	port_type_t retVal = kFloat;
	if(lilv_port_has_property(plugin, port, toggled_property))
	{
		retVal = kToggle;
	}
	else if (lilv_port_has_property(plugin, port, enum_property))
	{
		retVal = kEnumerated;
	}
	else if (lilv_port_has_property(plugin, port, int_property))
	{
		retVal = kInteger;
	};


	lilv_node_free(control_class);
	lilv_node_free(input_class);
	lilv_node_free(enum_property);
	lilv_node_free(int_property);
	lilv_node_free(toggled_property);

	return retVal;
}

int LV2Apply_getPortIndex(LV2Apply* self, LilvWorld* world, char* symbol)
{
	const LilvPlugin* plugin = self->plugin;
	LilvNode* symbol_node = lilv_new_string(world, symbol);
	const LilvPort* port = lilv_plugin_get_port_by_symbol(plugin, symbol_node);
	int index = lilv_port_get_index(plugin, port);
	lilv_node_free(symbol_node);
	return index;
}

bool LV2Apply_isLogarithmic(LV2Apply* self, LilvWorld* world, unsigned int index)
{
	LilvNode* log_property = lilv_new_uri(world, "http://lv2plug.in/ns/ext/port-props#logarithmic");

	const LilvPlugin* plugin = self->plugin;
	const LilvPort* port = lilv_plugin_get_port_by_index(plugin, index);

	bool ret = false;
	if(lilv_port_has_property(plugin, port, log_property))
		ret = true;

	lilv_node_free(log_property);

	return ret;
}

bool LV2Apply_hasStrictBounds(LV2Apply* self, LilvWorld* world, unsigned int index)
{
	LilvNode* strict_property = lilv_new_uri(world, "http://lv2plug.in/ns/ext/port-props#hasStrictBounds");

	const LilvPlugin* plugin = self->plugin;
	const LilvPort* port = lilv_plugin_get_port_by_index(plugin, index);

	bool ret = false;
	if(lilv_port_has_property(plugin, port, strict_property))
		ret = true;

	lilv_node_free(strict_property);

	return ret;
}

const char* LV2Apply_getPluginName(LV2Apply* self)
{
	const LilvPlugin* plugin = self->plugin;
	LilvNode* plugin_name = lilv_plugin_get_name(plugin);
	const char* name = lilv_node_as_string(plugin_name);
	lilv_node_free(plugin_name);

	return name;

}
