// Author: Lexi Anderson
// CS 4760
// config.h

#define NUMPROC 20 // max num of procs to be forked

#ifndef CONFIG_H
#define CONFIG_H

extern int nlicenses; // number of available licenses

int getlicense(void);	// blocks until a license is available
int returnlicense(void);  // increments the number of available licenses
int initlicense(void); 	// performs any need initialization of the license object
void addtolicenses(int n);  // adds n licenses to the number available
void removelicenses(int n);  // decrements the number of licenses by n
void logmsg(const char* msg);  // write the specified message to the log file

#endif

