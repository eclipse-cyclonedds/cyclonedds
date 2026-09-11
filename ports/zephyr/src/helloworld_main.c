#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <dds/dds.h>

#include "HelloWorldData.h"
#include "helloworld_config.h"

int helloworld_pub(int argc, char **argv);
int helloworld_sub(int argc, char **argv);

/* cdds_xml_config[] is generated from config.xml by GENERATE_CDDS_CONF and holds
 * the NUL-terminated string "CYCLONEDDS_URI=<CycloneDDS>...</CycloneDDS>".
 */
#define CDDS_URI_PREFIX "CYCLONEDDS_URI="
#define CDDS_GENERAL    "<General>"

/* Boards on which the DDS interface has to be selected explicitly. */
#if defined(CONFIG_BOARD_QEMU_X86)
#define CDDS_NETIF "eth0"
#elif defined(CONFIG_BOARD_S32Z270DC2_RTU0_R52)
#define CDDS_NETIF "ethernet@74b00000"
#endif

static void set_cyclonedds_uri(void)
{
  const char *xml = (const char *)cdds_xml_config + strlen(CDDS_URI_PREFIX);

#ifdef CDDS_NETIF
  /* Inject <Interfaces> into <General> so that the network interface is chosen
     explicitly, like the raw-config based version of this example did. */
  static const char iface[] =
    "<Interfaces><NetworkInterface name=\"" CDDS_NETIF "\"/></Interfaces>";
  static char buf[sizeof(cdds_xml_config) + sizeof(iface)];
  const char *general = strstr(xml, CDDS_GENERAL);

  if (general != NULL) {
    size_t head = (size_t)(general - xml) + strlen(CDDS_GENERAL);

    memcpy(buf, xml, head);
    memcpy(&buf[head], iface, sizeof(iface) - 1);
    strcpy(&buf[head + sizeof(iface) - 1], general + strlen(CDDS_GENERAL));
    xml = buf;
  }
#endif

  /* Publish the configuration through the environment; the POSIX layer owns
     the environ block, so no application-side environ definition is needed. */
  if (setenv("CYCLONEDDS_URI", xml, 1) != 0)
    printf("Failed to set CYCLONEDDS_URI\n");
}

int main(int argc, char **argv)
{
    printf("CycloneDDS Hello World! %s\n", CONFIG_BOARD);
    set_cyclonedds_uri();
#if BUILD_HELLOWORLD_PUB
    helloworld_pub(argc, argv);
#else
    helloworld_sub(argc, argv);
#endif
    printf("Done\n");
    return 0;
}
