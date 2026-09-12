//============================================================================
// Borg 2.1 -- Layer 3, rebuilt.
//
// WHAT THIS REPLACES
//
// The 1995 Layer 3 is UI.CPP (106 KB), GRAPH.CPP, BORG.CPP and BORG.RC: a Win16
// window class with a WM_PAINT switch, nine HRGN clip regions, a BWCC-skinned
// child-dialog speed bar, four BorDlg_Gray modal dialogs, and a message pump
// that ran exactly one task.clockTick() per idle message.  None of it can be
// ported -- it is PASCAL/FAR/_export calling conventions, HDC drawing, and
// GetPrivateProfileString.  All four files stay in the tree unbuilt, as the
// preserved artifact.
//
// What CAN be kept is the arrangement and the semantics, because Layers 1 and 2
// were already written to be driven from outside.  Every number on screen here
// comes from a getter that exists in the 1995 source for the sole purpose of
// feeding UI.CPP: getAgent, getClassifier, getMsgBoardPosting, readStats,
// getAgentSensorMsg, getObjects, getAxesRanges.  Not one of them was added or
// changed for this front end.
//
// THE SHAPE OF THE ORIGINAL WINDOW        (UI.CPP:1065, calcDimensions)
//
//     +-----------------------------------------------------+
//     | Run Step Stop Reset Batch | 1 2 ... 8   (SpeedBar)  |
//     +---------------------------+-------------------------+
//     | Environment  (square,     | Message Board     (40%) |
//     |  0.75 x min(w,h))         +-------------------------+
//     |  axes, goal, obstacles,   | Sensors           (10%) |
//     |  agent trail              +-------------------------+
//     |                           | Classifier List  (rest) |
//     +---------------------------+-------------------------+
//     | Status  (2 columns x 30 chars, 13 lines)            |
//     +-----------------------------------------------------+
//
// guiBuildDefaultLayout() reproduces exactly that, as an ImGui dock layout, and
// View > Reset to 1995 Layout puts it back.  Unlike 1995 the panels can then be
// dragged, resized, closed and reopened.
//
// WHAT IS DELIBERATELY DIFFERENT
//
//  - The field keeps the axes' aspect ratio inside whatever rectangle the panel
//    happens to be.  calcDimensions could compute dx and dy independently
//    because it had just forced the box square; a dockable panel has not.
//  - One tick per frame is the floor, not the rule.  1995's one-tick-per-idle-
//    message was a property of a 1993 machine, not of the simulation; here the
//    speed control sets ticks per frame and "display after iteration" runs the
//    whole simulation with a frame-time budget.  The tick ORDER is untouched,
//    which is what reproducibility depends on (CLAUDE.md, on numRandCalls).
//  - Errors numbered 100 and up are not fatal.  The 1995 displayError's default
//    case fell through to fatal = 1 and exit(1), which would kill the program on
//    a failed fopen; those codes did not exist then (see host/host.h).
//============================================================================

#ifndef BORG_GUI_H
#define BORG_GUI_H

#include "imgui.h"

//----------------------------------------------------------------------------
// 1995 allowed eight agents because the speed bar had eight buttons -- the
// comment on MAX_AGENTS in UI.CPP says so in as many words ("If this is to be
// set greater than 8, more buttons..."). taskEnvironment itself allocates by
// numAgents and has no such limit, but the saved .sim files, the .b scripts and
// the thesis data were all produced under it, so it stands.
//----------------------------------------------------------------------------
#define BORG_GUI_MAX_AGENTS 8

//----------------------------------------------------------------------------
// Run state.  The 1995 enum (UI.H) was { goal, normalRun, normalStop, batchRun,
// batchStop, fileWalk }; "goal" was never used and the Run/Stop pairs collapse
// into one state plus a flag.
//----------------------------------------------------------------------------
enum guiRunState
{
  GUI_IDLE,      // stopped, a simulation loaded and steppable
  GUI_RUNNING,   // ticking a single simulation
  GUI_BATCH,     // ticking under batchRunner
  GUI_WALK       // replaying files, no simulation running
};

//----------------------------------------------------------------------------
// The four display preferences BORG.INI held, and nothing else.  Same meanings,
// same file, same [display] section and key names, so a 1995 borg.ini is read
// correctly and one written here would have been read correctly in 1995.
//
//   antOn            draw the field while iterating, vs only when it stops
//   infoOn           update the text panels while iterating, vs only when it stops
//   axesOn           draw and label the axes
//   objectsToScale   size the animat/goal/obstacles from antWidth, vs 1/40 field
//----------------------------------------------------------------------------
struct guiDisplayPrefs
{
  int antOn, infoOn, axesOn, objectsToScale;
};

void guiPrefsLoad(guiDisplayPrefs *p);            // reads ./borg.ini
void guiPrefsSave(const guiDisplayPrefs *p);      // writes ./borg.ini

//----------------------------------------------------------------------------
// Asynchronous file choice.  SDL's dialogs call back when the user is done, and
// not necessarily on this thread, so the callback only records what was picked
// and the main loop acts on it between frames.  Doing the work in the callback
// would mean calling into taskEnvironment off-thread.
//----------------------------------------------------------------------------
enum guiFileOp
{
  GUI_FILE_NONE = 0,
  GUI_FILE_OPEN_SIM,
  GUI_FILE_SAVE_SIM,
  GUI_FILE_LOAD_RULES,
  GUI_FILE_SAVE_RULES,
  GUI_FILE_LOAD_POS,
  GUI_FILE_SAVE_POS,
  GUI_FILE_OPEN_BATCH,
  GUI_FILE_WALK_PICK,
  GUI_FILE_EXPORT_PNG
};

//----------------------------------------------------------------------------
// File-walk replay.  1995's two modes, unchanged:
//
//   type 0   <N>.SIM          for N = start..end, a full load per frame
//   type 1   <S>_<N>.P        N counting up inside series S; when a number is
//                             missing the series advances and N restarts at 1
//                             (findNext, UI.CPP:2588)
//
// The .P mode is how the thesis figures were reviewed, and Research/BBA is full
// of 1_1.P, 1_2.P ... files in exactly that shape.  Phase 4's reader means the
// 16-bit originals can be walked directly, not just files this build wrote.
//----------------------------------------------------------------------------
struct guiWalkState
{
  int  active;
  int  type;                 // 0 == .SIM series, 1 == .P series
  int  legacy;               // walking 1995 16-bit files through host/legacy.h
  char dir[1024];            // directory, with trailing separator
  int  startSeries, endSeries;
  int  startNum, endNum;
  int  series, num;          // cursor
  int  framesPerFile;        // hold each file this many frames
  int  frameHold;
  char current[64];          // name of the file on screen
  char restore[1024];        // snapshot taken before the walk, reloaded after
};

//----------------------------------------------------------------------------
// Everything the front end owns.  One instance, gui, in state.cpp -- mirroring
// the single global UI of 1995, and for the same reason: the panels are not
// reusable components, they are one window.
//----------------------------------------------------------------------------
struct guiState
{
  guiRunState run;
  int  agent;                  // which agent the text panels show
  guiDisplayPrefs prefs;

  // Speed.  ticksPerFrame applies when the field is drawn during iteration;
  // otherwise a frame-time budget decides, which is what "as fast as possible"
  // means here.
  int  ticksPerFrame;
  int  unlimited;

  int  stepOnce;               // one tick, then back to GUI_IDLE
  int  simFinished;            // the last tick returned done

  // Open windows.  Panels are dock nodes; settings are free-floating.
  bool showField, showMsgBoard, showClassList, showSensors, showStatus;
  bool showDisplaySettings, showSystemSettings, showAgentSettings,
       showNetworkSettings, showAbout, showBatchLog, showWalkSetup;

  // Current files, remembered the way UI.H remembered simFile/ruleFile/...
  char simFile[1024], ruleFile[1024], posFile[1024], batchFile[1024];

  // Where the Field panel ended up on screen this frame, in framebuffer pixels.
  // PNG export reads the rendered pixels back out of exactly this rectangle.
  int  fieldX, fieldY, fieldW, fieldH;
  int  exportRequest;
  char exportPath[1024];

  guiWalkState walk;

  // Error text queued by hostDisplayError, shown as a modal next frame.  A tick
  // can raise one from deep inside Layer 1, so it cannot be shown in place.
  bool errorPending, errorFatal;
  char errorText[512];

  // One-line transient status, bottom of the window.
  char notice[512];
  double noticeUntil;

  bool layoutReset;            // rebuild the 1995 dock layout this frame
  bool quitRequested;
};

extern guiState gui;

// The fixed-pitch font every data panel uses.  1995 selected G.FIXED for the
// message board, classifier list, sensors and status and the system font for
// everything else; same split here.  NULL if no system mono font was found, in
// which case ImGui's built-in (also fixed-pitch) is used and the columns still
// line up.
extern ImFont *guiMono;

void guiNotice(const char *fmt, ...);

//----------------------------------------------------------------------------
// Panels -- panels.cpp
//----------------------------------------------------------------------------
void guiPanelField(void);
void guiPanelMessageBoard(void);
void guiPanelClassifierList(void);
void guiPanelSensors(void);
void guiPanelStatus(void);
ImU32 guiAgentColor(int agent);

//----------------------------------------------------------------------------
// The four settings windows -- settings.cpp.  Each buffers its edits and
// applies them only on OK, exactly as the 1995 dialogs did, because several of
// these parameters cannot change between soft resets: softReset() performs no
// allocation, so list lengths, message length and agent count must be identical
// across a batch (CLAUDE.md, "Reset triad").  Applying a change therefore forces
// a full reset, and the window says so before you press OK.
//----------------------------------------------------------------------------
void guiDisplaySettingsWindow(void);
void guiSystemSettingsWindow(void);
void guiAgentSettingsWindow(void);
void guiNetworkSettingsWindow(void);
void guiAboutWindow(void);
void guiSettingsInvalidate(void);     // re-read the live values into the buffers

//----------------------------------------------------------------------------
// Files -- files.cpp
//----------------------------------------------------------------------------
void guiFilePick(guiFileOp op);       // opens the native dialog
void guiFileApplyPending(void);       // called once per frame from the main loop
void guiFileNew(void);
void guiFileSave(void);               // to gui.simFile, or prompts if unnamed
void guiSavePositionsAscii(void);

//----------------------------------------------------------------------------
// Batch -- batch_ui.cpp
//----------------------------------------------------------------------------
void guiBatchStart(const char *batchPath);
void guiBatchTick(void);
void guiBatchStop(void);
void guiBatchLogWindow(void);

//----------------------------------------------------------------------------
// File walk -- walk.cpp
//----------------------------------------------------------------------------
void guiWalkSetupWindow(void);
void guiWalkBegin(void);
void guiWalkTick(void);
void guiWalkEnd(void);

//----------------------------------------------------------------------------
// PNG export -- export.cpp.  Replaces File > Save Meta File and makeBitmap(),
// which wrote a Windows Metafile and a GDI .BMP.
//----------------------------------------------------------------------------
int guiExportFieldPNG(const char *path, int x, int y, int w, int h);

//----------------------------------------------------------------------------
// Simulation control -- main.cpp
//----------------------------------------------------------------------------
void guiSimReset(int resetEnvironmentOnly);
void guiSimRun(void);
void guiSimStop(void);
void guiSimStep(void);

#endif
