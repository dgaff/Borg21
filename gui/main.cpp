//============================================================================
// Borg 2.1 -- application shell: window, fonts, dock layout, menus, speed bar,
// and the loop that drives the simulation.
//
// This is the replacement for BORG.CPP (45 lines: construct G, then task, then
// UI, in that order) plus userInterface::WinMain, mainWndProc, processMenu,
// processTimer and processRequests.
//
// THE ONE THING THAT MATTERS FOR CORRECTNESS
//
// task.clockTick() is called in the same order, with nothing interleaved, and
// nothing else in this file touches the simulation while a tick is in progress.
// Reproducibility of a seeded run depends on the exact sequence of getRandom()
// draws (CLAUDE.md, on numRandCalls), so the front end may decide WHEN to tick
// and HOW MANY times, but never what happens inside one.  That is why the speed
// control is a tick count and not, say, a time step.
//============================================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL.h>

#include "imgui.h"
#include "imgui_internal.h"          // DockBuilder*, BeginViewportSideBar
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"

#include "borggui.h"

#include "task.h"
#include "cfs.h"
#include "ei.h"
#include <host.h>
#include <simfile.h>
#include <batch.h>

//----------------------------------------------------------------------------
// Shared with files.cpp (native dialogs need the parent window) and export.cpp
// (reading back rendered pixels needs the renderer).
//----------------------------------------------------------------------------
SDL_Window   *guiWindow   = NULL;
SDL_Renderer *guiRenderer = NULL;
ImFont       *guiMono     = NULL;

static const char *kLayoutIni = "borg_layout.ini";

//============================================================================
// Fonts.
//
// 1995 used two: the system proportional font for window chrome and G.FIXED (an
// OEM fixed-pitch font) for all four data panels, because every one of them is a
// table of aligned columns -- allele strings, bids, strengths.  That split is
// kept, with macOS's own faces.
//
// Both are tried by path and both may be absent (a non-Mac build, a stripped
// system), in which case ImGui's built-in font is used.  That font is also
// fixed-pitch, so the columns still line up; only the chrome looks different.
//============================================================================
static void guiLoadFonts(float dpiScale)
{
  ImGuiIO &io = ImGui::GetIO();

  static const char *uiCandidates[] =
  { "/System/Library/Fonts/SFNS.ttf",
    "/System/Library/Fonts/HelveticaNeue.ttc",
    "/System/Library/Fonts/Geneva.ttf",
    NULL };

  static const char *monoCandidates[] =
  { "/System/Library/Fonts/SFNSMono.ttf",
    "/System/Library/Fonts/Menlo.ttc",
    "/System/Library/Fonts/Courier.ttc",
    NULL };

  ImFont *uiFont = NULL;
  for (int i = 0; uiCandidates[i] && !uiFont; i++)
    uiFont = io.Fonts->AddFontFromFileTTF(uiCandidates[i],14.0f);

  if (!uiFont)
  {
    io.Fonts->AddFontDefault();
    guiNotice("No system UI font found; using the built-in font.");
  }

  for (int i = 0; monoCandidates[i] && !guiMono; i++)
    guiMono = io.Fonts->AddFontFromFileTTF(monoCandidates[i],13.0f);

  (void)dpiScale;   // handled by style.FontScaleDpi, set by the caller
}

//============================================================================
// Style.
//
// Restrained on purpose.  The 1995 program was BWCC-skinned grey boxes with
// 1-pixel borders and no rounding, and the thing on screen that should draw the
// eye is the field and the classifier list, not the window furniture.  So: small
// radii, tight padding, and a single accent used only for the run state.
//============================================================================
static void guiApplyStyle(float scale)
{
  ImGui::StyleColorsDark();
  ImGuiStyle &s = ImGui::GetStyle();

  s.WindowRounding    = 4.0f;
  s.ChildRounding     = 3.0f;
  s.FrameRounding     = 3.0f;
  s.GrabRounding      = 3.0f;
  s.TabRounding       = 3.0f;
  s.ScrollbarRounding = 3.0f;
  s.WindowBorderSize  = 1.0f;
  s.FrameBorderSize   = 1.0f;
  s.WindowPadding     = ImVec2(8,6);
  s.FramePadding      = ImVec2(7,3);
  s.ItemSpacing       = ImVec2(7,4);
  s.ItemInnerSpacing  = ImVec2(5,4);
  s.SeparatorTextBorderSize = 1.0f;

  ImVec4 *c = s.Colors;
  c[ImGuiCol_WindowBg]      = ImVec4(0.118f,0.122f,0.133f,1.00f);
  c[ImGuiCol_ChildBg]       = ImVec4(0.098f,0.102f,0.113f,1.00f);
  c[ImGuiCol_PopupBg]       = ImVec4(0.137f,0.141f,0.153f,0.98f);
  c[ImGuiCol_Border]        = ImVec4(0.267f,0.275f,0.298f,1.00f);
  c[ImGuiCol_FrameBg]       = ImVec4(0.169f,0.176f,0.192f,1.00f);
  c[ImGuiCol_FrameBgHovered]= ImVec4(0.216f,0.227f,0.251f,1.00f);
  c[ImGuiCol_TitleBg]       = ImVec4(0.086f,0.090f,0.098f,1.00f);
  c[ImGuiCol_TitleBgActive] = ImVec4(0.149f,0.157f,0.176f,1.00f);
  c[ImGuiCol_MenuBarBg]     = ImVec4(0.106f,0.110f,0.121f,1.00f);
  c[ImGuiCol_Header]        = ImVec4(0.204f,0.216f,0.239f,1.00f);
  c[ImGuiCol_HeaderHovered] = ImVec4(0.259f,0.275f,0.306f,1.00f);
  c[ImGuiCol_Button]        = ImVec4(0.192f,0.204f,0.227f,1.00f);
  c[ImGuiCol_ButtonHovered] = ImVec4(0.251f,0.267f,0.298f,1.00f);
  c[ImGuiCol_ButtonActive]  = ImVec4(0.310f,0.329f,0.365f,1.00f);
  c[ImGuiCol_Tab]           = ImVec4(0.141f,0.149f,0.165f,1.00f);
  c[ImGuiCol_TabHovered]    = ImVec4(0.259f,0.275f,0.306f,1.00f);
  c[ImGuiCol_TabSelected]   = ImVec4(0.204f,0.216f,0.239f,1.00f);
  c[ImGuiCol_DockingPreview]= ImVec4(0.392f,0.553f,0.804f,0.70f);
  c[ImGuiCol_Separator]     = ImVec4(0.239f,0.247f,0.271f,1.00f);

  s.ScaleAllSizes(scale);
}

//============================================================================
// The 1995 layout, as a dock tree.
//
//     +-----------------------------------------------------+
//     |  Field (square)           | Message Board      0.40 |
//     |                           +-------------------------+
//     |                           | Sensors            0.10 |
//     |                           +-------------------------+
//     |                           | Classifier List    0.50 |
//     +---------------------------+-------------------------+
//     | Status                                              |
//     +-----------------------------------------------------+
//
// The fractions are calcDimensions's own: the message board took 40% of the
// environment box's height, the sensors 10%, and the classifier list the rest
// (UI.CPP:1255-1290).  The status box spanned the full width below both.
//
// The left column's width is computed rather than fixed, so the Field node comes
// out square at the current window size -- which is what calcDimensions did when
// it set envSize = 0.75 * min(width,height) and made the box envSize on a side.
//============================================================================
static void guiBuildDefaultLayout(ImGuiID rootId, ImVec2 size)
{
  ImGui::DockBuilderRemoveNode(rootId);
  ImGui::DockBuilderAddNode(rootId,ImGuiDockNodeFlags_DockSpace);
  ImGui::DockBuilderSetNodeSize(rootId,size);

  const float statusFrac = 0.22f;

  ImGuiID statusId, upperId;
  ImGui::DockBuilderSplitNode(rootId,ImGuiDir_Down,statusFrac,&statusId,&upperId);

  // Make the field square: the left column should be as wide as the upper band
  // is tall.  Clamped so a very wide or very tall window still leaves both
  // columns usable.
  float upperH = size.y * (1.0f - statusFrac);
  float rightFrac = (size.x > 1.0f) ? (1.0f - upperH/size.x) : 0.45f;
  if (rightFrac < 0.28f) rightFrac = 0.28f;
  if (rightFrac > 0.62f) rightFrac = 0.62f;

  ImGuiID rightId, leftId;
  ImGui::DockBuilderSplitNode(upperId,ImGuiDir_Right,rightFrac,&rightId,&leftId);

  ImGuiID msgId, belowMsgId;
  ImGui::DockBuilderSplitNode(rightId,ImGuiDir_Up,0.40f,&msgId,&belowMsgId);

  // 0.10 of the column, which is 0.10/0.60 of what is left below the board.
  ImGuiID sensorsId, classListId;
  ImGui::DockBuilderSplitNode(belowMsgId,ImGuiDir_Up,0.1667f,&sensorsId,
                              &classListId);

  ImGui::DockBuilderDockWindow("Field",          leftId);
  ImGui::DockBuilderDockWindow("Message Board",  msgId);
  ImGui::DockBuilderDockWindow("Sensors",        sensorsId);
  ImGui::DockBuilderDockWindow("Classifier List",classListId);
  ImGui::DockBuilderDockWindow("Status",         statusId);
  ImGui::DockBuilderDockWindow("Batch Log",      statusId);

  ImGui::DockBuilderFinish(rootId);
}

//============================================================================
// Simulation control.  Each of these is one arm of processRequests().
//============================================================================
void guiSimRun(void)
{
  if (gui.run == GUI_IDLE) { gui.run = GUI_RUNNING;  gui.simFinished = 0; }
}

void guiSimStop(void)
{
  if (gui.run == GUI_RUNNING) gui.run = GUI_IDLE;
}

void guiSimStep(void)
{
  if (gui.run == GUI_IDLE) gui.stepOnce = 1;
}

//----------------------------------------------------------------------------
// resetEnvironmentOnly == 0  ->  Reset All,         task.reset()
// resetEnvironmentOnly == 1  ->  Reset Environment, task.softReset()
//
// The difference is the whole reset triad: reset() allocates and regenerates the
// rule base, softReset() restores the starting conditions and KEEPS the learned
// rules, allocating nothing.  runBatch used softReset between simulations, which
// is why a batch carries its rules -- and its random sequence -- forward.
//----------------------------------------------------------------------------
void guiSimReset(int resetEnvironmentOnly)
{
  gui.run = GUI_IDLE;
  gui.simFinished = 0;

  if (resetEnvironmentOnly) task.softReset();
  else                      task.reset();

  guiSettingsInvalidate();
  guiNotice(resetEnvironmentOnly
            ? "Environment soft-reset; learned rules kept."
            : "Full reset; rule base regenerated.");
}

//============================================================================
// Ticking.
//
// 1995 ran exactly one clockTick() per idle message.  On a 1993 machine that was
// as fast as it went and it kept the display alive; it is not a property of the
// simulation.  Here:
//
//   "display during iteration" off  ->  tick until a frame-time budget runs out,
//                                       so a run finishes in seconds
//   on                              ->  tick ticksPerFrame times, then repaint
//
// Either way the ticks are consecutive calls with nothing between them.
//============================================================================
#define GUI_TICK_BUDGET_MS 10.0

static void guiTickSimulation(void)
{
  if (gui.run != GUI_RUNNING && !gui.stepOnce) return;

  if (gui.stepOnce)
  {
    gui.stepOnce = 0;
    if (task.clockTick()) { gui.simFinished = 1;  gui.run = GUI_IDLE; }
    return;
  }

  if (gui.unlimited || !gui.prefs.antOn)
  {
    Uint64 start = SDL_GetTicksNS();
    while (gui.run == GUI_RUNNING)
    {
      if (task.clockTick()) { gui.simFinished = 1;  gui.run = GUI_IDLE;  break; }
      if ((double)(SDL_GetTicksNS()-start)/1.0e6 > GUI_TICK_BUDGET_MS) break;
    }
  }
  else
  {
    for (int i = 0; i < gui.ticksPerFrame && gui.run == GUI_RUNNING; i++)
      if (task.clockTick()) { gui.simFinished = 1;  gui.run = GUI_IDLE;  break; }
  }
}

//============================================================================
// Menu bar -- the four 1995 popups, plus View, which is new because the panels
// can now be closed and there has to be a way back.
//============================================================================
static void guiMenuBar(void)
{
  const bool busy = (gui.run == GUI_BATCH || gui.run == GUI_WALK);

  if (!ImGui::BeginMainMenuBar()) return;

  if (ImGui::BeginMenu("File"))
  {
    if (ImGui::MenuItem("New","Cmd+N",false,!busy))             guiFileNew();
    if (ImGui::MenuItem("Open...","Cmd+O",false,!busy))         guiFilePick(GUI_FILE_OPEN_SIM);
    if (ImGui::MenuItem("Walk...","Cmd+W",false,!busy))         gui.showWalkSetup = true;
    ImGui::Separator();
    if (ImGui::MenuItem("Save","Cmd+S",false,!busy))            guiFileSave();
    if (ImGui::MenuItem("Save As...",NULL,false,!busy))         guiFilePick(GUI_FILE_SAVE_SIM);
    ImGui::Separator();
    if (ImGui::MenuItem("Export Field as PNG...","Cmd+E",false,gui.showField))
      guiFilePick(GUI_FILE_EXPORT_PNG);
    ImGui::Separator();
    if (ImGui::MenuItem("Load Rules...",NULL,false,!busy))      guiFilePick(GUI_FILE_LOAD_RULES);
    if (ImGui::MenuItem("Save Rules...",NULL,false,!busy))      guiFilePick(GUI_FILE_SAVE_RULES);
    ImGui::Separator();
    if (ImGui::MenuItem("Save Positions in ASCII",NULL,false,!busy))
      guiSavePositionsAscii();
    if (ImGui::MenuItem("Save Positions in Binary...",NULL,false,!busy))
      guiFilePick(GUI_FILE_SAVE_POS);
    if (ImGui::MenuItem("Load Positions (Binary)...",NULL,false,!busy))
      guiFilePick(GUI_FILE_LOAD_POS);
    ImGui::Separator();
    if (ImGui::MenuItem("Quit","Cmd+Q"))                        gui.quitRequested = true;
    ImGui::EndMenu();
  }

  if (ImGui::BeginMenu("Simulate"))
  {
    if (ImGui::MenuItem("Run","Cmd+R",false,gui.run == GUI_IDLE))      guiSimRun();
    if (ImGui::MenuItem("Step","Space",false,gui.run == GUI_IDLE))     guiSimStep();
    if (ImGui::MenuItem("Stop","Cmd+.",false,gui.run == GUI_RUNNING))  guiSimStop();
    ImGui::Separator();
    if (ImGui::MenuItem("Reset All",NULL,false,!busy))                 guiSimReset(0);
    if (ImGui::MenuItem("Reset Environment",NULL,false,!busy))         guiSimReset(1);
    ImGui::Separator();
    if (ImGui::MenuItem("Start Batch...","Cmd+B",false,gui.run == GUI_IDLE))
      guiFilePick(GUI_FILE_OPEN_BATCH);
    if (ImGui::MenuItem("Stop Batch",NULL,false,gui.run == GUI_BATCH)) guiBatchStop();
    ImGui::EndMenu();
  }

  if (ImGui::BeginMenu("Options"))
  {
    if (ImGui::MenuItem("Display...",NULL,gui.showDisplaySettings)) gui.showDisplaySettings = !gui.showDisplaySettings;
    if (ImGui::MenuItem("System...", NULL,gui.showSystemSettings))  gui.showSystemSettings  = !gui.showSystemSettings;
    if (ImGui::MenuItem("Agent...",  NULL,gui.showAgentSettings))   gui.showAgentSettings   = !gui.showAgentSettings;
#ifdef NETWORK
    if (ImGui::MenuItem("Network...",NULL,gui.showNetworkSettings)) gui.showNetworkSettings = !gui.showNetworkSettings;
#else
    // The dialog existed only #ifdef NETWORK in 1995 too (UI.H declares
    // networkSettings inside the guard), so it is absent, not greyed out.
    ImGui::MenuItem("Network...",NULL,false,false);
#endif
    ImGui::EndMenu();
  }

  if (ImGui::BeginMenu("View"))
  {
    ImGui::MenuItem("Field",          NULL,&gui.showField);
    ImGui::MenuItem("Message Board",  NULL,&gui.showMsgBoard);
    ImGui::MenuItem("Sensors",        NULL,&gui.showSensors);
    ImGui::MenuItem("Classifier List",NULL,&gui.showClassList);
    ImGui::MenuItem("Status",         NULL,&gui.showStatus);
    ImGui::MenuItem("Batch Log",      NULL,&gui.showBatchLog);
    ImGui::Separator();
    if (ImGui::MenuItem("Reset to 1995 Layout"))
    {
      gui.showField = gui.showMsgBoard = gui.showSensors = true;
      gui.showClassList = gui.showStatus = true;
      gui.layoutReset = true;
    }
    ImGui::EndMenu();
  }

  if (ImGui::BeginMenu("Help"))
  {
    if (ImGui::MenuItem("About Borg...")) gui.showAbout = true;
    ImGui::EndMenu();
  }

  //--------------------------------------------------------------------------
  // Right-aligned run state.  1995 put this in the title bar ("Borg : File Walk
  // Mode, 3_7.p"; "Borg : Batch Mode").
  //--------------------------------------------------------------------------
  const char *stateText = "stopped";
  ImVec4 stateCol(0.60f,0.62f,0.66f,1.0f);
  switch (gui.run)
  {
    case GUI_RUNNING: stateText = "running";  stateCol = ImVec4(0.46f,0.80f,0.49f,1.0f); break;
    case GUI_BATCH:   stateText = "batch";    stateCol = ImVec4(0.98f,0.76f,0.33f,1.0f); break;
    case GUI_WALK:    stateText = "walk";     stateCol = ImVec4(0.55f,0.70f,0.95f,1.0f); break;
    case GUI_IDLE:    if (gui.simFinished) stateText = "finished"; break;
  }

  char right[128];
  snprintf(right,sizeof(right),"agent %d of %d   tick %d   %s",
           gui.agent+1,task.getNumAgents(),task.readClock(gui.agent),stateText);
  float w = ImGui::CalcTextSize(right).x;
  ImGui::SameLine(ImGui::GetWindowWidth() - w - 16.0f);
  ImGui::TextColored(stateCol,"%s",right);

  ImGui::EndMainMenuBar();
}

//============================================================================
// The speed bar.
//
// SpeedBar DIALOG in BORG.RC: five 26x26 push buttons -- Run, Step, Stop, Reset,
// Batch -- followed by eight numbered agent buttons that calcDimensions showed or
// hid according to task.getNumAgents().  Same buttons, same order, same
// show/hide rule.  The speed control on the right is new; 1995 had no use for
// one because one tick per idle message was the machine's top speed.
//============================================================================
static void guiSpeedBar(void)
{
  ImGuiViewport *vp = ImGui::GetMainViewport();
  float h = ImGui::GetFrameHeight() + ImGui::GetStyle().WindowPadding.y*2.0f;

  ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar |
                           ImGuiWindowFlags_NoSavedSettings;

  if (!ImGui::BeginViewportSideBar("##SpeedBar",vp,ImGuiDir_Up,h,flags))
  { ImGui::End();  return; }

  const bool idle  = (gui.run == GUI_IDLE);
  const bool busy  = (gui.run == GUI_BATCH || gui.run == GUI_WALK);

  ImGui::BeginDisabled(!idle);
  if (ImGui::Button("Run"))   guiSimRun();
  ImGui::SameLine();
  if (ImGui::Button("Step"))  guiSimStep();
  ImGui::EndDisabled();

  ImGui::SameLine();
  ImGui::BeginDisabled(gui.run != GUI_RUNNING);
  if (ImGui::Button("Stop"))  guiSimStop();
  ImGui::EndDisabled();

  ImGui::SameLine();
  ImGui::BeginDisabled(busy);
  if (ImGui::Button("Reset")) guiSimReset(0);
  ImGui::EndDisabled();

  ImGui::SameLine();
  ImGui::BeginDisabled(!idle);
  if (ImGui::Button("Batch")) guiFilePick(GUI_FILE_OPEN_BATCH);
  ImGui::EndDisabled();

  //--------------------------------------------------------------------------
  // Agent selectors, in each agent's own trail colour so the button and the
  // path on the field read as the same thing.  1995 numbered them 1..8 and left
  // them grey.
  //--------------------------------------------------------------------------
  int numAgents = task.getNumAgents();
  if (numAgents > BORG_GUI_MAX_AGENTS) numAgents = BORG_GUI_MAX_AGENTS;

  ImGui::SameLine();
  ImGui::TextUnformatted("|");

  for (int a = 0; a < numAgents; a++)
  {
    ImGui::SameLine();
    char label[16];
    snprintf(label,sizeof(label),"%d",a+1);

    ImU32 col = guiAgentColor(a);
    ImVec4 c  = ImGui::ColorConvertU32ToFloat4(col);
    bool  sel = (a == gui.agent);

    ImGui::PushStyleColor(ImGuiCol_Button,
        sel ? ImVec4(c.x*0.60f,c.y*0.60f,c.z*0.60f,1.0f)
            : ImVec4(c.x*0.22f,c.y*0.22f,c.z*0.22f,1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
        ImVec4(c.x*0.45f,c.y*0.45f,c.z*0.45f,1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text,
        sel ? ImVec4(1,1,1,1) : ImVec4(0.80f,0.80f,0.84f,1.0f));

    if (ImGui::Button(label)) gui.agent = a;

    ImGui::PopStyleColor(3);
  }

  //--------------------------------------------------------------------------
  // Speed.
  //--------------------------------------------------------------------------
  ImGui::SameLine();
  ImGui::TextUnformatted("|");
  ImGui::SameLine();
  ImGui::TextUnformatted("ticks/frame");
  ImGui::SameLine();
  ImGui::SetNextItemWidth(110.0f);
  ImGui::BeginDisabled(gui.unlimited || !gui.prefs.antOn);
  ImGui::SliderInt("##speed",&gui.ticksPerFrame,1,50,"%d",
                   ImGuiSliderFlags_AlwaysClamp);
  ImGui::EndDisabled();

  ImGui::SameLine();
  bool unlimited = (gui.unlimited != 0);
  if (ImGui::Checkbox("full speed",&unlimited)) gui.unlimited = unlimited ? 1 : 0;
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip("Tick for up to %.0f ms per frame instead of a fixed count.\n"
                      "Display Settings > Environment Display > After Iteration\n"
                      "does the same thing and also stops repainting the field.",
                      GUI_TICK_BUDGET_MS);

  ImGui::End();
}

//============================================================================
// The error modal.  Queued by hostDisplayError; see the long note there on why
// it cannot be raised at the point of failure.
//============================================================================
static void guiErrorModal(void)
{
  if (gui.errorPending)
  {
    ImGui::OpenPopup(gui.errorFatal ? "Fatal Error" : "Error");
    gui.errorPending = false;
  }

  ImGui::SetNextWindowSize(ImVec2(460,0),ImGuiCond_Appearing);
  if (ImGui::BeginPopupModal("Error",NULL,ImGuiWindowFlags_AlwaysAutoResize))
  {
    ImGui::PushTextWrapPos(440.0f);
    ImGui::TextUnformatted(gui.errorText);
    ImGui::PopTextWrapPos();
    ImGui::Separator();
    if (ImGui::Button("OK",ImVec2(120,0))) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
  }

  ImGui::SetNextWindowSize(ImVec2(460,0),ImGuiCond_Appearing);
  if (ImGui::BeginPopupModal("Fatal Error",NULL,ImGuiWindowFlags_AlwaysAutoResize))
  {
    ImGui::PushTextWrapPos(440.0f);
    ImGui::TextColored(ImVec4(0.95f,0.45f,0.40f,1.0f),"%s",gui.errorText);
    ImGui::PopTextWrapPos();
    ImGui::Separator();
    // 1995 called exit(1) from inside displayError.  Quitting through the normal
    // path instead means borg.ini and the dock layout are still written out.
    if (ImGui::Button("Quit",ImVec2(120,0))) gui.quitRequested = true;
    ImGui::EndPopup();
  }
}

//============================================================================
// Keyboard shortcuts.  1995's ACCELERATORS table assigned bare letters and
// collided with itself repeatedly -- "S" is listed for Options>System, File>Save
// and Simulate>Step, "A" for three more.  Only one of each could ever have fired.
// These are Cmd-chords instead, which on macOS is what ImGuiMod_Ctrl means.
//============================================================================
static void guiShortcuts(void)
{
  if (ImGui::GetIO().WantTextInput) return;

  if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_N)) guiFileNew();
  if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_O)) guiFilePick(GUI_FILE_OPEN_SIM);
  if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_S)) guiFileSave();
  if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_W)) gui.showWalkSetup = true;
  if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_B)) guiFilePick(GUI_FILE_OPEN_BATCH);
  if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_E) && gui.showField)
    guiFilePick(GUI_FILE_EXPORT_PNG);
  if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_R)) guiSimRun();
  if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Period)) guiSimStop();
  if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Q)) gui.quitRequested = true;
  if (ImGui::IsKeyPressed(ImGuiKey_Space,false))            guiSimStep();
}

//============================================================================
static void guiNoticeBar(void)
{
  if (gui.notice[0] == '\0' || ImGui::GetTime() > gui.noticeUntil) return;

  ImGuiViewport *vp = ImGui::GetMainViewport();
  float h = ImGui::GetTextLineHeight() + ImGui::GetStyle().WindowPadding.y*2.0f;

  if (ImGui::BeginViewportSideBar("##Notice",vp,ImGuiDir_Down,h,
          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings))
    ImGui::TextColored(ImVec4(0.72f,0.76f,0.84f,1.0f),"%s",gui.notice);
  ImGui::End();
}

//============================================================================
int main(int argc, char **argv)
{
  (void)argc;  (void)argv;

  //--------------------------------------------------------------------------
  // Front-end defaults.  The global `task` is already constructed by the time
  // main() runs -- host/globals.cpp defines it, as BORG.CPP did, and
  // taskEnvironment's constructor calls reset(1).  BORG.CPP's comment about
  // construction order (G, then task, then UI last) no longer applies: nothing
  // here is a global with a constructor that touches the simulation.
  //--------------------------------------------------------------------------
  gui.run            = GUI_IDLE;
  gui.agent          = 0;
  gui.ticksPerFrame  = 1;
  gui.unlimited      = 0;
  gui.showField      = true;
  gui.showMsgBoard   = true;
  gui.showSensors    = true;
  gui.showClassList  = true;
  gui.showStatus     = true;
  gui.showBatchLog   = false;

  guiPrefsLoad(&gui.prefs);

  //--------------------------------------------------------------------------
  if (!SDL_Init(SDL_INIT_VIDEO))
  { fprintf(stderr,"SDL_Init: %s\n",SDL_GetError());  return 1; }

  float mainScale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
  if (mainScale <= 0.0f) mainScale = 1.0f;

  SDL_WindowFlags wf = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN |
                       SDL_WINDOW_HIGH_PIXEL_DENSITY;

  guiWindow = SDL_CreateWindow("Borg 2.1",
                               int(1380*mainScale),int(920*mainScale),wf);
  if (!guiWindow)
  { fprintf(stderr,"SDL_CreateWindow: %s\n",SDL_GetError());  return 1; }

  guiRenderer = SDL_CreateRenderer(guiWindow,NULL);
  if (!guiRenderer)
  { fprintf(stderr,"SDL_CreateRenderer: %s\n",SDL_GetError());  return 1; }
  SDL_SetRenderVSync(guiRenderer,1);

  SDL_SetWindowPosition(guiWindow,SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED);
  SDL_ShowWindow(guiWindow);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
  io.IniFilename  = kLayoutIni;           // the dock layout, next to borg.ini

  guiApplyStyle(mainScale);
  ImGui::GetStyle().FontScaleDpi = mainScale;

  ImGui_ImplSDL3_InitForSDLRenderer(guiWindow,guiRenderer);
  ImGui_ImplSDLRenderer3_Init(guiRenderer);

  guiLoadFonts(mainScale);
  guiSettingsInvalidate();

  // No saved layout means a first run: build the 1995 arrangement.
  bool haveLayout = false;
  { FILE *f = fopen(kLayoutIni,"rb");  if (f) { haveLayout = true;  fclose(f); } }
  gui.layoutReset = !haveLayout;

  //--------------------------------------------------------------------------
  bool done = false;
  while (!done)
  {
    SDL_Event ev;
    while (SDL_PollEvent(&ev))
    {
      ImGui_ImplSDL3_ProcessEvent(&ev);
      if (ev.type == SDL_EVENT_QUIT) done = true;
      if (ev.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
          ev.window.windowID == SDL_GetWindowID(guiWindow)) done = true;
    }

    if (SDL_GetWindowFlags(guiWindow) & SDL_WINDOW_MINIMIZED)
    { SDL_Delay(10);  continue; }

    //------------------------------------------------------------------------
    // File dialogs land on another thread; act on the result here, between
    // frames, where calling into taskEnvironment is safe.
    //------------------------------------------------------------------------
    guiFileApplyPending();

    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    guiMenuBar();
    guiSpeedBar();
    guiNoticeBar();

    ImGuiID dockId = ImGui::DockSpaceOverViewport(0,ImGui::GetMainViewport());
    if (gui.layoutReset)
    {
      gui.layoutReset = false;
      guiBuildDefaultLayout(dockId,ImGui::GetMainViewport()->WorkSize);
    }

    guiShortcuts();

    //------------------------------------------------------------------------
    // Advance the simulation, the batch, or the walk -- exactly one of them.
    //------------------------------------------------------------------------
    if      (gui.run == GUI_BATCH) guiBatchTick();
    else if (gui.run == GUI_WALK)  guiWalkTick();
    else                           guiTickSimulation();

    guiPanelField();
    guiPanelMessageBoard();
    guiPanelClassifierList();
    guiPanelSensors();
    guiPanelStatus();
    guiBatchLogWindow();

    guiDisplaySettingsWindow();
    guiSystemSettingsWindow();
    guiAgentSettingsWindow();
    guiNetworkSettingsWindow();
    guiWalkSetupWindow();
    guiAboutWindow();
    guiErrorModal();

    if (gui.quitRequested) done = true;

    //------------------------------------------------------------------------
    ImGui::Render();
    SDL_SetRenderScale(guiRenderer,io.DisplayFramebufferScale.x,
                                   io.DisplayFramebufferScale.y);
    SDL_SetRenderDrawColor(guiRenderer,18,19,22,255);
    SDL_RenderClear(guiRenderer);
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(),guiRenderer);

    //------------------------------------------------------------------------
    // PNG export reads the rendered frame back, so it has to happen after the
    // draw data is submitted and before the buffers are swapped.
    //------------------------------------------------------------------------
    if (gui.exportRequest)
    {
      gui.exportRequest = 0;
      if (guiExportFieldPNG(gui.exportPath,gui.fieldX,gui.fieldY,
                            gui.fieldW,gui.fieldH))
        guiNotice("Wrote %s  (%d x %d)",gui.exportPath,gui.fieldW,gui.fieldH);
    }

    SDL_RenderPresent(guiRenderer);
  }

  //--------------------------------------------------------------------------
  // 1995 wrote borg.ini on WM_DESTROY; ImGui writes the dock layout itself when
  // the context is destroyed.
  //--------------------------------------------------------------------------
  guiPrefsSave(&gui.prefs);

  ImGui_ImplSDLRenderer3_Shutdown();
  ImGui_ImplSDL3_Shutdown();
  ImGui::DestroyContext();

  SDL_DestroyRenderer(guiRenderer);
  SDL_DestroyWindow(guiWindow);
  SDL_Quit();
  return 0;
}
