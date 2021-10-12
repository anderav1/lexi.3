// Author: Lexi Anderson
// CS 4760
// Last modified: Oct 12, 2021
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
#define SEMKEY 193578  // semaphore key
#define SHMSZ sizeof(int)  // size of shared memory segment

extern int nlicenses;

int shmid, semid;
union semun arg;
pid_t ppid;

volatile sig_atomic_t got_interrupt = 0;

char** tokenizestr(char* str);
void docommand(char* cline);
int getsem(void);
int releasesem(void);

union semun {
	int val;
	struct semid_ds *buf;
	unsigned short *array;
};

// function: deallocshm
// deallocate shared memory
void deallocshm() {
	if (shmctl(shmid, IPC_RMID, NULL) == -1) {
		perror("runsim: Error: shmctl rmid");
		exit(1);
	}

	if (semctl(semid, 1, IPC_RMID, 0) == -1) {
		perror("runsim: Error: semctl rmid");
		exit(1);
	}
}

// function: sighandler
void sighandler(int signum) {

	// get current time
	time_t now;
	time(&now);
	char* time = ctime(&now); // time string

	// print termination time to logfile
	char logstr[128];
	snprintf(logstr, 128, "Program terminated at %s", time);
	logmsg(logstr);

	if (getpid() == ppid) deallocshm();

	switch (signum) {
		case SIGALRM:
			printf("\nProgram runsim timed out at %s", time);
			got_interrupt = 1;
			break;
		case SIGINT:
			printf("\nReceived signal %d, execution aborted.\n", signum);
	}

	kill(0, SIGKILL);
}


/* main function */

int main(int argc, char* argv[]) {
	signal(SIGINT, sighandler);
	signal(SIGALRM, sighandler);

	int* shm;  // ptr to shared memory segment
	ppid = getpid();

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

	while (!got_interrupt) {
		if (optind >= argc) {  // too few args
			perror("runsim: Error: argc");
			exit(1);
		}
		int narg = atoi(argv[optind]);
		int n;
		if (narg > 20) {
/*TODO statement printing multiple times throughout program*/
			if (getpid() == ppid) puts("Number of processes capped at 20");
			n = 20;
		} else n = narg;

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
		if ((semid = semget(SEMKEY, 1, 0777 | IPC_CREAT)) == -1) {
			perror("runsim: Error: semget");
			exit(1);
		}

		// attach shmem seg to program's space
		if ((shm = shmat(shmid, NULL, 0)) == (int*)(-1)) {
			perror("runsim: Error: shmat");
			deallocshm();
			exit(1);
		}

		*shm = nlicenses;

		// initialize semaphore
		arg.val = nlicenses;
/*XXX test value*/
//		arg.val = 1;
		if ((semctl(semid, 0, SETVAL, arg)) == -1) {
			perror("runsim: Error: semctl setval");
			deallocshm();
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

//TODO	get semaphore??

			// fork child process that calls docommand
			pid = fork();
			switch (pid) {
				case -1:  // error
					perror("runsim: Error: fork");
					exit(1);
				case 0:  // child

					// get semaphore and exec
					if (getsem() == 0) {
						docommand(inputBuffer);
					} else {
						perror("runsim: Error: getsem");
						exit(1);
					}

					// release semaphore
					if (releasesem() == -1) {
						perror("runsim: Error: semop v");
						exit(1);
					}

					exit(0);
				default:  // parent

					// get semaphore
					if (getsem() == -1) {
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
					if (releasesem() == -1) {
						perror("runsim: Error: semop v");
						deallocshm();
						exit(1);
					}

					break;
			}

//TODO release semaphore??
		}

		// at EOF, wait for all children to finish
		if (pid > 0) {
			wait(&status);
		}
	}

	deallocshm();

	if (got_interrupt) {
		if (killpg(getpgid(ppid), SIGTERM)) {
			perror("runsim: Error: killgp");
			exit(1);
		}
	}

	/* detach shared mem seg from program space */
	if (shmdt(shm) == -1) {
		perror("runsim: Error: shmdt");
		exit(1);
	}
	return(0);
}

// function: tokenizestr
// split the string into an array of tokens
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

// function: docommand
// execute the command specified by string cline
void docommand(char* cline) {
	// request license
	getlicense();

	// get command from string
	char** argv = tokenizestr(cline);

	// exec the specified command
	execvp(argv[0], argv);
	free(argv);
}

// function: getsem
// access the semaphore
int getsem() {
	int ret;
	struct sembuf ops[1];
	ops[0].sem_num = 0;  // index of the sem in the sem array
	ops[0].sem_op = -1;
	ops[0].sem_flg = SEM_UNDO;

	if ((ret = semop(semid, ops, 1)) == -1) {
		perror("runsim: Error: getsem semop");
		exit(1);
	}

	return ret;
}

// function: releasesem
// release the semaphore
int releasesem() {
	int ret;
	struct sembuf ops[1];
	ops[0].sem_num = 0;
	ops[0].sem_op = 1;
	ops[0].sem_flg = SEM_UNDO;

	if ((ret = semop(semid, ops, 1)) == -1) {
		perror("runsim: Error: releasesem semop");
		exit(1);
	}

	return ret;
}
