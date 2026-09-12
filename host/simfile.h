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
// because int was 2 bytes then and is 4 bytes now.  borgLoadSim() deliberately
// does not try to guess: the widening reader for those files is host/legacy.h,
// which converts a 1995 .SIM into this format and then hands it back here.
//============================================================================

#ifndef BORG_HOST_SIMFILE_H
#define BORG_HOST_SIMFILE_H

// All four return 1 on success, 0 on failure.  A failure is reported through
// error() / hostDisplayError() as well, exactly as the 1995 code did.
int borgSaveSim(const char *filename);
int borgLoadSim(const char *filename);

//----------------------------------------------------------------------------
// .P -- the binary agent position dump, written by taskEnvironment::
// saveAgentPosBinary() and read by loadAgentPosBinary().  Lifted from
// userInterface::savePositionsBin()/loadPositionsBin() (UI.CPP), which were the
// same two calls wrapped in a file dialog and a cursor change.
//
// Note what loadAgentPosBinary() does beyond reading: the second field of a .P
// is the maximum number of positions a run could hold, so the loader derives
// maxCount from it, pushes it through classifierSystem::setSettings(), and calls
// reset().  Loading a .P therefore CHANGES THE LIVE SETTINGS and discards the
// current simulation -- it is not a read-only operation, and the 1995 GUI
// followed it with a full RESET_UPDATE repaint for exactly that reason.
//
// A .P holds no version stamp and no statics, so unlike a .sim it is the same
// shape in a NETWORK and a plain-LCS build.
//----------------------------------------------------------------------------
int borgSavePositions(const char *filename);
int borgLoadPositions(const char *filename);

#endif
