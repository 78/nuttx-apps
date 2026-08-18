/****************************************************************************
 * apps/examples/busy/busy_main.c
 *
 * SPDX-License-Identifier: Apache-2.0
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
 * Private Functions
 ****************************************************************************/

static void usage(FAR const char *progname)
{
  fprintf(stderr, "usage: %s [--threads N]\n", progname);
}

static int parse_threads(int argc, FAR char *argv[])
{
  int nthreads = 1;
  int i;

  for (i = 1; i < argc; i++)
    {
      FAR const char *value = NULL;

      if (strcmp(argv[i], "--threads") == 0)
        {
          if (i + 1 >= argc)
            {
              usage(argv[0]);
              return -EINVAL;
            }

          value = argv[++i];
        }
      else if (strncmp(argv[i], "--threads=", 10) == 0)
        {
          value = argv[i] + 10;
        }
      else
        {
          usage(argv[0]);
          return -EINVAL;
        }

      nthreads = (int)strtol(value, NULL, 10);
      if (nthreads < 1)
        {
          fprintf(stderr, "busy: --threads must be >= 1\n");
          return -EINVAL;
        }
    }

  return nthreads;
}

static void *busy_spin(FAR void *arg)
{
  volatile uint32_t mix = (uint32_t)(uintptr_t)arg | 1u;

  printf("BUSY:THREAD tid=%d cpu=%d\n", gettid(), sched_getcpu());
  fflush(stdout);

  for (; ; )
    {
      mix = mix * 1664525u + 1013904223u;
    }

  return (FAR void *)(uintptr_t)mix;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  pthread_attr_t attr;
  pthread_t thread;
  int nthreads;
  int ret;
  int i;

  nthreads = parse_threads(argc, argv);
  if (nthreads < 0)
    {
      return 2;
    }

  printf("BUSY:START pid=%d mode=auto cpu=%d priority=%d rr-ms=%d "
         "threads=%d\n",
         getpid(), sched_getcpu(), CONFIG_EXAMPLES_BUSY_PRIORITY,
         CONFIG_RR_INTERVAL, nthreads);
  fflush(stdout);

  pthread_attr_init(&attr);
  pthread_attr_setstacksize(&attr, CONFIG_EXAMPLES_BUSY_STACKSIZE);

  for (i = 1; i < nthreads; i++)
    {
      ret = pthread_create(&thread, &attr, busy_spin,
                           (FAR void *)(uintptr_t)(i + 1));
      if (ret != 0)
        {
          fprintf(stderr, "busy: pthread_create failed: %s\n",
                  strerror(ret));
          pthread_attr_destroy(&attr);
          return 1;
        }

      pthread_detach(thread);
    }

  pthread_attr_destroy(&attr);
  busy_spin((FAR void *)(uintptr_t)1);
  return 0;
}
