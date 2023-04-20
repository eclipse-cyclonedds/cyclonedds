// Copyright(c) 2006 to 2019 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#define _GNU_SOURCE /* Required for RUSAGE_THREAD. */
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <sys/resource.h>

#include "dds/ddsrt/rusage.h"

#if defined __linux
#include <stdio.h>
#include <dirent.h>

dds_return_t
ddsrt_getrusage_anythread (
  ddsrt_thread_list_id_t tid,
  ddsrt_rusage_t * __restrict usage)
{
  /* Linux' man pages happily state that the second field is the process/task name
     in parentheses, and that %s is the correct scanf conversion.  As it turns out
     the process name itself can contain spaces and parentheses ... so %s is not a
     good choice for the general case.  The others are spec'd as a character or a
     number, which suggests the correct procedure is to have the 2nd field start at
     the first ( and end at the last ) ...

     RSS is per-process, so no point in populating that one
     field 14, 15: utime, stime (field 1 is first)

     Voluntary and involuntary context switches can be found in .../status, but
     not in stat; and .../status does not give the time.  Crazy.  */
  const double hz = (double) sysconf (_SC_CLK_TCK);
  char file[100];
  FILE *fp;
  int pos;
  pos = snprintf (file, sizeof (file), "/proc/self/task/%lu/stat", (unsigned long) tid);
  if (pos < 0 || pos >= (int) sizeof (file))
    return DDS_RETCODE_ERROR;
  if ((fp = fopen (file, "r")) == NULL)
    return DDS_RETCODE_NOT_FOUND;
  /* max 2 64-bit ints plus some whitespace; need 1 extra for detecting something
     went wrong and we ended up gobbling up garbage; 64 will do */
  char save[64];
  size_t savepos = 0;
  int prevc, c;
  int field = 1;
  for (prevc = 0; (c = fgetc (fp)) != EOF; prevc = c)
  {
    if (field == 1)
    {
      if (c == '(')
        field = 2;
    }
    else if (field >= 2)
    {
      /* each close paren resets the field counter to 3 (the first is common,
         further ones are rare and occur only if the thread name contains a
         closing parenthesis), as well as the save space for fields 14 & 15
         that we care about. */
      if (c == ')')
      {
        field = 2;
        savepos = 0;
      }
      else
      {
        /* next field on transition of whitespace to non-whitespace */
        if (c != ' ' && prevc == ' ')
          field++;
        /* save fields 14 & 15 while continuing scanning to EOF on the off-chance
           that 14&15 initially appear to be in what ultimately turns out to be
           task name */
        if (field == 14 || field == 15)
        {
          if (savepos < sizeof (save) - 1)
            save[savepos++] = (char) c;
        }
      }
    }
  }
  fclose (fp);
  assert (savepos < sizeof (save));
  save[savepos] = 0;
  if (savepos == sizeof (save) - 1)
    return DDS_RETCODE_ERROR;
  /* it's really integer, but the conversion from an unknown HZ value is much
     less tricky in floating-point */
  double user, sys;
  if (sscanf (save, "%lf %lf%n", &user, &sys, &pos) != 2 || (save[pos] != 0 && save[pos] != ' '))
    return DDS_RETCODE_ERROR;
  usage->utime = (dds_time_t) (1e9 * user / hz);
  usage->stime = (dds_time_t) (1e9 * sys / hz);
  usage->idrss = 0;
  usage->maxrss = 0;
  usage->nvcsw = 0;
  usage->nivcsw = 0;

  pos = snprintf (file, sizeof (file), "/proc/self/task/%lu/status", (unsigned long) tid);
  if (pos < 0 || pos >= (int) sizeof (file))
    return DDS_RETCODE_ERROR;
  if ((fp = fopen (file, "r")) == NULL)
    return DDS_RETCODE_NOT_FOUND;
  enum { ERROR = 1, READ_HEADING, SKIP_TO_EOL, READ_VCSW, READ_IVCSW } state = READ_HEADING;
  savepos = 0;
  while (state != ERROR && (c = fgetc (fp)) != EOF)
  {
    switch (state)
    {
      case READ_HEADING:
        if (savepos < sizeof (save) - 1)
          save[savepos++] = (char) c;
        if (c == ':')
        {
          save[savepos] = 0;
          savepos = 0;
          if (strcmp (save, "voluntary_ctxt_switches:") == 0)
            state = READ_VCSW;
          else if (strcmp (save, "nonvoluntary_ctxt_switches:") == 0)
            state = READ_IVCSW;
          else
            state = SKIP_TO_EOL;
        }
        break;
      case SKIP_TO_EOL:
        if (c == '\n')
          state = READ_HEADING;
        break;
      case READ_VCSW:
      case READ_IVCSW:
        if (fscanf (fp, "%zu", (state == READ_VCSW) ? &usage->nvcsw : &usage->nivcsw) != 1)
          state = ERROR;
        else
          state = SKIP_TO_EOL;
        break;
      case ERROR:
        break;
    }
  }
  fclose (fp);
  return (state == ERROR) ? DDS_RETCODE_ERROR : DDS_RETCODE_OK;
}

dds_return_t
ddsrt_getrusage (enum ddsrt_getrusage_who who, ddsrt_rusage_t *usage)
{
  struct rusage buf;

  assert (who == DDSRT_RUSAGE_SELF || who == DDSRT_RUSAGE_THREAD);
  assert (usage != NULL);

  memset (&buf, 0, sizeof(buf));
  if (getrusage ((who == DDSRT_RUSAGE_SELF) ? RUSAGE_SELF : RUSAGE_THREAD, &buf) == -1)
    return DDS_RETCODE_ERROR;

  usage->utime = (buf.ru_utime.tv_sec * DDS_NSECS_IN_SEC) + (buf.ru_utime.tv_usec * DDS_NSECS_IN_USEC);
  usage->stime = (buf.ru_stime.tv_sec * DDS_NSECS_IN_SEC) + (buf.ru_stime.tv_usec * DDS_NSECS_IN_USEC);
  usage->maxrss = 1024 * (size_t) buf.ru_maxrss;
  usage->idrss = (size_t) buf.ru_idrss;
  usage->nvcsw = (size_t) buf.ru_nvcsw;
  usage->nivcsw = (size_t) buf.ru_nivcsw;
  return DDS_RETCODE_OK;
}
#elif defined (__APPLE__)
#include <mach/mach_init.h>
#include <mach/mach_port.h>
#include <mach/thread_act.h>

dds_return_t
ddsrt_getrusage_anythread (
  ddsrt_thread_list_id_t tid,
  ddsrt_rusage_t * __restrict usage)
{
  mach_msg_type_number_t cnt;
  thread_basic_info_data_t info;
  cnt = THREAD_BASIC_INFO_COUNT;
  if (thread_info ((mach_port_t) tid, THREAD_BASIC_INFO, (thread_info_t) &info, &cnt) != KERN_SUCCESS)
    return DDS_RETCODE_ERROR;

  /* Don't see an (easy) way to get context switch counts */
  usage->utime = info.user_time.seconds * DDS_NSECS_IN_SEC + info.user_time.microseconds * DDS_NSECS_IN_USEC;
  usage->stime = info.system_time.seconds * DDS_NSECS_IN_SEC + info.system_time.microseconds * DDS_NSECS_IN_USEC;
  usage->idrss = 0;
  usage->maxrss = 0;
  usage->nivcsw = 0;
  usage->nvcsw = 0;
  return DDS_RETCODE_OK;
}

dds_return_t
ddsrt_getrusage (enum ddsrt_getrusage_who who, ddsrt_rusage_t *usage)
{
  struct rusage buf;
  dds_return_t rc;

  assert (usage != NULL);

  memset (&buf, 0, sizeof(buf));
  if (getrusage (RUSAGE_SELF, &buf) == -1)
    return DDS_RETCODE_ERROR;

  switch (who) {
    case DDSRT_RUSAGE_THREAD:
      if ((rc = ddsrt_getrusage_anythread (pthread_mach_thread_np (pthread_self()), usage)) < 0)
        return rc;
      break;
    case DDSRT_RUSAGE_SELF:
      usage->utime = (buf.ru_utime.tv_sec * DDS_NSECS_IN_SEC) + (buf.ru_utime.tv_usec * DDS_NSECS_IN_USEC);
      usage->stime = (buf.ru_stime.tv_sec * DDS_NSECS_IN_SEC) + (buf.ru_stime.tv_usec * DDS_NSECS_IN_USEC);
      usage->nvcsw = (size_t) buf.ru_nvcsw;
      usage->nivcsw = (size_t) buf.ru_nivcsw;
      break;
  }
  usage->maxrss = (size_t) buf.ru_maxrss;
  usage->idrss = (size_t) buf.ru_idrss;
  return DDS_RETCODE_OK;
}
#endif
