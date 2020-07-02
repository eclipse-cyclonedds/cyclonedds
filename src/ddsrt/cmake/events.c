#include "dds/ddsrt/events.h"

#if defined __APPLE__
# error "cmake_HAVE_EVENTS=kqueue"
# else
# error "cmake_HAVE_EVENTS=true"
#endif


