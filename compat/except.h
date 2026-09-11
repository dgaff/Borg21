#ifndef BORG_COMPAT_EXCEPT_H
#define BORG_COMPAT_EXCEPT_H

//============================================================================
// 2026 PORT SHIM -- stands in for Borland C++ 4.02's <except.h>.
//
// Borland's operator new threw 'xalloc' on failure, and the 1995 sources wrap
// every allocation in a catch(xalloc) block (see the comment block in CFS.H).
// Standard C++ throws std::bad_alloc instead, so aliasing the old name keeps
// all ~30 existing catch sites working without touching one of them.
//
// The original sources say #include<except.h>, so this file is found through
// the compat/ include directory rather than by editing those lines.
//============================================================================

#include <new>

typedef std::bad_alloc xalloc;

#endif
