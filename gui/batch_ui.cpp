//============================================================================
// Running a .b batch from the GUI.
//
// host/batch.h already holds the whole thing, deliberately left as a stepwise
// API rather than a loop, so the front end could drive it a tick at a time and
// repaint between ticks the way BORG.EXE did.  This is the caller that uses it
// that way.  The call order is the one documented at the top of batch.h and used
// by the CLI:
//
//     open()                  read the starting sim number, write the .O header
//     while (startNextSim())  read and apply one spec line
//     {  while (!tick()) ;    run that simulation to its end
//        finishSim();         write <N>.sim, append the .O rows, ++simNum
//     }
//     close()
//
// The only difference is that the inner while is spread across frames.
//
// THE TEMP FILE IS BACK
//
// batch.h records that it does not reproduce startBatch()'s save-and-restore of
// the user's in-progress simulation, on the grounds that a headless run has no
// unsaved work to protect.  That reasoning does not hold here -- the GUI is
// exactly the case it was written for -- so the snapshot is restored: the current
// state goes to a temp .sim before the batch and comes back afterwards, whether
// the batch finished or was stopped.  This is 1995's mktemp()/load(tempFile)/
// remove(tempFile) (UI.CPP:2250, endWalk, and the resetRequest arm).
//============================================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL.h>

#include "borggui.h"
#include "task.h"
#include "cfs.h"
#include <host.h>
#include <simfile.h>
#include <batch.h>

extern SDL_Window *guiWindow;

static batchRunner *runner = NULL;
static char  batchRestore[1024];
static int   haveRestore = 0;

// The .O as written so far, re-read whenever a simulation completes so the panel
// shows the file's real contents rather than a parallel reconstruction of it.
static char *batchLog     = NULL;
static long  batchLogLen  = 0;
static int   batchFailed  = 0;

#define GUI_BATCH_BUDGET_MS 10.0

//----------------------------------------------------------------------------
static void batchLogReload(const char *path)
{
  free(batchLog);
  batchLog = NULL;
  batchLogLen = 0;

  FILE *f = fopen(path,"rb");
  if (!f) return;

  fseek(f,0,SEEK_END);
  long n = ftell(f);
  rewind(f);
  if (n < 0) { fclose(f); return; }

  batchLog = (char*)malloc((size_t)n+1);
  if (!batchLog) { fclose(f); return; }

  batchLogLen = (long)fread(batchLog,1,(size_t)n,f);
  batchLog[batchLogLen] = '\0';
  fclose(f);
}

//============================================================================
void guiBatchStart(const char *batchPath)
{
  if (gui.run != GUI_IDLE) return;

  //--------------------------------------------------------------------------
  // Snapshot the current simulation.  If this fails the batch still runs -- the
  // snapshot is a convenience, and refusing to start because of it would be
  // worse than losing it.
  //--------------------------------------------------------------------------
  const char *tmpDir = SDL_GetPrefPath("borg","borg");
  snprintf(batchRestore,sizeof(batchRestore),"%sbatch-restore.sim",
           tmpDir ? tmpDir : "");
  if (tmpDir) SDL_free((void*)tmpDir);

  haveRestore = borgSaveSim(batchRestore);

  delete runner;
  runner = new batchRunner;
  batchFailed = 0;

  if (!runner->open(batchPath))
  { delete runner;  runner = NULL;  return; }

  int started = runner->startNextSim();
  if (started <= 0)
  {
    guiNotice(started < 0 ? "Batch script is malformed; nothing ran."
                          : "Batch script contained no simulations.");
    guiBatchStop();
    return;
  }

  batchLogReload(runner->outputPath());

  gui.run = GUI_BATCH;
  gui.showBatchLog = true;
  gui.simFinished = 0;

  // 1995 announced the mode in the title bar; same here.
  char title[1100];
  snprintf(title,sizeof(title),"Borg 2.1 - Batch Mode, %s",batchPath);
  SDL_SetWindowTitle(guiWindow,title);

  guiNotice("Batch started: %s -> %s",batchPath,runner->outputPath());
}

//============================================================================
// One frame's worth of batch.
//
// Batches are long -- the BBA dataset is fifty simulations of several hundred
// ticks -- so this always runs on the frame-time budget rather than honouring
// ticks/frame.  The field still repaints once per frame, which is what the 1995
// display did in batch mode too: displayStatus only bothered updating every
// tenth global tick while a batch was running.
//============================================================================
void guiBatchTick(void)
{
  if (!runner) { gui.run = GUI_IDLE;  return; }

  Uint64 start = SDL_GetTicksNS();

  while (gui.run == GUI_BATCH)
  {
    if (runner->tick())
    {
      //--------------------------------------------------------------------
      // This simulation is done: write <N>.sim, append its .O rows, advance.
      //--------------------------------------------------------------------
      if (!runner->finishSim()) { batchFailed = 1;  guiBatchStop();  return; }

      batchLogReload(runner->outputPath());

      int more = runner->startNextSim();
      if (more < 0) { batchFailed = 1;  guiBatchStop();  return; }
      if (more == 0)
      {
        guiNotice("Batch complete: %d simulation(s), results in %s",
                  runner->simsCompleted(),runner->outputPath());
        guiBatchStop();
        return;
      }
      // A new simulation has been set up; fall through and keep ticking.
    }

    if ((double)(SDL_GetTicksNS()-start)/1.0e6 > GUI_BATCH_BUDGET_MS) break;
  }
}

//============================================================================
void guiBatchStop(void)
{
  int completed = runner ? runner->simsCompleted() : 0;
  char outPath[600];
  outPath[0] = '\0';
  if (runner) snprintf(outPath,sizeof(outPath),"%s",runner->outputPath());

  if (runner) { runner->close();  delete runner;  runner = NULL; }

  gui.run = GUI_IDLE;

  //--------------------------------------------------------------------------
  // Put the user's simulation back, as startBatch()/endWalk() did.
  //--------------------------------------------------------------------------
  if (haveRestore)
  {
    borgLoadSim(batchRestore);
    remove(batchRestore);
    haveRestore = 0;
    guiSettingsInvalidate();
  }

  if (gui.simFile[0])
  {
    char title[1100];
    snprintf(title,sizeof(title),"Borg 2.1 - %s",gui.simFile);
    SDL_SetWindowTitle(guiWindow,title);
  }
  else SDL_SetWindowTitle(guiWindow,"Borg 2.1");

  if (batchFailed)
    guiNotice("Batch stopped after %d simulation(s); see the error above. "
              "Completed runs are still in %s",completed,outPath);
  else if (completed)
    guiNotice("Batch ended after %d simulation(s); your simulation has been "
              "restored.",completed);

  batchFailed = 0;
}

//============================================================================
// The .O, as a panel.
//
// .O is a fixed-column text table -- sim #, agent, seed, iterations, sexual and
// asexual crossovers, mutations, CDOs, CEOs, crashes -- one row per agent per
// completed run, and the column layout is regression-tested against a surviving
// 1995 .O file (tests/batch_format.cpp).  So it is shown verbatim, in the fixed
// font, rather than re-laid-out into an ImGui table: the alignment IS the format.
//============================================================================
void guiBatchLogWindow(void)
{
  if (!gui.showBatchLog) return;

  if (!ImGui::Begin("Batch Log",&gui.showBatchLog))
  { ImGui::End();  return; }

  if (gui.run == GUI_BATCH && runner)
  {
    ImGui::Text("simulation %d,  %d completed,  tick %d",
                runner->currentSimNum(),runner->simsCompleted(),
                task.readGlobalClock());
    ImGui::SameLine();
    if (ImGui::SmallButton("Stop Batch")) guiBatchStop();
    ImGui::Separator();
  }
  else if (!batchLog)
  {
    ImGui::TextDisabled("No batch has run yet.");
    ImGui::TextDisabled("Simulate > Start Batch... picks a .b script; results go "
                        "to the matching .O file.");
    ImGui::End();
    return;
  }

  if (batchLog)
  {
    bool atBottom = true;
    if (ImGui::BeginChild("##batchOut",ImVec2(0,0),ImGuiChildFlags_Borders,
                          ImGuiWindowFlags_HorizontalScrollbar))
    {
      atBottom = (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f);

      bool mono = (guiMono != NULL);
      if (mono) ImGui::PushFont(guiMono,0.0f);
      ImGui::TextUnformatted(batchLog,batchLog+batchLogLen);
      if (mono) ImGui::PopFont();

      // Follow the tail while the batch is running, unless the user scrolled up.
      if (gui.run == GUI_BATCH && atBottom) ImGui::SetScrollHereY(1.0f);
    }
    ImGui::EndChild();
  }

  ImGui::End();
}
