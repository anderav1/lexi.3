// Author: Lexi Anderson
// CS 4760
// Last modified: Oct 9, 2021
// runsim.c -- main program executable

/* runsim is invoked with the command:  runsim [-t sec] n < testing.data
	n -- number of licenses
	sec -- how long to run
*/


#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <unistd.h>
#include "config.h"
#include "limits.h"

#define SHMKEY 101107 // shared memory segment key
#define SHMSZ sizeof(int)  // size of shared memory segment

extern int nlicenses;

int shmid, semid;
union semun arg;

char** tokenizestr(char* str);
void docommand(char* cline);

union semun {
	int val;
	struct semid_ds *buf;
	unsigned short *array;
	struct seminfo *__buf;
};

// deallocshm
// deallocate shared memory
void deallocshm(/*int shmid, int semid*/) {
	if (shmctl(shmid, IPC_RMID, NULL) == -1) {
		perror("runsim: Error: shmctl");
		exit(1);
	}

	if (semctl(semid, 0, IPC_RMID) == -1) {
		perror("runsim: Error: semctl");
		exit(1);
	}
}

// sighandler
void sighandler(int signum) {
	printf("Received signal %d, execution aborted.\n", signum);
	deallocshm(/*shmid, semid*/);
	abort();
}


/* main function */

int main(int argc, char* argv[]) {
	// set nlicenses

/*TODO: rewrite command line args check to account for optional arg*/
	int opt;
	int sec = 100; // default value
	while ((opt = getopt(argc, argv, "t:")) != -1) {
		switch (opt) {
			case 't':
				sec = atoi(optarg);
				break;
			default:
				perror("runsim: Error: getopt");
				exit(1);
		}
	}

	if (optind >= argc) {  // too few args
		perror("runsim: Error: argc");
		exit(1);
	}
	int narg = atoi(argv[optind]);
	int n = (narg < 20) ? narg : 20; // max num of procs is 20
/*TEST*/
	printf("Number of processes is %d", n);

	if (initlicense() != 0) {
		perror("runsim: Error: initlicense");
		exit(1);
	}
	nlicenses = n;

	/* allocate shared memory */
	int* shm;  // ptr to shared memory segment

	// create shared memory segment
	if ((shmid = shmget(SHMKEY, SHMSZ, 0777 | IPC_CREAT)) == -1) {
		perror("runsim: Error: shmget");
		exit(1);
	}

	// create semaphore
	if ((semid = semget(IPC_PRIVATE, 1, 0777 | IPC_CREAT)) == -1) {
		perror("runsim: Error: semget");
		exit(1);
	}

	// attach shmem seg to program's space
	if ((shm = shmat(shmid, NULL, 0)) == (int*)(-1)) {
		perror("runsim: Error: shmat");
		deallocshm(/*shmid, semid*/);
		exit(1);
	}

	*shm = nlicenses;

	// initialize semaphore
	arg.val = nlicenses;
	if ((semctl(semid, 0, SETVAL, arg)) == -1) {
		perror("runsim: Error: semctl");
		deallocshm(/*shmid, semid*/);
		exit(1);
	}

	/* main loop */

	int status = 0;
	pid_t pid, w;
	char inputBuffer[MAX_CANON];
	// read in one line at a time until EOF
	while (fgets(inputBuffer, MAX_CANON, stdin) != NULL) {
		// request a license
		getlicense();

		// fork child process that calls docommand
		pid = fork();
		switch (pid) {
			case -1:  // error
				perror("runsim: Error: fork");
				exit(1);
			case 0:  // child
				docommand(inputBuffer);
				exit(0);
			default:  // parent
				// check if any children have finished execution
				while ((w = waitpid(pid, &status, WNOHANG)) > 0) {
					if (w > 0) returnlicense();
				}
				if (w == -1) {  // error
					perror("runsim: Error: waitpid");
					signal(SIGINT, sighandler);
				}
				returnlicense();
				break;
		}
	}

	// at EOF, wait for all children to finish
	if (pid > 0) {
		wait(&status);
	}

	// implement signal handling to terminate after a specified num of secs
		// test by sending child into infinite loop

	// fork and exec multiple children until the specific limits

	deallocshm(/*shmid, semid*/);

	/* detach shared mem seg from program space */
	if (shmdt(shm) == -1) {
		perror("runsim: Error: shmdt");
		exit(1);
	}

	return(0);
}

char** tokenizestr(char* str) {
	int sz = 3;
	char** tokenarr = (char**)malloc(sizeof(char*)*sz);
	int i = 0;
	const char delims[] = {" "};
	char* token = strtok(str, delims);
	while (token != NULL) {
		tokenarr[i++] = token;
		token = strtok(NULL, delims);
	}
	// arr terminates with null
	tokenarr[i] = NULL;

	return tokenarr;
}

void docommand(char* cline) {
	// request license
	getlicense();

	// get command from string
	char** argv = tokenizestr(cline);

	// exec the specified command
	execvp(argv[0], argv);
	free(argv);
}
