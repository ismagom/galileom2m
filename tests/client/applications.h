
/*
 * Copyright (c) 2013, Ismael Gomez - gomezi@tcd.ie
 *
 */

#define IDLE		0
#define DOWNLOADING	1
#define DOWNLOADED	2
#define RUNNING		3
#define ERROR		4

#define NO_ERROR		0
#define NO_SPACE		2
#define OUT_OF_MEMORY	3
#define CONNECTION_LOST	4
#define CRC_FAILURE		5
#define ERROR_RUNNING	6
#define INVALID_URL		7

typedef struct {
	char name[64];
	char args[64];
	char version[64];
	char URL[64];
	uint64_t state;
	uint64_t error;
}applications_t;


#define NOF_APPLICATIONS	2

objectConfig_t* newObjectApplications();
void applications_update();
