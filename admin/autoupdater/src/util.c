/*
  Copyright (c) 2018, Tobias Schramm <tobleminer@gmail.com>
  Copyright (c) 2017, Jan-Philipp Litza <janphilipp@litza.de>
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


#include "util.h"

#include <fcntl.h>
#include <glob.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <sys/wait.h>

#include <netinet/in.h>

void hwaddr_to_lladdr(struct in6_addr *addr, unsigned char *hwaddr) {
	memset(addr, 0, sizeof(struct in6_addr));
	unsigned char* octets = addr->s6_addr;
	octets[0] = 0xfe;
	octets[1] = 0x80;
	octets[8] = hwaddr[0] ^ 0b10;
	octets[9] = hwaddr[1];
	octets[10] = hwaddr[2];
	octets[11] = 0xff;
	octets[12] = 0xfe;
	octets[13] = hwaddr[3];
	octets[14] = hwaddr[4];
	octets[15] = hwaddr[5];
}

void run_dir(const char *dir) {
	char pat[strlen(dir) + 3];
	sprintf(pat, "%s/*", dir);
	glob_t globbuf;
	if (glob(pat, 0, NULL, &globbuf))
		return;

	for (size_t i = 0; i < globbuf.gl_pathc; i++) {
		char *path = globbuf.gl_pathv[i];
		if (access(path, X_OK) < 0)
			continue;

		pid_t pid = fork();
		if (pid < 0) {
			fputs("autoupdater: warning: failed to fork: %m", stderr);
			continue;
		}

		if (pid == 0) {
			execl(path, path, (char *)NULL);
			exit(EXIT_FAILURE);
		}

		int wstatus;
		if (waitpid(pid, &wstatus, 0) != pid) {
			fprintf(stderr, "autoupdater: warning: failed waiting for child %d corresponding to %s: ", pid, path);
			perror(NULL);
		} else if (!WIFEXITED(wstatus)) {
			fprintf(stderr, "autoupdater: warning: execution of %s exited abnormally\n", path);
		} else if (WEXITSTATUS(wstatus)) {
			fprintf(stderr, "autoupdater: warning: execution of %s exited with status code %d\n", path, WEXITSTATUS(wstatus));
		}
	}

	globfree(&globbuf);
}


void randomize(void) {
	struct timespec tv;
	if (clock_gettime(CLOCK_MONOTONIC, &tv)) {
		perror("autoupdater: error: clock_gettime");
		exit(1);
	}

	srandom(tv.tv_nsec);
}


float get_uptime(void) {
	FILE *f = fopen("/proc/uptime", "r");
	if (f) {
		float uptime;
		int match = fscanf(f, "%f", &uptime);
		fclose(f);

		if (match == 1)
			return uptime;
	}

	fputs("autoupdater: error: unable to determine uptime\n", stderr);
	exit(1);
}

void * safe_malloc(size_t size) {
	void *ret = malloc(size);
	if (!ret) {
		fprintf(stderr, "autoupdater: error: failed to allocate memory\n");
		abort();
	}

	return ret;
}

void * safe_realloc(void *ptr, size_t size) {
	void *ret = realloc(ptr, size);
	if (!ret) {
		fprintf(stderr, "autoupdater: error: failed to allocate memory\n");
		abort();
	}

	return ret;
}
