#ifndef BORG_COMPAT_BORLAND_RAND_H
#define BORG_COMPAT_BORLAND_RAND_H

//============================================================================
// 2026 PORT -- reimplementation of Borland C++ 4.02 / Turbo C rand().
//
// WHY THIS EXISTS (this is the single most important file in the port):
//
// getRandom() in CFS.H is, unchanged since 1995:
//
//     return (int)(((long)rand()*__num)/(RAND_MAX+1));
//
// Borland's RAND_MAX was 32767, so RAND_MAX+1 evaluated to 32768 and the
// division produced a uniform value in [0,__num).  On a modern toolchain
// RAND_MAX is 2147483647, so RAND_MAX+1 OVERFLOWS int to -2147483648 and
// every single draw returns 0 or a negative number.  Measured on macOS with
// the stock generator: getRandom(2) returned 0 on 504 of 1000 draws and 1 on
// none of them; getRandom(100) returned 0, -15, -56, -86, ...
//
// Nothing reports an error.  The simulation compiles, links, runs thousands
// of ticks, and produces a motionless animat with a rule list that can never
// diversify -- and negative draws used as array subscripts segfault.
//
// Rather than "fix" getRandom()'s arithmetic, restore the generator it was
// written against.  That matters beyond uniformity: classifierSystem::load()
// resumes a saved run by calling rand() exactly numRandCalls times, so the
// sequence itself is part of the saved-file contract and of every result in
// the thesis.  Changing the divisor would yield good-looking numbers that
// match neither the 1995 runs nor the .sim resume path.
//
// The generator is Turbo C's 32-bit LCG, taking the middle 15 bits:
//
//     seed = seed * 0x015A4E35 + 1;   return (seed >> 16) & 0x7FFF;
//
// NOTE ON ORDERING: <stdlib.h> is included FIRST, while RAND_MAX and rand
// still have their platform meanings, so that the real declarations are seen
// before the macros below shadow them.
//============================================================================

#include <stdlib.h>

#undef RAND_MAX
#define RAND_MAX 32767

//----------------------------------------------------------------------------
// Generator state.  Defined in compat/borland_rand.cpp.  Borland's runtime
// started from 1 if srand() was never called; classifierSystem::reset() always
// calls srand(seed) before drawing, so this initial value is a backstop only.
//----------------------------------------------------------------------------
extern unsigned long bc_randseed;

inline void bc_srand(unsigned seed)
{ bc_randseed = (unsigned long)seed; }

inline int bc_rand(void)
{ bc_randseed = bc_randseed * 0x015A4E35UL + 1UL;
  return (int)((bc_randseed >> 16) & 0x7FFFUL);
}

//----------------------------------------------------------------------------
// Shadow the standard names so the 1995 sources need no edits.  Every module
// reaches this header through CFS.H, which all of Layers 1 and 2 include.
//----------------------------------------------------------------------------
#define srand bc_srand
#define rand  bc_rand

//----------------------------------------------------------------------------
// Guard against a future toolchain or header order silently reintroducing the
// overflow this file exists to prevent.
//----------------------------------------------------------------------------
#if defined(__cplusplus) && __cplusplus >= 201103L
static_assert(RAND_MAX == 32767,
              "RAND_MAX must be Borland's 32767 or getRandom() in CFS.H "
              "overflows int and every random draw becomes 0 or negative.");
static_assert((long)RAND_MAX + 1L == 32768L,
              "RAND_MAX+1 must not overflow; see compat/borland_rand.h.");
#endif

#endif
