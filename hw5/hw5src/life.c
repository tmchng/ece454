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
  int i_size;
  int j_size;
};

struct pkg {
  int t_i;
  char *inboard;
  char *outboard;
  int board_size;
  int gens_max;
  pthread_mutex_t *runner_wait_mutex;
  pthread_cond_t *runner_wait_cv;
  pthread_mutex_t *runner_start_mutex;
  pthread_cond_t *runner_start_cv;
  int *waiting_runners;
  int *runner_start;
};


void resume_task_runners(struct pkg pkgs[]);
void get_board_section(struct board_section *section, int board_size, int t_i);
void *gol_task_runner(void *args);
int get_thread_index();


/*****************************************************************************
 * Game of life implementation
 ****************************************************************************/
char*
game_of_life (char* outboard,
	      char* inboard,
	      const int nrows,
	      const int ncols,
	      const int gens_max)
{
  assert(outboard && inboard);
  assert(nrows == ncols);
  const int board_size = nrows;
  assert(board_size >= MIN_BOARD_SIZE);
  assert((board_size & (board_size-1)) == 0);

  if (gens_max <= 0) return inboard;

  if (board_size <= 32) {
    // No need to parallelize
    return sequential_game_of_life (outboard, inboard, nrows, ncols, gens_max);
  }

  // Parallelize starting here.
  int i, retval;

  // Declaring everything needed to synchronize threads
  pthread_t threads[NUM_THREADS];
  int waiting_runners = 0; // Number of runners waiting
  int runner_start[NUM_THREADS] = {0}; // Indicates if a runner should start
  pthread_mutex_t runner_wait_mutex;
  pthread_cond_t runner_wait_cv;
  pthread_mutex_t runner_start_mutexes[NUM_THREADS];
  pthread_cond_t runner_start_cvs[NUM_THREADS];

  // Initialize mutexes and cvs.
  pthread_mutex_init(&runner_wait_mutex, NULL);
  pthread_cond_init(&runner_wait_cv, NULL);
  for (i = 0; i < NUM_THREADS; i++){
    pthread_mutex_init(&runner_start_mutexes[i], NULL);
    pthread_cond_init(&runner_start_cvs[i], NULL);
  }

  // construct packages because global variables are not allowed.
  struct pkg pkgs[NUM_THREADS];
  for (i = 0; i < NUM_THREADS; i++) {
    pkgs[i].t_i = i;
    pkgs[i].inboard = inboard;
    pkgs[i].outboard = outboard;
    pkgs[i].board_size = board_size;
    pkgs[i].gens_max = gens_max;
    pkgs[i].runner_wait_mutex = &runner_wait_mutex;
    pkgs[i].runner_wait_cv = &runner_wait_cv;
    pkgs[i].runner_start_mutex = &runner_start_mutexes[i];
    pkgs[i].runner_start_cv = &runner_start_cvs[i];
    pkgs[i].waiting_runners = &waiting_runners;
    pkgs[i].runner_start = &runner_start[i];
  }

  // Start task runners
  for (i = 0; i < NUM_THREADS; i++) {
    retval = pthread_create(&threads[i], NULL, &gol_task_runner, &pkgs[i]);
    if (retval) {
      printf("\n*** Error: pthread_create() return code: %d\n", retval);
      exit(EXIT_FAILURE);
    }
  }

  int curgen = 0;
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
    pthread_mutex_unlock(&runner_wait_mutex);

    resume_task_runners(pkgs);
  }

  // Wait for runners to finish
  //for (i = 0; i < NUM_THREADS; i++) {
  //  pthread_join(threads[i], NULL);
  //}

  return inboard;
}

void resume_task_runners(struct pkg pkgs[]) {
  int i;
  for (i = 0; i < NUM_THREADS; i++) {
    //printf("resume task runner %d\n", i);
    pthread_mutex_lock(pkgs[i].runner_start_mutex);
    *(pkgs[i].runner_start) = 1;
    pthread_cond_signal(pkgs[i].runner_start_cv);
    pthread_mutex_unlock(pkgs[i].runner_start_mutex);
  }
}

void get_board_section(struct board_section *section, int board_size, int t_i) {
  assert(t_i >= 0);
  assert(section);

  int start_i,  start_j;
  int i_size = board_size;
  int j_size = (int)board_size / NUM_THREADS;

  start_i = 0;
  start_j = (int)(t_i * j_size);

  section->start_i = start_i;
  section->start_j = start_j;
  section->i_size = i_size;
  section->j_size = j_size;
}

void gol_task_runner_2(void *args) {
  // Tiled version
  // Unpack pkg
  const struct pkg *pkg = (struct pkg*) args;
  const int board_size = pkg->board_size;
  const int LDA = board_size;
  const int t_i = pkg->t_i;
  char *inboard = pkg->inboard;
  char *outboard = pkg->outboard;
  const int gens_max = pkg->gens_max;
  pthread_mutex_t *runner_wait_mutex = pkg->runner_wait_mutex;
  pthread_cond_t *runner_wait_cv = pkg->runner_wait_cv;
  pthread_mutex_t *runner_start_mutex = pkg->runner_start_mutex;
  pthread_cond_t *runner_start_cv = pkg->runner_start_cv;
  int *waiting_runners = pkg->waiting_runners;
  int *runner_start = pkg->runner_start;

  int start_i, start_j, i_size, j_size;
  int i, j, i2, j2;
  int inorth, isouth, jwest, jeast;
  char neighbor_count;
  struct board_section section;

  get_board_section(&section, board_size, t_i);
  start_i = section.start_i;
  start_j = section.start_j;
  j_size = section.j_size;
  i_size = section.i_size;

  int i_chunk_size = 128;
  int j_chunk_size = 64;
  int i_end, j_end;
  int j2_end = start_j + j_size;
  int i2_end = start_i + i_size;

  int curgen = 0;

  while (curgen < gens_max) {
    // TODO: tile the loop
    for (j2 = start_j; j2 < j2_end; j2+=j_chunk_size) {
      j_end = j2 + j_chunk_size;
      if (j_end > j2_end) j_end = j2_end;

      for (i2 = start_i; i2 < i2_end; i2+=i_chunk_size) {
        i_end = i2 + i_chunk_size;
        if (i_end > i2_end) i_end = i2_end;

        for (j = j2; j < j_end; j++) {
          jwest = mod (j-1, board_size);
          jeast = mod (j+1, board_size);

          for (i = i2; i < i_end; i++) {
            inorth = mod (i-1, board_size);
            isouth = mod (i+1, board_size);

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
      }
    }

    // Signal that the runner is done and waiting
    pthread_mutex_lock(runner_wait_mutex);
    *waiting_runners = *waiting_runners + 1;
    //printf("runner %d done: %dth\n", t_i, *waiting_runners);
    if (*waiting_runners >= NUM_THREADS) {
      //printf("runner %d signal wait cv\n", t_i);
      pthread_cond_signal(runner_wait_cv);
    }
    pthread_mutex_unlock(runner_wait_mutex);

    // Wait for start signal before running next gen
    pthread_mutex_lock(runner_start_mutex);
    //printf("runner %d wait for start signal\n", t_i);
    while (*runner_start == 0) {
      pthread_cond_wait(runner_start_cv, runner_start_mutex);
    }
    //printf("runner %d resuming\n", t_i);
    *runner_start = 0;
    pthread_mutex_unlock(runner_start_mutex);

    SWAP_BOARDS(outboard, inboard);
    curgen++;
  }
}

void gol_task_runner_1(void *args) {
  // Unpack pkg
  const struct pkg *pkg = (struct pkg*) args;
  const int board_size = pkg->board_size;
  const int LDA = board_size;
  const int t_i = pkg->t_i;
  char *inboard = pkg->inboard;
  char *outboard = pkg->outboard;
  const int gens_max = pkg->gens_max;
  pthread_mutex_t *runner_wait_mutex = pkg->runner_wait_mutex;
  pthread_cond_t *runner_wait_cv = pkg->runner_wait_cv;
  pthread_mutex_t *runner_start_mutex = pkg->runner_start_mutex;
  pthread_cond_t *runner_start_cv = pkg->runner_start_cv;
  int *waiting_runners = pkg->waiting_runners;
  int *runner_start = pkg->runner_start;

  int start_i, start_j, i_size, j_size;
  int i, j;
  int inorth, isouth, jwest, jeast;
  char neighbor_count;
  struct board_section section;
  int curgen = 0;

  get_board_section(&section, board_size, t_i);
  start_i = section.start_i;
  start_j = section.start_j;
  j_size = section.j_size;
  i_size = section.i_size;

  //printf("runner %d gets %d to %d\n", t_i, start_j, start_j + j_size - 1);

  while (curgen < gens_max) {
    // TODO: tile the loop
    for (j = start_j; j < start_j+j_size; j++) {
      jwest = mod (j-1, board_size);
      jeast = mod (j+1, board_size);

      for (i = start_i; i < start_i+i_size; i++) {
        inorth = mod (i-1, board_size);
        isouth = mod (i+1, board_size);

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
    pthread_mutex_lock(runner_wait_mutex);
    *waiting_runners = *waiting_runners + 1;
    //printf("runner %d done: %dth\n", t_i, *waiting_runners);
    if (*waiting_runners >= NUM_THREADS) {
      //printf("runner %d signal wait cv\n", t_i);
      pthread_cond_signal(runner_wait_cv);
    }
    pthread_mutex_unlock(runner_wait_mutex);

    // Wait for start signal before running next gen
    pthread_mutex_lock(runner_start_mutex);
    //printf("runner %d wait for start signal\n", t_i);
    while (*runner_start == 0) {
      pthread_cond_wait(runner_start_cv, runner_start_mutex);
    }
    //printf("runner %d resuming\n", t_i);
    *runner_start = 0;
    pthread_mutex_unlock(runner_start_mutex);

    SWAP_BOARDS(outboard, inboard);
    curgen++;
  }

}

void *gol_task_runner(void *args) {
  //gol_task_runner_1(args);
  gol_task_runner_2(args);
  pthread_exit(NULL);
}

