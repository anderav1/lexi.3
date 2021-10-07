# Author: Lexi Anderson
# CS 4760
# Last modified: Oct 6, 2021
# Makefile (Project 3)

CC = gcc
CFLAGS = -g -Wall -Wshadow -L. -llicense
TAR = runsim testsim
DEPS = runsim.c testsim.c license.c config.h
OBJ = runsim.o testsim.o license.o


all: runsim testsim

# generate main program executables
$(TAR): %: %.o liblicense.a
	$(CC) -lm $(CFLAGS) -o $@ $^


# generate obj files from c files
$(OBJ): %.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -o $@ -c $<


# generate static library
liblicense.a: license.o config.h
	ar rcs $@ $^


# clean up generated files
.PHONY: clean
clean:
	rm -f $(TAR) $(OBJ) *.a logfile
