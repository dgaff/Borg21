#ifndef BORG_HOST_H
#define BORG_HOST_H

//============================================================================
// 2026 PORT -- the one seam where the simulation core calls back into its
// host application.
//
// In 1995 the global error() in EI.CPP forwarded straight to UI.displayError,
// which hard-wired Layers 1 and 2 to the Win16 GUI.  That single call was the
// ONLY Windows dependency below Layer 3.  Routing it through this hook lets
// the core link against any front end: the determinism test and the CLI print
// to stderr, and the ImGui GUI will raise a dialog.
//
// Whatever links borgcore must define this function.
//============================================================================

int hostDisplayError(int num, const char *str);

#endif
