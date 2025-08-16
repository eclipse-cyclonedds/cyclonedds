// Copyright(c) 2024 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef SCHEDULING_HPP
#define SCHEDULING_HPP

#include <cstdint>
#include <array>
#include <optional>

#if defined(__linux) || defined(__APPLE__)
#include <pthread.h>
#include <sched.h>
#elif defined(_WIN32)
#include <windows.h>
#endif

namespace iox_psmx { namespace sched {

struct cpuset_t {
#if defined(__linux)
  cpuset_t() { CPU_ZERO (&x); }
  cpu_set_t x;
#elif defined(_WIN32)
  cpuset_t() : mask(0) { }
  uintptr_t mask;
#endif
};

#if defined(__linux) || defined(__APPLE__)
typedef int prioclass_t;
#elif defined(_WIN32)
typedef uint32_t prioclass_t;
#else
typedef int prioclass_t; // so we have something
#endif

struct class_prio {
  prioclass_t schedclass;
  int priority;
};

struct sched_info {
  std::optional<class_prio> prio;
  std::optional<cpuset_t> affinity;
};

bool sched_info_setpriority(sched_info& si, const std::string& x);
bool sched_info_setaffinity(sched_info& si, const std::string& x);
bool sched_info_apply(const sched_info& x);

} };

#endif /* SCHEDULING_HPP */
