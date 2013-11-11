/*
 * Copyright (c) 2013, Ismael Gomez - gomezi@tcd.ie
 *
 */
#include "core/liblwm2m.h"
#include "status.h"
#include "object.h"
#include "object_device.h"
#include "applications.h"

#include <sys/timerfd.h>
#include <string.h>
#include <unistd.h>

#include <sys/time.h>

#include <sys/timerfd.h>


status_t status_ctx;

int status_init() {
	struct itimerspec new_value;

	status_ctx.timerfd = timerfd_create(CLOCK_MONOTONIC, 0);

	if (status_ctx.timerfd < 0) {
		return -1;
	}

	memcpy(&new_value.it_interval, &status_ctx.period, sizeof(struct timespec));
	memcpy(&new_value.it_value, &status_ctx.period, sizeof(struct timespec));

	return timerfd_settime(status_ctx.timerfd, 0, &new_value, NULL);
}

int status_run() {
	device_state_update();
	applications_update();
	return 0;
}

void status_stop() {
	close(status_ctx.timerfd);
}



