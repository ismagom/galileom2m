/*
 * Copyright (c) 2013, Ismael Gomez - gomezi@tcd.ie
 *
 */


typedef struct {
	lwm2m_context_t * lwm2mH;
	lwm2m_object_t ** objArray;
	struct timespec period;
	int nof_objects;
	int timerfd;
}status_t;

int status_init();
int status_run();
void status_stop();
