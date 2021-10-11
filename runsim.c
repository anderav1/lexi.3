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
#include <time.h>
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

struct sembuf p = { 0, -1, SEM_UNDO };
struct sembuf v = { 0, +1, SEM_UNDO };

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
	// get current time
	time_t now;
	time(&now);
	char* time = ctime(&now); // time string

	if (signum == SIGALRM) {
		printf("\nProgram runsim timed out at %s", time);
	} else if (signum == SIGINT) {
		printf("\nReceived signal %d, execution aborted.\n", signum);
	}

/*TODO print time of termination to log file*/
	char logstr[128];
	snprintf(logstr, 128, "Program terminated at %s", time);
	logmsg(logstr);

/*TODO kill all child processes*/
	deallocshm(/*shmid, semid*/);
	abort();

}


/* main function */

/*TODO set up timeout signal for runsim*/
/*TODO handle SIGINT*/

int main(int argc, char* argv[]) {
	signal(SIGINT, sighandler);
	signal(SIGALRM, sighandler);

	int* shm;  // ptr to shared memory segment

	while (1) {
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

		// start timer once sec value is set
		alarm(sec);

		if (optind >= argc) {  // too few args
			perror("runsim: Error: argc");
			exit(1);
		}
		int narg = atoi(argv[optind]);
		int n = (narg < 20) ? narg : 20; // max num of procs is 20
		printf("Num of processes is set to %d", n);

		if (initlicense() != 0) {
			perror("runsim: Error: initlicense");
			exit(1);
		}
		nlicenses = n;

		/* allocate shared memory */

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
			//getlicense();

			// get semaphore
			if (semop(semid, &p, 1) == -1) {
				perror("runsim: Error: semop p");
				deallocshm();
				exit(1);
			}

			// fork child process that calls docommand
			pid = fork();
			switch (pid) {
				case -1:  // error
					perror("runsim: Error: fork");
					exit(1);
				case 0:  // child

					// get semaphore
					if (semop(semid, &p, 1) == -1) {
						perror("runsim: Error: semop p");
						exit(1);
					}

					docommand(inputBuffer);

					// release semaphore
					if (semop(semid, &v, 1) == -1) {
						perror("runsim: Error: semop v");
						exit(1);
					}

					exit(0);
				default:  // parent

					// get semaphore
					if (semop(semid, &p, 1) == -1) {
						perror("runsim: Error: semop p");
						deallocshm();
						exit(1);
					}

					// check if any children have finished execution
					while ((w = waitpid(pid, &status, WNOHANG)) > 0) {
						if (w > 0) returnlicense();
					}
					if (w == -1) {  // error
						perror("runsim: Error: waitpid");
						signal(SIGINT, sighandler);
					}
					returnlicense();

					// release semaphore
					if (semop(semid, &v, 1) == -1) {
						perror("runsim: Error: semop v");
						deallocshm();
						exit(1);
					}

					break;
			}

			// release semaphore
			if (semop(semid, &v, 1) == -1) {
				perror("runsim: Error: semop v");
				deallocshm();
				exit(1);
			}
		}

		// at EOF, wait for all children to finish
		if (pid > 0) {
			wait(&status);
		}
	}

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
