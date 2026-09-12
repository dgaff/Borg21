//============================================================================
// .sim snapshot read/write, lifted out of Layer 3.
//
// The format is unchanged from 1995 (see CLAUDE.md, "File formats"):
//
//     char[4]              version string, currently "2.1"
//     task.saveStatic()    the shared static parameters
//     task.save()          per-agent instance state
//
// Raw fwrite of binary structs, so field order IS the format.  These functions
// are byte-compatible with what BORG.EXE wrote only for files produced by a
// build with the same type widths; a 16-bit .SIM from 1995 is NOT readable here
// because int was 2 bytes then and is 4 bytes now.  Reading those is Phase 4's
// job and will need a separate widening reader -- borgLoadSim() deliberately
// does not try to guess.
//============================================================================

#ifndef BORG_HOST_SIMFILE_H
#define BORG_HOST_SIMFILE_H

// Both return 1 on success, 0 on failure.  A failure is reported through
// error() / hostDisplayError() as well, exactly as the 1995 code did.
int borgSaveSim(const char *filename);
int borgLoadSim(const char *filename);

#endif
