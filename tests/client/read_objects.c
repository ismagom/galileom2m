/*
 * Copyright (c) 2013, Ismael Gomez - gomezi@tcd.ie
 *
 */

#include "core/liblwm2m.h"
#include "object.h"
#include "read_objects.h"
#include "object_device.h"
#include "applications.h"

#include <stdio.h>
#include <stdlib.h>

#define NOF_OBJECTS 	2

int read_objects(lwm2m_object_t*** objArray) {
	objectConfig_t *objectConfig;

	*objArray = (lwm2m_object_t**) malloc(NOF_OBJECTS * sizeof(lwm2m_object_t*));
	if (*objArray) {
		objectConfig = newObjectDevice();
		(*objArray)[0] = newObject(3, objectConfig);
		if (NULL == (*objArray)[0]) {
			fprintf(stderr, "Failed to create Device object\r\n");
			return -1;
		}
		objectConfig = newObjectApplications();
		(*objArray)[1] = newObject(10, objectConfig);
		if (NULL == (*objArray)[1]) {
			fprintf(stderr, "Failed to create Device object\r\n");
			return -1;
		}
	} else {
		return -1;
	}

	return NOF_OBJECTS;
}

void free_objects(lwm2m_object_t** objArray) {
	int i;
	for (i=0;i<NOF_OBJECTS;i++) {
		free(objArray[i]->userData);
		free(objArray[i]);
	}
	free(objArray);
}
