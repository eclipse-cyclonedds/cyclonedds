/* #include <zephyr/kernel.h> */

#include <stdio.h>
#include <stdlib.h>
#include <dds/ddsrt/environ.h>

#include "ddsperf_config.h"

int ddsperf_main(int argc, char **argv);

char **environ = (char*[]) {
  (char*)&cdds_xml_config,
  NULL
};

void main(void)
{
    printf("CycloneDDS DDSPerf (%s)\n", CONFIG_BOARD);
#if BUILD_DDSPERF_PING
    char *args[] = { "ddsperf", "ping" };
#else
    char *args[] = { "ddsperf", "pong" };
#endif
    ddsperf_main(sizeof(args)/sizeof(args[0]), args);
    printf("Done\n");
    return;
}
