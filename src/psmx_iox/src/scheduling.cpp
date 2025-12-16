//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <algorithm>
#include <cctype>
#include <string>
#include <cstring>
#include <memory>
#include <functional>

#include <stddef.h>
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"

#include "scheduling.hpp"

namespace iox_psmx { namespace sched {

#if defined(__linux) || defined(__APPLE__)
static bool valid_priority(prioclass_t cl, int prio)
{
  if (cl == SCHED_OTHER)
    return (prio == 0);
  return (prio >= sched_get_priority_min(cl) && prio <= sched_get_priority_max(cl));
}
#elif defined(_WIN32)
static bool valid_priority(prioclass_t cl, int prio)
{
  switch (prio)
  {
    case THREAD_PRIORITY_ABOVE_NORMAL:
    case THREAD_PRIORITY_BELOW_NORMAL:
    case THREAD_PRIORITY_HIGHEST:
    case THREAD_PRIORITY_IDLE:
    case THREAD_PRIORITY_LOWEST:
    case THREAD_PRIORITY_NORMAL:
    case THREAD_PRIORITY_TIME_CRITICAL:
      return true;
    case -7: case -6: case -5: case -4: case -3:
    case 3: case 4: case 5: case 6:
      return (cl == REALTIME_PRIORITY_CLASS);
    default:
      return false;
  }
}
#else
static bool valid_priority(prioclass_t cl, int prio)
{
  static_cast<void>(cl);
  static_cast<void>(prio);
  return false;
}
#endif

static void trim(std::string &s)
{
  s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {return !std::isspace(ch);}).base(), s.end());
  s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {return !std::isspace(ch);}));
}

bool sched_info_setpriority(sched_info& si, const std::string& x)
{
  size_t priostart = x.find(':');
  prioclass_t cl;
#if defined(__linux) || defined(__APPLE_)
  // The reason to raise the priority of the Iceoryx listener thread is to get the latencies
  // down, so some a real-time scheduling class seems the most reasonable default
  cl = SCHED_FIFO;
#elif defined(_WIN32)
  cl = GetPriorityClass(GetCurrentProcess());
#else
  cl = 0;
#endif
  if (priostart == x.npos)
    priostart = 0;
  else
  {
    std::string cstr = x.substr(0, priostart);
    trim(cstr);
    std::transform(cstr.begin(), cstr.end(), cstr.begin(), [](unsigned char c){ return std::toupper(c); });
#if defined(__linux) || defined(__APPLE__)
    if (cstr == "OTHER")         cl = SCHED_OTHER;
    else if (cstr == "FIFO")     cl = SCHED_FIFO;
    else if (cstr == "RT")       cl = SCHED_RR;
    else return false;
#else
    return false;
#endif
    ++priostart;
  }
  int prio;
  try {
    prio = std::stoi(x.substr(priostart));
  } catch (std::exception()) {
    return false;
  }
  if (!valid_priority(cl, prio))
    return false;
  si.prio = class_prio{cl, prio};
  return true;
}

static bool cpuset_set(cpuset_t& set, int id)
{
#if defined(__linux)
  if (id < 0 || id >= CPU_SETSIZE)
    return false;
  CPU_SET(id, &set.x);
  return true;
#elif defined(_WIN32)
  if (id < 0 || id >= CHAR_BIT * sizeof (uintptr_t))
    return false;
  set.mask |= (uintptr_t)1 << id;
  return true;
#else
  static_cast<void>(set);
  static_cast<void>(id);
  return false;
#endif
}

bool sched_info_setaffinity(sched_info& si, const std::string& x)
{
  cpuset_t cpuset;
  std::unique_ptr<char, std::function<void(void *)>> copy{ddsrt_strdup(x.c_str()), ddsrt_free};
  char *cursor = copy.get(), *tok;
  while ((tok = ddsrt_strsep(&cursor, ",")) != nullptr)
  {
    int v, e, pos;
    if (sscanf(tok, "%d-%d%n", &v, &e, &pos) == 2) {
      // skip
    } else if (sscanf(tok, "%d%n", &v, &pos) == 1) {
      e = v;
    } else {
      return false;
    }
    if (e < v || tok[pos] != 0) {
      return false;
    }
    for (int i = v; i <= e; i++) {
      if (!cpuset_set(cpuset, v))
        return false;
    }
  }
  si.affinity = cpuset;
  return true;
}

static bool set_thread_priority(const class_prio& cp)
{
#if defined(__linux) || defined(__APPLE__)
  struct sched_param param;
  memset(static_cast<void *>(&param), 0, sizeof(param));
  param.sched_priority = cp.priority;
  if (pthread_setschedparam(pthread_self(), cp.schedclass, &param) == 0)
    return true;
#elif defined(_WIN32)
  if (SetThreadPriority(GetCurrentThread(), cp.priority))
    return true;
#else
  static_cast<void>(cp);
#endif
  return false;
}

static bool set_thread_affinity(const cpuset_t& affinity)
{
#if defined(__linux)
  if (pthread_setaffinity_np(pthread_self(), sizeof(affinity.x), &affinity.x) == 0)
    return true;
#elif defined(_WIN32)
  if (SetThreadAffinityMask(GetCurrentThread(), affinity.mask))
    return true;
#else
  static_cast<void>(affinity);
#endif
  return false;
}

bool sched_info_apply(const sched_info& si)
{
  if (si.prio.has_value()) {
    if (!set_thread_priority(si.prio.value()))
      return false;
  }
  if (si.affinity.has_value()) {
    if (!set_thread_affinity(si.affinity.value()))
      return false;
  }
  return true;
}

} } // namespace
