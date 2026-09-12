//============================================================================
// File commands, through native macOS dialogs.
//
// 1995 used userInterface::selectFile (UI.CPP:2115), a wrapper round the Win16
// common dialogs with the filter strings from BORG.RC's STRINGTABLE:
//
//     Simulation Files(*.sim)      Binary Position Files(*.p)
//     Batch Files(*.b)             Rule Files(*.rul)
//     Position Files(*.sim,*.p)    -- for the file walk, which took either
//
// Those filters are reproduced below.  SDL's dialogs are native on macOS, which
// means sandbox-friendly paths, drag-and-drop, and recent places for free.
//
// ASYNCHRONY
//
// SDL_ShowOpenFileDialog returns immediately and calls back when the user is
// done, and the documentation does not promise which thread that happens on.  So
// the callback does nothing but record the choice under a mutex, and
// guiFileApplyPending() -- called once per frame from the main loop, outside any
// ImGui window and between ticks -- is what actually touches taskEnvironment.
//============================================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <SDL3/SDL.h>

#include "borggui.h"
#include "task.h"
#include "cfs.h"
#include <host.h>
#include <simfile.h>
#include <legacy.h>

extern SDL_Window *guiWindow;

//----------------------------------------------------------------------------
static const SDL_DialogFileFilter kSimFilter[]   = {{ "Simulation Files", "sim;SIM" }};
static const SDL_DialogFileFilter kRuleFilter[]  = {{ "Rule Files",       "rul;RUL" }};
static const SDL_DialogFileFilter kPosFilter[]   = {{ "Binary Position Files", "p;P" }};
static const SDL_DialogFileFilter kBatchFilter[] = {{ "Batch Files",      "b;B" }};
static const SDL_DialogFileFilter kWalkFilter[]  = {{ "Position Files",   "sim;SIM;p;P" }};
static const SDL_DialogFileFilter kPngFilter[]   = {{ "PNG Image",        "png" }};

//----------------------------------------------------------------------------
// The pending choice.
//----------------------------------------------------------------------------
static SDL_Mutex *pendMutex = NULL;
static guiFileOp  pendOp    = GUI_FILE_NONE;
static char       pendPath[1024];

static void pendInit(void)
{
  if (!pendMutex) pendMutex = SDL_CreateMutex();
}

static void SDLCALL guiDialogCallback(void *userdata, const char * const *files,
                                      int filter)
{
  (void)filter;
  guiFileOp op = (guiFileOp)(intptr_t)userdata;

  // files == NULL is an error; files[0] == NULL is a plain cancel.
  if (!files)
  {
    fprintf(stderr,"file dialog: %s\n",SDL_GetError());
    return;
  }
  if (!files[0]) return;

  pendInit();
  SDL_LockMutex(pendMutex);
  pendOp = op;
  snprintf(pendPath,sizeof(pendPath),"%s",files[0]);
  SDL_UnlockMutex(pendMutex);
}

//----------------------------------------------------------------------------
void guiFilePick(guiFileOp op)
{
  pendInit();

  const SDL_DialogFileFilter *filters = NULL;
  int nFilters = 0;
  int saving = 0;

  switch (op)
  {
    case GUI_FILE_OPEN_SIM:    filters = kSimFilter;   nFilters = 1; break;
    case GUI_FILE_SAVE_SIM:    filters = kSimFilter;   nFilters = 1; saving = 1; break;
    case GUI_FILE_LOAD_RULES:  filters = kRuleFilter;  nFilters = 1; break;
    case GUI_FILE_SAVE_RULES:  filters = kRuleFilter;  nFilters = 1; saving = 1; break;
    case GUI_FILE_LOAD_POS:    filters = kPosFilter;   nFilters = 1; break;
    case GUI_FILE_SAVE_POS:    filters = kPosFilter;   nFilters = 1; saving = 1; break;
    case GUI_FILE_OPEN_BATCH:  filters = kBatchFilter; nFilters = 1; break;
    case GUI_FILE_WALK_PICK:   filters = kWalkFilter;  nFilters = 1; break;
    case GUI_FILE_EXPORT_PNG:  filters = kPngFilter;   nFilters = 1; saving = 1; break;
    default: return;
  }

  void *ud = (void*)(intptr_t)op;

  if (saving)
    SDL_ShowSaveFileDialog(guiDialogCallback,ud,guiWindow,filters,nFilters,NULL);
  else
    SDL_ShowOpenFileDialog(guiDialogCallback,ud,guiWindow,filters,nFilters,NULL,
                           false);
}

//============================================================================
// Is this a 1995 16-bit .SIM, or one this build wrote?
//
// Both begin with the same four bytes, "2.1\0" -- the version string never
// changed, because the format never changed; what changed underneath it is that
// `int` went from 2 bytes to 4.  So the stamp cannot tell them apart and
// something else has to.
//
// The field right after the stamp is message::length, written by
// message::saveStatic as a single int.  Read as a 4-byte little-endian integer:
//
//   a modern file   ->  the real message length, 9 by default, always small
//   a 1995 file     ->  9 in the low two bytes and messageBoard::length in the
//                       high two, so 9 + 65536*boardLen -- at least 65545
//
// which is exactly the test borgLoadSim() uses to refuse a 1995 file
// (host/simfile.cpp).  Same test here, so Open can route the file to the right
// reader instead of failing and then guessing.
//
// Returns 1 modern, 0 legacy 16-bit, -1 unreadable or not a .sim at all.
//============================================================================
static int guiSimFlavour(const char *path)
{
  unsigned char head[8];
  FILE *f = fopen(path,"rb");
  if (!f) return -1;
  size_t n = fread(head,1,8,f);
  fclose(f);
  if (n != 8) return -1;

  if (!(head[0]=='2' && head[1]=='.' && head[2]=='1' && head[3]=='\0'))
    return -1;

  long msgLen = (long)head[4] | ((long)head[5]<<8) |
                ((long)head[6]<<16) | ((long)head[7]<<24);

  return (msgLen >= 1 && msgLen <= 4096) ? 1 : 0;
}

//----------------------------------------------------------------------------
// Load a .sim of either vintage.  A 1995 file is widened by host/legacy.h into a
// temporary native-layout file and then loaded through the ordinary path, which
// is the same two-step the CLI's `legacy replay` uses -- one reader, not two.
//----------------------------------------------------------------------------
static int guiLoadSimAnyVintage(const char *path)
{
  int flavour = guiSimFlavour(path);

  if (flavour < 0)
  {
    char msg[1200];
    snprintf(msg,sizeof(msg),
             "'%s' is not a Borg .sim file (no \"2.1\" version stamp).",path);
    error(BORG_ERR_LEGACY_FILE,msg);
    return 0;
  }

  if (flavour == 1)
  {
    if (!borgLoadSim(path)) return 0;
    guiNotice("Loaded %s",path);
    return 1;
  }

  // 1995, 16-bit.
  borgLegacySim sim;
  if (!borgLegacyReadSim(path,&sim)) return 0;

  char tmp[1100];
  snprintf(tmp,sizeof(tmp),"%s.borgtmp",path);

  int ok = borgLegacyWriteModernSim(&sim,tmp) && borgLoadSim(tmp);
  remove(tmp);

  if (ok)
    guiNotice("Loaded %s -- a 1995 16-bit .SIM, widened from %ld to %ld bytes "
              "(%s layout, %d agent(s), %d iterations)",
              path,sim.fileSize,sim.modernSize,
              sim.network ? "DLCS" : "plain LCS",sim.numAgents,sim.globalClock);

  borgLegacyFreeSim(&sim);
  return ok;
}

//============================================================================
// Is this a 1995 16-bit .P, or one this build wrote?
//
// Harder than a .sim, because a .P has no version stamp and no statics at all --
// just numbers.  taskEnvironment::saveAgentPosBinary writes:
//
//     int numAgents
//     int maxCount+1                        the allocation hint
//     per agent:  int ptCount
//                 coord[ptCount]            2 floats, 8 bytes, unchanged since 1995
//                 float[ptCount]            4 bytes, unchanged since 1995
//
// Only the ints changed width, 2 bytes to 4.  So the whole header chain can be
// walked as native ints and the implied file size compared with the real one; if
// they agree exactly the file is native, and if they do not it is not.  On a 1995
// file the very first read is already absurd -- numAgents and maxCount+1 sit in
// the low and high halves of one 4-byte read, so two agents and 1001 positions
// read back as 65,601,538 agents.
//
// This test is needed because borgLoadPositions() cannot fail: loadAgentPosBinary
// does no validation, so handing it a 1995 file would succeed and fill the
// trails with garbage rather than report anything.
//
// Returns 1 native, 0 not native (try the widening reader), -1 unreadable.
//============================================================================
static int guiPosIsNative(const char *path)
{
  FILE *f = fopen(path,"rb");
  if (!f) return -1;

  if (fseek(f,0,SEEK_END) != 0) { fclose(f); return -1; }
  long fileSize = ftell(f);
  rewind(f);

  int numAgents = 0, maxPts = 0;
  if (fread(&numAgents,sizeof(int),1,f) != 1 ||
      fread(&maxPts,   sizeof(int),1,f) != 1)
  { fclose(f); return -1; }

  if (numAgents < 1 || numAgents > 1024 || maxPts < 1)
  { fclose(f); return 0; }

  long implied = 2*(long)sizeof(int);

  for (int i = 0; i < numAgents; i++)
  {
    int ptCount = 0;
    if (fread(&ptCount,sizeof(int),1,f) != 1) { fclose(f); return 0; }
    if (ptCount < 0 || ptCount > maxPts)      { fclose(f); return 0; }

    long block = (long)ptCount * (long)(sizeof(coord) + sizeof(float));
    implied += (long)sizeof(int) + block;

    if (implied > fileSize) { fclose(f); return 0; }
    if (fseek(f,block,SEEK_CUR) != 0) { fclose(f); return 0; }
  }

  fclose(f);
  return (implied == fileSize) ? 1 : 0;
}

//============================================================================
// File > New.  restoreDefaults, then reset, then forget the filename -- the
// newRequest arm of processRequests (UI.CPP:337).
//============================================================================
void guiFileNew(void)
{
  if (gui.run == GUI_BATCH || gui.run == GUI_WALK) return;

  task.restoreDefaults();
  gui.agent = 0;
  task.reset();

  gui.simFile[0] = '\0';
  gui.run = GUI_IDLE;
  gui.simFinished = 0;

  guiSettingsInvalidate();
  SDL_SetWindowTitle(guiWindow,"Borg 2.1");
  guiNotice("New simulation: all parameters back to their defaults.");
}

//============================================================================
void guiFileSave(void)
{
  if (gui.simFile[0] == '\0') { guiFilePick(GUI_FILE_SAVE_SIM);  return; }
  if (borgSaveSim(gui.simFile)) guiNotice("Saved %s",gui.simFile);
}

//============================================================================
// File > Save Positions in ASCII.
//
// taskEnvironment::saveAgentPositions() takes no filename: it writes agent1.pos
// .. agentN.pos, obst.pos and goal.pos into the current directory, with fixed
// 8.3 names.  1995's menu item therefore had no dialog either -- it just ran.
// Kept as-is, with the file names reported, since otherwise there is no way to
// know where they went.
//============================================================================
void guiSavePositionsAscii(void)
{
  task.saveAgentPositions();

  char cwd[1024];
  const char *base = SDL_GetCurrentDirectory();
  snprintf(cwd,sizeof(cwd),"%s",base ? base : ".");
  if (base) SDL_free((void*)base);

  guiNotice("Wrote agent1.pos..agent%d.pos, obst.pos and goal.pos in %s",
            task.getNumAgents(),cwd);
}

//============================================================================
// Act on whatever the dialog produced.  Called once per frame.
//============================================================================
void guiFileApplyPending(void)
{
  if (!pendMutex) return;

  guiFileOp op;
  char path[1024];

  SDL_LockMutex(pendMutex);
  op = pendOp;
  snprintf(path,sizeof(path),"%s",pendPath);
  pendOp = GUI_FILE_NONE;
  SDL_UnlockMutex(pendMutex);

  if (op == GUI_FILE_NONE) return;

  switch (op)
  {
    //----------------------------------------------------------------------
    case GUI_FILE_OPEN_SIM:
      if (guiLoadSimAnyVintage(path))
      {
        snprintf(gui.simFile,sizeof(gui.simFile),"%s",path);
        gui.agent = 0;
        gui.run = GUI_IDLE;
        gui.simFinished = 0;
        guiSettingsInvalidate();

        char title[1100];
        snprintf(title,sizeof(title),"Borg 2.1 - %s",path);
        SDL_SetWindowTitle(guiWindow,title);
      }
      break;

    //----------------------------------------------------------------------
    case GUI_FILE_SAVE_SIM:
      if (borgSaveSim(path))
      {
        snprintf(gui.simFile,sizeof(gui.simFile),"%s",path);
        guiNotice("Saved %s",path);

        char title[1100];
        snprintf(title,sizeof(title),"Borg 2.1 - %s",path);
        SDL_SetWindowTitle(guiWindow,title);
      }
      break;

    //----------------------------------------------------------------------
    // .RUL is plain text and is the one format that IS interchangeable between
    // a 16-bit and a 64-bit build, and between a DLCS and a plain-LCS build --
    // it holds no binary scalars at all, just "condition action strength" lines
    // under a "; Agent N" header.  So the 1995 files load directly, with no
    // widening step; tests/fixtures/legacy/BBA.RUL is one of them.
    //----------------------------------------------------------------------
    case GUI_FILE_LOAD_RULES:
    {
      FILE *f = fopen(path,"r");
      if (!f)
      {
        char msg[1200];
        snprintf(msg,sizeof(msg),"Cannot open rule file '%s'",path);
        error(BORG_ERR_LEGACY_FILE,msg);
        break;
      }
      task.loadRules(f);
      fclose(f);
      snprintf(gui.ruleFile,sizeof(gui.ruleFile),"%s",path);
      guiNotice("Loaded rules from %s into %d agent(s)",path,task.getNumAgents());
      break;
    }

    case GUI_FILE_SAVE_RULES:
    {
      FILE *f = fopen(path,"w");
      if (!f)
      {
        char msg[1200];
        snprintf(msg,sizeof(msg),"Cannot create rule file '%s'",path);
        error(BORG_ERR_FILE_WRITE,msg);
        break;
      }
      task.saveRules(f);
      fclose(f);
      snprintf(gui.ruleFile,sizeof(gui.ruleFile),"%s",path);
      guiNotice("Wrote rules for %d agent(s) to %s",task.getNumAgents(),path);
      break;
    }

    //----------------------------------------------------------------------
    // A .P has no version stamp and no statics, so the only way to tell a 1995
    // one from a modern one is its size -- which host/legacy.h does by walking
    // the schema to exact EOF.  Try the native reader first, because that is the
    // cheap case, and fall back to the widening reader.
    //
    // Worth knowing before using this: loading a .P is NOT read-only.  The
    // second field is the maximum number of positions a run could hold, so
    // loadAgentPosBinary derives maxCount from it, pushes it through
    // setSettings, and resets -- discarding the current simulation.
    //----------------------------------------------------------------------
    case GUI_FILE_LOAD_POS:
    {
      int native = guiPosIsNative(path);

      if (native < 0)
      {
        char msg[1200];
        snprintf(msg,sizeof(msg),"Cannot read position file '%s'",path);
        error(BORG_ERR_LEGACY_FILE,msg);
        break;
      }

      if (native == 1)
      {
        if (borgLoadPositions(path))
        {
          snprintf(gui.posFile,sizeof(gui.posFile),"%s",path);
          gui.run = GUI_IDLE;
          guiSettingsInvalidate();
          guiNotice("Loaded positions from %s (this reset the simulation and "
                    "changed maxCount)",path);
        }
        break;
      }

      borgLegacyPos pos;
      if (!borgLegacyReadPos(path,&pos)) break;

      char tmp[1100];
      snprintf(tmp,sizeof(tmp),"%s.borgtmp",path);
      int ok = borgLegacyWriteModernPos(&pos,tmp) && borgLoadPositions(tmp);
      remove(tmp);

      if (ok)
      {
        snprintf(gui.posFile,sizeof(gui.posFile),"%s",path);
        gui.run = GUI_IDLE;
        guiSettingsInvalidate();
        guiNotice("Loaded %s -- a 1995 16-bit .P, %d agent(s), widened from "
                  "%ld to %ld bytes",path,pos.numAgents,pos.fileSize,
                  pos.modernSize);
      }
      borgLegacyFreePos(&pos);
      break;
    }

    case GUI_FILE_SAVE_POS:
      if (borgSavePositions(path))
      {
        snprintf(gui.posFile,sizeof(gui.posFile),"%s",path);
        guiNotice("Wrote positions to %s",path);
      }
      break;

    //----------------------------------------------------------------------
    case GUI_FILE_OPEN_BATCH:
      snprintf(gui.batchFile,sizeof(gui.batchFile),"%s",path);
      guiBatchStart(path);
      break;

    //----------------------------------------------------------------------
    // The walk dialog wants the directory and the series numbers, not one file;
    // picking a file is just the convenient way to say which directory.  1995's
    // fileWalkSetup dialog took a path plus start and end numbers the same way.
    //----------------------------------------------------------------------
    case GUI_FILE_WALK_PICK:
    {
      snprintf(gui.walk.dir,sizeof(gui.walk.dir),"%s",path);
      char *slash = strrchr(gui.walk.dir,'/');
      if (slash) slash[1] = '\0';
      else gui.walk.dir[0] = '\0';

      const char *dot = strrchr(path,'.');
      gui.walk.type = (dot && (dot[1] == 'p' || dot[1] == 'P')) ? 1 : 0;
      gui.showWalkSetup = true;
      break;
    }

    //----------------------------------------------------------------------
    // Deferred: the pixels only exist after the frame is rendered, so main()
    // does this between RenderDrawData and RenderPresent.
    //----------------------------------------------------------------------
    case GUI_FILE_EXPORT_PNG:
    {
      snprintf(gui.exportPath,sizeof(gui.exportPath),"%s",path);

      size_t n = strlen(gui.exportPath);
      if (n < 4 || strcmp(gui.exportPath+n-4,".png") != 0)
        snprintf(gui.exportPath+n,sizeof(gui.exportPath)-n,".png");

      gui.exportRequest = 1;
      break;
    }

    default: break;
  }
}
