//============================================================================
// The one taskEnvironment.
//
// TASK.H:187 declares `extern taskEnvironment task;` and both EI.CPP and
// TASK.CPP reach for it by name, so borgcore is not linkable without a
// definition somewhere.  In 1995 that definition lived in BORG.CPP -- Layer 3 --
// next to G and UI, and the comment there records the required construction
// order: G, then task, then UI.
//
// 2026 PORT: the definition moves into the library instead.  Leaving it to the
// front end meant every new main() had to know to declare it, which is a trap
// rather than a design.  The 1995 ordering constraint does not follow it here:
// it existed because `UI`'s constructor read the task environment and `G` had to
// own a device context first.  `task` itself needs only the classifierSystem and
// message statics, and those (CFS.CPP:12, MESSAGE.CPP:4) are constant-
// initialised, which the standard sequences before any dynamic initialisation.
//
// Note that taskEnvironment's constructor (TASK.H:106) already calls reset(1),
// so the environment is fully allocated before main() runs and a front end
// should call the plain reset() from then on -- reset(1) a second time would
// skip deleteAll() and leak every array.
//============================================================================

#include "task.h"

taskEnvironment task;
