/*****************************************************************************
 * life.c
 * Parallelized and optimized implementation of the game of life resides here
 ****************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <assert.h>
#include <string.h>
#include "life.h"
#include "util.h"

/*****************************************************************************
 * Helper function definitions
 ****************************************************************************/
#define NUM_THREADS 4
#define MIN_BOARD_SIZE 32
#define BENCH_BOARD_SIZE 1024

/**
 * Swapping the two boards only involves swapping pointers, not
 * copying values.
 */
#define SWAP_BOARDS( b1, b2 )  do { \
  char* temp = b1; \
  b1 = b2; \
  b2 = temp; \
} while(0)

/**
 * Helper macros
 */
#define BOARD( __board, __i, __j )  (__board[(__i) + board_size*(__j)])
#define IS_ALIVE(cell) ((cell >> 4) & 1)
#define MARK_ALIVE(cell) (cell |= (1 << (4)))
#define MARK_DEAD(cell) (cell &= ~(1 << (4)))
#define SHOULD_DIE(cell) ((cell <= (char)0x11 || cell >= (char)0x14))
#define SHOULD_SPAWN(cell) (cell == (char)0x3)
#define INCR_NEIGHBOURS(__board, __i, __j)  (BOARD(__board, __i, __j)+=1)
#define DECR_NEIGHBOURS(__board, __i, __j)  (BOARD(__board, __i, __j)-=1)

/**
 * Structure that contains board section info
 */
struct board_section {
  int start_i;
  int start_j;
  int i_size;
  int j_size;
};

/**
 * Structure that contains all things needed by a thread to run correctly.
 */
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
  pthread_barrier_t *barrier;
};

/**
 * Function declarations
 */
void init_board_neighbours(char *board, int board_size);
static inline void process_cell(char *outboard, char *inboard, int i, int j, const int board_size);
void resume_task_runners(struct pkg pkgs[]);
void get_board_section(struct board_section *section, int board_size, int t_i);
void *gol_task_runner(void *args);
void gol_task_neighbours(void *args);
int get_thread_index();
static inline int mod_i(int i, int m);


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

  if (board_size <= 64) {
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
  pthread_barrier_t barrier;

  // Initialize mutexes and cvs and barriers.
  pthread_mutex_init(&runner_wait_mutex, NULL);
  pthread_cond_init(&runner_wait_cv, NULL);
  for (i = 0; i < NUM_THREADS; i++){
    pthread_mutex_init(&runner_start_mutexes[i], NULL);
    pthread_cond_init(&runner_start_cvs[i], NULL);
  }
  pthread_barrier_init(&barrier, NULL, NUM_THREADS);

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
    pkgs[i].barrier = &barrier;
  }

  // Initialize the board for neighbor count
  init_board_neighbours(inboard, board_size);
  memmove(outboard, inboard, board_size*board_size*sizeof(char));

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
    //printf("copied boards\n");
    pthread_mutex_unlock(&runner_wait_mutex);

    // copy the outboard
    memmove(inboard, outboard, board_size*board_size*sizeof(char));

    resume_task_runners(pkgs);
  }

  // Wait for runners to finish
  for (i = 0; i < NUM_THREADS; i++) {
    pthread_join(threads[i], NULL);
  }

  // Format board to 1 and 0
  // Loop unroll
  for (i = 0; i < board_size*board_size; i+=2) {
    outboard[i] = IS_ALIVE(outboard[i]);
    outboard[i+1] = IS_ALIVE(outboard[i+1]);
  }
  return outboard;
}

/**
 * Signal the runners to resume
 */
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

/**
 * Assigns board section to each thread in order to
 * parallelize work.
 */
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

/**
 * Main runner task that processes the board.
 */
void gol_task_neighbours(void *args) {
  // Unpack pkg to avoid using pointers
  const struct pkg *pkg = (struct pkg*) args;
  const int board_size = pkg->board_size;
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
  pthread_barrier_t *barrier = pkg->barrier;

  int start_i, start_j, i_size, j_size;
  int i, j;
  struct board_section section;
  int curgen = 0;

  get_board_section(&section, board_size, t_i);
  start_i = section.start_i;
  start_j = section.start_j;
  j_size = section.j_size;
  i_size = section.i_size;

  //printf("runner %d gets %d to %d\n", t_i, start_j, start_j + j_size - 1);

  while (curgen < gens_max) {
    // Compute upper bound first
    for (i = start_i; i < start_i+i_size; i++) {
      process_cell(outboard, inboard, i, start_j, board_size);
    }
    // Wait for other threads to finish upper bound
    pthread_barrier_wait(barrier);

    // Then compute lower bound
    for (i = start_i; i < start_i+i_size; i++) {
      process_cell(outboard, inboard, i, start_j+j_size-1, board_size);
    }
    // Wait for other threads to finish lower bound
    pthread_barrier_wait(barrier);

    // Compute middle
    for (j = start_j+1; j < start_j+j_size-1; j++) {
      for (i = start_i; i < start_i+i_size; i++) {
        process_cell(outboard, inboard, i, j, board_size);
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

    curgen++;
  }
}

/**
 * Process a cell and determine the new state.
 */
static inline void process_cell(char *outboard, char *inboard, int i, int j, const int board_size) {
  int inorth, isouth, jwest, jeast;
  char cell = BOARD(inboard, i, j);
  if (IS_ALIVE(cell)) {
    // Check if we should kill the cell
    if (SHOULD_DIE(cell)) {
      MARK_DEAD(BOARD(outboard, i, j));

      jwest = mod_i (j-1, board_size);
      jeast = mod_i (j+1, board_size);
      inorth = mod_i (i-1, board_size);
      isouth = mod_i (i+1, board_size);

      // Update neighbours;
      DECR_NEIGHBOURS(outboard, inorth, jwest);
      DECR_NEIGHBOURS(outboard, inorth, j);
      DECR_NEIGHBOURS(outboard, inorth, jeast);
      DECR_NEIGHBOURS(outboard, i, jwest);
      DECR_NEIGHBOURS(outboard, i, jeast);
      DECR_NEIGHBOURS(outboard, isouth, jwest);
      DECR_NEIGHBOURS(outboard, isouth, j);
      DECR_NEIGHBOURS(outboard, isouth, jeast);
    }
  } else {
    // Check if we should make a cell
    if (SHOULD_SPAWN(cell)) {
      MARK_ALIVE(BOARD(outboard, i, j));

      jwest = mod_i (j-1, board_size);
      jeast = mod_i (j+1, board_size);
      inorth = mod_i (i-1, board_size);
      isouth = mod_i (i+1, board_size);

      // Update neighbours;
      INCR_NEIGHBOURS(outboard, inorth, jwest);
      INCR_NEIGHBOURS(outboard, inorth, j);
      INCR_NEIGHBOURS(outboard, inorth, jeast);
      INCR_NEIGHBOURS(outboard, i, jwest);
      INCR_NEIGHBOURS(outboard, i, jeast);
      INCR_NEIGHBOURS(outboard, isouth, jwest);
      INCR_NEIGHBOURS(outboard, isouth, j);
      INCR_NEIGHBOURS(outboard, isouth, jeast);
    }
  }
}

/**
 * The runner thread function.
 */
void *gol_task_runner(void *args) {
  gol_task_neighbours(args); // neighbour count
  pthread_exit(NULL);
}

/**
 * Initialize neighbour counts for a new board.
 */
void init_board_neighbours(char *board, int board_size) {
  // Initialize board for neighbour counts
  int i, j;
  for (j = 0; j < board_size*board_size; j++) {
    if (board[j] == (char)1) {
      board[j] = 0;
      MARK_ALIVE(board[j]);
    }
  }

  int inorth, isouth, jwest, jeast;
  for (j = 0; j < board_size; j++) {
    for (i = 0; i < board_size; i++) {
      if (IS_ALIVE(BOARD(board, i, j))) {
        jwest = mod_i (j-1, board_size);
        jeast = mod_i (j+1, board_size);
        inorth = mod_i (i-1, board_size);
        isouth = mod_i (i+1, board_size);

        INCR_NEIGHBOURS(board, inorth, jwest);
        INCR_NEIGHBOURS(board, inorth, j);
        INCR_NEIGHBOURS(board, inorth, jeast);
        INCR_NEIGHBOURS(board, i, jwest);
        INCR_NEIGHBOURS(board, i, jeast);
        INCR_NEIGHBOURS(board, isouth, jwest);
        INCR_NEIGHBOURS(board, isouth, j);
        INCR_NEIGHBOURS(board, isouth, jeast);

      }
    }
  }
}

/**
 * Simpler and faster mod that works for GOL
 */
static inline int mod_i(int i, int m) {
  if (i < 0) return m + i;
  return (i < m) ? i : i-m;
}

