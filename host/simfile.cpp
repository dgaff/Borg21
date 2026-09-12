//============================================================================
// Lifted from userInterface::save() (UI.CPP:1999) and userInterface::load()
// (UI.CPP:2037).  The file I/O is reproduced exactly; what is dropped is the
// Layer 3 scaffolding those two functions were wrapped in:
//
//   SetCursor(LoadCursor(NULL,IDC_WAIT)) / IDC_ARROW   hourglass while writing
//   calcDimensions()                                   rescale the window
//   updateScreen(RESET_UPDATE)                         repaint
//
// calcDimensions() and updateScreen() are pure presentation -- they recompute
// pixel scaling for the drawing area -- so a headless load must not call them.
// The new GUI will call its own equivalents after borgLoadSim() returns.
//============================================================================

#include <stdio.h>
#include <string.h>

#include "task.h"
#include "ei.h"
#include "host.h"
#include "simfile.h"

//----------------------------------------------------------------------------
int borgSaveSim(const char *filename)
{
  char verStr[4] = "2.1";
  char msg[300];

  remove(filename);
  FILE *fptr = fopen(filename,"wb");

  // 2026 PORT: 1995 did not check this and wrote through a null FILE*.  With one
  // user watching one window that was survivable; in a long unattended batch a
  // read-only directory would take the whole run down with no explanation.
  if (!fptr)
  {
    snprintf(msg,sizeof(msg),"Cannot open '%s' for writing",filename);
    error(BORG_ERR_FILE_WRITE,msg);
    return 0;
  }

  fwrite(verStr,sizeof(char),4,fptr);
  task.saveStatic(fptr);
  task.save(fptr);

  // 2026 PORT: also unchecked in 1995.  A full disk partway through a batch
  // would otherwise leave a short .sim that silently loads as garbage.
  int writeFailed = ferror(fptr);
  if (fclose(fptr) != 0 || writeFailed)
  {
    snprintf(msg,sizeof(msg),
             "Failed while writing '%s' -- the file is incomplete",filename);
    error(BORG_ERR_FILE_WRITE,msg);
    return 0;
  }

  return 1;
}

//----------------------------------------------------------------------------
// Returns 0 and stays quiet if the file is simply not there, which is what the
// 1995 load() did (`if (!fptr) return 0;`) and what callers expect.  A file that
// exists but is not a Borg 2.1 snapshot raises error 12, as it did in 1995.
//----------------------------------------------------------------------------
int borgLoadSim(const char *filename)
{
  char verStr[4];

  FILE *fptr = fopen(filename,"rb");
  if (!fptr) return 0;

  // 2026 PORT: guard the short-file case.  1995 fread() 4 bytes into verStr
  // without checking the count, then strcmp()'d it; on an empty or truncated
  // file that compares uninitialised stack bytes and runs off the buffer looking
  // for a terminator.  save() always writes "2.1\0", so forcing the terminator
  // changes nothing for a well-formed file.
  if (fread(verStr,sizeof(char),4,fptr) != 4)
  { fclose(fptr); error(12,filename); return 0; }
  verStr[3] = '\0';

  if (strcmp(verStr,"2.1") != 0)
  { fclose(fptr); error(12,filename); return 0; }

  //--------------------------------------------------------------------------
  // 2026 PORT: the version string does NOT distinguish a 1995 16-bit .SIM from
  // a native one.  Both open with the same four bytes "2.1\0", because the
  // stamp is a char[4] and char was one byte in both worlds.  So the check above
  // passes for a 1995 file and task.loadStatic() then reads every field at the
  // wrong width -- no error, no crash, just a simulation built out of nonsense.
  // That is precisely the failure mode this port has been written to refuse.
  //
  // The first field after the stamp is message::length, an int.  Read natively
  // it is 4 bytes; in a 1995 file those 4 bytes hold the 2-byte length followed
  // by the 2-byte seed, so a file with seed 100 and length 9 presents as
  // 100*65536 + 9 = 6553609.  Any message length outside a sane range therefore
  // means the widths are wrong, and the fix is host/legacy.h.
  //
  // This is a heuristic, not a proof: a 1995 file saved with seed 0 would slip
  // through.  The legacy reader's exact-EOF rule is the check that cannot be
  // fooled; this one exists to turn the common case into a sentence instead of
  // silent garbage.
  //--------------------------------------------------------------------------
  int peekMsgLen = 0;
  if (fread(&peekMsgLen,sizeof(int),1,fptr) != 1)
  { fclose(fptr); error(12,filename); return 0; }
  fseek(fptr,(long)(4*sizeof(char)),SEEK_SET);

  if (peekMsgLen < 1 || peekMsgLen > 4096)
  {
    char msg[500];
    snprintf(msg,sizeof(msg),
             "'%s' carries a \"2.1\" stamp but its first field reads as a message "
             "length of %d, so its integers are not this build's width. This is "
             "almost certainly a 1995 16-bit .SIM -- convert it first with "
             "\"borg legacy convert %s <out.sim>\".",
             filename,peekMsgLen,filename);
    fclose(fptr);
    error(BORG_ERR_LEGACY_FILE,msg);
    return 0;
  }

  task.loadStatic(fptr);
  task.load(fptr);

  fclose(fptr);
  return 1;
}

//============================================================================
// .P -- binary agent positions.  Lifted from userInterface::savePositionsBin()
// and loadPositionsBin() (UI.CPP).  What is dropped is again only Layer 3: the
// file dialog, the wait cursor, and updateScreen(RESET_UPDATE).
//
// 1995 passed the FILE* straight in without checking it, so a bad path meant
// fwrite() through a null pointer.  Checked here for the same reason as in
// borgSaveSim(): a headless run has nobody watching a window.
//============================================================================
int borgSavePositions(const char *filename)
{
  char msg[300];

  remove(filename);
  FILE *fptr = fopen(filename,"wb");
  if (!fptr)
  {
    snprintf(msg,sizeof(msg),"Cannot open '%s' for writing",filename);
    error(BORG_ERR_FILE_WRITE,msg);
    return 0;
  }

  task.saveAgentPosBinary(fptr);

  int writeFailed = ferror(fptr);
  if (fclose(fptr) != 0 || writeFailed)
  {
    snprintf(msg,sizeof(msg),
             "Failed while writing '%s' -- the file is incomplete",filename);
    error(BORG_ERR_FILE_WRITE,msg);
    return 0;
  }

  return 1;
}

//----------------------------------------------------------------------------
// Remember that this resets the simulation -- see the note in simfile.h.
//----------------------------------------------------------------------------
int borgLoadPositions(const char *filename)
{
  FILE *fptr = fopen(filename,"rb");
  if (!fptr) return 0;

  task.loadAgentPosBinary(fptr);

  fclose(fptr);
  return 1;
}
