//============================================================================
// The four settings windows.
//
// One per 1995 dialog, in the same order, with the same group boxes, the same
// field labels and the same display precision (bid constant at %.3f, everything
// else at %.2f -- envSettings's own sprintf formats).  Resources in BORG.RC:
//
//   DisplaySettings DIALOG      ->  guiDisplaySettingsWindow()
//   EnvSettings DIALOG          ->  guiSystemSettingsWindow()
//        captioned "Classifier System Settings", reached from Options > System
//   AgentSettings DIALOG        ->  guiAgentSettingsWindow()
//   NetworkSettings DIALOG      ->  guiNetworkSettingsWindow()
//
// WHY OK/CANCEL AND NOT LIVE EDITING
//
// Not taste -- these parameters cannot safely change mid-run.  softReset()
// performs no allocation (the reset triad, CLAUDE.md), so the number of
// classifiers, the message length, the board length and the agent count must be
// identical across every soft reset in a batch.  The 1995 dialogs buffered edits
// and applied them on OK, then set resetRequest = 1 so a FULL reset followed
// (UI.CPP:776).  That is reproduced exactly, and the window says so above the
// buttons rather than letting it be a surprise.
//
// Each window re-reads the live values when it opens, which is what
// WM_INITDIALOG did.  Cancel therefore discards; there is nothing to undo.
//
// DERIVED, NOT EDITED
//
// maxMsgBoardLength never appears on the dialog.  envSettings computes it on OK:
//
//     actionPassingEnabled ?  maxActionsToPost + maxActionsToRx
//                          :  maxActionsToPost
//
// i.e. the board must hold this agent's own postings plus whatever action
// messages can arrive from the network in a tick.  Kept verbatim -- getting this
// wrong would silently truncate the board under action passing.
//
// And note the label: the "Max # of Postings" box inside the "Message Board"
// group is maxActionsToPost, not the board length.
//============================================================================

#include <stdio.h>
#include <string.h>

#include "borggui.h"
#include "task.h"
#include "cfs.h"
#include "ei.h"

//----------------------------------------------------------------------------
// Edit buffers.  One struct standing in for the `static` locals each 1995 dialog
// procedure kept.
//----------------------------------------------------------------------------
struct guiSettingsBuf
{
  // Classifier system -- EnvSettings
  unsigned seed;
  int numAgents, envType, geneticInterval, maxCount, maxMsgBoardLength,
      classListLength, numConditions, BBAenabled, BBAstyle, elitismEnabled,
      producerTaxInterval, CDOenabled, CEOenabled, TCOenabled, maxActionsToPost;
  double bidConstant, sexualProb, mutationProb, startStrength, headTax, bidTax,
         producerTax, eliteThresh, strengthCap, bidCap;

  // Agent -- AgentSettings
  float goalRange, obstRange, sensorRange, timeConst, antWidth,
        goalReward, dirReward, obstReward, crashPenalty;

  // Network -- NetworkSettings
  int netOn, actionPassingEnabled, classifierPassingEnabled,
      classifierTxInterval, maxClassifiersToTx, maxClassifiersToRx,
      actionTxInterval, maxActionsToTx, maxActionsToRx;
  double TxStrenThresh, RxStrenThresh, TxBidThresh, RxBidThresh;
};

static guiSettingsBuf sb;

// "Has this window just been opened?" -- the WM_INITDIALOG edge.
static bool wasDisplayOpen = false, wasSystemOpen = false,
            wasAgentOpen   = false, wasNetworkOpen = false;

//----------------------------------------------------------------------------
static void guiSettingsRead(void)
{
  sb.numAgents = task.getNumAgents();
  sb.envType   = task.getEnvType();

#ifdef NETWORK
  classifierSystem::getSettings(&sb.seed,&sb.geneticInterval,&sb.maxCount,
       &sb.netOn,&sb.maxMsgBoardLength,
       &sb.classListLength,&sb.numConditions,&sb.BBAenabled,&sb.BBAstyle,
       &sb.elitismEnabled,&sb.producerTaxInterval,&sb.CDOenabled,&sb.CEOenabled,
       &sb.TCOenabled,&sb.maxActionsToPost,&sb.bidConstant,&sb.sexualProb,
       &sb.mutationProb,&sb.startStrength,&sb.headTax,&sb.bidTax,
       &sb.producerTax,&sb.eliteThresh,&sb.strengthCap,&sb.bidCap,
       &sb.actionPassingEnabled,&sb.classifierPassingEnabled,
       &sb.classifierTxInterval,&sb.maxClassifiersToTx,&sb.maxClassifiersToRx,
       &sb.actionTxInterval,&sb.maxActionsToTx,&sb.maxActionsToRx,
       &sb.TxStrenThresh,&sb.RxStrenThresh,&sb.TxBidThresh,&sb.RxBidThresh);
#else
  // 1995's #else branch here passed &netOn to the 24-argument overload, which
  // does not take it, and read a netOn that was declared only #ifdef NETWORK --
  // so envSettings could not have compiled in a plain-LCS build either.  Same
  // family of defect as the .RUL header bug, same cause: the compiler never saw
  // the branch.  This is what that branch was trying to do.
  sb.netOn = 0;
  sb.actionPassingEnabled = sb.classifierPassingEnabled = 0;
  sb.classifierTxInterval = sb.maxClassifiersToTx = sb.maxClassifiersToRx = 0;
  sb.actionTxInterval = sb.maxActionsToTx = sb.maxActionsToRx = 0;
  sb.TxStrenThresh = sb.RxStrenThresh = sb.TxBidThresh = sb.RxBidThresh = 0.0;
  classifierSystem::getSettings(&sb.seed,&sb.geneticInterval,&sb.maxCount,
       &sb.maxMsgBoardLength,
       &sb.classListLength,&sb.numConditions,&sb.BBAenabled,&sb.BBAstyle,
       &sb.elitismEnabled,&sb.producerTaxInterval,&sb.CDOenabled,&sb.CEOenabled,
       &sb.TCOenabled,&sb.maxActionsToPost,&sb.bidConstant,&sb.sexualProb,
       &sb.mutationProb,&sb.startStrength,&sb.headTax,&sb.bidTax,
       &sb.producerTax,&sb.eliteThresh,&sb.strengthCap,&sb.bidCap);
#endif

  environmentInterface::getAgentSettings(&sb.goalRange,&sb.obstRange,
                                         &sb.sensorRange,&sb.timeConst,
                                         &sb.antWidth);
  environmentInterface::getEnvSettings(&sb.goalReward,&sb.dirReward,
                                       &sb.obstReward,&sb.crashPenalty);
}

void guiSettingsInvalidate(void)
{
  guiSettingsRead();
}

//----------------------------------------------------------------------------
// OK.  setSettings, then a full reset -- resetRequest = 1 in 1995.
//----------------------------------------------------------------------------
static void guiSettingsApply(void)
{
  if (sb.numAgents < 1)                   sb.numAgents = 1;
  if (sb.numAgents > BORG_GUI_MAX_AGENTS) sb.numAgents = BORG_GUI_MAX_AGENTS;

  task.setNumAgents(sb.numAgents);
  task.setEnvType(sb.envType);

#ifdef NETWORK
  // Verbatim from UI.CPP:749.
  if (sb.actionPassingEnabled)
    sb.maxMsgBoardLength = sb.maxActionsToPost + sb.maxActionsToRx;
  else
    sb.maxMsgBoardLength = sb.maxActionsToPost;

  classifierSystem::setSettings(sb.seed,sb.geneticInterval,sb.maxCount,
       sb.netOn,sb.maxMsgBoardLength,
       sb.classListLength,sb.numConditions,sb.BBAenabled,sb.BBAstyle,
       sb.elitismEnabled,sb.producerTaxInterval,sb.CDOenabled,sb.CEOenabled,
       sb.TCOenabled,sb.maxActionsToPost,sb.bidConstant,sb.sexualProb,
       sb.mutationProb,sb.startStrength,sb.headTax,sb.bidTax,sb.producerTax,
       sb.eliteThresh,sb.strengthCap,sb.bidCap,
       sb.actionPassingEnabled,sb.classifierPassingEnabled,
       sb.classifierTxInterval,sb.maxClassifiersToTx,sb.maxClassifiersToRx,
       sb.actionTxInterval,sb.maxActionsToTx,sb.maxActionsToRx,
       sb.TxStrenThresh,sb.RxStrenThresh,sb.TxBidThresh,sb.RxBidThresh);
#else
  sb.maxMsgBoardLength = sb.maxActionsToPost;
  classifierSystem::setSettings(sb.seed,sb.geneticInterval,sb.maxCount,
       sb.maxMsgBoardLength,
       sb.classListLength,sb.numConditions,sb.BBAenabled,sb.BBAstyle,
       sb.elitismEnabled,sb.producerTaxInterval,sb.CDOenabled,sb.CEOenabled,
       sb.TCOenabled,sb.maxActionsToPost,sb.bidConstant,sb.sexualProb,
       sb.mutationProb,sb.startStrength,sb.headTax,sb.bidTax,sb.producerTax,
       sb.eliteThresh,sb.strengthCap,sb.bidCap);
#endif

  environmentInterface::setAgentSettings(sb.goalRange,sb.obstRange,
                                         sb.sensorRange,sb.timeConst,
                                         sb.antWidth);
  environmentInterface::setEnvSettings(sb.goalReward,sb.dirReward,
                                       sb.obstReward,sb.crashPenalty);

  if (gui.agent >= sb.numAgents) gui.agent = sb.numAgents-1;

  gui.run = GUI_IDLE;
  gui.simFinished = 0;
  task.reset();

  guiNotice("Settings applied; full reset performed (parameters cannot change "
            "between soft resets).");
}

//----------------------------------------------------------------------------
// Shared furniture.
//----------------------------------------------------------------------------
static void guiGroup(const char *title)
{
  ImGui::Dummy(ImVec2(0,2));
  ImGui::SeparatorText(title);
}

// A field that existed on the 1995 dialog but is declared NOT USED.  Five of
// these are WS_DISABLED in BORG.RC already (numConditions, bid tax, producer tax
// and its interval, TCO); eliteThresh was editable in 1995 but CLAUDE.md records
// that it too must stay at its documented value, so it is disabled here.
#define GUI_UNUSED_TIP "Declared but not implemented; must stay at this value. " \
                       "See CLAUDE.md, \"Things to know before changing code\"."

static void guiUnusedInt(const char *label, int v)
{
  ImGui::BeginDisabled(true);
  int copy = v;
  ImGui::SetNextItemWidth(90);
  ImGui::InputInt(label,&copy,0,0);
  ImGui::EndDisabled();
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
    ImGui::SetTooltip(GUI_UNUSED_TIP);
}

static void guiUnusedDouble(const char *label, double v, const char *fmt)
{
  ImGui::BeginDisabled(true);
  double copy = v;
  ImGui::SetNextItemWidth(90);
  ImGui::InputDouble(label,&copy,0.0,0.0,fmt);
  ImGui::EndDisabled();
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
    ImGui::SetTooltip(GUI_UNUSED_TIP);
}

static void guiInt(const char *label, int *v)
{ ImGui::SetNextItemWidth(90);  ImGui::InputInt(label,v,0,0); }

static void guiDbl(const char *label, double *v, const char *fmt)
{ ImGui::SetNextItemWidth(90);  ImGui::InputDouble(label,v,0.0,0.0,fmt); }

static void guiFlt(const char *label, float *v, const char *fmt)
{ ImGui::SetNextItemWidth(90);  ImGui::InputFloat(label,v,0.0f,0.0f,fmt); }

//----------------------------------------------------------------------------
// OK / Cancel, plus the cross-navigation buttons the 1995 dialogs carried: each
// one could hand off to the other two, applying the current edits on the way
// (IDC_ENVagentButton, IDC_AGENTsystemButton, IDC_NETWORKagentButton, ...).
//----------------------------------------------------------------------------
static bool guiOkCancel(bool *open, const char *gotoA, bool *openA,
                        const char *gotoB, bool *openB)
{
  ImGui::Dummy(ImVec2(0,4));
  ImGui::Separator();
  ImGui::TextDisabled("OK applies every field on this window and performs a full "
                      "reset.");

  bool applied = false;

  if (ImGui::Button("OK",ImVec2(90,0)))
  { guiSettingsApply();  applied = true;  *open = false; }

  ImGui::SameLine();
  if (ImGui::Button("Cancel",ImVec2(90,0)))
  { guiSettingsRead();   *open = false; }      // discard: re-read the live values

  if (gotoA)
  {
    ImGui::SameLine(0,24);
    char label[64];  snprintf(label,sizeof(label),"%s...",gotoA);
    if (ImGui::Button(label,ImVec2(100,0)))
    { guiSettingsApply();  applied = true;  *open = false;  *openA = true; }
  }
  if (gotoB)
  {
    ImGui::SameLine();
    char label[64];  snprintf(label,sizeof(label),"%s...",gotoB);
    if (ImGui::Button(label,ImVec2(100,0)))
    { guiSettingsApply();  applied = true;  *open = false;  *openB = true; }
  }

  return applied;
}

//============================================================================
// DisplaySettings DIALOG -- four radio pairs, 210 x 159 dialog units.
//
// This is the one window with no OK/Cancel behaviour to preserve: the 1995
// dialog did have them, but every setting on it is a drawing preference that
// touches nothing in the simulation, so they apply immediately and are written
// to borg.ini on exit.  Nothing here can invalidate a soft reset.
//============================================================================
void guiDisplaySettingsWindow(void)
{
  if (!gui.showDisplaySettings) { wasDisplayOpen = false;  return; }
  wasDisplayOpen = true;

  ImGui::SetNextWindowSize(ImVec2(420,0),ImGuiCond_FirstUseEver);
  if (!ImGui::Begin("Display Settings",&gui.showDisplaySettings,
                    ImGuiWindowFlags_AlwaysAutoResize))
  { ImGui::End();  return; }

  guiGroup("Environment Display");
  int v = gui.prefs.antOn;
  ImGui::RadioButton("During Iteration##env",&v,1);  ImGui::SameLine(200);
  ImGui::RadioButton("After Iteration##env", &v,0);
  gui.prefs.antOn = v;
  ImGui::TextDisabled("  After Iteration stops repainting the field while a run "
                      "is in progress,\n  which is also what makes it fast.");

  guiGroup("Information Display");
  v = gui.prefs.infoOn;
  ImGui::RadioButton("During Iteration##info",&v,1);  ImGui::SameLine(200);
  ImGui::RadioButton("After Iteration##info", &v,0);
  gui.prefs.infoOn = v;

  guiGroup("Axes Display");
  v = gui.prefs.axesOn;
  ImGui::RadioButton("On##axes", &v,1);  ImGui::SameLine(200);
  ImGui::RadioButton("Off##axes",&v,0);
  gui.prefs.axesOn = v;

  guiGroup("Object Display");
  v = gui.prefs.objectsToScale;
  ImGui::RadioButton("To Scale##obj",    &v,1);  ImGui::SameLine(200);
  ImGui::RadioButton("Not To Scale##obj",&v,0);
  gui.prefs.objectsToScale = v;
  ImGui::TextDisabled("  Not To Scale draws every object at 1/40 of the field so "
                      "it stays visible.");

  ImGui::Dummy(ImVec2(0,4));
  ImGui::Separator();
  ImGui::TextDisabled("Saved to borg.ini on exit, in the 1995 format.");

  ImGui::End();
}

//============================================================================
// EnvSettings DIALOG -- "Classifier System Settings".  Group order, field order
// and labels as in BORG.RC: Classifier List, Taxes, Genetics, System, Message
// Board, Rule Discovery.
//============================================================================
void guiSystemSettingsWindow(void)
{
  if (!gui.showSystemSettings) { wasSystemOpen = false;  return; }
  if (!wasSystemOpen) { guiSettingsRead();  wasSystemOpen = true; }  // WM_INITDIALOG

  ImGui::SetNextWindowSize(ImVec2(720,0),ImGuiCond_FirstUseEver);
  if (!ImGui::Begin("Classifier System Settings",&gui.showSystemSettings,
                    ImGuiWindowFlags_AlwaysAutoResize))
  { ImGui::End();  return; }

  if (ImGui::BeginTable("sysCols",2,ImGuiTableFlags_None))
  {
    ImGui::TableNextRow();

    //----------------------------------------------------------------------
    ImGui::TableSetColumnIndex(0);
    guiGroup("Classifier List");
    guiInt("# of Classifiers",&sb.classListLength);
    guiUnusedInt("# of Conditions",sb.numConditions);
    guiDbl("Initial Strengths",&sb.startStrength,"%.2f");
    guiDbl("Bid Constant",&sb.bidConstant,"%.3f");
    guiDbl("Bid Cap",&sb.bidCap,"%.2f");
    guiDbl("Strength Cap",&sb.strengthCap,"%.2f");

    guiGroup("Genetics");
    guiDbl("Crossover Probability",&sb.sexualProb,"%.2f");
    guiDbl("Mutation Probability",&sb.mutationProb,"%.2f");
    guiInt("Genetic Interval",&sb.geneticInterval);
    { bool elite = (sb.elitismEnabled != 0);
      if (ImGui::Checkbox("Elitism Enabled",&elite))
        sb.elitismEnabled = elite ? 1 : 0; }
    guiUnusedDouble("Elite Threshold",sb.eliteThresh,"%.2f");
    ImGui::TextDisabled("  Probabilities have 2-decimal resolution; do not set "
                        "one below 0.01.");

    //----------------------------------------------------------------------
    ImGui::TableSetColumnIndex(1);
    guiGroup("Taxes");
    guiDbl("Head Tax",&sb.headTax,"%.2f");
    guiUnusedDouble("Bid Tax",sb.bidTax,"%.2f");
    guiUnusedDouble("Producer Tax",sb.producerTax,"%.2f");
    guiUnusedInt("Producer Tax Interval",sb.producerTaxInterval);

    guiGroup("System");
    { int s = (int)sb.seed;  guiInt("Random Seed",&s);
      if (s < 0) s = 0;  sb.seed = (unsigned)s; }
    guiInt("Max Iterations",&sb.maxCount);
    guiInt("Number of Agents",&sb.numAgents);
    ImGui::TextDisabled("  1 to %d; 1995 allowed eight because the speed bar had "
                        "eight buttons.",BORG_GUI_MAX_AGENTS);

    bool bba = (sb.BBAenabled != 0);
    if (ImGui::Checkbox("BBA Enabled",&bba)) sb.BBAenabled = bba ? 1 : 0;

    ImGui::RadioButton("Standard Environment",&sb.envType,taskEnvironment::STANDARD);
    ImGui::RadioButton("Concave Obstacle",    &sb.envType,taskEnvironment::CONCAVE_OBST);

    guiGroup("Message Board");
    guiInt("Max # of Postings",&sb.maxActionsToPost);
    ImGui::TextDisabled("  The board length itself is derived on OK, not edited:\n"
                        "  action passing on -> postings + max actions to "
                        "receive.");

    guiGroup("Rule Discovery");
    bool cdo = (sb.CDOenabled != 0), ceo = (sb.CEOenabled != 0);
    if (ImGui::Checkbox("CDO Enabled",&cdo)) sb.CDOenabled = cdo ? 1 : 0;
    if (ImGui::Checkbox("CEO Enabled",&ceo)) sb.CEOenabled = ceo ? 1 : 0;
    { ImGui::BeginDisabled(true);
      bool tco = (sb.TCOenabled != 0);
      ImGui::Checkbox("TCO Enabled",&tco);
      ImGui::EndDisabled();
      if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip(GUI_UNUSED_TIP); }

    ImGui::EndTable();
  }

  guiOkCancel(&gui.showSystemSettings,
              "Agent",  &gui.showAgentSettings,
#ifdef NETWORK
              "Network",&gui.showNetworkSettings);
#else
              NULL,NULL);
#endif

  ImGui::End();
}

//============================================================================
// AgentSettings DIALOG -- Detection Range, Mechanical Constants, Payoffs.
//============================================================================
void guiAgentSettingsWindow(void)
{
  if (!gui.showAgentSettings) { wasAgentOpen = false;  return; }
  if (!wasAgentOpen) { guiSettingsRead();  wasAgentOpen = true; }

  ImGui::SetNextWindowSize(ImVec2(560,0),ImGuiCond_FirstUseEver);
  if (!ImGui::Begin("Agent Settings",&gui.showAgentSettings,
                    ImGuiWindowFlags_AlwaysAutoResize))
  { ImGui::End();  return; }

  if (ImGui::BeginTable("agentCols",2))
  {
    ImGui::TableNextRow();

    ImGui::TableSetColumnIndex(0);
    guiGroup("Detection Range");
    guiFlt("Goal",    &sb.goalRange,  "%.2f");
    guiFlt("Obstacle",&sb.obstRange,  "%.2f");
    guiFlt("Sensor",  &sb.sensorRange,"%.2f");
    ImGui::TextDisabled("  Sensor is the half-angle that splits left\n"
                        "  from right (checkGoalSensors, TASK.CPP).");

    ImGui::TableSetColumnIndex(1);
    guiGroup("Mechanical Constants");
    guiFlt("Time Constant", &sb.timeConst,"%.2f");
    guiFlt("Agent Diameter",&sb.antWidth, "%.2f");
    ImGui::TextDisabled("  Agent Diameter also sizes the drawn\n"
                        "  animat when Objects are To Scale.");

    ImGui::EndTable();
  }

  guiGroup("Payoffs");
  if (ImGui::BeginTable("payoffCols",2))
  {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    guiFlt("Goal Reward",     &sb.goalReward,"%.2f");
    guiFlt("Direction Reward",&sb.dirReward, "%.2f");
    ImGui::TableSetColumnIndex(1);
    guiFlt("Obstacle Reward", &sb.obstReward,   "%.2f");
    guiFlt("Crash Penalty",   &sb.crashPenalty, "%.2f");
    ImGui::EndTable();
  }
  ImGui::TextDisabled("  Payoff is shaped: full reward for closing on the goal "
                      "and for improving heading,\n"
                      "  half-magnitude penalty otherwise, plus the crash "
                      "penalty -- and it is paid only\n"
                      "  to the supplier of message board slot 0 "
                      "(adjustStrengths, EI.CPP).");

  guiOkCancel(&gui.showAgentSettings,
              "System", &gui.showSystemSettings,
#ifdef NETWORK
              "Network",&gui.showNetworkSettings);
#else
              NULL,NULL);
#endif

  ImGui::End();
}

//============================================================================
// NetworkSettings DIALOG.
//
// Declared #ifdef NETWORK in UI.H, so it does not exist in a plain-LCS build and
// this function compiles to nothing there.
//============================================================================
void guiNetworkSettingsWindow(void)
{
#ifdef NETWORK
  if (!gui.showNetworkSettings) { wasNetworkOpen = false;  return; }
  if (!wasNetworkOpen) { guiSettingsRead();  wasNetworkOpen = true; }

  ImGui::SetNextWindowSize(ImVec2(640,0),ImGuiCond_FirstUseEver);
  if (!ImGui::Begin("Network Settings",&gui.showNetworkSettings,
                    ImGuiWindowFlags_AlwaysAutoResize))
  { ImGui::End();  return; }

  bool netOn = (sb.netOn != 0);
  if (ImGui::Checkbox("Network Enabled",&netOn)) sb.netOn = netOn ? 1 : 0;
  ImGui::TextDisabled("  Clearing this is how DLCS is disabled without "
                      "recompiling. Measured: a DLCS build\n"
                      "  with the network off reproduces a plain-LCS build's "
                      "run exactly.");

  if (ImGui::BeginTable("netCols",2))
  {
    ImGui::TableNextRow();

    ImGui::TableSetColumnIndex(0);
    guiGroup("Classifier Passing");
    { bool on = (sb.classifierPassingEnabled != 0);
      if (ImGui::Checkbox("Enabled##class",&on))
        sb.classifierPassingEnabled = on ? 1 : 0; }
    guiInt("Transmit Interval##class",     &sb.classifierTxInterval);
    guiInt("Max to Transmit##class",       &sb.maxClassifiersToTx);
    guiDbl("TX Strength Threshold",&sb.TxStrenThresh,"%.2f");
    guiInt("Max to Receive##class",        &sb.maxClassifiersToRx);
    guiDbl("RX Strength Threshold",&sb.RxStrenThresh,"%.2f");
    ImGui::TextDisabled("  Shares high-strength rules between agents.");

    ImGui::TableSetColumnIndex(1);
    guiGroup("Action Passing");
    { bool on = (sb.actionPassingEnabled != 0);
      if (ImGui::Checkbox("Enabled##action",&on))
        sb.actionPassingEnabled = on ? 1 : 0; }
    guiInt("Transmit Interval##action",&sb.actionTxInterval);
    guiInt("Max to Transmit##action",  &sb.maxActionsToTx);
    guiDbl("TX Bid Threshold",&sb.TxBidThresh,"%.2f");
    guiInt("Max to Receive##action",   &sb.maxActionsToRx);
    guiDbl("RX Bid Threshold",&sb.RxBidThresh,"%.2f");
    ImGui::TextDisabled("  Lets one agent's action fire another's rule,\n"
                        "  and lengthens the message board on OK.");

    ImGui::EndTable();
  }

  guiOkCancel(&gui.showNetworkSettings,
              "System",&gui.showSystemSettings,
              "Agent", &gui.showAgentSettings);

  ImGui::End();
#endif
}

//============================================================================
// AboutBox DIALOG.  The text is the dialog's, line for line.
//============================================================================
void guiAboutWindow(void)
{
  if (!gui.showAbout) return;

  ImGui::SetNextWindowSize(ImVec2(440,0),ImGuiCond_Appearing);
  if (!ImGui::Begin("About Borg",&gui.showAbout,
                    ImGuiWindowFlags_AlwaysAutoResize))
  { ImGui::End();  return; }

  float w = ImGui::GetContentRegionAvail().x;
  #define GUI_CENTER(s) ImGui::SetCursorPosX((w - ImGui::CalcTextSize(s).x)*0.5f \
                                             + ImGui::GetStyle().WindowPadding.x); \
                        ImGui::TextUnformatted(s)

  ImGui::Dummy(ImVec2(0,4));
  GUI_CENTER("BORG");
  GUI_CENTER("Basic Organized Robot Group");
  ImGui::Dummy(ImVec2(0,4));
  GUI_CENTER("A distributed learning classifier system simulation");
  GUI_CENTER("environment by Doug Gaff");
  ImGui::Dummy(ImVec2(0,4));
  GUI_CENTER("Version 2.1");
  #undef GUI_CENTER

  ImGui::Dummy(ImVec2(0,8));
  ImGui::Separator();
  ImGui::TextDisabled(
    "Layers 1 and 2 are the 1995 Borland C++ sources, essentially unchanged.\n"
    "Layer 3 -- this interface -- is a 2026 rebuild on Dear ImGui and SDL3;\n"
    "the Win16 original is preserved in UI.CPP, GRAPH.CPP, BORG.CPP and BORG.RC.\n"
#ifdef NETWORK
    "Build: DLCS (NETWORK defined)."
#else
    "Build: plain LCS (NETWORK not defined)."
#endif
  );

  ImGui::Dummy(ImVec2(0,6));
  if (ImGui::Button("OK",ImVec2(90,0))) gui.showAbout = false;

  ImGui::End();
}
