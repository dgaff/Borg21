//============================================================================
// File Walk -- replaying a series of saved files as an animation.
//
// THIS IS THE FEATURE THAT READS THE 1995 RESEARCH DATA
//
// The walk is how the thesis figures were reviewed: point it at a directory of
// numbered snapshots and it loads them one after another, so a run that took
// minutes to compute plays back in seconds.  Research/BBA holds exactly that --
// 1.SIM .. 50.SIM, and 1_1.P, 1_2.P ... -- and Phase 4's widening reader means
// those 16-bit originals can be walked directly, not just files this build wrote.
// Nothing else in this port shows the 1995 data moving.
//
// 1995's two modes, unchanged (fileWalkCont, UI.CPP:2616):
//
//   type 0, .SIM   load "<N>.SIM" for N = start..end; a file that fails to load
//                  is SKIPPED, not an error -- the do/while just tries the next
//                  number, which is how a gap in the numbering was tolerated.
//
//   type 1, .P     load "<series>_<num>.p", counting num up within a series;
//                  when that file is missing the series advances and num restarts
//                  at 1 (findNext, UI.CPP:2588).  The walk ends when the series
//                  passes the end, or when findNext runs out of files.
//
// And as in 1995 the current simulation is saved to a temp file first and
// restored when the walk ends (startWalk/endWalk), because a walk overwrites the
// live state repeatedly.
//
// ONE ADDITION: framesPerFile.  1995 loaded one file per WM_TIMER tick, and on a
// 1993 machine loading a .SIM was itself the frame rate.  Here a .P loads in
// microseconds, so an unthrottled walk would flash past; the control sets how
// many frames each file is held.
//============================================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL.h>

#include "borggui.h"
#include "task.h"
#include <host.h>
#include <simfile.h>
#include <legacy.h>

extern SDL_Window *guiWindow;

static char walkRestore[1024];
static int  haveWalkRestore = 0;

//----------------------------------------------------------------------------
// Load one file of either vintage, quietly.
//
// "Quietly" matters: a walk probes for files that may not exist -- that is how
// both modes detect the end of a series -- so a failed load here must not raise
// an error dialog.  The legacy readers do report through error() on a malformed
// file, so they are only reached once the file has been opened successfully.
//----------------------------------------------------------------------------
static int walkLoadSim(const char *path)
{
  unsigned char head[8];
  FILE *f = fopen(path,"rb");
  if (!f) return 0;
  size_t n = fread(head,1,8,f);
  fclose(f);
  if (n != 8) return 0;

  if (!(head[0]=='2' && head[1]=='.' && head[2]=='1' && head[3]=='\0')) return 0;

  long msgLen = (long)head[4] | ((long)head[5]<<8) |
                ((long)head[6]<<16) | ((long)head[7]<<24);

  if (msgLen >= 1 && msgLen <= 4096) return borgLoadSim(path);

  // 1995 16-bit: widen, then load through the ordinary path.
  borgLegacySim sim;
  if (!borgLegacyReadSim(path,&sim)) return 0;

  char tmp[1100];
  snprintf(tmp,sizeof(tmp),"%s.borgtmp",path);
  int ok = borgLegacyWriteModernSim(&sim,tmp) && borgLoadSim(tmp);
  remove(tmp);
  borgLegacyFreeSim(&sim);
  return ok;
}

static int walkLoadPos(const char *path)
{
  FILE *f = fopen(path,"rb");
  if (!f) return 0;

  // Same native-vs-1995 test as files.cpp, reduced to what a walk needs: read the
  // header chain as native ints and see whether the implied size is the real one.
  long fileSize;
  fseek(f,0,SEEK_END);  fileSize = ftell(f);  rewind(f);

  int numAgents = 0, maxPts = 0, native = 0;
  if (fread(&numAgents,sizeof(int),1,f) == 1 &&
      fread(&maxPts,   sizeof(int),1,f) == 1 &&
      numAgents >= 1 && numAgents <= 1024 && maxPts >= 1)
  {
    long implied = 2*(long)sizeof(int);
    native = 1;
    for (int i = 0; i < numAgents && native; i++)
    {
      int ptCount = 0;
      if (fread(&ptCount,sizeof(int),1,f) != 1 || ptCount < 0 || ptCount > maxPts)
      { native = 0;  break; }
      long block = (long)ptCount * (long)(sizeof(coord) + sizeof(float));
      implied += (long)sizeof(int) + block;
      if (implied > fileSize || fseek(f,block,SEEK_CUR) != 0) native = 0;
    }
    if (native && implied != fileSize) native = 0;
  }
  fclose(f);

  if (native) return borgLoadPositions(path);

  borgLegacyPos pos;
  if (!borgLegacyReadPos(path,&pos)) return 0;

  char tmp[1100];
  snprintf(tmp,sizeof(tmp),"%s.borgtmp",path);
  int ok = borgLegacyWriteModernPos(&pos,tmp) && borgLoadPositions(tmp);
  remove(tmp);
  borgLegacyFreePos(&pos);
  return ok;
}

//============================================================================
// fileWalkSetup DIALOG's replacement.
//============================================================================
void guiWalkSetupWindow(void)
{
  if (!gui.showWalkSetup) return;

  ImGui::SetNextWindowSize(ImVec2(620,0),ImGuiCond_FirstUseEver);
  if (!ImGui::Begin("File Walk",&gui.showWalkSetup,
                    ImGuiWindowFlags_AlwaysAutoResize))
  { ImGui::End();  return; }

  ImGui::TextWrapped(
    "Replays a numbered series of saved files as an animation. 1995 files are "
    "read directly -- a 16-bit .SIM or .P is widened on the way in.");
  ImGui::Dummy(ImVec2(0,4));

  ImGui::SeparatorText("Files");

  ImGui::SetNextItemWidth(420);
  ImGui::InputText("Directory",gui.walk.dir,sizeof(gui.walk.dir));
  ImGui::SameLine();
  if (ImGui::Button("Browse..."))
    guiFilePick(GUI_FILE_WALK_PICK);
  ImGui::TextDisabled("  Pick any file in the directory; only its folder is used.");

  ImGui::RadioButton("Simulation files:  <N>.SIM",&gui.walk.type,0);
  ImGui::RadioButton("Position files:    <series>_<num>.P",&gui.walk.type,1);

  ImGui::Dummy(ImVec2(0,4));
  ImGui::SeparatorText("Range");

  if (gui.walk.type == 0)
  {
    ImGui::SetNextItemWidth(100);
    ImGui::InputInt("First N",&gui.walk.startSeries,0,0);
    ImGui::SameLine(0,24);
    ImGui::SetNextItemWidth(100);
    ImGui::InputInt("Last N",&gui.walk.endSeries,0,0);
    ImGui::TextDisabled("  A number with no file is skipped, as in 1995 -- gaps "
                        "in the numbering are fine.");
  }
  else
  {
    ImGui::SetNextItemWidth(100);
    ImGui::InputInt("First series",&gui.walk.startSeries,0,0);
    ImGui::SameLine(0,24);
    ImGui::SetNextItemWidth(100);
    ImGui::InputInt("Last series",&gui.walk.endSeries,0,0);

    ImGui::SetNextItemWidth(100);
    ImGui::InputInt("First number",&gui.walk.startNum,0,0);
    ImGui::SameLine(0,24);
    ImGui::SetNextItemWidth(100);
    ImGui::InputInt("Last number",&gui.walk.endNum,0,0);
    ImGui::TextDisabled("  e.g. series 1, numbers 1..24 plays 1_1.P through "
                        "1_24.P, then moves to series 2.");
  }

  ImGui::Dummy(ImVec2(0,4));
  ImGui::SetNextItemWidth(160);
  ImGui::SliderInt("Frames per file",&gui.walk.framesPerFile,1,30,"%d",
                   ImGuiSliderFlags_AlwaysClamp);
  ImGui::TextDisabled("  1995 loaded one file per timer tick; these load far "
                      "faster than that now.");

  ImGui::Dummy(ImVec2(0,6));
  ImGui::Separator();

  bool canStart = (gui.run == GUI_IDLE) && (gui.walk.dir[0] != '\0');
  ImGui::BeginDisabled(!canStart);
  if (ImGui::Button("Start Walk",ImVec2(110,0)))
  { guiWalkBegin();  gui.showWalkSetup = false; }
  ImGui::EndDisabled();

  ImGui::SameLine();
  ImGui::BeginDisabled(gui.run != GUI_WALK);
  if (ImGui::Button("Stop Walk",ImVec2(110,0))) guiWalkEnd();
  ImGui::EndDisabled();

  ImGui::SameLine();
  if (ImGui::Button("Close",ImVec2(90,0))) gui.showWalkSetup = false;

  if (gui.run != GUI_IDLE && gui.run != GUI_WALK)
    ImGui::TextColored(ImVec4(0.98f,0.76f,0.33f,1.0f),
        "Stop the run or the batch first. (1995 said the same thing, as error 13:"
        "\n\"Exit batch mode before executing a file walk-through\".)");

  ImGui::End();
}

//============================================================================
void guiWalkBegin(void)
{
  if (gui.run != GUI_IDLE || gui.walk.dir[0] == '\0') return;

  if (gui.walk.startSeries < 1) gui.walk.startSeries = 1;
  if (gui.walk.endSeries < gui.walk.startSeries)
    gui.walk.endSeries = gui.walk.startSeries;
  if (gui.walk.startNum < 1) gui.walk.startNum = 1;
  if (gui.walk.endNum < 1)   gui.walk.endNum = 1;
  if (gui.walk.framesPerFile < 1) gui.walk.framesPerFile = 1;

  // Make sure the directory ends in a separator; 1995's fileWalkPath did too,
  // because every sprintf below concatenates straight onto it.
  size_t n = strlen(gui.walk.dir);
  if (n && gui.walk.dir[n-1] != '/' && n+1 < sizeof(gui.walk.dir))
  { gui.walk.dir[n] = '/';  gui.walk.dir[n+1] = '\0'; }

  const char *pref = SDL_GetPrefPath("borg","borg");
  snprintf(walkRestore,sizeof(walkRestore),"%swalk-restore.sim",pref ? pref : "");
  if (pref) SDL_free((void*)pref);
  haveWalkRestore = borgSaveSim(walkRestore);

  gui.walk.series    = gui.walk.startSeries;
  gui.walk.num       = gui.walk.startNum;
  gui.walk.frameHold = 0;
  gui.walk.current[0]= '\0';
  gui.walk.active    = 1;

  gui.run = GUI_WALK;
  guiNotice("File walk started in %s",gui.walk.dir);
}

//============================================================================
// One frame of walking.
//============================================================================
void guiWalkTick(void)
{
  if (!gui.walk.active) { gui.run = GUI_IDLE;  return; }

  if (gui.walk.frameHold > 0) { --gui.walk.frameHold;  return; }

  char path[1200], name[64];

  if (gui.walk.type == 0)
  {
    //----------------------------------------------------------------------
    // .SIM mode.  The 1995 loop skipped numbers that would not load, which is
    // why this is a do/while and not a single attempt.
    //----------------------------------------------------------------------
    for (;;)
    {
      if (gui.walk.series > gui.walk.endSeries) { guiWalkEnd();  return; }

      snprintf(name,sizeof(name),"%d.SIM",gui.walk.series);
      snprintf(path,sizeof(path),"%s%s",gui.walk.dir,name);
      ++gui.walk.series;

      if (walkLoadSim(path)) break;
    }
  }
  else
  {
    //----------------------------------------------------------------------
    // .P mode, findNext's rule: try <series>_<num>.p; a miss advances the
    // series and restarts num at 1.
    //----------------------------------------------------------------------
    if (gui.walk.series == gui.walk.endSeries && gui.walk.num > gui.walk.endNum)
    { guiWalkEnd();  return; }

    int loaded = 0;
    while (!loaded)
    {
      if (gui.walk.series > gui.walk.endSeries) { guiWalkEnd();  return; }

      snprintf(name,sizeof(name),"%d_%d.P",gui.walk.series,gui.walk.num);
      snprintf(path,sizeof(path),"%s%s",gui.walk.dir,name);

      if (walkLoadPos(path)) { ++gui.walk.num;  loaded = 1; }
      else                   { ++gui.walk.series;  gui.walk.num = 1; }
    }
  }

  snprintf(gui.walk.current,sizeof(gui.walk.current),"%s",name);
  gui.walk.frameHold = gui.walk.framesPerFile - 1;

  // 1995: SetWindowText(hWnd,"Borg : File Walk Mode, <file>").
  char title[256];
  snprintf(title,sizeof(title),"Borg 2.1 - File Walk Mode, %s",gui.walk.current);
  SDL_SetWindowTitle(guiWindow,title);
}

//============================================================================
void guiWalkEnd(void)
{
  gui.walk.active = 0;
  gui.run = GUI_IDLE;

  if (haveWalkRestore)
  {
    borgLoadSim(walkRestore);
    remove(walkRestore);
    haveWalkRestore = 0;
    guiSettingsInvalidate();
  }

  if (gui.simFile[0])
  {
    char title[1100];
    snprintf(title,sizeof(title),"Borg 2.1 - %s",gui.simFile);
    SDL_SetWindowTitle(guiWindow,title);
  }
  else SDL_SetWindowTitle(guiWindow,"Borg 2.1");

  guiNotice("File walk ended; your simulation has been restored.");
}
