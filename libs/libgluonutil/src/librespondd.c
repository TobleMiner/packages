/*
   Copyright (c) 2018, Tobias Schramm <tobleminer@gmail.com>
   
   All rights reserved.
   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are met:
   1. Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.
   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
   DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
   FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
   SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
   CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
   OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
   */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <stdbool.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "librespondd.h"

#define RX_BUFF_SIZE 1500

static void getclock(struct timeval *tv) {
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	tv->tv_sec = ts.tv_sec;
	tv->tv_usec = ts.tv_nsec / 1000;
}

static ssize_t recv_timeout(int sock, char *buff, size_t max_len, struct timeval *timeout) {
	if(setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, timeout, sizeof(*timeout))) {
		return -errno;
	}

	return recv(sock, buff, max_len, 0);
}

static inline bool timeout_elapsed(struct timeval *timeout) {
	return timeout->tv_sec < 0;
}

int respondd_request(const struct sockaddr_in6 *dst, const char* query, struct timeval *timeout_, respondd_cb callback, void *cb_priv) {
	int err = 0;

	struct timeval timeout, now, after;
	timeout = *timeout_;
	getclock(&now);

	int sock = socket(PF_INET6, SOCK_DGRAM, 0);
	if(sock < 0) {
		err = sock;
		goto fail;
	}

	if(sendto(sock, query, strlen(query), 0, (struct sockaddr*)dst, sizeof(struct sockaddr_in6)) < 0) {
		err = -errno;
		goto fail_sock;
	}

	char rx_buff[RX_BUFF_SIZE];
	memset(rx_buff, 0, RX_BUFF_SIZE);

	getclock(&after);
	timersub(&after, &after, &now);
	timersub(&timeout, &timeout, &after);

	while(!timeout_elapsed(&timeout)) {
		getclock(&now);

		ssize_t recv_size = recv_timeout(sock, rx_buff, RX_BUFF_SIZE, &timeout);
		if(recv_size < 0) {
			// Not an error, timeout elapsed
			if(recv_size == EAGAIN) {
				break;
			}

			err = recv_size;
			goto fail_sock;
		}

		// Socket has been shut down
		if(recv_size == 0) {
			break;
		}

		int res = callback(rx_buff, recv_size, cb_priv);
		if(res) {
			if(res == RESPONDD_CB_CANCEL) {
				break;
			}
			err = res;
			goto fail_sock;
		}

		getclock(&after);
		timersub(&after, &after, &now);
		timersub(&timeout, &timeout, &after);
	}

fail_sock:
	close(sock);
fail:
	return err;
}
