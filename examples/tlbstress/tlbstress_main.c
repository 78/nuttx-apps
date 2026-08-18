/****************************************************************************
 * apps/examples/tlbstress/tlbstress_main.c
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Cross-core address-environment stress for G.2.1b.  Two threads in the same
 * process share one Sv32 addrenv.  The producer grows the heap on one CPU;
 * the consumer immediately reads and writes the new pages on the other CPU.
 * Without mapping-side TLB shootdown, a remote hart may keep a stale
 * translation and fault or observe garbage.
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

#define TLBSTRESS_PAGE_SIZE       4096u
#define TLBSTRESS_DEFAULT_ROUNDS  64
#define TLBSTRESS_DEFAULT_PAGES   16
#define TLBSTRESS_PATTERN(base, i) \
  ((uint32_t)(0xa5a50000u ^ (uint32_t)(uintptr_t)(base) ^ (uint32_t)(i)))

/****************************************************************************
 * Private Types
 ****************************************************************************/

enum tlbstress_mode_e
{
  TLBSTRESS_MODE_AUTO = 0,
  TLBSTRESS_MODE_PIN
};

struct tlbstress_slot_s
{
  FAR uint8_t *page;
  uint32_t     generation;
  volatile int ready;
  volatile int done;
  volatile int fault;
};

struct tlbstress_state_s
{
  enum tlbstress_mode_e mode;
  int rounds;
  int pages_per_round;
  int producer_cpu;
  int consumer_cpu;
  volatile int stop;
  volatile int errors;
  struct tlbstress_slot_s slot;
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static void usage(FAR const char *progname)
{
  fprintf(stderr,
          "usage: %s [--mode auto|pin] [--rounds N] [--pages N]\n",
          progname);
}

static int parse_args(int argc, FAR char *argv[],
                      FAR struct tlbstress_state_s *state)
{
  int i;

  state->mode = TLBSTRESS_MODE_AUTO;
  state->rounds = TLBSTRESS_DEFAULT_ROUNDS;
  state->pages_per_round = TLBSTRESS_DEFAULT_PAGES;
  state->producer_cpu = 0;
  state->consumer_cpu = 1;

  for (i = 1; i < argc; i++)
    {
      FAR const char *value = NULL;

      if (strcmp(argv[i], "--mode") == 0)
        {
          if (i + 1 >= argc)
            {
              usage(argv[0]);
              return -EINVAL;
            }

          value = argv[++i];
          if (strcmp(value, "auto") == 0)
            {
              state->mode = TLBSTRESS_MODE_AUTO;
            }
          else if (strcmp(value, "pin") == 0)
            {
              state->mode = TLBSTRESS_MODE_PIN;
            }
          else
            {
              usage(argv[0]);
              return -EINVAL;
            }
        }
      else if (strcmp(argv[i], "--rounds") == 0)
        {
          if (i + 1 >= argc)
            {
              usage(argv[0]);
              return -EINVAL;
            }

          state->rounds = (int)strtol(argv[++i], NULL, 10);
        }
      else if (strcmp(argv[i], "--pages") == 0)
        {
          if (i + 1 >= argc)
            {
              usage(argv[0]);
              return -EINVAL;
            }

          state->pages_per_round = (int)strtol(argv[++i], NULL, 10);
        }
      else
        {
          usage(argv[0]);
          return -EINVAL;
        }
    }

  if (state->rounds < 1 || state->pages_per_round < 1)
    {
      fprintf(stderr, "tlbstress: rounds and pages must be >= 1\n");
      return -EINVAL;
    }

  return OK;
}

static int pin_self(int cpu)
{
  cpu_set_t cpuset;

  CPU_ZERO(&cpuset);
  CPU_SET(cpu, &cpuset);
  if (sched_setaffinity(0, sizeof(cpuset), &cpuset) < 0)
    {
      return -errno;
    }

  return OK;
}

static int verify_page(FAR uint8_t *page, uint32_t generation)
{
  FAR uint32_t *words = (FAR uint32_t *)page;
  size_t nwords = TLBSTRESS_PAGE_SIZE / sizeof(uint32_t);
  size_t i;

  for (i = 0; i < nwords; i++)
    {
      uint32_t expected = TLBSTRESS_PATTERN(page, i) ^ generation;

      if (words[i] != expected)
        {
          return -EILSEQ;
        }

      words[i] = expected ^ 0x5a5a5a5au;
    }

  return OK;
}

static void fill_page(FAR uint8_t *page, uint32_t generation)
{
  FAR uint32_t *words = (FAR uint32_t *)page;
  size_t nwords = TLBSTRESS_PAGE_SIZE / sizeof(uint32_t);
  size_t i;

  for (i = 0; i < nwords; i++)
    {
      words[i] = TLBSTRESS_PATTERN(page, i) ^ generation;
    }
}

static void *consumer_thread(FAR void *arg)
{
  FAR struct tlbstress_state_s *state = arg;
  int ret;

  if (state->mode == TLBSTRESS_MODE_PIN)
    {
      ret = pin_self(state->consumer_cpu);
      if (ret < 0)
        {
          fprintf(stderr, "tlbstress: consumer pin failed: %s\n",
                  strerror(-ret));
          state->stop = 1;
          state->errors++;
          return NULL;
        }
    }

  printf("TLBSTRESS:CONSUMER tid=%d cpu=%d\n", gettid(), sched_getcpu());
  fflush(stdout);

  while (!state->stop)
    {
      while (!state->slot.ready && !state->stop)
        {
          /* Busy-wait so we stay on the assigned CPU without sleeping into
           * a different scheduling window that would mask TLB races.
           */
        }

      if (state->stop)
        {
          break;
        }

      /* Pairs with the producer's release fence: page and generation are
       * plain fields, so they may only be read once ready is observed.
       */

      __atomic_thread_fence(__ATOMIC_SEQ_CST);

      ret = verify_page(state->slot.page, state->slot.generation);
      if (ret < 0)
        {
          state->slot.fault = 1;
          state->errors++;
          state->stop = 1;
        }

      state->slot.ready = 0;
      state->slot.done = 1;
    }

  return NULL;
}

static int run_producer(FAR struct tlbstress_state_s *state)
{
  int round;
  int page;
  int ret;

  if (state->mode == TLBSTRESS_MODE_PIN)
    {
      ret = pin_self(state->producer_cpu);
      if (ret < 0)
        {
          fprintf(stderr, "tlbstress: producer pin failed: %s\n",
                  strerror(-ret));
          return ret;
        }
    }

  printf("TLBSTRESS:PRODUCER tid=%d cpu=%d\n", gettid(), sched_getcpu());
  fflush(stdout);

  for (round = 0; round < state->rounds && !state->stop; round++)
    {
      for (page = 0; page < state->pages_per_round && !state->stop; page++)
        {
          FAR uint8_t *block;
          uint32_t generation = ((uint32_t)round << 16) | (uint32_t)page;

          block = malloc(TLBSTRESS_PAGE_SIZE);
          if (block == NULL)
            {
              fprintf(stderr, "tlbstress: malloc failed round=%d page=%d\n",
                      round, page);
              state->errors++;
              return -ENOMEM;
            }

          fill_page(block, generation);

          state->slot.page = block;
          state->slot.generation = generation;
          state->slot.fault = 0;
          state->slot.done = 0;
          __atomic_thread_fence(__ATOMIC_SEQ_CST);
          state->slot.ready = 1;

          while (!state->slot.done && !state->stop)
            {
            }

          if (state->slot.fault || state->stop)
            {
              free(block);
              return state->slot.fault ? -EFAULT : -EIO;
            }

          free(block);
        }
    }

  return OK;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  struct tlbstress_state_s state;
  pthread_attr_t attr;
  pthread_t consumer;
  int ret;

  memset(&state, 0, sizeof(state));
  ret = parse_args(argc, argv, &state);
  if (ret < 0)
    {
      return 2;
    }

  printf("TLBSTRESS:START pid=%d mode=%s rounds=%d pages=%d "
         "producer-cpu=%d consumer-cpu=%d\n",
         getpid(),
         state.mode == TLBSTRESS_MODE_PIN ? "pin" : "auto",
         state.rounds, state.pages_per_round,
         state.producer_cpu, state.consumer_cpu);
  fflush(stdout);

  pthread_attr_init(&attr);
  pthread_attr_setstacksize(&attr, CONFIG_EXAMPLES_TLBSTRESS_STACKSIZE);

  ret = pthread_create(&consumer, &attr, consumer_thread, &state);
  pthread_attr_destroy(&attr);
  if (ret != 0)
    {
      fprintf(stderr, "tlbstress: pthread_create failed: %s\n",
              strerror(ret));
      return 1;
    }

  ret = run_producer(&state);
  state.stop = 1;
  pthread_join(consumer, NULL);

  if (ret < 0 || state.errors != 0)
    {
      printf("TLBSTRESS:FAIL errors=%d ret=%d\n", state.errors, ret);
      fflush(stdout);
      return 1;
    }

  printf("TLBSTRESS:PASS rounds=%d pages=%d errors=0\n",
         state.rounds, state.pages_per_round);
  fflush(stdout);
  return 0;
}
