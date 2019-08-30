#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include <dds/ddsrt/heap.h>
#include <dds/ddsrt/string.h>
#include <dds/ddsrt/netstat.h>

struct ddsrt_netstat_control {
  char *name;
};

dds_return_t ddsrt_netstat_get (struct ddsrt_netstat_control *control, struct ddsrt_netstat *stats)
{
  FILE *fp;
  char save[256];
  int c;
  size_t np;
  int field = 0;
  if ((fp = fopen ("/proc/net/dev", "r")) == NULL)
    return DDS_RETCODE_ERROR;
  /* expected format: 2 header lines, then on each line, white space/interface
     name/colon and then numbers. Received bytes is 1st data field, transmitted
     bytes is 9th.

     SKIP_HEADER_1 skips up to and including the first newline; then SKIP_TO_EOL
     skips up to and including the second newline, so the first line that gets
     interpreted is the third.
   */
  dds_return_t res = DDS_RETCODE_NOT_FOUND;
  enum { SKIP_HEADER_1, SKIP_WHITE, READ_NAME, SKIP_TO_EOL, READ_SEP, READ_INT } state = SKIP_HEADER_1;
  np = 0;
  while (res == DDS_RETCODE_NOT_FOUND && (c = fgetc (fp)) != EOF) {
    switch (state) {
      case SKIP_HEADER_1:
        if (c == '\n') {
          state = SKIP_TO_EOL;
        }
        break;
      case SKIP_WHITE:
        if (c != ' ' && c != '\t') {
          save[np++] = (char) c;
          state = READ_NAME;
        }
        break;
      case READ_NAME:
        if (c == ':') {
          save[np] = 0;
          np = 0;
          if (strcmp (save, control->name) != 0)
            state = SKIP_TO_EOL;
          else
            state = READ_SEP;
        } else if (np < sizeof (save) - 1) {
          save[np++] = (char) c;
        }
        break;
      case SKIP_TO_EOL:
        if (c == '\n') {
          state = SKIP_WHITE;
        }
        break;
      case READ_SEP:
        if (c == '\n')
        {
          /* unexpected end of line */
          res = DDS_RETCODE_ERROR;
        }
        else if (c >= '0' && c <= '9')
        {
          field++;
          save[np++] = (char) c;
          state = READ_INT;
        }
        break;
      case READ_INT:
        if (c >= '0' && c <= '9')
        {
          if (np == sizeof (save) - 1)
          {
            res = DDS_RETCODE_ERROR;
            break;
          }
          save[np++] = (char) c;
        }
        else
        {
          save[np] = 0;
          np = 0;
          if (field == 1 || field == 2 || field == 9 || field == 10)
          {
            int pos;
            uint64_t val;
            if (sscanf (save, "%"SCNu64"%n", &val, &pos) != 1 || save[pos] != 0)
              res = DDS_RETCODE_ERROR;
            else
            {
              switch (field)
              {
                case  1: stats->ibytes = val; break;
                case  2: stats->ipkt   = val; break;
                case  9: stats->obytes = val; break;
                case 10: stats->opkt   = val; res = DDS_RETCODE_OK; break;
              }
            }
          }
          if (c == '\n' && res != DDS_RETCODE_OK)
          {
            /* newline before all expected fields have been read */
            res = DDS_RETCODE_ERROR;
          }
          state = READ_SEP;
        }
        break;
    }
  }
  fclose (fp);
  return res;
}

dds_return_t ddsrt_netstat_new (struct ddsrt_netstat_control **control, const char *device)
{
  struct ddsrt_netstat_control *c = ddsrt_malloc (sizeof (*c));
  struct ddsrt_netstat dummy;
  c->name = ddsrt_strdup (device);
  if (ddsrt_netstat_get (c, &dummy) != DDS_RETCODE_OK)
  {
    ddsrt_free (c->name);
    ddsrt_free (c);
    *control = NULL;
    return DDS_RETCODE_ERROR;
  }
  else
  {
    *control = c;
    return DDS_RETCODE_OK;
  }
}

dds_return_t ddsrt_netstat_free (struct ddsrt_netstat_control *control)
{
  ddsrt_free (control->name);
  ddsrt_free (control);
  return DDS_RETCODE_OK;
}
