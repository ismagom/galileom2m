
/*
 * Copyright (c) 2013, Ismael Gomez - gomezi@tcd.ie
 *
 */

#include "core/liblwm2m.h"
#include "object.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#define PRV_TLV_BUFFER_SIZE 128

#define GET_OBJCONFIG(objectP) (objectConfig_t*) objectP->userData
#define GET_RESOURCES(objectP) ((objectConfig_t*) objectP->userData)->resources
#define GET_NOF_RESOURCES(objectP) ((objectConfig_t*) objectP->userData)->nof_resources
#define GET_NOF_INSTANCES(objectP) ((objectConfig_t*) objectP->userData)->nof_instances

static resourceDB_t* findId(objectConfig_t* objectConfig, int resourceId) {
	int i = 0;
	while(i<objectConfig->nof_resources && objectConfig->resources[i].id != resourceId) {
		i++;
	}
	if (i == objectConfig->nof_resources) {
		return NULL;
	} else {
		return &objectConfig->resources[i];
	}
}


static void prv_output_buffer(uint8_t * buffer, int length) {
	int i;
	uint8_t array[16];

	i = 0;
	while (i < length) {
		int j;
		fprintf(stderr, "  ");

		memcpy(array, buffer + i, 16);

		for (j = 0; j < 16 && i + j < length; j++) {
			fprintf(stderr, "%02X ", array[j]);
		}
		while (j < 16) {
			fprintf(stderr, "   ");
			j++;
		}
		fprintf(stderr, "  ");
		for (j = 0; j < 16 && i + j < length; j++) {
			if (isprint(array[j]))
				fprintf(stderr, "%c ", array[j]);
			else
				fprintf(stderr, ". ");
		}
		fprintf(stderr, "\n");

		i += 16;
	}
}

static int prv_get_object_tlv(lwm2m_object_t * objectP, char ** bufferP) {
	int length = 0;
	int result;
	char temp_buffer[64];
	int temp_length;
	int i;
	int n;

	resourceDB_t* resources = GET_RESOURCES(objectP);

	*bufferP = (uint8_t *) malloc(PRV_TLV_BUFFER_SIZE);

	if (NULL == *bufferP)
		return 0;

	if (GET_NOF_INSTANCES(objectP) > 1) {
		fprintf(stderr, "\r\n Can't send multiple object instances. Sending instance 0\r\n");
	}

	for (i=0; i<GET_NOF_RESOURCES(objectP); i++) {
		if (resources[i].at != E) {
			/* resource is a string */
			if (resources[i].type == STRING) {
				if (resources[i].nof_values == 1) {
					if (resources[i].values[0][0])
					result = lwm2m_opaqueToTLV(TLV_RESSOURCE,
							resources[i].values[0][0], strlen(resources[i].values[0][0]), resources[i].id, *bufferP + length,
							PRV_TLV_BUFFER_SIZE - length);
					if (0 == result)
						goto error;
					length += result;
				} else {
					fprintf(stderr, "\r\n Multiple string resources not supported (/%d/%d/%d)\r\n", objectP->objID, 0, i);
				}
			} else {
				/* single value */
				if (resources[i].nof_values == 1
						&& resources[i].values[0][0]) {
					result = lwm2m_intToTLV(TLV_RESSOURCE, *((uint64_t*) resources[i].values[0][0]), resources[i].id,
							*bufferP + length, PRV_TLV_BUFFER_SIZE - length);
					if (0 == result)
						goto error;
					length += result;

				/* multiple instances */
				} else {
					result = 0;
					temp_length = 0;
					for (n=0; n<resources[i].nof_values; n++) {
						if (resources[i].values[0][n]) {
							result = lwm2m_intToTLV(TLV_RESSOURCE_INSTANCE, *((uint64_t*) resources[i].values[0][n]), n,
									temp_buffer + temp_length, 64 - temp_length);
							if (0 == result)
								goto error;
							temp_length += result;
						}
					}
					result = lwm2m_opaqueToTLV(TLV_MULTIPLE_INSTANCE, temp_buffer, temp_length,
							resources[i].id, *bufferP + length, PRV_TLV_BUFFER_SIZE - length);
					if (0 == result)
						goto error;
					length += result;
				}
			}
		}
	}


	fprintf(stderr, "TLV (%d bytes):\r\n", length);
	prv_output_buffer(*bufferP, length);

	return length;

error: fprintf(stderr, "TLV generation failed:\r\n");
	free(*bufferP);
	*bufferP = NULL;
	return 0;
}

static uint8_t prv_object_read(lwm2m_uri_t * uriP, char ** bufferP,
		int * lengthP, lwm2m_object_t * objectP) {
	int n;

	*bufferP = NULL;
	*lengthP = 0;

	resourceDB_t* resource = findId(GET_OBJCONFIG(objectP), uriP->resourceId);
	if (!resource) {
		printf("Resource %u/%u/%u not found\n", uriP->objectId,uriP->instanceId, uriP->resourceId);
		return COAP_404_NOT_FOUND;
	}
	if (!LWM2M_URI_IS_SET_INSTANCE(uriP)) {
		uriP->instanceId = 0;
	}

	fprintf(stdout, "\n\t Received READ for object %u resource %s\n", uriP->objectId, resource->desc);

	// is the server asking for the full object ?
	if (!LWM2M_URI_IS_SET_RESOURCE(uriP)) {
		*lengthP = prv_get_object_tlv(objectP, bufferP);
		if (0 != *lengthP) {
			return COAP_205_CONTENT ;
		} else {
			return COAP_500_INTERNAL_SERVER_ERROR ;
		}
	}

	if (resource->at == E) {
		return COAP_405_METHOD_NOT_ALLOWED ;
	}


	/* Resource is a string */
	if (resource->type == STRING) {
		if (resource->nof_values == 1) {
			*bufferP = strdup(resource->values[uriP->instanceId][0]);
			if (NULL != *bufferP) {
				*lengthP = strlen(*bufferP);
				return COAP_205_CONTENT ;
			} else {
				return COAP_500_INTERNAL_SERVER_ERROR ;
			}
		} else {
			return COAP_501_NOT_IMPLEMENTED;
		}
	/* Resource is an integer */
	} else {
		/* Does not support multiple instances */
		if (resource->nof_values == 1) {
			if (resource->values[uriP->instanceId][0]) {
				*lengthP = lwm2m_int64ToPlainText(*((uint64_t*) resource->values[uriP->instanceId][0]), bufferP);
				if (0 != *lengthP) {
					return COAP_205_CONTENT;
				} else {
					return COAP_500_INTERNAL_SERVER_ERROR ;
				}
			} else {
				return COAP_500_INTERNAL_SERVER_ERROR ;
			}
		/* Multiple values */
		} else {
			char buffer1[64];
			char buffer2[64];
			int result = 0;
			int instance_length = 0;
			printf("TLV: %d values\n", resource->nof_values);
			for (n=0; n<resource->nof_values; n++) {
				if (resource->values[uriP->instanceId][n]) {
					printf("value %d = %d\n",n,*((uint64_t*) resource->values[uriP->instanceId][n]));
					result = lwm2m_intToTLV(TLV_RESSOURCE_INSTANCE, *((uint64_t*) resource->values[uriP->instanceId][n]), n,
							buffer1 + instance_length, 64 - instance_length);
					if (0 == result) {
						return COAP_500_INTERNAL_SERVER_ERROR;
					}
					instance_length += result;
				} else {
					return COAP_500_INTERNAL_SERVER_ERROR;
				}
			}
			*lengthP = lwm2m_opaqueToTLV(TLV_MULTIPLE_INSTANCE, buffer1,
					instance_length, resource->id, buffer2, 64);

			if (0 == *lengthP)
				return COAP_500_INTERNAL_SERVER_ERROR ;

			*bufferP = (char *) malloc(*lengthP);
			if (NULL == *bufferP)
				return COAP_500_INTERNAL_SERVER_ERROR ;

			memmove(*bufferP, buffer2, *lengthP);

			fprintf(stderr, "TLV (%d bytes):\r\n", *lengthP);
			prv_output_buffer(*bufferP, *lengthP);

			return COAP_205_CONTENT ;
		}
	}
}

static uint8_t prv_object_write(lwm2m_uri_t * uriP, char * buffer, int length,
		lwm2m_object_t * objectP) {
	int maxvalue;
	if (!LWM2M_URI_IS_SET_RESOURCE(uriP))
		return COAP_501_NOT_IMPLEMENTED ;

	resourceDB_t* resource = findId(GET_OBJCONFIG(objectP), uriP->resourceId);
	if (!resource) {
		return COAP_404_NOT_FOUND;
	}
	if (!LWM2M_URI_IS_SET_INSTANCE(uriP)) {
		uriP->instanceId = 0;
	}

	if (resource->at != RW) {
		return COAP_405_METHOD_NOT_ALLOWED ;
	}

	fprintf(stdout, "\n\t Received WRITE for object %u resource %s\n", uriP->objectId, resource->desc);
	if (resource->type == STRING) {
		if (resource->nof_values == 1) {
			maxvalue = length;
			if (maxvalue > resource->str_buff_sz) {
				maxvalue = resource->str_buff_sz;
			}
			memcpy(resource->values[uriP->instanceId][0], buffer, maxvalue);
			*((char*) resource->values[uriP->instanceId][0]+maxvalue) = '\0';

			return COAP_204_CHANGED ;
		} else {
			fprintf(stderr, "\r\n Multiple string values not supported\r\n");
			return COAP_501_NOT_IMPLEMENTED;
		}
	} else {
		if (resource->nof_values == 1) {
			if (resource->values[uriP->instanceId][0]) {
				if (1 == lwm2m_PlainTextToInt64(buffer, length, resource->values[uriP->instanceId][0])) {
					return COAP_204_CHANGED ;
				} else {
					return COAP_400_BAD_REQUEST ;
				}
			} else {
				fprintf(stderr, "\n\t Object %u resource %s instance %d has no value pointer\r\n",
						uriP->objectId, resource->desc, uriP->instanceId);
				return COAP_400_BAD_REQUEST ;
			}
		} else {
			fprintf(stdout, "\n\t TLV to int not implemented\n");
			return COAP_400_BAD_REQUEST;
		}
	}
}

static uint8_t prv_object_execute(lwm2m_uri_t * uriP, char * buffer, int length,
		lwm2m_object_t * objectP) {

	if (length != 0)
		return COAP_400_BAD_REQUEST ;

	if (!LWM2M_URI_IS_SET_RESOURCE(uriP))
		return COAP_501_NOT_IMPLEMENTED ;

	resourceDB_t* resource = findId(GET_OBJCONFIG(objectP), uriP->resourceId);
	if (!resource) {
		return COAP_404_NOT_FOUND;
	}

	if (!LWM2M_URI_IS_SET_INSTANCE(uriP)) {
		uriP->instanceId = 0;
	}

	if (resource->at != E) {
		return COAP_405_METHOD_NOT_ALLOWED ;
	}

	fprintf(stdout, "\n\t Received EXEC for object %u resource %s\n", uriP->objectId, resource->desc);

	if (resource->exec_fnc) {
		if (resource->exec_fnc(uriP->instanceId)) {
			return COAP_400_BAD_REQUEST;
		} else {
			return COAP_204_CHANGED;
		}
	} else {
		return COAP_204_CHANGED;
	}
}

lwm2m_object_t * newObject(uint16_t objID, objectConfig_t *objectConfig) {
	lwm2m_object_t * deviceObj;

	deviceObj = (lwm2m_object_t *) malloc(sizeof(lwm2m_object_t));

	if (NULL != deviceObj) {
		memset(deviceObj, 0, sizeof(lwm2m_object_t));

		deviceObj->objID = objID;
		deviceObj->readFunc = prv_object_read;
		deviceObj->writeFunc = prv_object_write;
		deviceObj->executeFunc = prv_object_execute;
		deviceObj->userData = objectConfig;
	}

	return deviceObj;
}

objectConfig_t* newObjectConfig(int nof_resources) {
	objectConfig_t * objectConfig;
	resourceDB_t* resourceDB;
	int i;

	objectConfig = (objectConfig_t *) malloc(sizeof(objectConfig_t));
	if (NULL != objectConfig) {
		resourceDB = (resourceDB_t*) calloc(nof_resources, sizeof(resourceDB_t));
		if (NULL != resourceDB) {
			objectConfig->nof_resources = nof_resources;
			objectConfig->resources = resourceDB;
			objectConfig->nof_instances = 1;
			for (i=0;i<nof_resources;i++) {
				objectConfig->resources[i].id = -1;
			}
		} else {
			free(objectConfig);
			objectConfig = NULL;
		}
	}
	return objectConfig;
}

static int _objectAddResource(objectConfig_t* objectConfig, int resourceId, char *desc, resType_t type,
		resAccType_t at, exec_fnc_t exec_fnc) {
	int i, freeIdx;
	if (!desc || resourceId < 0) {
		return -1;
	}

	/* check it does not exist while save first free position */
	i = 0;
	freeIdx = -1;
	while(i<objectConfig->nof_resources && objectConfig->resources[i].id != resourceId) {
		if (objectConfig->resources[i].id == -1 && freeIdx == -1) {
			freeIdx = i;
		}
		i++;
	}
	if (i == objectConfig->nof_resources) {
		if (freeIdx == -1) {
			fprintf(stderr, "\r\n Can't create more resources\r\n");
			return -1;
		} else {
			i = freeIdx;
		}
	}
	objectConfig->resources[i].id = resourceId;
	objectConfig->resources[i].desc = desc;
	objectConfig->resources[i].type = type;
	objectConfig->resources[i].at = at;
	objectConfig->resources[i].exec_fnc = exec_fnc;
	objectConfig->resources[i].nof_values = 1;
	return i;
}

int objectAddResourceRead(objectConfig_t* objectConfig, char *desc, int resourceId, resType_t type) {
	return _objectAddResource(objectConfig, resourceId, desc, type, R, NULL);
}

int objectAddResourceReadWrite(objectConfig_t* objectConfig, char *desc, int resourceId, resType_t type)  {
	return _objectAddResource(objectConfig, resourceId, desc, type, RW, NULL);
}

int objectAddResourceExec(objectConfig_t* objectConfig, char *desc, int resourceId, exec_fnc_t exec_fnc) {
	return _objectAddResource(objectConfig, resourceId, desc, INT, E, exec_fnc);
}

int objectResourceStringSize(objectConfig_t* objectConfig, int resourceId, int str_buff_sz) {
	resourceDB_t* resource = findId(objectConfig, resourceId);
	if (resource != NULL) {
		resource->str_buff_sz = str_buff_sz;
		return 0;
	} else {
		return -1;
	}
}

int objectAddResourceString(objectConfig_t* objectConfig, char *desc, int resourceId, char *value) {
	if (objectAddResourceRead(objectConfig, desc, resourceId, STRING) != -1) {
		objectResourceStringSize(objectConfig, resourceId, strlen(value));
		return objectSetStringValue(objectConfig, resourceId, value);
	} else {
		return -1;
	}
}

int objectAddResourceStringRW(objectConfig_t* objectConfig, char *desc, int resourceId, char *value, int str_buff_sz) {
	if (objectAddResourceReadWrite(objectConfig, desc, resourceId, STRING) != -1) {
		objectResourceStringSize(objectConfig, resourceId, str_buff_sz);
		return objectSetStringValue(objectConfig, resourceId, value);
	} else {
		return -1;
	}
}

int objectAddResourceInt(objectConfig_t* objectConfig, char *desc, int resourceId, uint64_t *value) {
	if (objectAddResourceRead(objectConfig, desc, resourceId, INT) != -1) {
		return objectSetIntValue(objectConfig, resourceId, value);
	} else {
		return -1;
	}
}

int objectAddResourceIntRW(objectConfig_t* objectConfig, char *desc, int resourceId, uint64_t *value) {
	if (objectAddResourceReadWrite(objectConfig, desc, resourceId, INT) != -1) {
		return objectSetIntValue(objectConfig, resourceId, value);
	} else {
		return -1;
	}
}

int objectSetStringValueInstanceMultiple(objectConfig_t* objectConfig, int resourceId, int instanceId, int idx, char *value) {
	resourceDB_t* resource = findId(objectConfig, resourceId);
	if (resource != NULL) {
		if (resource->type != STRING) {
			return -1;
		} else {
			if (instanceId > MAX_OBJ_INSTANCES || idx > MAX_DATA_VALUES) {
				return -1;
			} else {
				if (idx > resource->nof_values) {
					resource->nof_values = idx;
				}
				if (instanceId > objectConfig->nof_instances) {
					objectConfig->nof_instances = instanceId;
				}
				resource->values[instanceId][idx] = value;
				return 0;
			}
		}
	} else {
		return -1;
	}
}
int objectSetStringValue(objectConfig_t* objectConfig, int resourceId, char *value) {
	return objectSetStringValueInstanceMultiple(objectConfig, resourceId, 0, 0, value);
}

int objectSetStringValueInstance(objectConfig_t* objectConfig, int resourceId, int instanceId, char *value)  {
	return objectSetStringValueInstanceMultiple(objectConfig, resourceId, instanceId, 0, value);
}

int objectSetIntValueInstanceMultiple(objectConfig_t* objectConfig, int resourceId, int instanceId, int idx, uint64_t *value) {
	resourceDB_t* resource = findId(objectConfig, resourceId);
	if (resource != NULL) {
		if (resource->type != INT) {
			return -1;
		} else {
			if (instanceId > MAX_OBJ_INSTANCES || idx > MAX_DATA_VALUES) {
				return -1;
			} else {
				if (idx > resource->nof_values) {
					resource->nof_values = idx+1;
				}
				if (instanceId > objectConfig->nof_instances) {
					objectConfig->nof_instances = instanceId;
				}
				resource->values[instanceId][idx] = value;
				return 0;
			}
		}
	} else {
		return -1;
	}
}

int objectSetIntValue(objectConfig_t* objectConfig, int resourceId, uint64_t *value) {
	return objectSetIntValueInstanceMultiple(objectConfig, resourceId, 0, 0, value);
}

int objectSetIntValueInstance(objectConfig_t* objectConfig, int resourceId, int instanceId, uint64_t *value) {
	return objectSetIntValueInstanceMultiple(objectConfig, resourceId, instanceId, 0, value);
}



