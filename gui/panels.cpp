//============================================================================
// The five panels.
//
// Each one is a direct translation of a 1995 display function, and each reads
// the same getters UI.CPP read -- the ones TASK.H groups under the heading
// "Access functions for a user interface".  The correspondence is:
//
//   guiPanelField()           repaintWindow()       UI.CPP:1330  (field portion)
//                             drawAnt/drawObstacles/drawGoal/labelAxes
//   guiPanelMessageBoard()    displayMessageBoard() UI.CPP:1740
//   guiPanelClassifierList()  displayClassifierList()
//   guiPanelSensors()         displaySensors()
//   guiPanelStatus()          displayStatus()
//
// The column layouts of the three text panels are the 1995 sprintf formats,
// unchanged, because those formats are how the thesis figures were read.
//============================================================================

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "borggui.h"
#include "imgui_internal.h"          // ImRect, for the field's clip handling

#include "task.h"
#include "cfs.h"
#include "ei.h"

//----------------------------------------------------------------------------
// Agent colours.  1995: antColor[i] = G.RED + i, indexing the nine-entry
// palette in GRAPH.CPP -- so agent 1 is red, agent 2 green, agent 3 blue, and so
// on through purple, yellow, cyan, grey, white.  Those are pure saturated RGB
// values, which is what a 16-colour VGA palette had; they are kept exactly,
// because the agent colours are how multi-agent figures in the thesis are read.
//----------------------------------------------------------------------------
static const ImU32 guiPalette[BORG_GUI_MAX_AGENTS] =
{
  IM_COL32(255,  0,  0,255),   // RED
  IM_COL32(  0,255,  0,255),   // GREEN
  IM_COL32(  0,  0,255,255),   // BLUE
  IM_COL32(255,  0,255,255),   // PURPLE
  IM_COL32(255,255,  0,255),   // YELLOW
  IM_COL32(  0,255,255,255),   // CYAN
  IM_COL32(192,192,192,255),   // GRAY
  IM_COL32(255,255,255,255)    // WHITE
};

ImU32 guiAgentColor(int agent)
{
  if (agent < 0) agent = 0;
  return guiPalette[agent % BORG_GUI_MAX_AGENTS];
}

// obstacleColor(G.BLUE), goalColor(G.RED) -- userInterface's constructor.
#define GUI_OBSTACLE_COLOR IM_COL32(  0,  0,255,255)
#define GUI_GOAL_COLOR     IM_COL32(255,  0,  0,255)

// The field is drawn on paper, not on the window's dark chrome: the 1995 client
// area was white and every colour above was chosen to sit on white.  Putting the
// plot on its own near-white ground keeps those colours legible and keeps the
// panel reading as the plot it is.
#define GUI_PAPER          IM_COL32(250,249,246,255)
#define GUI_INK            IM_COL32( 40, 40, 44,255)
#define GUI_INK_FAINT      IM_COL32(150,150,156,255)

//----------------------------------------------------------------------------
// Scope guard for the fixed-pitch font.
//----------------------------------------------------------------------------
// Must be released BEFORE ImGui::End(): ImGui requires every PushFont to be
// matched inside the window that pushed it, and a guard whose destructor runs at
// the end of the enclosing function pops after End() -- which ImGui reports as
// "Missing PopFont()" in one window and "Calling PopFont() too many times!" in
// the next.  Hence pop(), called explicitly, with the destructor as the backstop
// for an early return.
struct guiMonoScope
{
  bool pushed;
  guiMonoScope() { pushed = (guiMono != NULL); if (pushed) ImGui::PushFont(guiMono,0.0f); }
  void pop() { if (pushed) { ImGui::PopFont();  pushed = false; } }
  ~guiMonoScope() { pop(); }
};

//============================================================================
// FIELD
//============================================================================
void guiPanelField(void)
{
  if (!gui.showField) return;

  if (!ImGui::Begin("Field",&gui.showField))
  { ImGui::End();  return; }

  int x1,x2,y1,y2;
  task.getAxesRanges(&x1,&x2,&y1,&y2);

  coord *obstPts = 0;  int numObst = 0;  coord goalPt;
  task.getObjects(&obstPts,&numObst,&goalPt);

  float gRange,oRange,sRange,tConst,agentWidth;
  environmentInterface::getAgentSettings(&gRange,&oRange,&sRange,&tConst,
                                         &agentWidth);

  ImDrawList *dl  = ImGui::GetWindowDrawList();
  ImVec2 boxMin   = ImGui::GetCursorScreenPos();
  ImVec2 avail    = ImGui::GetContentRegionAvail();

  if (avail.x < 80.0f || avail.y < 80.0f)
  { ImGui::TextUnformatted("(panel too small)");  ImGui::End();  return; }

  ImVec2 boxMax(boxMin.x+avail.x, boxMin.y+avail.y);
  dl->AddRectFilled(boxMin,boxMax,GUI_PAPER);

  //--------------------------------------------------------------------------
  // Margins for the axis decoration.  1995 (calcDimensions) reserved
  //   left   = 2*charHeight + widest-y-label width + "Y" width
  //   bottom = 4*charHeight       (tick, label, title, trailing space)
  //   top    = 1*charHeight
  //   right  = 1*charHeight
  // computed from the system font.  Same shape, measured from the font in use.
  //--------------------------------------------------------------------------
  const float ch = ImGui::GetTextLineHeight();
  float mL = 2.0f, mR = 2.0f, mT = 2.0f, mB = 2.0f;

  char wid[32];
  if (gui.prefs.axesOn)
  {
    snprintf(wid,sizeof(wid),"%d ",y1);
    float w1 = ImGui::CalcTextSize(wid).x;
    snprintf(wid,sizeof(wid),"%d ",y2);
    float w2 = ImGui::CalcTextSize(wid).x;
    float widest = (w1 > w2) ? w1 : w2;

    mL = widest + ch*2.0f;
    mB = ch*3.5f;
    mT = ch*0.5f;
    mR = ch*0.5f;
  }

  //--------------------------------------------------------------------------
  // The plot rectangle.  1995 forced the environment box square in
  // calcDimensions and then computed dx and dy independently, which was safe
  // because both axis ranges are square too -- STANDARD is -600..600 on both
  // axes and CONCAVE_OBST is 800 units wide by 800 tall (TASK.CPP:687,698).
  //
  // A dock node is whatever shape the user dragged it to, so the aspect ratio is
  // honoured here explicitly and the plot is centred in what is left.  Without
  // this, the animat's circular body and its turning circles would be drawn as
  // ellipses in any panel that is not square.
  //--------------------------------------------------------------------------
  float availW = avail.x - mL - mR, availH = avail.y - mT - mB;
  if (availW < 16.0f || availH < 16.0f)
  { ImGui::TextUnformatted("(panel too small)");  ImGui::End();  return; }

  float dataW = float(x2-x1), dataH = float(y2-y1);
  if (dataW <= 0.0f) dataW = 1.0f;
  if (dataH <= 0.0f) dataH = 1.0f;

  float plotW = availW, plotH = availW * dataH / dataW;
  if (plotH > availH) { plotH = availH;  plotW = availH * dataW / dataH; }

  float plotL = boxMin.x + mL + (availW-plotW)*0.5f;
  float plotT = boxMin.y + mT + (availH-plotH)*0.5f;
  float plotR = plotL + plotW, plotB = plotT + plotH;

  // xCoord()/yCoord() from UI.H, with the same y flip.
  const float sx = plotW/dataW, sy = plotH/dataH;
  #define FX(v) (plotL + (float(v) - float(x1))*sx)
  #define FY(v) (plotB - (float(v) - float(y1))*sy)

  //--------------------------------------------------------------------------
  // Object size.  Verbatim from calcDimensions:
  //   objectsToScale ? agentWidth*dx : (field width)/40
  // The second branch exists so the animat stays visible on a small screen; it
  // is not to scale and the Display Settings window says so.
  //--------------------------------------------------------------------------
  float objW = gui.prefs.objectsToScale ? (agentWidth*sx) : (plotW/40.0f);
  if (objW < 2.0f) objW = 2.0f;
  const float objR = objW*0.5f;

  dl->PushClipRect(boxMin,boxMax,true);

  //--------------------------------------------------------------------------
  // Axes.  5 tics per axis, labelled at x1 .. x2 in four equal steps, ticks
  // outside the frame, titles "X" and "Y" -- labelAxes(hDC,x1,x2,5,"X",...).
  //--------------------------------------------------------------------------
  if (gui.prefs.axesOn)
  {
    dl->AddRect(ImVec2(plotL,plotT),ImVec2(plotR,plotB),GUI_INK_FAINT);

    const int numTics = 4;                      // numXTics - 1, as in 1995
    for (int i = 0; i <= numTics; i++)
    {
      int v = x1 + (x2-x1)*i/numTics;
      float x = (i == numTics) ? plotR-1.0f : FX(v);
      dl->AddLine(ImVec2(x,plotB),ImVec2(x,plotB+ch*0.5f),GUI_INK);

      snprintf(wid,sizeof(wid),"%d",v);
      ImVec2 ts = ImGui::CalcTextSize(wid);
      float tx = (i == numTics) ? (x - ts.x) : (x - ts.x*0.5f);
      dl->AddText(ImVec2(tx,plotB+ch*0.6f),GUI_INK,wid);
    }
    ImVec2 ts = ImGui::CalcTextSize("X");
    dl->AddText(ImVec2((plotL+plotR)*0.5f - ts.x*0.5f, plotB+ch*1.9f),GUI_INK,"X");

    for (int i = 0; i <= numTics; i++)
    {
      int v = y1 + (y2-y1)*i/numTics;
      float y = (i == numTics) ? plotT : FY(v);
      dl->AddLine(ImVec2(plotL-ch*0.5f,y),ImVec2(plotL,y),GUI_INK);

      snprintf(wid,sizeof(wid),"%d",v);
      ImVec2 lt = ImGui::CalcTextSize(wid);
      dl->AddText(ImVec2(plotL-ch*0.7f-lt.x, y-ch*0.5f),GUI_INK,wid);
    }
    ts = ImGui::CalcTextSize("Y");
    dl->AddText(ImVec2(boxMin.x+2.0f,(plotT+plotB)*0.5f - ts.y*0.5f),GUI_INK,"Y");
  }

  //--------------------------------------------------------------------------
  // Obstacles: filled squares of side obstWidth, centred on the point
  // (drawObstacles).  Goal: filled circle (drawGoal).
  //--------------------------------------------------------------------------
  for (int i = 0; i < numObst; i++)
  {
    float ox = FX(obstPts[i].getX()), oy = FY(obstPts[i].getY());
    dl->AddRectFilled(ImVec2(ox-objR,oy-objR),ImVec2(ox+objR,oy+objR),
                      GUI_OBSTACLE_COLOR);
  }

  dl->AddCircleFilled(ImVec2(FX(goalPt.getX()),FY(goalPt.getY())),objR,
                      GUI_GOAL_COLOR);

  //--------------------------------------------------------------------------
  // Agent trails.  1995 drew an ant at EVERY recorded position, each a filled
  // circle with a short line showing the heading (drawAnt), so the trail is a
  // thick dotted path with direction ticks along it.  That is reproduced here,
  // for every agent, in the agent's own colour -- repaintWindow looped over all
  // agents, not just the selected one.
  //
  // The one addition: the current position gets a ring round it.  With two
  // agents crossing the same ground it is otherwise genuinely hard to see where
  // each one has got to, which on a 1993 screen mattered less because the
  // display was redrawn incrementally as it moved.
  //--------------------------------------------------------------------------
  int numAgents = task.getNumAgents();
  if (numAgents > BORG_GUI_MAX_AGENTS) numAgents = BORG_GUI_MAX_AGENTS;

  for (int a = 0; a < numAgents; a++)
  {
    coord huge *pts = 0;  float huge *dirs = 0;  int count = 0;
    float gDist,gAng,oDist,oAng;
    task.getAgent(a,&pts,&dirs,&count,&gDist,&gAng,&oDist,&oAng);
    if (!pts || count <= 0) continue;

    ImU32 col = guiAgentColor(a);

    for (int i = 0; i < count; i++)
    {
      float px = FX(pts[i].getX()), py = FY(pts[i].getY());
      dl->AddCircleFilled(ImVec2(px,py),objR,col);

      // drawAnt's heading line: out objW/2 data-units along the heading.
      float hx = px + cosf(dirs[i])*objR;
      float hy = py - sinf(dirs[i])*objR;    // screen y is inverted
      dl->AddLine(ImVec2(px,py),ImVec2(hx,hy),GUI_INK);
    }

    float hx = FX(pts[count-1].getX()), hy = FY(pts[count-1].getY());
    dl->AddCircle(ImVec2(hx,hy),objR+2.0f,GUI_INK,0,1.5f);
  }

  dl->PopClipRect();

  //--------------------------------------------------------------------------
  // Remember where this went, in framebuffer pixels, for PNG export.  ImGui
  // works in logical points and SDL_SetRenderScale multiplies by the display
  // scale, so the pixels to read back are the points times that scale.
  //--------------------------------------------------------------------------
  ImVec2 fbScale = ImGui::GetIO().DisplayFramebufferScale;
  gui.fieldX = int(boxMin.x * fbScale.x);
  gui.fieldY = int(boxMin.y * fbScale.y);
  gui.fieldW = int(avail.x  * fbScale.x);
  gui.fieldH = int(avail.y  * fbScale.y);

  ImGui::Dummy(avail);              // claim the space so the window scrolls right
  #undef FX
  #undef FY
  ImGui::End();
}

//============================================================================
// MESSAGE BOARD
//
// displayMessageBoard's format, unchanged:
//   NETWORK    "%s %4d %4lu %6.2f"     message, supplier, agent ID, bid
//   plain      "%s %4d %6.2f"          message, supplier, bid
//
// and the empty slots below the current postings were filled with X's to the
// same width -- a real detail, not noise: it showed how much of the board the
// bidding had actually used.  Kept.
//============================================================================
void guiPanelMessageBoard(void)
{
  if (!gui.showMsgBoard) return;

  if (!ImGui::Begin("Message Board",&gui.showMsgBoard))
  { ImGui::End();  return; }

  guiMonoScope mono;

  int msgLen   = message::size();
  int slots    = messageBoard::size();
  int inUse    = task.getMsgBoardCurrentSize(gui.agent);

  ImGui::TextColored(ImVec4(0.62f,0.62f,0.66f,1.0f),
#ifdef NETWORK
                     "%-*s %4s %4s %6s",msgLen,"message","sup","ID","bid");
#else
                     "%-*s %4s %6s",msgLen,"message","sup","bid");
#endif
  ImGui::Separator();

  char *str = new char[msgLen+1];
  char line[160];

  for (int i = 0; i < slots; i++)
  {
    if (i < inUse)
    {
      message msg;  int sup;  double bid;
#ifdef NETWORK
      unsigned long ID;
      task.getMsgBoardPosting(gui.agent,i,msg,&sup,&ID,&bid);
      msg.msgToStr(str);
      snprintf(line,sizeof(line),"%s %4d %4lu %6.2f",str,sup,ID,bid);
#else
      task.getMsgBoardPosting(gui.agent,i,msg,&sup,&bid);
      msg.msgToStr(str);
      snprintf(line,sizeof(line),"%s %4d %6.2f",str,sup,bid);
#endif
      ImGui::TextUnformatted(line);
    }
    else
    {
      for (int j = 0; j < msgLen; j++) str[j] = 'X';
      str[msgLen] = '\0';
#ifdef NETWORK
      snprintf(line,sizeof(line),"%s XXXX XXXX XXXXXX",str);
#else
      snprintf(line,sizeof(line),"%s XXXX XXXXXX",str);
#endif
      ImGui::TextDisabled("%s",line);
    }
  }

  delete [] str;
  mono.pop();
  ImGui::End();
}

//============================================================================
// CLASSIFIER LIST
//
// displayClassifierList's format, unchanged:
//   NETWORK  "%c%c%c %4d %s %s %6.2f %4d %4lu %6.2f %6.2f"
//            match, selected, elite, rule #, condition, action,
//            support, supplier, agent ID, bid, strength
//   plain    "%c%c%c %4d %s %s %4.2f %6.2f %4d %6.2f"
//            ... specificity, support, supplier, strength
//
// The two differ by more than the ID column -- the plain build prints
// specificity where the NETWORK build prints the agent ID -- so the header here
// is per-variant too.
//
// NOTE on the plain branch: displayClassifierList declares `elite` and prints it
// but the non-NETWORK getClassifier() has no elite parameter to fill it from
// (TASK.H), so in 1995 that column printed an uninitialised value in a plain
// build.  This is in UI.CPP, which is not built, and is listed here only because
// it is the same class of bug as the .RUL header: a defect that could not be
// seen because the compiler never looked at the branch.  Nothing in the
// simulation depends on it.
//============================================================================
void guiPanelClassifierList(void)
{
  if (!gui.showClassList) return;

  if (!ImGui::Begin("Classifier List",&gui.showClassList))
  { ImGui::End();  return; }

  guiMonoScope mono;

  int msgLen = message::size();
  int count  = classifierList::size();

  ImGui::TextColored(ImVec4(0.62f,0.62f,0.66f,1.0f),
#ifdef NETWORK
                     "msc %4s %-*s %-*s %6s %4s %4s %6s %6s",
                     "rule",msgLen,"condition",msgLen,"action",
                     "sup","splr","ID","bid","stren");
#else
                     "msc %4s %-*s %-*s %4s %6s %4s %6s",
                     "rule",msgLen,"condition",msgLen,"action",
                     "spec","sup","splr","stren");
#endif
  ImGui::Separator();

  char *condStr = new char[msgLen+1];
  char *actStr  = new char[msgLen+1];
  char line[256];

  for (int i = 0; i < count; i++)
  {
    message cond,act;
    double stren,spec,sup,ruleBid;
    int match,sel,supplier,elite,ruleNum;

#ifdef NETWORK
    unsigned long ID;
    task.getClassifier(gui.agent,i,cond,act,&stren,&spec,&sup,&ruleBid,
                       &match,&sel,&supplier,&ID,&elite,&ruleNum);
    cond.msgToStr(condStr);
    act.msgToStr(actStr);
    snprintf(line,sizeof(line),"%c%c%c %4d %s %s %6.2f %4d %4lu %6.2f %6.2f",
             match ? '*' : ' ', sel ? '*' : ' ', elite ? '*' : ' ',
             ruleNum,condStr,actStr,sup,supplier,ID,ruleBid,stren);
#else
    task.getClassifier(gui.agent,i,cond,act,&stren,&spec,&sup,&ruleBid,
                       &match,&sel,&supplier,&elite,&ruleNum);
    cond.msgToStr(condStr);
    act.msgToStr(actStr);
    snprintf(line,sizeof(line),"%c%c%c %4d %s %s %4.2f %6.2f %4d %6.2f",
             match ? '*' : ' ', sel ? '*' : ' ', elite ? '*' : ' ',
             ruleNum,condStr,actStr,spec,sup,supplier,stren);
#endif

    // A matched rule is the one thing on this panel the eye hunts for during a
    // step-by-step run, so it is emphasised rather than left to the '*' column.
    if (sel)       ImGui::TextColored(ImVec4(1.00f,0.85f,0.35f,1.0f),"%s",line);
    else if (match) ImGui::TextUnformatted(line);
    else            ImGui::TextDisabled("%s",line);
  }

  delete [] condStr;
  delete [] actStr;
  mono.pop();
  ImGui::End();
}

//============================================================================
// SENSORS
//
// displaySensors printed one line: the sensor message as a string.  The animat
// encoding (EI.CPP) puts GL GR OL OR in alleles 0-3 -- goal-left, goal-right,
// obstacle-left, obstacle-right -- and the actuator command the classifiers
// write back is alleles 2 and 3, left and right wheel.  The legend below names
// the alleles, which the 1995 panel did not: the panel was one row of digits and
// you had to know.  (CLAUDE.md records that the comment above moveAnt claiming
// the command is in the LAST two alleles is stale; trust the mask.)
//============================================================================
void guiPanelSensors(void)
{
  if (!gui.showSensors) return;

  if (!ImGui::Begin("Sensors",&gui.showSensors))
  { ImGui::End();  return; }

  message sensorMsg = task.getAgentSensorMsg(gui.agent);
  int msgLen = message::size();

  char *str = new char[msgLen+1];
  sensorMsg.msgToStr(str);

  {
    guiMonoScope mono;
    ImGui::TextUnformatted(str);
    if (msgLen >= 4)
      ImGui::TextDisabled("GL GR OL OR");
  }

  ImGui::SameLine();
  ImGui::TextDisabled("  goal L/R, obstacle L/R");

  delete [] str;
  ImGui::End();
}

//============================================================================
// STATUS
//
// displayStatus's thirteen lines and its two-column break after item 6, with the
// same labels, the same field widths and the same units.  degrees() is EI.H's.
//============================================================================
void guiPanelStatus(void)
{
  if (!gui.showStatus) return;

  if (!ImGui::Begin("Status",&gui.showStatus))
  { ImGui::End();  return; }

  guiMonoScope mono;

  int clockNow = task.readClock(gui.agent);

  int numCrossovers,numSexual,numMutations,numCDOs,numCEOs,numCrashes,count;
  coord huge *pts = 0;  float huge *dirs = 0;
  float goalDist,goalAngle,obstDist,obstAngle;

  task.readStats(gui.agent,&numCrossovers,&numSexual,&numMutations,&numCDOs,
                 &numCEOs,&numCrashes);
  task.getAgent(gui.agent,&pts,&dirs,&count,&goalDist,&goalAngle,
                &obstDist,&obstAngle);

  char c0[13][64];
  int haveTrail = (pts && count > 0);

  snprintf(c0[ 0],64,"Seed:              %4u",classifierSystem::getSeed());
  snprintf(c0[ 1],64,"x loc:           %6.1f",haveTrail ? pts[count-1].getX() : 0.0f);
  snprintf(c0[ 2],64,"y loc:           %6.1f",haveTrail ? pts[count-1].getY() : 0.0f);
  snprintf(c0[ 3],64,"Direction:      %+7.2f",haveTrail ? degrees(dirs[count-1]) : 0.0);
  snprintf(c0[ 4],64,"Distance:        %6.1f",goalDist);
  snprintf(c0[ 5],64,"Angle:          %+7.2f",degrees(goalAngle));
  snprintf(c0[ 6],64,"Iteration:         %4d",clockNow);
  snprintf(c0[ 7],64,"Sexual Crossover:  %4d",numSexual);
  snprintf(c0[ 8],64,"Asexual Crossover: %4d",numCrossovers-numSexual);
  snprintf(c0[ 9],64,"Mutations:         %4d",numMutations);
  snprintf(c0[10],64,"CDOs:              %4d",numCDOs);
  snprintf(c0[11],64,"CEOs:              %4d",numCEOs);
  snprintf(c0[12],64,"Crashes:           %4d",numCrashes);

  // 1995: columnSpace = 30 fixed-font characters, break to column 1 at item 6.
  float colWidth = ImGui::CalcTextSize("X").x * 30.0f;

  ImGui::BeginGroup();
  for (int i = 0; i < 6; i++) ImGui::TextUnformatted(c0[i]);
  ImGui::EndGroup();

  ImGui::SameLine(0.0f,0.0f);
  ImGui::SetCursorPosX(ImGui::GetCursorPosX() +
                       (colWidth - ImGui::CalcTextSize(c0[0]).x));

  ImGui::BeginGroup();
  for (int i = 6; i < 13; i++) ImGui::TextUnformatted(c0[i]);
  ImGui::EndGroup();

  mono.pop();
  ImGui::End();
}
