
CC = g++-4.7
CFLAGS = -O3
PTFLAG = -lpthread
TMFLAG = -fgnu-tm

all: randtrack randtrack_global_lock randtrack_tm randtrack_list_lock

randtrack: list.h hash.h defs.h randtrack.cc
	$(CC) $(CFLAGS) randtrack.cc -o randtrack

randtrack_global_lock: list.h hash.h defs.h randtrack_global_lock.cc
	$(CC) $(CFLAGS) randtrack_global_lock.cc -o randtrack_global_lock $(PTFLAG)

randtrack_tm: list.h hash.h defs.h randtrack_tm.cc
	$(CC) $(CFLAGS) randtrack_tm.cc -o randtrack_tm $(PTFLAG) $(TMFLAG)

randtrack_list_lock: list.h hash_list_lock.h defs.h randtrack_list_lock.cc
	$(CC) $(CFLAGS) randtrack_list_lock.cc -o randtrack_list_lock $(PTFLAG) $(TMFLAG)

clean:
	rm -f *.o randtrack randtrack_global_lock randtrack_tm randtrack_list_lock
