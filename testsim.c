// Author: Lexi Anderson
// CS 4270
// Last modified: Oct 4, 2021
// testsim.c -- application program

#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include "config.h"

int main(int argc, char* argv[]) {
	if (argc != 3) {
		perror("testsim: Error: argc");
		exit(1);
	}

	// take in two command line args -- sleep time, repeat factor
	int sleeptime = atoi(argv[1]);
	int repeat = atoi(argv[2]);

	// main loop
	for (int i = 0; i < repeat; i++) {
		sleep(sleeptime);

		// get time
		time_t now;
		time(&now);
		char* time = ctime(&now); // current time as string

		pid_t pid = getpid();  // get pid

		char msgstr[400];
		sprintf(msgstr, "%.24s\t %ld\t %d of %d\n", time, (intmax_t)pid, i + 1, repeat);

		logmsg(msgstr);
	}

	return(0);
}
