/*****************************************************************************
 * life.c
 * Parallelized and optimized implementation of the game of life resides here
 ****************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>
#include "life.h"
#include "util.h"

/*****************************************************************************
 * Helper function definitions
 ****************************************************************************/
#define NUM_THREADS 4
#define MIN_BOARD_SIZE 32

/**
 * Swapping the two boards only involves swapping pointers, not
 * copying values.
 */
#define SWAP_BOARDS( b1, b2 )  do { \
  char* temp = b1; \
  b1 = b2; \
  b2 = temp; \
} while(0)

#define BOARD( __board, __i, __j )  (__board[(__i) + LDA*(__j)])

struct board_section {
  int start_i;
  int start_j;
  int size;
};

// Global board variables
char *outboard;
char *inboard;
unsigned board_size;
unsigned curgen = 0;
unsigned gens_max;

pthread_t threads[NUM_THREADS];
int waiting_runners = 0; // Number of runners waiting
pthread_mutex_t runner_wait_mutex;
pthread_cond_t runner_wait_cv;
int runner_start[NUM_THREADS] = {0}; // Indicates if a runner should start
pthread_mutex_t runner_start_mutexes[NUM_THREADS];
pthread_cond_t runner_start_cvs[NUM_THREADS];


void multithread_init();
void resume_task_runners();
void get_board_section(struct board_section *section, int t_i);
void *gol_task_runner(void *args);
int get_thread_index();

/*****************************************************************************
 * Game of life implementation
 ****************************************************************************/
char*
game_of_life (char* _outboard,
	      char* _inboard,
	      const int nrows,
	      const int ncols,
	      const int _gens_max)
{
  assert(_outboard && _inboard);
  assert(nrows == ncols);
  board_size = nrows;
  assert(board_size >= MIN_BOARD_SIZE);
  assert((board_size & (board_size-1)) == 0);

  if (board_size <= 32) {
    // No need to parallelize
    return sequential_game_of_life (outboard, inboard, nrows, ncols, gens_max);
  }

  // Parallelize
  int i, retval;

  inboard = _inboard;
  outboard = _outboard;
  gens_max = _gens_max;
  curgen = 0;

  multithread_init();

  // Start task runners
  for (i = 0; i < NUM_THREADS; i++) {
    retval = pthread_create(&threads[i], NULL, &gol_task_runner, NULL);
    if (retval) {
      printf("\n*** Error: pthread_create() return code: %d\n", retval);
      exit(EXIT_FAILURE);
    }
  }

  while (curgen < gens_max) {
    //printf("wait curgen\n");
    // Wait for threads to finish the current gen
    pthread_mutex_lock(&runner_wait_mutex);
    while (waiting_runners < NUM_THREADS) {
      pthread_cond_wait(&runner_wait_cv, &runner_wait_mutex);
    }
    //printf("exit from wait curgen\n");
    curgen++;
    waiting_runners = 0;
    SWAP_BOARDS(outboard, inboard);
    resume_task_runners();
    pthread_mutex_unlock(&runner_wait_mutex);
  }

  // Wait for runners to finish
  for (i = 0; i < NUM_THREADS; i++) {
    pthread_join(threads[i], NULL);
  }

  return outboard;
}

void resume_task_runners() {
  int i;
  for (i = 0; i < NUM_THREADS; i++) {
    //printf("resume task runner %d\n", i);
    pthread_mutex_lock(&runner_start_mutexes[i]);
    runner_start[i] = 1;
    pthread_cond_signal(&runner_start_cvs[i]);
    pthread_mutex_unlock(&runner_start_mutexes[i]);
  }
}

void get_board_section(struct board_section *section, int t_i) {
  assert(section);
  int start_i,  start_j;
  int size = (int) board_size/2;

  assert(t_i >= 0);
  assert(size*2 == board_size);

  // Divide board into 4 sections, from left to right and top to bottom 1 2 3 4
  switch (t_i) {
    case 0:
      start_i = 0;
      start_j = 0;
      break;

    case 1:
      start_i = size;
      start_j = 0;
      break;

    case 2:
      start_i = 0;
      start_j = size;
      break;

    case 3:
      start_i = size;
      start_j = size;
      break;

    default:
      assert(!"Invalid number of threads");
      break;
  }

  section->start_i = start_i;
  section->start_j = start_j;
  section->size = size;
}

void *gol_task_runner(void *args) {
  const int t_i = get_thread_index();
  const int LDA = board_size;

  int start_i, start_j, section_size;
  struct board_section section;
  int i, j;
  int inorth, isouth, jwest, jeast;
  char neighbor_count;

  get_board_section(&section, t_i);
  start_i = section.start_i;
  start_j = section.start_j;
  section_size = section.size;

  while (curgen < gens_max) {
    // TODO: tile the loop
    for (j = start_j; j < section_size; j++) {
      for (i = start_i; i < section_size; i++) {
        inorth = mod (i-1, board_size);
        isouth = mod (i+1, board_size);
        jwest = mod (j-1, board_size);
        jeast = mod (j+1, board_size);

        neighbor_count =
          BOARD (inboard, inorth, jwest) +
          BOARD (inboard, inorth, j) +
          BOARD (inboard, inorth, jeast) +
          BOARD (inboard, i, jwest) +
          BOARD (inboard, i, jeast) +
          BOARD (inboard, isouth, jwest) +
          BOARD (inboard, isouth, j) +
          BOARD (inboard, isouth, jeast);

        BOARD(outboard, i, j) = alivep(neighbor_count, BOARD(inboard, i, j));
      }
    }

    // Signal that the runner is done and waiting
    pthread_mutex_lock(&runner_wait_mutex);
    //printf("runner %d done\n", t_i);
    waiting_runners++;
    if (waiting_runners >= NUM_THREADS) {
      //printf("runner %d signal wait cv\n", t_i);
      pthread_cond_signal(&runner_wait_cv);
    }
    pthread_mutex_unlock(&runner_wait_mutex);

    // Wait for start signal before running next gen
    pthread_mutex_lock(&runner_start_mutexes[t_i]);
    //printf("runner %d wait for start signal\n", t_i);
    while (runner_start[t_i] == 0) {
      pthread_cond_wait(&runner_start_cvs[t_i], &runner_start_mutexes[t_i]);
    }
    //printf("runner %d resuming\n", t_i);
    runner_start[t_i] = 0;
    pthread_mutex_unlock(&runner_start_mutexes[t_i]);
  }

  pthread_exit(NULL);
}

void multithread_init() {
  int i;
  pthread_mutex_init(&runner_wait_mutex, NULL);
  pthread_cond_init(&runner_wait_cv, NULL);
  for (i = 0; i < NUM_THREADS; i++){
    pthread_mutex_init(&runner_start_mutexes[i], NULL);
    pthread_cond_init(&runner_start_cvs[i], NULL);
  }
}

int get_thread_index() {
  int i;
  pthread_t self = pthread_self();
  for (i = 0; i < NUM_THREADS; i++) {
    if (pthread_equal(self, threads[i])) return i;
  }
  return -1;
}
