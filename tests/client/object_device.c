
/*
 * Copyright (c) 2013, Ismael Gomez - gomezi@tcd.ie
 *
 */

#include "core/liblwm2m.h"
#include "object.h"
#include "object_device.h"

#include <stdio.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#define PRV_MANUFACTURER      	"CTVR"
#define PRV_MODEL_NUMBER      	"Galileo Board"
#define PRV_SERIAL_NUMBER     	"123456789"
#define PRV_LWM2M_VERSION  		"1.0"
#define PRV_FIRMWARE_VERSION  	"1.0"
#define PRV_SUPPORTED_BINDING  	"U"	// UDP

int exec_reboot(int instanceId);
int exec_reset(int instanceId);
int exec_reset_error(int instanceId);


/* default values for device state parameters */
device_state_t device_state = {
			{POWER_SOURCE_DC, POWER_SOURCE_EXT_BATTERY, POWER_SOURCE_SOLAR},
			{100, 80, 80},
			{10, 8, 8},
			0, 0, 0, 0, 0, "+00.00", "GMT"};

#define DEVICE_NOF_RESOURCEID	17

objectConfig_t* newObjectDevice() {
	int i;
	objectConfig_t* config = newObjectConfig(DEVICE_NOF_RESOURCEID);
	if (config != NULL) {
		objectAddResourceString(config, "Manufacturer", 0, PRV_MANUFACTURER);
		objectAddResourceString(config, "Model Number", 1, PRV_MODEL_NUMBER);
		objectAddResourceString(config, "Serial Number", 2, PRV_SERIAL_NUMBER);
		objectAddResourceString(config, "Firmware version", 3, PRV_FIRMWARE_VERSION);
		objectAddResourceExec(config, "Reboot", 4, exec_reboot);
		objectAddResourceExec(config, "Factory Reset", 5, exec_reset);
		objectAddResourceRead(config, "Available Power Sources", 6, INT);
		objectAddResourceRead(config, "Power Source Voltage", 7, INT);
		objectAddResourceRead(config, "Power Source Current", 8, INT);

		for (i=0;i<NOF_POWER_SOURCES;i++) {
			objectSetIntValueInstanceMultiple(config, 6, 0, i, &device_state.power_sources[i]);
			objectSetIntValueInstanceMultiple(config, 7, 0, i, &device_state.ps_voltage[i]);
			objectSetIntValueInstanceMultiple(config, 8, 0, i, &device_state.ps_current[i]);
		}
		objectAddResourceInt(config, "Battery Level", 9, &device_state.battery_level);
		objectAddResourceInt(config, "Memory free", 10, &device_state.memory_free);
		objectAddResourceInt(config, "Error Code", 11, &device_state.error_code);
		objectAddResourceExec(config, "Reset Error Code", 12, exec_reset_error);
		objectAddResourceIntRW(config, "Error Code", 13, &device_state.cur_time);
		objectAddResourceStringRW(config, "UTC Offset", 14, device_state.utc_offset, 64);
		objectAddResourceStringRW(config, "Timezone", 15, device_state.time_zone, 64);
		objectAddResourceString(config, "Supported Binding and Modes", 16, PRV_SUPPORTED_BINDING);
	}
	return config;
}

int exec_reboot(int instanceId) {
	printf("Rebooting\n");
	return 0;
}

int exec_reset(int instanceId) {
	printf("Rebooting\n");
	return 0;
}

int exec_reset_error(int instanceId) {
	printf("Reset error\n");
	return -1;
}

/** This function returns the free memory */
long long free_memory() {
#ifdef KK
	unsigned long long ps = sysconf(_SC_PAGESIZE);
	unsigned long long pn = sysconf(_SC_AVPHYS_PAGES);
	return ps * pn;
#else
	return 0;
#endif
}

#define GPIO_MAP_MAX 	2
char* gpio_map[GPIO_MAP_MAX] = {"37", "36"};

int sysfs_write(char *str, char *file) {
	FILE *f;
	int ret = -1;
	f = fopen(file, "w");
	if (f != NULL) {
		if (-1 != fwrite(str, strlen(str), 1, f)) {
			ret = 0;
		} else {
			perror("fwrite");
		}
	} else {
		perror("fopen");
	}
	fclose(f);
	return ret;
}

int sysfs_read(char *file) {
	FILE *f;
	int ret = -1;
	f = fopen(file, "r");
	if (f != NULL) {
		char tmp[64];
		if (-1 != fread(tmp, 64, 1, f)) {
			ret = atoi(tmp);
		} else {
			perror("fread");
		}
	} else {
		perror("fopen");
	}
	fclose(f);
	return ret;
}

int ad_init(int idx) {
#ifdef ENABLE_AD
	if (idx >=0 && idx < GPIO_MAP_MAX) {
		char tmp[256];
		sysfs_write(gpio_map[idx], "/sys/class/gpio/export");
		snprintf(tmp, 256, "/sys/class/gpio/gpio%s/direction",gpio_map[idx]);
		sysfs_write("out", tmp);
		snprintf(tmp, 256, "/sys/class/gpio/gpio%s/value",gpio_map[idx]);
		sysfs_write("0", tmp);
		return 0;
	} else {
		return -1;
	}
#endif
}

uint64_t ad_read(int idx) {
#ifdef ENABLE_AD
	char tmp[256];
	snprintf(tmp, 256, "/sys/bus/iio/devices/iio:device0/in_voltage%d_raw",idx);
	return (uint64_t) sysfs_read(tmp);
#else
	return 0;
#endif
}

static int first_time = 0;

/* This function is called periodically by status_run(). It updates the device_state_t
 * data, which is accessed by the READ command in object device
 */
void device_state_update() {
	struct timeval now;

	if (!first_time) {
		first_time = 1;
		ad_init(0);
		ad_init(1);
	}

	gettimeofday(&now, NULL);
	device_state.cur_time = now.tv_sec;
	device_state.ps_voltage[0] = 5000;
	device_state.ps_current[0] = 500;

	device_state.ps_current[1] = 500;
	device_state.ps_voltage[1] = ad_read(0);

	device_state.ps_current[2] = 500;
	device_state.ps_voltage[2] = ad_read(1);

	device_state.error_code = 0;
	device_state.memory_free = free_memory();
	device_state.state_code = 0;
}



#ifdef TIME_OFFSET_CHAR
/*
 Copyright (c) 2013, Intel Corporation

 Redistribution and use in source and binary forms, with or without modification,
 are permitted provided that the following conditions are met:

 * Redistributions of source code must retain the above copyright notice,
 this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 this list of conditions and the following disclaimer in the documentation
 and/or other materials provided with the distribution.
 * Neither the name of Intel Corporation nor the names of its contributors
 may be used to endorse or promote products derived from this software
 without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 THE POSSIBILITY OF SUCH DAMAGE.

 David Navarro <david.navarro@intel.com>

 */

// basic check that the time offset value is at ISO 8601 format
// bug: +12:30 is considered a valid value by this function
static int prv_check_time_offset(char * buffer, int length) {
	int min_index;

	if (length != 3 && length != 5 && length != 6)
		return 0;
	if (buffer[0] != '-' && buffer[0] != '+')
		return 0;
	switch (buffer[1]) {
	case '0':
		if (buffer[2] < '0' || buffer[2] > '9')
			return 0;
		break;
	case '1':
		if (buffer[2] < '0' || buffer[2] > '2')
			return 0;
		break;
	default:
		return 0;
	}
	switch (length) {
	case 3:
		return 1;
	case 5:
		min_index = 3;
		break;
	case 6:
		if (buffer[3] != ':')
			return 0;
		min_index = 4;
		break;
	default:
		// never happen
		return 0;
	}
	if (buffer[min_index] < '0' || buffer[min_index] > '5')
		return 0;
	if (buffer[min_index + 1] < '0' || buffer[min_index + 1] > '9')
		return 0;

	return 1;
}
#endif


