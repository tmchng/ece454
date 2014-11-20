
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <pthread.h>

#include "defs.h"
#include "hash_list_lock.h"

#define SAMPLES_TO_COLLECT   10000000
#define RAND_NUM_UPPER_BOUND   100000
#define NUM_SEED_STREAMS            4

/*
 * ECE454 Students:
 * Please fill in the following team struct
 */
team_t team = {
    "TeamTW",                  /* Team name */

    "Ting-Hao (Tim) Cheng",                    /* First member full name */
    "997289090",                 /* First member student number */
    "chengti2@ecf.utoronto.ca",                 /* First member email address */

    "Corey Yen",                           /* Second member full name */
    "999830090",                           /* Second member student number */
    "yenyung1@ecf.utoronto.ca"                            /* Second member email address */
};

unsigned num_threads;
unsigned samples_to_skip;

class sample;

class sample {
  unsigned my_key;
  pthread_mutex_t mutex;
 public:
  sample *next;
  unsigned count;

  sample(unsigned the_key){
    my_key = the_key;
    count = 0;
    pthread_mutex_init(&mutex, NULL);
  };

  void incrementCount() {
    pthread_mutex_lock(&mutex);
    count++;
    pthread_mutex_unlock(&mutex);
  };

  unsigned key(){return my_key;}
  void print(FILE *f){printf("%d %d\n",my_key,count);}
};

// This instantiates an empty hash table
// it is a C++ template, which means we define the types for
// the element and key value here: element is "class sample" and
// key value is "unsigned".
hash<sample,unsigned> h;

// Threads
pthread_t threads[4];

// Function declarations
void multithreaded_randtrack(void);
void *randtrack (void *args);
void start_randtrack(void);
int get_thread_index(void);
int get_start_stream(int i);


void *randtrack (void *args) {
  int i,j,k;
  int rnum;
  unsigned key;
  sample *s;

  int num_streams = (int) NUM_SEED_STREAMS / num_threads;
  int start_stream = get_start_stream(get_thread_index());
  int end_stream = start_stream + num_streams;

  // process streams starting with different initial numbers
  for (i=start_stream; i<end_stream; i++){
    rnum = i;

    // collect a number of samples
    for (j=0; j<SAMPLES_TO_COLLECT; j++){

      // skip a number of samples
      for (k=0; k<samples_to_skip; k++){
	rnum = rand_r((unsigned int*)&rnum);
      }

      // force the sample to be within the range of 0..RAND_NUM_UPPER_BOUND-1
      key = rnum % RAND_NUM_UPPER_BOUND;

      // Start critical region

      // if this sample has not been counted before
      if (!(s = h.lookup(key))){
        // insert a new element for it into the hash table
        // Need to lock the list before we create a new sample, otherwise
        // two threads might create two different objects for the same sample.
        h.lock_list(key);
        s = new sample(key);
        h.insert(s);
        h.unlock_list(key);
      }

      // increment the count for the sample
      s->incrementCount();
    }
  }

  pthread_exit(NULL);
}

void multithreaded_randtrack() {
  int i, retval;

  // create threads
  for (i = 0; i < num_threads; i++) {
    retval = pthread_create(&threads[i], NULL, &randtrack, NULL);
    if (retval) {
      printf("\n*** Error: pthread_create() return code: %d\n", retval);
      exit(EXIT_FAILURE);
    }
  }

  // wait for threads
  for (i = 0; i < num_threads; i++) {
    pthread_join(threads[i], NULL);
  }
}

int get_thread_index() {
  int i;
  pthread_t self = pthread_self();
  for (i = 0; i < num_threads; i++) {
    if (pthread_equal(self, threads[i])) return i;
  }
  return -1;
}

int get_start_stream(int i) {
  switch (num_threads) {
    case 1:
      return 0;

    case 2:
      return i*2;

    case 4:
      return i;

    default:
      // should never come here
      assert(!"Error: invalid number of threads in get_start_stream()");
  }
}

int
main (int argc, char* argv[]){
  // Print out team information
  printf( "Team Name: %s\n", team.team );
  printf( "\n" );
  printf( "Student 1 Name: %s\n", team.name1 );
  printf( "Student 1 Student Number: %s\n", team.number1 );
  printf( "Student 1 Email: %s\n", team.email1 );
  printf( "\n" );
  printf( "Student 2 Name: %s\n", team.name2 );
  printf( "Student 2 Student Number: %s\n", team.number2 );
  printf( "Student 2 Email: %s\n", team.email2 );
  printf( "\n" );

  // Parse program arguments
  if (argc != 3){
    printf("Usage: %s <num_threads> <samples_to_skip>\n", argv[0]);
    exit(1);
  }
  sscanf(argv[1], " %d", &num_threads);
  sscanf(argv[2], " %d", &samples_to_skip);

  // Check num_threads is either 1, 2, or 4.
  assert(num_threads == 1 || num_threads == 2 || num_threads == 4);

  // initialize a 16K-entry (2**14) hash of empty lists
  h.setup(14);

  // Start threaded randtrack
  multithreaded_randtrack();

  // print a list of the frequency of all samples
  h.print();
  return (EXIT_SUCCESS);
}
