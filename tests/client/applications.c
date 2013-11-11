
/*
 * Copyright (c) 2013, Ismael Gomez - gomezi@tcd.ie
 *
 */


#define _GNU_SOURCE
#include <unistd.h>

#include "core/liblwm2m.h"
#include "object.h"
#include "applications.h"

#include <sys/stat.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <pthread.h>
#include <string.h>

int exec_download(int instanceId);
int exec_run(int instanceId);
int exec_kill(int instanceId);

applications_t applicationsDB[NOF_APPLICATIONS];

#define APPLICATIONS_NOF_RESOURCEID		8

typedef struct {
	applications_t *appObject;
	char *cmd;
}run_console_cmd_t;

typedef enum {URL_TFTP, URL_UNKNOWN} URLproto_t;


objectConfig_t* newObjectApplications() {
	int j;
	objectConfig_t* config = newObjectConfig(APPLICATIONS_NOF_RESOURCEID);
	if (config != NULL) {
		objectAddResourceReadWrite(config, "Name", 0, STRING);
		objectResourceStringSize(config, 0, 64);
		objectAddResourceReadWrite(config, "Version", 1, STRING);
		objectResourceStringSize(config, 1, 64);
		objectAddResourceReadWrite(config, "URL", 2, STRING);
		objectResourceStringSize(config, 2, 64);
		objectAddResourceExec(config, "Download", 3, exec_download);
		objectAddResourceExec(config, "Run", 4, exec_run);
		objectAddResourceExec(config, "Kill", 5, exec_kill);
		objectAddResourceRead(config, "State", 6, INT);
		objectAddResourceRead(config, "Error Code", 7, INT);
		objectAddResourceReadWrite(config, "Args", 8, STRING);
		objectResourceStringSize(config, 8, 64);

		// we create now all the object instances, since CREATE command is not supported yet
		for (j=0;j<NOF_APPLICATIONS;j++) {
			objectSetStringValueInstance(config, 0, j, applicationsDB[j].name);
			objectSetStringValueInstance(config, 1, j, applicationsDB[j].version);
			objectSetStringValueInstance(config, 2, j, applicationsDB[j].URL);
			objectSetIntValueInstance(config, 6, j, &applicationsDB[j].state);
			objectSetIntValueInstance(config, 7, j, &applicationsDB[j].error);
			objectSetStringValueInstance(config, 8, j, applicationsDB[j].args);
		}
	}
	return config;
}

void *download_thread(void *arg) {
	run_console_cmd_t *obj = (run_console_cmd_t*) arg;
	int ret;
	ret = system(obj->cmd);
	obj->appObject->error = ret;
	if (ret) {
		obj->appObject->state = ERROR;
	} else {
		obj->appObject->state = DOWNLOADED;
		/* make executable */
		if (chmod(obj->appObject->name, S_IXUSR | S_IRUSR | S_IWUSR)) {
			perror("chmod");
		}
	}
	free(obj->cmd);
	free(obj);
	return NULL;
}

int download_cmd(applications_t *obj, char *cmd) {
	int ret;
	pthread_t thread;

	run_console_cmd_t *cmdObject = malloc(sizeof(run_console_cmd_t));
	if (cmdObject != NULL) {
		cmdObject->cmd = cmd;
		cmdObject->appObject = obj;
		ret = pthread_create(&thread, NULL, download_thread, cmdObject);
		if (ret != -1) {
			pthread_detach(thread);
		} else {
			free(cmdObject);
		}
	} else {
		ret = -1;
	}

	return ret;
}

URLproto_t getURLproto(char *URL) {
	if (strstr(URL, "tftp://")) {
		return URL_TFTP;
	} else {
		return URL_UNKNOWN;
	}
}

char *tftpHost(char *URL) {
	char *host = URL + strlen("tftp://");
	char *slash = strstr(host, "/");
	if (slash) {
		return strndup(host, slash - host);
	} else {
		return NULL;
	}
}

char *tftpFile(char *URL) {
	char *host = URL + strlen("tftp://");
	char *filename = strstr(host, "/") + 1;
	if (filename) {
		return strdup(filename);
	} else {
		return NULL;
	}
}

int exec_download(int instanceId) {
	char *host, *file, *cmd;
	int cmdlen;

	printf("Downloading %s from %s\n", applicationsDB[instanceId].name,
			applicationsDB[instanceId].URL);

	switch(getURLproto(applicationsDB[instanceId].URL)) {
	case URL_TFTP:
		host = tftpHost(applicationsDB[instanceId].URL);
		file = tftpFile(applicationsDB[instanceId].URL);
		if (!host || !file) {
			fprintf(stderr, "\r\nInvalid TFTP address %s\r\n",
					applicationsDB[instanceId].URL);
			return -1;
		}
		cmdlen = strlen(host)+strlen(file)+64;
		cmd = malloc(cmdlen);
		snprintf(cmd, cmdlen, "tftp %s -m binary -c get %s", host, file);
		applicationsDB[instanceId].state = DOWNLOADING;
		applicationsDB[instanceId].error = 0;
		free(host);
		free(file);
		return download_cmd(&applicationsDB[instanceId], cmd);

	case URL_UNKNOWN:
		fprintf(stderr, "\r\n Unknown URL protocol: %s\r\n",
				applicationsDB[instanceId].URL);
		return -1;
	}
	return 0;
}

int exec_run(int instanceId) {
	char *cmd, *path;
	int cmdLen, ret;
	printf("Running %s\n", applicationsDB[instanceId].name);
	path = get_current_dir_name();
	cmdLen = 2*(strlen(path) + strlen(applicationsDB[instanceId].name)) + 512;
	cmd = malloc(cmdLen);
	if (cmd != NULL) {
		snprintf(cmd, cmdLen, "start-stop-daemon --start --pidfile %s/%s.pid --startas %s/%s %s--background -m",
				path, applicationsDB[instanceId].name, path, applicationsDB[instanceId].name,
				applicationsDB[instanceId].args);
		ret = system(cmd);
		free(cmd);
		free(path);
		if (ret == 0) {
			applicationsDB[instanceId].state = RUNNING;
		} else {
			applicationsDB[instanceId].state = ERROR;
			applicationsDB[instanceId].error = ERROR_RUNNING;
		}
		return ret;
	} else {
		return -1;
	}
	return 0;
}

int exec_kill(int instanceId) {
	char *cmd, *path;
	int cmdLen, ret;
	printf("Killing %s\n", applicationsDB[instanceId].name);
	path = get_current_dir_name();
	cmdLen = (strlen(path) + strlen(applicationsDB[instanceId].name)) + 64;
	cmd = malloc(cmdLen);
	if (cmd != NULL) {
		snprintf(cmd, cmdLen, "start-stop-daemon --stop --pidfile %s/%s.pid",
				path, applicationsDB[instanceId].name);
		applicationsDB[instanceId].state = IDLE;
		ret = system(cmd);
		free(cmd);
		free(path);
		return ret;
	} else {
		return -1;
	}
	return 0;
}


int exec_check(int instanceId) {
	char *cmd, *path;
	int cmdLen, ret;
	printf("Checking %s\n", applicationsDB[instanceId].name);
	path = get_current_dir_name();
	cmdLen = (strlen(path) + strlen(applicationsDB[instanceId].name)) + 64;
	cmd = malloc(cmdLen);
	if (cmd != NULL) {
		snprintf(cmd, cmdLen, "start-stop-daemon --status --pidfile %s/%s.pid",
				path, applicationsDB[instanceId].name);
		ret = system(cmd);
		free(cmd);
		free(path);
		return ret;
	} else {
		return -1;
	}
	return 0;
}

void applications_update() {
	int i;

	for (i=0;i<NOF_APPLICATIONS;i++) {
		if (applicationsDB[i].state == RUNNING) {
			if (exec_check(i)) {
				applicationsDB[i].state = ERROR;
				applicationsDB[i].error = ERROR_RUNNING;
				fprintf(stdout, "\r\n Application %s is not running\r\n", applicationsDB[i].name);
			}
		}
	}

}

