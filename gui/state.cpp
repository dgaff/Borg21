//============================================================================
// Front-end state, the borg.ini preferences, and the one callback the
// simulation core makes into its host.
//============================================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include "borggui.h"
#include <host.h>

guiState gui;

//============================================================================
// borg.ini
//
// 1995 used GetPrivateProfileString/WritePrivateProfileString against
// ".\\borg.ini" (readINI/writeINI, UI.CPP).  Those are Win16 API calls, but the
// file they read is a plain four-line text file and the checked-in BORG.INI is
// still in the tree:
//
//     [display]
//     Environment=On
//     Info=On
//     Axes=On
//     Objects=Scaled
//
// So this reads and writes that exact format rather than inventing a new
// preferences file.  The defaults here are the defaults the 1995 calls passed
// as their fallback arguments, which is why Objects defaults to Scaled even
// though userInterface's constructor initialised objectsToScale to 0 -- the INI
// read happened afterwards and won.
//============================================================================

#define BORG_INI_PATH "borg.ini"

static void iniLookup(const char *text, const char *key, const char *dflt,
                      char *out, int outSize)
{
  // Good enough for a four-key file: find "<key>=" at the start of a line and
  // copy to end of line, trimming CR so a DOS borg.ini reads correctly.
  snprintf(out,outSize,"%s",dflt);
  if (!text) return;

  int keyLen = (int)strlen(key);
  const char *p = text;
  while (p && *p)
  {
    const char *eol = strchr(p,'\n');
    int lineLen = eol ? (int)(eol-p) : (int)strlen(p);

    if (lineLen > keyLen && strncmp(p,key,keyLen) == 0 && p[keyLen] == '=')
    {
      int n = lineLen - keyLen - 1;
      while (n > 0 && (p[keyLen+1+n-1] == '\r' || p[keyLen+1+n-1] == ' ')) --n;
      if (n >= outSize) n = outSize-1;
      memcpy(out,p+keyLen+1,n);
      out[n] = '\0';
      return;
    }
    p = eol ? eol+1 : 0;
  }
}

void guiPrefsLoad(guiDisplayPrefs *p)
{
  p->antOn = p->infoOn = p->axesOn = 1;
  p->objectsToScale = 1;

  FILE *f = fopen(BORG_INI_PATH,"rb");
  if (!f) return;                       // absent is not an error; keep defaults

  char buf[2048];
  size_t n = fread(buf,1,sizeof(buf)-1,f);
  buf[n] = '\0';
  fclose(f);

  char v[32];
  iniLookup(buf,"Environment","On",v,sizeof(v));  p->antOn  = !strcmp(v,"On");
  iniLookup(buf,"Info",       "On",v,sizeof(v));  p->infoOn = !strcmp(v,"On");
  iniLookup(buf,"Axes",       "On",v,sizeof(v));  p->axesOn = !strcmp(v,"On");
  iniLookup(buf,"Objects","Scaled",v,sizeof(v));
  p->objectsToScale = !strcmp(v,"Scaled");
}

void guiPrefsSave(const guiDisplayPrefs *p)
{
  FILE *f = fopen(BORG_INI_PATH,"w");
  if (!f) return;
  fprintf(f,"[display]\n");
  fprintf(f,"Environment=%s\n",p->antOn ? "On" : "Off");
  fprintf(f,"Info=%s\n",       p->infoOn ? "On" : "Off");
  fprintf(f,"Axes=%s\n",       p->axesOn ? "On" : "Off");
  fprintf(f,"Objects=%s\n",    p->objectsToScale ? "Scaled" : "NotScaled");
  fclose(f);
}

//============================================================================
// The transient one-line notice along the bottom.  1995 put this sort of thing
// in the title bar (SetWindowText, "Borg : File Walk Mode, 3_7.p").
//============================================================================
void guiNotice(const char *fmt, ...)
{
  va_list ap;
  va_start(ap,fmt);
  vsnprintf(gui.notice,sizeof(gui.notice),fmt,ap);
  va_end(ap);
  gui.noticeUntil = ImGui::GetTime() + 6.0;
}

//============================================================================
// hostDisplayError -- the single seam between the simulation and its host.
//
// In 1995 the global error() in EI.CPP called UI.displayError directly, and that
// one call was the entire Windows dependency of Layers 1 and 2.  Phase 1 routed
// it through this hook instead.
//
// TWO DELIBERATE DIFFERENCES from userInterface::displayError (UI.CPP:2063):
//
//  1. It cannot show the message itself.  An error can be raised in the middle
//     of a clockTick(), several frames deep inside Layer 1, where there is no
//     ImGui frame to draw into.  So it queues, and the main loop raises the
//     modal next frame.  The 1995 code got away with a MessageBox because Win16
//     MessageBox ran its own message loop.
//
//  2. Codes 100 and up are NOT fatal.  The 1995 switch has no case for them, so
//     they fall into default:, which leaves fatal = 1 and calls exit(1) -- a
//     failed fopen would have taken the program down.  Those codes did not exist
//     in 1995 (host/host.h introduces them for conditions a Win16 program that
//     picked filenames from a common dialog could assume away), so honouring
//     that fall-through would be reproducing an accident, not a decision.
//
// The wording of the fatal messages is kept verbatim, typos included -- "Window
// creation failture" is what the program said.
//============================================================================
int hostDisplayError(int num, const char *str)
{
  const char *s = str ? str : "";
  int fatal = 1;

  switch (num)
  {
    case 0:  snprintf(gui.errorText,sizeof(gui.errorText),
                      "Memory allocation error in %s\n\n"
                      "Application will terminate",s);  break;
    case 1:  snprintf(gui.errorText,sizeof(gui.errorText),
                      "Array out of range error in %s\n\n"
                      "Application will terminate",s);  break;
    case 2:  snprintf(gui.errorText,sizeof(gui.errorText),
                      "Infinite Loop in %s\n\n"
                      "Application will terminate",s);  break;
    case 3:
    case 12: snprintf(gui.errorText,sizeof(gui.errorText),
                      "Incompatible file type\n\nOpen File Cancelled");
             fatal = 0;  break;
    case 10: snprintf(gui.errorText,sizeof(gui.errorText),
                      "Window creation failture in %s\n\n"
                      "Application will terminate",s);  break;
    case 11: snprintf(gui.errorText,sizeof(gui.errorText),
                      "Process creation failture in %s\n\n"
                      "Application will terminate",s);  break;
    case 13: snprintf(gui.errorText,sizeof(gui.errorText),
                      "Exit batch mode before executing a file walk-through\n\n"
                      "Request cancelled");
             fatal = 0;  break;
    default: snprintf(gui.errorText,sizeof(gui.errorText),"%s",s);
             fatal = (num < 100);      // see note 2 above
             break;
  }

  // Also to stderr, so a crash that never reaches a frame still leaves a trace.
  fprintf(stderr,"[borg error %d] %s\n",num,gui.errorText);

  gui.errorFatal  = fatal ? true : false;
  gui.errorPending = true;

  // A run must not continue past an error; 1995 stopped because the MessageBox
  // blocked the message pump that was delivering the ticks.
  if (gui.run == GUI_RUNNING) gui.run = GUI_IDLE;

  return 0;
}
