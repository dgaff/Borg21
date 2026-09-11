//============================================================================
// 2026 PORT -- state for the Borland/Turbo C rand() reimplementation.
// See compat/borland_rand.h for why this is necessary.
//============================================================================

#include "borland_rand.h"

unsigned long bc_randseed = 1UL;
