
#define MAX_OBJ_INSTANCES 	10
#define MAX_DATA_VALUES 	10

typedef int (*exec_fnc_t) (int instanceId);

typedef enum {
	R, RW, E
} resAccType_t; /* Resource Access Type */

typedef enum {
	STRING, INT
}resType_t;

typedef struct {
	char *desc;
	int id;
	resAccType_t at;
	resType_t type;
	exec_fnc_t exec_fnc;
	int nof_values;
	int str_buff_sz;
	void *values[MAX_OBJ_INSTANCES][MAX_DATA_VALUES];
} resourceDB_t;


typedef struct {
	resourceDB_t *resources;
	int nof_resources;
	int nof_instances;
}objectConfig_t;

objectConfig_t* newObjectConfig(int nof_resources);
lwm2m_object_t * newObject(uint16_t objID, objectConfig_t *objectConfig);

int objectAddResourceRead(objectConfig_t* objectConfig, char *desc, int resourceId, resType_t type);
int objectAddResourceReadWrite(objectConfig_t* objectConfig, char *desc, int resourceId, resType_t type);
int objectAddResourceExec(objectConfig_t* objectConfig, char *desc, int resourceId, exec_fnc_t exec_fnc);
int objectAddResourceString(objectConfig_t* objectConfig, char *desc, int resourceId, char *value);
int objectAddResourceStringRW(objectConfig_t* objectConfig, char *desc, int resourceId, char *value, int str_buff_sz);
int objectResourceStringSize(objectConfig_t* objectConfig, int resourceId, int str_buff_sz);
int objectAddResourceInt(objectConfig_t* objectConfig, char *desc, int resourceId, uint64_t *value);
int objectAddResourceIntRW(objectConfig_t* objectConfig, char *desc, int resourceId, uint64_t *value);

int objectResourceStringSize(objectConfig_t* objectConfig, int resourceId, int str_buff_sz);

int objectSetStringValue(objectConfig_t* objectConfig, int resourceId, char *value); // set to instance 0 by default
int objectSetStringValueInstance(objectConfig_t* objectConfig, int resourceId, int instanceId, char *value);
int objectSetStringValueInstanceMultiple(objectConfig_t* objectConfig, int resourceId, int instanceId, int idx, char *value);

int objectSetIntValue(objectConfig_t* objectConfig, int resourceId, uint64_t *value); // set to instance 0 by default
int objectSetIntValueInstance(objectConfig_t* objectConfig, int resourceId, int instanceId, uint64_t *value);
int objectSetIntValueInstanceMultiple(objectConfig_t* objectConfig, int resourceId, int instanceId, int idx, uint64_t *value);

