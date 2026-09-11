#ifndef BORG_COMPAT_VALUES_H
#define BORG_COMPAT_VALUES_H

//============================================================================
// 2026 PORT SHIM -- stands in for Borland C++ 4.02's <values.h>.
//
// TASK.CPP is the only consumer, and MAXFLOAT is the only symbol it wants.
//============================================================================

#include <float.h>

#ifndef MAXFLOAT
#define MAXFLOAT FLT_MAX
#endif

#endif
