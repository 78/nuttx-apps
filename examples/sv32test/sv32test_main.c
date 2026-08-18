/****************************************************************************
 * apps/examples/sv32test/sv32test_main.c
 *
 * SPDX-License-Identifier: Apache-2.0
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <nuttx/compiler.h>

#include <stdint.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define SV32_HEAP_GROW_SIZE (1024 * 1024)
#define SV32_PAGE_SIZE      4096
#define SV32_WIFI_MAC_ADDR  0x20104000

/****************************************************************************
 * Private Data
 ****************************************************************************/

static volatile uint32_t g_user_data = 0x13579bdf;
static volatile uint32_t g_exec_data aligned_data(4) = 0x00008067;

/****************************************************************************
 * Private Functions
 ****************************************************************************/

static noinline_function int sv32_text_probe(int value)
{
  return value + 1;
}

static void sv32_marker(FAR const char *marker)
{
  printf("SV32TEST:%s pid=%d\n", marker, getpid());
  fflush(stdout);
}

static int sv32_usage(FAR const char *progname)
{
  fprintf(stderr,
          "usage: %s ok|heap-grow|busy|illegal|write-text|exec-data|"
          "kernel-read|wifi-reg-read|forever\n",
          progname);
  return 2;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

int main(int argc, FAR char *argv[])
{
  FAR volatile uint8_t *text;

  if (argc != 2)
    {
      return sv32_usage(argv[0]);
    }

  text = (FAR volatile uint8_t *)(uintptr_t)sv32_text_probe;

  if (strcmp(argv[1], "ok") == 0)
    {
      uint8_t opcode = *text;
      uint32_t expected = 0x13579bdf ^ 0x2468ace0;
      uint32_t actual;
      int probe;

      g_user_data ^= 0x2468ace0;
      probe = sv32_text_probe(41);
      actual = g_user_data;
      printf("SV32TEST:OK values probe=%d data=%08x expected=%08x\n",
             probe, (unsigned int)actual, (unsigned int)expected);
      if (probe != 42 || actual != expected)
        {
          sv32_marker("OK FAIL");
          return 1;
        }

      printf("SV32TEST:OK PASS pid=%d text=%p opcode=%02x data=%08x\n",
             getpid(), (FAR void *)(uintptr_t)sv32_text_probe, opcode,
             (unsigned int)g_user_data);
      return 0;
    }

  if (strcmp(argv[1], "heap-grow") == 0)
    {
      FAR uint8_t *buffer;
      uint32_t checksum = 0;
      size_t offset;

      sv32_marker("HEAP_GROW BEGIN");
      buffer = malloc(SV32_HEAP_GROW_SIZE);
      if (buffer == NULL)
        {
          sv32_marker("HEAP_GROW FAIL");
          return 1;
        }

      for (offset = 0; offset < SV32_HEAP_GROW_SIZE;
           offset += SV32_PAGE_SIZE)
        {
          buffer[offset] = (uint8_t)(offset >> 12);
          checksum += buffer[offset];
        }

      buffer[SV32_HEAP_GROW_SIZE - 1] = 0xa5;
      checksum += buffer[SV32_HEAP_GROW_SIZE - 1];
      free(buffer);

      printf("SV32TEST:HEAP_GROW PASS bytes=%u checksum=%08x\n",
             (unsigned int)SV32_HEAP_GROW_SIZE, (unsigned int)checksum);
      return 0;
    }

  if (strcmp(argv[1], "write-text") == 0)
    {
      uint8_t opcode = *text;

      sv32_marker("WRITE_TEXT BEGIN");
      *text = opcode;
      sv32_marker("WRITE_TEXT FAIL");
      return 1;
    }

  if (strcmp(argv[1], "busy") == 0)
    {
      uint32_t i;
      volatile uint32_t mix = 1;

      sv32_marker("BUSY BEGIN");
      for (i = 0; i < 200000000u; i++)
        {
          mix = mix * 1664525u + 1013904223u;
        }

      printf("SV32TEST:BUSY PASS mix=%08x\n", (unsigned int)mix);
      return 0;
    }

  if (strcmp(argv[1], "forever") == 0)
    {
      uint32_t iterations = 0;
      volatile uint32_t mix = 1;

      sv32_marker("FOREVER BEGIN");
      for (;;)
        {
          mix = mix * 1664525u + 1013904223u;
          if (++iterations == 1000000u)
            {
              /* Keep this task CPU-bound while still entering the kernel
               * periodically so a directed termination signal is observed
               * deterministically by the E.1 concurrency test.
               */

              iterations = 0;
              sched_yield();
            }
        }
    }

  if (strcmp(argv[1], "illegal") == 0)
    {
      sv32_marker("ILLEGAL BEGIN");
      __asm__ __volatile__(".word 0xffffffff");
      sv32_marker("ILLEGAL FAIL");
      return 1;
    }

  if (strcmp(argv[1], "exec-data") == 0)
    {
      FAR void (*data_function)(void) =
        (FAR void (*)(void))(uintptr_t)&g_exec_data;

      sv32_marker("EXEC_DATA BEGIN");
      data_function();
      sv32_marker("EXEC_DATA FAIL");
      return 1;
    }

  if (strcmp(argv[1], "kernel-read") == 0)
    {
      FAR volatile uint32_t *kernel_sram =
        (FAR volatile uint32_t *)(uintptr_t)CONFIG_RAM_VSTART;
      volatile uint32_t value;

      sv32_marker("KERNEL_READ BEGIN");
      value = *kernel_sram;
      printf("SV32TEST:KERNEL_READ FAIL value=%08x\n",
             (unsigned int)value);
      return 1;
    }

  if (strcmp(argv[1], "wifi-reg-read") == 0)
    {
      FAR volatile uint32_t *wifi_mac =
        (FAR volatile uint32_t *)(uintptr_t)SV32_WIFI_MAC_ADDR;
      volatile uint32_t value;

      sv32_marker("WIFI_REG_READ BEGIN");
      value = *wifi_mac;
      printf("SV32TEST:WIFI_REG_READ FAIL value=%08x\n",
             (unsigned int)value);
      return 1;
    }

  return sv32_usage(argv[0]);
}
