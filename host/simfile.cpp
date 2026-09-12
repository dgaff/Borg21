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

  task.loadStatic(fptr);
  task.load(fptr);

  fclose(fptr);
  return 1;
}
