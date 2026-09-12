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

//----------------------------------------------------------------------------
// Error numbers.
//
// The 1995 table is in userInterface::displayError (UI.CPP:2063) and is closed:
//
//     0   memory allocation failed in <str>        fatal
//     1   array index out of range in <str>        fatal
//     2   infinite loop in <str>                   fatal
//     3   incompatible file type                   not fatal, <str> ignored
//    10   window creation failed in <str>          fatal
//    11   process creation failed in <str>         fatal
//    12   incompatible file type                   not fatal, <str> ignored
//    13   cannot file-walk during a batch run      not fatal
//
// There is no code for "could not open that file", because a Win16 program that
// picked its filenames from a common dialog could assume they existed.  A
// headless batch cannot.  Rather than borrow a 1995 number and have the old GUI
// announce a disk error as an infinite loop, new conditions get numbers from 100
// up and carry a complete sentence in <str> -- displayError's default case
// prints <str> verbatim, so these still read correctly in the 1995 handler.
//----------------------------------------------------------------------------
#define BORG_ERR_FILE_WRITE   100   // could not create or finish writing a file
#define BORG_ERR_BATCH_SCRIPT 101   // malformed .b batch script
#define BORG_ERR_LEGACY_FILE  102   // unreadable 1995 16-bit .SIM / .P / .RUL

#endif
