


#define NOF_POWER_SOURCES	3

#define POWER_SOURCE_DC				0
#define POWER_SOURCE_EXT_BATTERY	2
#define POWER_SOURCE_SOLAR			7

typedef struct {
	uint64_t power_sources[NOF_POWER_SOURCES];
	uint64_t ps_voltage[NOF_POWER_SOURCES];
	uint64_t ps_current[NOF_POWER_SOURCES];
	uint64_t battery_level;
	uint64_t memory_free;
	uint64_t state_code;
	uint64_t error_code;
	uint64_t cur_time;
	char utc_offset[64];
	char time_zone[64];
} device_state_t;

objectConfig_t* newObjectDevice();
void device_state_update();
