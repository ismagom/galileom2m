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

/* Modifications by Ismael Gomez - gomezi@tcd.ie
 *
 */

#include "core/liblwm2m.h"
#include "commandline.h"
#include "read_objects.h"
#include "status.h"

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <signal.h>

extern status_t status_ctx;

#define MAX_PACKET_SIZE 128

static int g_quit = 0;

static void prv_quit(char * buffer, void * user_data) {
	g_quit = 1;
}

void handle_sigint(int signum) {
	g_quit = 2;
}

void print_usage(void) {
	fprintf(stderr, "Usage: lwm2mclient\r\n");
	fprintf(stderr, "Launch a LWM2M client.\r\n\n");
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

int get_remote_socket(char *ip, int port) {
	struct sockaddr_in6 addr;
	int s = -1;

	bzero(&addr,sizeof(addr));
	addr.sin6_family = AF_INET6;
	addr.sin6_port=htons(port);
	if (inet_pton(AF_INET6, ip, &(addr.sin6_addr)) != 1) {
		perror("inet_pton");
		return -1;
	}

	printf("Connecting to %s:%d\n", ip, port);

	s = socket(PF_INET6, SOCK_DGRAM, 0);
	if (s >= 0) {
		if (connect(s, (struct sockaddr*) &addr, (socklen_t) sizeof(struct sockaddr_in6))) {
			close(s);
			s = -1;
			perror("connect");
		}
	} else {
		perror("socket");
	}
	return s;
}


int get_remote_socket_ip4(char *ip, int port) {
	struct sockaddr_in addr;
	int s = -1;

	bzero(&addr,sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr=inet_addr(ip);
	addr.sin_port=htons(port);

	printf("Connecting to %s:%d\n", ip, port);

	s = socket(PF_INET, SOCK_DGRAM, 0);
	if (s >= 0) {
		if (connect(s, (struct sockaddr*) &addr, (socklen_t) sizeof(addr))) {
			close(s);
			s = -1;
			perror("connect");
		}
	} else {
		perror("socket");
	}
	if (s>=0) {
		if (-1 == write(s, &addr, sizeof(addr))) {
			perror("send");
		}
	}
	printf("open socket %d\n", s);
	return s;
}

int get_socket() {
	int s = -1;
	struct addrinfo hints;
	struct addrinfo *res;
	struct addrinfo *p;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = PF_INET6;
	hints.ai_socktype = SOCK_DGRAM;
	hints.ai_flags = AI_PASSIVE;

	getaddrinfo(NULL, "5683", &hints, &res);

	for (p = res; p != NULL && s == -1; p = p->ai_next) {
		s = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (s >= 0) {
			if (-1 == bind(s, p->ai_addr, p->ai_addrlen)) {
				close(s);
				s = -1;
			} else {
				perror("bind");
			}
		} else {
			perror("socket");
		}
	}

	freeaddrinfo(res);

	return s;
}

static void prv_output_servers(char * buffer, void * user_data) {
	lwm2m_context_t * lwm2mH = (lwm2m_context_t *) user_data;
	lwm2m_server_t * targetP;

	targetP = lwm2mH->serverList;

	if (targetP == NULL) {
		fprintf(stdout, "No server.\r\n");
		return;
	}

	for (targetP = lwm2mH->serverList; targetP != NULL ; targetP =
			targetP->next) {

		fprintf(stdout, "Server ID %d:\r\n", targetP->shortID);
		fprintf(stdout, "\thost: \"%s\" port: %hu\r\n", targetP->host,
				targetP->port);
		fprintf(stdout, "\tstatus: ");
		switch (targetP->status) {
		case STATE_UNKNOWN:
			fprintf(stdout, "UNKNOWN\r\n");
			break;
		case STATE_REG_PENDING:
			fprintf(stdout, "REGISTRATION PENDING\r\n");
			break;
		case STATE_REGISTERED:
			fprintf(stdout, "REGISTERED location: \"%s\"\r\n",
					targetP->location);
			break;
		}
		fprintf(stdout, "\r\n");
	}
}

static void prv_change(char * buffer, void * user_data) {
	lwm2m_context_t * lwm2mH = (lwm2m_context_t *) user_data;
	lwm2m_uri_t uri;
	int result;
	char * space;
	int uriLen;

	if (buffer[0] == 0)
		goto syntax_error;

	space = strstr(buffer, " ");
	if (space) {
		uriLen = space - buffer;
	} else {
		uriLen = strlen(buffer);
	}

	result = lwm2m_stringToUri(buffer, uriLen, &uri);
	if (result == 0)
		goto syntax_error;

	lwm2m_resource_value_changed(lwm2mH, &uri);

	return;

	syntax_error: fprintf(stdout, "Syntax error !");
}

void usage(char *arg) {
	fprintf(stderr, "Usage: %s endpointName server_addr\n", arg);
}

int main(int argc, char *argv[]) {
	int socket;
	int result;
	lwm2m_context_t * lwm2mH = NULL;
	lwm2m_object_t** objArray;
	int nof_objects;
	lwm2m_security_t security;
	int i;
	char *endpointName;

	command_desc_t commands[] = { { "list", "List known servers.", NULL,
			prv_output_servers, NULL }, { "change",
			"Change the value of resource.", " change URI DATA\r\n"
					"   URI: uri of the resource such as /3/0, /3/0/2\r\n"
					"   DATA: new value\r\n", prv_change, NULL }, { "quit",
			"Quit the client gracefully.", NULL, prv_quit, NULL }, { "^C",
			"Quit the client abruptly (without sending a de-register message).",
			NULL, NULL, NULL },

	COMMAND_END_LIST };

	if (argc < 3) {
		usage(argv[0]);
		return -1;
	}

	endpointName = argv[1];

	socket = get_remote_socket(argv[2], 5684);
	if (socket < 0) {
		fprintf(stderr, "Failed to open socket: %d\r\n", errno);
		return -1;
	}

	nof_objects = read_objects(&objArray);
	if (nof_objects < 0) {
		fprintf(stderr, "Failed to read objects: %d\r\n", errno);
		return -1;
	}

	lwm2mH = lwm2m_init(socket, endpointName, nof_objects, objArray);
	if (NULL == lwm2mH) {
		fprintf(stderr, "lwm2m_init() failed\r\n");
		return -1;
	}

	status_ctx.lwm2mH = lwm2mH;
	status_ctx.objArray = objArray;
	status_ctx.nof_objects = nof_objects;
	status_ctx.timerfd = -1;
	status_ctx.period.tv_sec = 1;
	status_ctx.period.tv_nsec = 0;

	if (status_init()) {
		fprintf(stderr, "status_thread_run() failed\r\n");
		return -1;
	}

	signal(SIGINT, handle_sigint);

	memset(&security, 0, sizeof(lwm2m_security_t));
	result = lwm2m_add_server(lwm2mH, 123, argv[2], 5684, &security);
	if (result != 0) {
		fprintf(stderr, "lwm2m_add_server() failed: 0x%X\r\n", result);
		return -1;
	}
	result = lwm2m_register(lwm2mH);
	if (result != 0) {
		fprintf(stderr, "lwm2m_register() failed: 0x%X\r\n", result);
		return -1;
	}

	for (i = 0; commands[i].name != NULL ; i++) {
		commands[i].userData = (void *) lwm2mH;
	}
	fprintf(stdout, "> ");
	fflush(stdout);

	while (0 == g_quit) {
		struct timeval tv;
		fd_set readfds;

		FD_ZERO(&readfds);
		FD_SET(socket, &readfds);
		FD_SET(STDIN_FILENO, &readfds);
		FD_SET(status_ctx.timerfd, &readfds);

		tv.tv_sec = 60;
		tv.tv_usec = 0;

		result = lwm2m_step(lwm2mH, &tv);
		if (result != 0) {
			fprintf(stderr, "lwm2m_step() failed: 0x%X\r\n", result);
			return -1;
		}

		result = select(FD_SETSIZE, &readfds, NULL, NULL, &tv);

		if (result < 0) {
			if (errno != EINTR) {
				fprintf(stderr, "Error in select(): %d\r\n", errno);
			}
		} else if (result > 0) {
			uint8_t buffer[MAX_PACKET_SIZE];
			int numBytes;

			if (FD_ISSET(socket, &readfds)) {
				struct sockaddr_storage addr;
				socklen_t addrLen;

				addrLen = sizeof(addr);
				numBytes = recvfrom(socket, buffer, MAX_PACKET_SIZE, 0,
						(struct sockaddr *) &addr, &addrLen);

				if (numBytes == -1) {
					fprintf(stderr, "Error in recvfrom(): %d\r\n", errno);
				} else {
					char s[INET6_ADDRSTRLEN];

					fprintf(stdout, "%d bytes received from [%s]:%hu\r\n",
							numBytes,
							inet_ntop(addr.ss_family,
									&(((struct sockaddr_in6*) &addr)->sin6_addr),
									s,
									INET6_ADDRSTRLEN),
							ntohs(((struct sockaddr_in6*) &addr)->sin6_port));
					prv_output_buffer(buffer, numBytes);

					lwm2m_handle_packet(lwm2mH, buffer, numBytes,
							(struct sockaddr *) &addr, addrLen);
				}
			} else if (FD_ISSET(STDIN_FILENO, &readfds)) {
				numBytes = read(STDIN_FILENO, buffer, MAX_PACKET_SIZE);

				if (numBytes > 1) {
					buffer[numBytes - 1] = 0;
					handle_command(commands, buffer);
				}
				if (g_quit == 0) {
					fprintf(stdout, "\r\n> ");
					fflush(stdout);
				} else {
					fprintf(stdout, "\r\n");
				}
			} else if (FD_ISSET(status_ctx.timerfd, &readfds)) {
				numBytes = read(status_ctx.timerfd, buffer, MAX_PACKET_SIZE);
				if (numBytes > 1) {
					status_run();
				} else if (numBytes == -1) {
					g_quit = 0;
					fprintf(stderr, "Error in read(): %d\r\n", errno);
				}
			}
		}
	}

	free_objects(objArray);

	status_stop();

	if (g_quit == 1) {
		lwm2m_close(lwm2mH);
	}
	close(socket);

	return 0;
}
