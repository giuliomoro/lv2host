/** Control port value set from the command line */
typedef struct Param {
	const char* sym;    ///< Port symbol
	float       value;  ///< Control value
} Param;

/** Port type (only float ports are supported) */
typedef enum {
	TYPE_CONTROL,
	TYPE_AUDIO
} PortType;

/** Runtime port information */
typedef struct {
	const LilvPort* lilv_port;  ///< Port description
	PortType        type;       ///< Datatype
	uint32_t        index;      ///< Port index
	float           minValue;      ///< Control value min (if applicable)
	float           maxValue;      ///< Control value max (if applicable)
	float           value;      ///< Control value (if applicable)
	bool            is_input;   ///< True iff an input port
	bool            optional;   ///< True iff connection optional
} Port;

/** Application state */
typedef struct _lv2apply {
	const LilvPlugin* plugin;
	LilvInstance*     instance;
	unsigned          n_params;
	Param*            params;
	unsigned          n_ports;
	unsigned          n_audio_in;
	unsigned          n_audio_out;
	float** in_bufs;
	float** out_bufs;
	Port*             ports;
	bool bypass;
} LV2Apply;

