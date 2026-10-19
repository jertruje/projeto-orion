#ifndef _ASSERT_H
#define _ASSERT_H

#include "panic.h"

#define assert(condition) do { if (!(condition)) { orion_panic("Assertion failed"); } } while(0)

#endif
