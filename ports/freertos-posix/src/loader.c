/*
 * Copyright(c) 2006 to 2019 ZettaScale Technology and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */

/*
 * Launcher to run existing programs in the FreeRTOS+lwIP Simulator.
 *
 * Verification of FreeRTOS+lwIP compatibility in Continuous Integration (CI)
 * setups is another intended purpose.
 */

#include <assert.h>
#include <getopt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if _WIN32
# define EX_OK (0)
# define EX_USAGE (64)
# define LF "\r\n"
#else
# include <sysexits.h>
# define LF "\n"
#endif

#include <FreeRTOS.h>
#include <task.h>

/* Setup system hardware. */
void prvSetupHardware(void)
{
  /* No hardware to setup when running in the simulator. */
  return;
}

void vAssertCalled(unsigned long ulLine, const char * const pcFileName)
{
  taskENTER_CRITICAL();
  {
    fprintf(stderr, "[ASSERT] %s:%lu"LF, pcFileName, ulLine);
  }
  taskEXIT_CRITICAL();
  abort();
}

void vApplicationMallocFailedHook(void)
{
  vAssertCalled(__LINE__, __FILE__);
}

void vApplicationIdleHook(void) { return; }

void vApplicationTickHook( void ) { return; }

static void usage(const char *name)
{
  static const char fmt[] =
    "Usage: %s LAUNCHER_OPTIONS -- PROGRAM_OPTIONS"LF
    "Try '%s -h' for more information"LF;

  fprintf(stderr, fmt, name, name);
}

static void help(const char *name)
{
  static const char fmt[] =
    "Usage: %s LAUNCHER_OPTIONS -- PROGRAM_OPTIONS"LF
    ""LF
    "Launcher options:"LF
    " -h            Show this help message and exit"LF;

  fprintf(stdout, fmt, name);
}

typedef struct {
  int argc;
  char **argv;
} args_t;

extern int real_main(int argc, char *argv[]);

static void vMainTask(void *ptr)
{
  args_t *args = (args_t *)ptr;
  /* Reset getopt global variables. */
  opterr = 1;
  optind = 1;
  (void)real_main(args->argc, args->argv);
  vPortFree(args->argv);
  vPortFree(args);
  vTaskDelete(NULL);
  _Exit(0);
}

int
main(int argc, char *argv[])
{
  int opt;
  char *name;
  args_t *args = NULL;

  /* Determine program name. */
  assert(argc >= 0 && argv[0] != NULL);
  name = strrchr(argv[0], '/');
  if (name != NULL) {
    name++;
  } else {
    name = argv[0];
  }

  if ((args = pvPortMalloc(sizeof(*args))) == NULL) {
    return EX_OSERR;
  }

  memset(args, 0, sizeof(*args));

  /* Parse command line options. */
  while ((opt = getopt(argc, argv, ":a:dg:hn:")) != -1) {
    switch (opt) {
      case 'h':
        help(name);
        exit(EX_OK);
      case '?':
        fprintf(stderr, "Unknown option '%c'"LF, opt);
        usage(name);
        exit(EX_USAGE);
      case ':':
      /* fall through */
      default:
        fprintf(stderr, "Option '%c' requires an argument"LF, opt);
        usage(name);
        exit(EX_USAGE);
    }
  }

  /* Copy leftover arguments into a new array. */
  args->argc = (argc - optind) + 1;
  args->argv = pvPortMalloc((args->argc + 1) * sizeof(*args->argv));
  if (args->argv == NULL) {
    return EX_OSERR;
  }
  args->argv[0] = argv[0];
  for (int i = optind, j = 1; i < argc; i++, j++) {
    args->argv[j] = argv[i];
  }

  prvSetupHardware();

  xTaskCreate(vMainTask, name,
    configMINIMAL_STACK_SIZE, args, (tskIDLE_PRIORITY + 1UL),
    (xTaskHandle *) NULL);

  vTaskStartScheduler();

  return EX_SOFTWARE;
}
