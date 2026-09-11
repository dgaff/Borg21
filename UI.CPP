#define STRICT
#define OEMRESOURCE
#include<windows.h>
#include<commdlg.h>
#include<dir.h>
#include"task.h"
#include"ui.h"
#include"borg.h"

#define MAX_AGENTS   8   // If this is to be set greater than 8, more buttons
                         // buttons must be added to the speed bar and more
                         // colors must be added to graph

//============================================================================
//============================================================================
userInterface::userInterface() :   obstacleColor(G.BLUE), goalColor(G.RED),
                    simState(normalStop), stopRequest(0), resetRequest(0),
                    batchRequest(0), runRequest(0), newRequest(0),
                    stepRequest(0), runStartRequest(0), resetEnvRequest(0),
                    walkRequest(0), infoIsOld(0),
                    windowMinimized(0), lastAgentIndexDisp(0),
                    msgBoardStart(0), classListStart(0),
                    fileWalkStart(0), fileWalkEnd(0), agent(0),
                    newAgentDisplayRequest(0), dialogBoxRequest(0),
                    objectsToScale(0)
{
  simFile[0] = 0; ruleFile[0] = 0; binPosFile[0] = 0;
  antColor = new int[MAX_AGENTS];
  for (int i=0; i<MAX_AGENTS; i++)
    antColor[i] = G.RED+i;
}

//============================================================================
//============================================================================
// Initializes the application when program is first loaded.
BOOL userInterface::initApplication(HINSTANCE hinstCurrent)
{
 WNDCLASS wc;

 wc.style = NULL;
 wc.lpfnWndProc = mainWndProcHook;
 wc.cbClsExtra = 0;
 wc.cbWndExtra = 0;
 wc.hInstance = hinstCurrent;
 wc.hIcon = LoadIcon(hinstCurrent,"BorgIcon");
 wc.hCursor = LoadCursor(NULL, IDC_ARROW);
 wc.hbrBackground = (HBRUSH) GetStockObject(WHITE_BRUSH);
 wc.lpszMenuName = "MainMenu";
 wc.lpszClassName = "BorgMainWindowClass";

 return RegisterClass(&wc);
}

//============================================================================
//============================================================================
// Used to initialize each instance of the application.
BOOL userInterface::initInstance(HINSTANCE hinstCurrent, int nCmdShow)
{
 // Initialize instance handle
 hInst = hinstCurrent;

 // Create and show main window

 strcpy(windowTitle,"Borg");

 HWND hWnd = CreateWindow("BorgMainWindowClass",
                     windowTitle,
                     WS_OVERLAPPEDWINDOW,
                     CW_USEDEFAULT,
                     CW_USEDEFAULT,
                     CW_USEDEFAULT,
                     CW_USEDEFAULT,
                     NULL,
                     NULL,
                     hinstCurrent,
                     NULL);

 if (hWnd == NULL) return FALSE;

 // Read the .ini file
 UI.readINI();

 // Create the process for the speed bat
 speedBarProc = MakeProcInstance((FARPROC) speedBarHook, hinstCurrent);
 if (!speedBarProc) error(11,"userInterface::initInstance()");

 // Create the scroll and the speed bar windows

 scrollBarWidth = GetSystemMetrics(SM_CXVSCROLL);

 hClassListScrollBar = CreateWindow("Scrollbar", NULL,
                                    WS_CHILD | SBS_VERT, 0, 0,
                                    scrollBarWidth, 0,
                                    hWnd, (HMENU) IDC_CLASSLISTscroll,
                                    hinstCurrent, NULL);

 hMsgBoardScrollBar =  CreateWindow("Scrollbar", NULL,
                                    WS_CHILD | SBS_VERT, 0, 0,
                                    scrollBarWidth, 0,
                                    hWnd, (HMENU) IDC_MSGBOARDscroll,
                                    hinstCurrent, NULL);

 hSpeedBar = CreateDialog(hinstCurrent, "SpeedBar", hWnd, (DLGPROC) speedBarProc);

 if (!hClassListScrollBar || !hMsgBoardScrollBar || !hSpeedBar)
   error(10,"userInterface::initInstance()");

 // Get the menu handle
 hMenu = GetMenu(hWnd);

 // Initialize all screen dimensions
 UI.calcDimensions(1);

 // Turn on the application window.  The first button in the speed bar is
 // highlighted by default, so a message is sent to unhighlight it.
 ShowWindow(hWnd,nCmdShow | SW_SHOWMAXIMIZED);
 SendDlgItemMessage(hSpeedBar,IDC_SPEEDBARrun,BM_SETSTYLE,
                    LOWORD(BS_PUSHBUTTON),1);

 return TRUE;
}

//============================================================================
//============================================================================
// Used to clean up each instance of the application.
void userInterface::cleanUpInstance()
{
  writeINI();
  DestroyWindow(hSpeedBar);           // destroy speedbar
  FreeProcInstance(speedBarProc);     // free speedbar process
  DestroyWindow(hClassListScrollBar);
  DestroyWindow(hMsgBoardScrollBar);
  deleteRegionObjects();
}

//============================================================================
//============================================================================
void userInterface::deleteRegionObjects()
{
  DeleteObject(hEnvRegion);
  DeleteObject(hFieldRegion);
  DeleteObject(hMsgBoardRegion);
  DeleteObject(hSensorsRegion);
  DeleteObject(hClassListRegion);
  DeleteObject(hStatusRegion);
  DeleteObject(hEnvBox);
  DeleteObject(hMsgBoardBox);
  DeleteObject(hClassListBox);
  DeleteObject(hSensorsBox);
  DeleteObject(hStatusBox);
  DeleteObject(hSpeedBarRegion);
  if (classListScrollOn) DeleteObject(hClassListScrollRegion);
  if (msgBoardScrollOn) DeleteObject(hMsgBoardScrollRegion);
}

//============================================================================
//============================================================================
// Message processing windows function.

LRESULT FAR PASCAL userInterface::mainWndProc(HWND hWnd, UINT wMessage,
                                              WPARAM wParam, LPARAM lParam)
{
 UI.hWnd = hWnd;
 UI.wMessage = wMessage;
 UI.wParam = wParam;
 UI.lParam = lParam;

 switch(wMessage)
 {
   case WM_MENUSELECT:  // Menu selection in progress
                    if (!windowMinimized &&
                        (simState == normalRun || simState == batchRun))
                      stopRequest = 1;
                    break;

   case WM_COMMAND: // for processing menu selections
                    if (!processMenu())
                      DefWindowProc(hWnd,wMessage,wParam,lParam);
                    break;

   case WM_SIZE:    // for client area resizing
                    processSize();
                    break;

   case WM_PAINT:   // for client area repainting
                    repaintWindow();
                    break;

   case WM_CREATE:  // message sent when window is created.  used for creating
                    // brush tools, etc. to use in drawing.
                    SetTimer(hWnd,1,16,0);
                    //!!! Check for a return value to make sure a timer is
                    // available.
                    break;

   case WM_DESTROY: // converse of WM_CREATE case.
                    KillTimer(hWnd,1);
                    cleanUpInstance();
                    PostQuitMessage(0);
                    break;

   case WM_TIMER:   // timer pop
                    processTimer();
                    break;

   case WM_VSCROLL: // Vertical scroll bar moved
                    processScroll();
                    break;

   default:
     return DefWindowProc(hWnd,wMessage,wParam,lParam);
 }

 return 0;
}

//============================================================================
//============================================================================
void userInterface::processSize()
{
  if (wParam == SIZE_MINIMIZED) windowMinimized = 1;
  else
  {
    windowMinimized = 0;
    calcDimensions();
    ShowWindow(hSpeedBar,SW_HIDE);
    ShowWindow(hMsgBoardScrollBar,SW_HIDE);
    ShowWindow(hClassListScrollBar,SW_HIDE);
    InvalidateRect(hWnd,NULL,1);
    UpdateWindow(hWnd);
  }
}

//============================================================================
//============================================================================
// returns 1 if message was processed.
int userInterface::processMenu()
{
  switch(wParam)
  {
    case IDM_FILEopen:            openFile(); return 1;
    case IDM_FILEwalk:            fileWalkSelect(); return 1;
    case IDM_FILEnew:             newRequest = 1; return 1;
    case IDM_FILEsaveAs:          saveFileAs(); return 1;
    case IDM_FILEsave:
       if (simFile[0] == 0) saveFileAs();
       else saveFile();
       return 1;
    case IDM_FILEsaveMeta:        //createMetaFile(); return 1;
                                  //makeBitmap(); return 1;
                                  task.loadAgentPositions();  return 1;
    case IDM_FILEloadRules:       loadRules(); return 1;
    case IDM_FILEsaveRules:       saveRules(); return 1;
    case IDM_FILEsavePositions:   saveAgentPositions(); return 1;
    case IDM_FILEsavePositionsBin: savePositionsBin(); return 1;
    case IDM_FILEexit:            DestroyWindow(hWnd); return 1;
    case IDM_SIMULATErun:         runStartRequest = 1; return 1;
    case IDM_SIMULATEstep:        stepRequest = 1; return 1;
    case IDM_SIMULATEstop:        stopRequest = 1; return 1;
    case IDM_SIMULATEreset:       resetRequest = 1; return 1;
    case IDM_SIMULATEresetEnv:    resetEnvRequest = 1; return 1;
    case IDM_SIMULATEstartBatch:  batchRequest = 1;  return 1;
    case IDM_OPTIONSdisplay:      dialogBoxRequest = DISP_SETTINGS; return 1;
    case IDM_OPTIONSenvironment:  dialogBoxRequest = ENV_SETTINGS; return 1;
    case IDM_OPTIONSagent:        dialogBoxRequest = AGENT_SETTINGS; return 1;
    case IDM_OPTIONSnetwork:      dialogBoxRequest = NETWORK_SETTINGS; return 1;
    case IDM_HELPabout:           dialogBoxRequest = ABOUT; return 1;
    default:
       return 0;
  }
}

/*
    This function processes user requests.  Certain requests take priority over
    others, and only one request is processed per iteration cycle.

displayState:  envOnly, infoOnly, all

*/

//============================================================================
//============================================================================
void PASCAL userInterface::processTimer()
{
  if (dialogBoxRequest)
  {
    FARPROC lpProc;
    int whichBox = dialogBoxRequest;
    dialogBoxRequest = 0;

    switch(whichBox)
    {
      case DISP_SETTINGS:
        lpProc = MakeProcInstance((FARPROC) displaySettingsHook, hInst);
        DialogBox(hInst, "DisplaySettings", hWnd, (DLGPROC) lpProc);
        FreeProcInstance(lpProc);
        break;
      case ENV_SETTINGS:
        lpProc = MakeProcInstance((FARPROC) envSettingsHook, hInst);
        DialogBox(hInst, "EnvSettings", hWnd, (DLGPROC) lpProc);
        FreeProcInstance(lpProc);
        break;
      case AGENT_SETTINGS:
        lpProc = MakeProcInstance((FARPROC) agentSettingsHook, hInst);
        DialogBox(hInst, "AgentSettings", hWnd, (DLGPROC) lpProc);
        FreeProcInstance(lpProc);
        break;
      case NETWORK_SETTINGS:
        lpProc = MakeProcInstance((FARPROC) networkSettingsHook, hInst);
        DialogBox(hInst, "NetworkSettings", hWnd, (DLGPROC) lpProc);
        FreeProcInstance(lpProc);
        break;
      case ABOUT:
        lpProc = MakeProcInstance((FARPROC) aboutHook, hInst);
        DialogBox(hInst, "AboutBox", hWnd, (DLGPROC) lpProc);
        FreeProcInstance(lpProc);
        break;
    }
  }

  if (stopRequest)
  {
    stopRequest = 0;
    if (simState == normalRun)
    { simState = normalStop; runRequest = 0; updateScreen(STOP_UPDATE); }
    else if (simState == batchRun)
    { simState = batchStop; runRequest = 0; updateScreen(STOP_UPDATE); }
    else if (simState == fileWalk)
    {
      MessageBeep(-1);
      if (MessageBox(hWnd,"Quit file walk through?","File Walk Mode",
                     MB_APPLMODAL | MB_ICONQUESTION | MB_YESNO) == IDYES)
        endWalk();
    }
  }

  if (newRequest)
  {
    newRequest = 0;
    if (simState == normalStop || simState == goal)
    {
      task.restoreDefaults();
      agent = 0;  // set display to first agent
      task.reset(); 
      updateScreen(RESET_UPDATE);
      simState = normalStop;
      strcpy(windowTitle,"Borg");
      SetWindowText(hWnd,windowTitle);
      simFile[0] = 0;
    }
  }

  if (resetRequest)
  {
    resetRequest = 0;
    if (simState == normalStop || simState == goal)
    { task.reset(); updateScreen(RESET_UPDATE); simState = normalStop; }
    else if (simState == batchStop)
    {
      MessageBeep(-1);
      if (MessageBox(hWnd,"Quit batch simulation?","Batch Run",
                     MB_APPLMODAL | MB_ICONQUESTION | MB_YESNO) == IDYES)
      {
        load(tempFile);
        remove(tempFile);
        SetWindowText(hWnd,windowTitle);
        simState = normalStop;
      }
    }
    else if (simState == fileWalk)
    {
      MessageBeep(-1);
      if (MessageBox(hWnd,"Quit file walk through?","File Walk Mode",
                     MB_APPLMODAL | MB_ICONQUESTION | MB_YESNO) == IDYES)
        endWalk();
    }
  }

  if (resetEnvRequest)
  {
    resetEnvRequest = 0;
    if (simState == normalStop || simState == goal)
    {
      task.softReset(); updateScreen(RESET_UPDATE); simState = normalStop;
    }
  }

  if (batchRequest)
  {
    batchRequest = 0;
    if (simState == normalStop) startBatch();
    else if (simState == batchRun) runBatch();
  }

  if (runStartRequest)
  {
    runStartRequest = 0;
    if (simState == normalStop || simState == batchStop)
    {
      updateScreen(START_UPDATE);
      runRequest = 1;
    }
  }

  if (runRequest)
  {
    runRequest = 0;
    if (simState == normalStop || simState == normalRun)
    {
      if (task.clockTick()) { simState = goal; updateScreen(STEP_UPDATE); }
      else { simState = normalRun; runRequest = 1; updateScreen(RUN_UPDATE); }
    }
    else if (simState == batchStop || simState == batchRun)
    {
      if (task.clockTick()) { batchRequest = 1; updateScreen(STEP_UPDATE); }
      else { simState = batchRun; runRequest = 1; updateScreen(RUN_UPDATE); }
    }
  }

  if (stepRequest)
  {
    stepRequest = 0;
    if (simState == normalStop)
    { if (task.clockTick()) simState = goal; updateScreen(STEP_UPDATE); }
    else if (simState == fileWalk)
    { walkRequest = 1; }
  }

  if (walkRequest)
  {
    walkRequest = 0;
    if (simState == normalStop) startWalk();
    else if (simState == fileWalk) fileWalkCont();
    else
    {
      error(13,"");
      walkRequest = 0;
    }
  }

  if (newAgentDisplayRequest)
  {
    if (simState == normalStop || simState == batchStop || infoOn ||
        simState == goal)
      if (newAgentDisplayRequest <= task.getNumAgents())
      {
        agent = newAgentDisplayRequest-1;
        updateScreen(BOX_UPDATE);
        updateScreen(STEP_UPDATE);
      }
    newAgentDisplayRequest = 0;
  }
}

//============================================================================
//============================================================================
/*
   This function adjusts the start index based on the scroll bar selection.
   The normalizeIndices() function actually checks to see if the range is
   valid.
*/
void userInterface::processScroll()
{
  if ((HWND) HIWORD(lParam) == hClassListScrollBar)
  {
    if (listScroll(&classListStart,&classListEnd,classifierList::size(),
                   maxClassListLength))
    {
      G.invalidateRgn(hWnd,hClassListRegion,0);
      G.invalidateRgn(hWnd,hClassListScrollRegion,0);
    }
  }
  else if ((HWND) HIWORD(lParam) == hMsgBoardScrollBar)
  {
    if (listScroll(&msgBoardStart,&msgBoardEnd,messageBoard::size(),
                   maxMsgBoardLength))
    {
      G.invalidateRgn(hWnd,hMsgBoardRegion,0);
      G.invalidateRgn(hWnd,hMsgBoardScrollRegion,0);
    }
  }
}

//============================================================================
//============================================================================
int userInterface::listScroll(int *start, int *end, int len, int max)
{
  int change = 0, tempStart = *start, tempEnd = *end;
  switch(wParam)
  {
    case SB_LINEUP:
      tempStart--; change = 1;
      break;
    case SB_LINEDOWN:
      tempStart++; change = 1;
      break;
    case SB_PAGEUP:
      tempStart -= max; change = 1;
      break;
    case SB_PAGEDOWN:
      tempStart += max; change = 1;
      break;
    case SB_THUMBPOSITION:
      return 1;
    case SB_THUMBTRACK:
      tempStart = LOWORD(lParam); change = 1;
      break;
  }

  if (change)
  {
    normalizeIndices(&tempStart,&tempEnd,len,max);
    if (tempStart != *start || tempEnd != *end)
    {
      *start = tempStart;  *end = tempEnd;
      return 1;
    }
  }
  return 0;
}

//============================================================================
//============================================================================
#pragma argsused
BOOL FAR PASCAL userInterface::about(HWND hDlg, WORD wMessage, WPARAM wParam,
                                     LPARAM lParam)
{
  switch (wMessage)
  {
    case WM_INITDIALOG:
      return TRUE;

    case WM_COMMAND:
      if (wParam == IDOK || wParam == IDCANCEL)
      {
        EndDialog(hDlg,TRUE);
        return TRUE;
      }
      break;
  }

  return FALSE;
}

//============================================================================
//============================================================================
#pragma argsused
BOOL FAR PASCAL userInterface::displaySettings(HWND hDlg, WORD wMessage,
                                               WPARAM wParam, LPARAM lParam)
{
  static int tempAntOn, tempInfoOn, tempAxesOn, tempObjectsToScale;

  switch (wMessage)
  {
    case WM_INITDIALOG:
      antOn  ? SendDlgItemMessage(hDlg,IDC_DISPLAYenvDuring,BM_SETCHECK,1,0):
               SendDlgItemMessage(hDlg,IDC_DISPLAYenvAfter,BM_SETCHECK,1,0);
      infoOn ? SendDlgItemMessage(hDlg,IDC_DISPLAYinfoDuring,BM_SETCHECK,1,0):
               SendDlgItemMessage(hDlg,IDC_DISPLAYinfoAfter,BM_SETCHECK,1,0);
      axesOn ? SendDlgItemMessage(hDlg,IDC_DISPLAYaxesOn,BM_SETCHECK,1,0):
               SendDlgItemMessage(hDlg,IDC_DISPLAYaxesOff,BM_SETCHECK,1,0);
      objectsToScale ? SendDlgItemMessage(hDlg,IDC_DISPLAYobjectToScale,BM_SETCHECK,1,0):
                       SendDlgItemMessage(hDlg,IDC_DISPLAYobjectNotToScale,BM_SETCHECK,1,0);
      tempAntOn = antOn;  tempInfoOn = infoOn;  tempAxesOn = axesOn;
      tempObjectsToScale = objectsToScale;
      return TRUE;

    case WM_COMMAND:
      switch (wParam)
      {
        case IDC_DISPLAYenvDuring:
          tempAntOn = 1;
          return TRUE;
        case IDC_DISPLAYenvAfter:
          tempAntOn = 0;
          return TRUE;
        case IDC_DISPLAYaxesOn:
          tempAxesOn = 1;
          return TRUE;
        case IDC_DISPLAYaxesOff:
          tempAxesOn = 0;
          return TRUE;
        case IDC_DISPLAYinfoDuring:
          tempInfoOn = 1;
          return TRUE;
        case IDC_DISPLAYinfoAfter:
          tempInfoOn = 0;
          return TRUE;
        case IDC_DISPLAYobjectToScale:
          tempObjectsToScale = 1;
          return TRUE;
        case IDC_DISPLAYobjectNotToScale:
          tempObjectsToScale = 0;
          return TRUE;
        case IDOK:
          if (axesOn != tempAxesOn || objectsToScale != tempObjectsToScale)
          {
            axesOn = tempAxesOn;
            objectsToScale = tempObjectsToScale;
            calcDimensions();
            G.invalidateRgn(hWnd,hEnvRegion,1);
          }
          antOn = tempAntOn;  infoOn = tempInfoOn;
          EndDialog(hDlg,TRUE);
          return TRUE;
        case IDCANCEL:
          EndDialog(hDlg,TRUE);
          return TRUE;
      }
      break;
  }

  return FALSE;
}

//============================================================================
//============================================================================
#pragma argsused
BOOL FAR PASCAL userInterface::envSettings(HWND hDlg, WORD wMessage,
                                           WPARAM wParam, LPARAM lParam)
{
  char buffer[10];
  static unsigned seed;

  static int numAgents, envType,
             geneticInterval, maxCount,
             maxMsgBoardLength,
             classListLength, numConditions, BBAenabled,  BBAstyle,
             elitismEnabled, producerTaxInterval, CDOenabled, CEOenabled,
             TCOenabled, maxActionsToPost;
  static double bidConstant, sexualProb, mutationProb, startStrength,
                headTax, bidTax, producerTax, eliteThresh, strengthCap, bidCap;
#ifdef NETWORK
  static int actionPassingEnabled, classifierPassingEnabled, classifierTxInterval,
      maxClassifiersToTx, maxClassifiersToRx, actionTxInterval, maxActionsToTx,
      maxActionsToRx, netOn;
  static double TxStrenThresh, RxStrenThresh, TxBidThresh, RxBidThresh;
#endif

  switch (wMessage)
  {
    case WM_INITDIALOG:
      numAgents = task.getNumAgents();
      envType = task.getEnvType();
#ifdef NETWORK
      classifierSystem::getSettings(&seed, &geneticInterval, &maxCount, &netOn,
           &maxMsgBoardLength,
           &classListLength, &numConditions, &BBAenabled,  &BBAstyle,
           &elitismEnabled, &producerTaxInterval, &CDOenabled, &CEOenabled,
           &TCOenabled, &maxActionsToPost, &bidConstant, &sexualProb,
           &mutationProb, &startStrength, &headTax, &bidTax, &producerTax,
           &eliteThresh, &strengthCap, &bidCap,
           &actionPassingEnabled, &classifierPassingEnabled, &classifierTxInterval,
           &maxClassifiersToTx, &maxClassifiersToRx, &actionTxInterval,
           &maxActionsToTx, &maxActionsToRx, &TxStrenThresh, &RxStrenThresh,
           &TxBidThresh, &RxBidThresh);
#else
      classifierSystem::getSettings(&seed, &geneticInterval, &maxCount, &netOn,
           &maxMsgBoardLength,
           &classListLength, &numConditions, &BBAenabled,  &BBAstyle,
           &elitismEnabled, &producerTaxInterval, &CDOenabled, &CEOenabled,
           &TCOenabled, &maxActionsToPost, &bidConstant, &sexualProb,
           &mutationProb, &startStrength, &headTax, &bidTax, &producerTax,
           &eliteThresh, &strengthCap, &bidCap);
#endif
      sprintf(buffer,"%d",geneticInterval);
      SendDlgItemMessage(hDlg,IDC_ENVgeneticsInterval,WM_SETTEXT,10,(LPARAM)buffer);
      sprintf(buffer,"%d",maxCount);
      SendDlgItemMessage(hDlg,IDC_ENVsimMaxIterations,WM_SETTEXT,10,(LPARAM)buffer);
      sprintf(buffer,"%d",seed);
      SendDlgItemMessage(hDlg,IDC_ENVsimSeed,WM_SETTEXT,10,(LPARAM)buffer);
      sprintf(buffer,"%d",classListLength);
      SendDlgItemMessage(hDlg,IDC_ENVnumClass,WM_SETTEXT,10,(LPARAM)buffer);
      sprintf(buffer,"%.3f",bidConstant);
      SendDlgItemMessage(hDlg,IDC_ENVclassBid,WM_SETTEXT,10,(LPARAM)buffer);
      sprintf(buffer,"%.2f",sexualProb);
      SendDlgItemMessage(hDlg,IDC_ENVgeneticsCrossProb,WM_SETTEXT,10,(LPARAM)buffer);
      sprintf(buffer,"%.2f",mutationProb);
      SendDlgItemMessage(hDlg,IDC_ENVgeneticsMutationProb,WM_SETTEXT,10,(LPARAM)buffer);
      SendDlgItemMessage(hDlg,IDC_ENVeliteEnabled,BM_SETCHECK,(WPARAM)elitismEnabled,0);
      sprintf(buffer,"%.2f",eliteThresh);
      SendDlgItemMessage(hDlg,IDC_ENVeliteThresh,WM_SETTEXT,10,(LPARAM)buffer);
      sprintf(buffer,"%.2f",startStrength);
      SendDlgItemMessage(hDlg,IDC_ENVclassStrength,WM_SETTEXT,10,(LPARAM)buffer);
      sprintf(buffer,"%.2f",strengthCap);
      SendDlgItemMessage(hDlg,IDC_ENVstrengthCap,WM_SETTEXT,10,(LPARAM)buffer);
      sprintf(buffer,"%.2f",bidCap);
      SendDlgItemMessage(hDlg,IDC_ENVbidCap,WM_SETTEXT,10,(LPARAM)buffer);
      sprintf(buffer,"%.2f",headTax);
      SendDlgItemMessage(hDlg,IDC_ENVheadTax,WM_SETTEXT,10,(LPARAM)buffer);
      SendDlgItemMessage(hDlg,IDC_ENVbbaEnabled,BM_SETCHECK,(WPARAM)BBAenabled,0);
      sprintf(buffer,"%d",maxActionsToPost);
      SendDlgItemMessage(hDlg,IDC_ENVmaxMsgBoard,WM_SETTEXT,10,(LPARAM)buffer);
      sprintf(buffer,"%d",numAgents);
      SendDlgItemMessage(hDlg,IDC_ENVnumAgents,WM_SETTEXT,10,(LPARAM)buffer);
      if (envType == task.STANDARD)
        SendDlgItemMessage(hDlg,IDC_ENVstandardObst,BM_SETCHECK,1,0);
      else
        SendDlgItemMessage(hDlg,IDC_ENVconcaveObst,BM_SETCHECK,1,0);
      SendDlgItemMessage(hDlg,IDC_ENVcdoOn,BM_SETCHECK,(WPARAM)CDOenabled,0);
      SendDlgItemMessage(hDlg,IDC_ENVceoOn,BM_SETCHECK,(WPARAM)CEOenabled,0);
      return TRUE;

    case WM_COMMAND:
      switch (wParam)
      {
        case IDOK:
        case IDC_ENVagentButton:
        case IDC_ENVnetworkButton:
          SendDlgItemMessage(hDlg,IDC_ENVgeneticsInterval,WM_GETTEXT,10,(LPARAM)buffer);
          geneticInterval = atoi(buffer);
          SendDlgItemMessage(hDlg,IDC_ENVsimMaxIterations,WM_GETTEXT,10,(LPARAM)buffer);
          maxCount = atoi(buffer);
          SendDlgItemMessage(hDlg,IDC_ENVsimSeed,WM_GETTEXT,10,(LPARAM)buffer);
          seed = atoi(buffer);
          SendDlgItemMessage(hDlg,IDC_ENVnumClass,WM_GETTEXT,10,(LPARAM)buffer);
          classListLength = atoi(buffer);
          SendDlgItemMessage(hDlg,IDC_ENVclassBid,WM_GETTEXT,10,(LPARAM)buffer);
          bidConstant = strtod(buffer,0);
          SendDlgItemMessage(hDlg,IDC_ENVgeneticsCrossProb,WM_GETTEXT,10,(LPARAM)buffer);
          sexualProb = strtod(buffer,0);
          SendDlgItemMessage(hDlg,IDC_ENVgeneticsMutationProb,WM_GETTEXT,10,(LPARAM)buffer);
          mutationProb = strtod(buffer,0);
          elitismEnabled = (int) SendDlgItemMessage(hDlg,IDC_ENVeliteEnabled,BM_GETCHECK,0,0);
          SendDlgItemMessage(hDlg,IDC_ENVeliteThresh,WM_GETTEXT,10,(LPARAM)buffer);
          eliteThresh = strtod(buffer,0);
          SendDlgItemMessage(hDlg,IDC_ENVclassStrength,WM_GETTEXT,10,(LPARAM)buffer);
          startStrength = strtod(buffer,0);
          SendDlgItemMessage(hDlg,IDC_ENVstrengthCap,WM_GETTEXT,10,(LPARAM)buffer);
          strengthCap = strtod(buffer,0);
          SendDlgItemMessage(hDlg,IDC_ENVbidCap,WM_GETTEXT,10,(LPARAM)buffer);
          bidCap = strtod(buffer,0);
          SendDlgItemMessage(hDlg,IDC_ENVheadTax,WM_GETTEXT,10,(LPARAM)buffer);
          headTax = strtod(buffer,0);
          BBAenabled = (int) SendDlgItemMessage(hDlg,IDC_ENVbbaEnabled,BM_GETCHECK,0,0);
          SendDlgItemMessage(hDlg,IDC_ENVmaxMsgBoard,WM_GETTEXT,10,(LPARAM)buffer);
          maxActionsToPost = atoi(buffer);
          SendDlgItemMessage(hDlg,IDC_ENVnumAgents,WM_GETTEXT,10,(LPARAM)buffer);
          numAgents = atoi(buffer);
          CDOenabled = (int) SendDlgItemMessage(hDlg,IDC_ENVcdoOn,BM_GETCHECK,0,0);
          CEOenabled = (int) SendDlgItemMessage(hDlg,IDC_ENVceoOn,BM_GETCHECK,0,0);
          if (SendDlgItemMessage(hDlg,IDC_ENVstandardObst,BM_GETCHECK,0,0))
            envType = task.STANDARD;
          else
            envType = task.CONCAVE_OBST;

          task.setNumAgents(numAgents);
          task.setEnvType(envType);
#ifdef NETWORK
          if (actionPassingEnabled)
            maxMsgBoardLength = maxActionsToPost + maxActionsToRx;
          else
            maxMsgBoardLength = maxActionsToPost;

          classifierSystem::setSettings(seed, geneticInterval, maxCount, netOn,
               maxMsgBoardLength,
               classListLength, numConditions, BBAenabled,  BBAstyle,
               elitismEnabled, producerTaxInterval, CDOenabled, CEOenabled,
               TCOenabled, maxActionsToPost, bidConstant, sexualProb,
               mutationProb, startStrength, headTax, bidTax, producerTax,
               eliteThresh, strengthCap, bidCap,
               actionPassingEnabled, classifierPassingEnabled, classifierTxInterval,
               maxClassifiersToTx, maxClassifiersToRx, actionTxInterval,
               maxActionsToTx, maxActionsToRx, TxStrenThresh, RxStrenThresh,
               TxBidThresh, RxBidThresh);
#else
          maxMsgBoardLength = maxActionsToPost;
          classifierSystem::setSettings(seed, geneticInterval, maxCount, netOn,
               maxMsgBoardLength,
               classListLength, numConditions, BBAenabled,  BBAstyle,
               elitismEnabled, producerTaxInterval, CDOenabled, CEOenabled,
               TCOenabled, maxActionsToPost, bidConstant, sexualProb,
               mutationProb, startStrength, headTax, bidTax, producerTax,
               eliteThresh, strengthCap, bidCap);
#endif
          resetRequest = 1;
          if (wParam == IDC_ENVagentButton) dialogBoxRequest = AGENT_SETTINGS;
          else if (wParam == IDC_ENVnetworkButton) dialogBoxRequest = NETWORK_SETTINGS;

          EndDialog(hDlg,TRUE);
          return TRUE;
        case IDCANCEL:
          EndDialog(hDlg,TRUE);
          return TRUE;
      }
      break;
  }

  return FALSE;
}

//============================================================================
//============================================================================
#pragma argsused
BOOL FAR PASCAL userInterface::agentSettings(HWND hDlg, WORD wMessage,
                                             WPARAM wParam, LPARAM lParam)
{
  char buffer[10];
  float goalRange, obstRange, sensorRange, timeConst, antWidth,
        goalReward, dirReward, obstReward, crashPenalty;

  switch (wMessage)
  {
    case WM_INITDIALOG:
      environmentInterface::getEnvSettings(&goalReward, &dirReward, &obstReward,
                                           &crashPenalty);
      environmentInterface::getAgentSettings(&goalRange, &obstRange,
                                             &sensorRange, &timeConst,
                                             &antWidth);
      sprintf(buffer,"%.1f",goalReward);
      SendDlgItemMessage(hDlg,IDC_ENVpayoffGoal,WM_SETTEXT,10,(LPARAM)buffer);
      sprintf(buffer,"%.1f",dirReward);
      SendDlgItemMessage(hDlg,IDC_ENVpayoffDirection,WM_SETTEXT,10,(LPARAM)buffer);
      sprintf(buffer,"%.1f",obstReward);
      SendDlgItemMessage(hDlg,IDC_ENVpayoffObst,WM_SETTEXT,10,(LPARAM)buffer);
      sprintf(buffer,"%.1f",crashPenalty);
      SendDlgItemMessage(hDlg,IDC_ENVpayoffCrash,WM_SETTEXT,10,(LPARAM)buffer);
      sprintf(buffer,"%.1f",timeConst);
      SendDlgItemMessage(hDlg,IDC_AGENTconstTime,WM_SETTEXT,10,(LPARAM)buffer);
      sprintf(buffer,"%.1f",antWidth);
      SendDlgItemMessage(hDlg,IDC_AGENTconstDiameter,WM_SETTEXT,10,(LPARAM)buffer);
      sprintf(buffer,"%.3f",sensorRange);
      SendDlgItemMessage(hDlg,IDC_AGENTrangeSensor,WM_SETTEXT,10,(LPARAM)buffer);
      sprintf(buffer,"%.1f",obstRange);
      SendDlgItemMessage(hDlg,IDC_AGENTrangeObst,WM_SETTEXT,10,(LPARAM)buffer);
      sprintf(buffer,"%.1f",goalRange);
      SendDlgItemMessage(hDlg,IDC_AGENTrangeGoal,WM_SETTEXT,10,(LPARAM)buffer);
      return TRUE;

    case WM_COMMAND:
      switch (wParam)
      {
        case IDOK:
        case IDC_AGENTsystemButton:
        case IDC_AGENTnetworkButton:
          SendDlgItemMessage(hDlg,IDC_ENVpayoffGoal,WM_GETTEXT,10,(LPARAM)buffer);
          goalReward = strtod(buffer,0);
          SendDlgItemMessage(hDlg,IDC_ENVpayoffDirection,WM_GETTEXT,10,(LPARAM)buffer);
          dirReward = strtod(buffer,0);
          SendDlgItemMessage(hDlg,IDC_ENVpayoffObst,WM_GETTEXT,10,(LPARAM)buffer);
          obstReward = strtod(buffer,0);
          SendDlgItemMessage(hDlg,IDC_ENVpayoffCrash,WM_GETTEXT,10,(LPARAM)buffer);
          crashPenalty = strtod(buffer,0);
          SendDlgItemMessage(hDlg,IDC_AGENTconstTime,WM_GETTEXT,10,(LPARAM)buffer);
          timeConst = strtod(buffer,0);
          SendDlgItemMessage(hDlg,IDC_AGENTconstDiameter,WM_GETTEXT,10,(LPARAM)buffer);
          antWidth = strtod(buffer,0);
          SendDlgItemMessage(hDlg,IDC_AGENTrangeSensor,WM_GETTEXT,10,(LPARAM)buffer);
          sensorRange = strtod(buffer,0);
          SendDlgItemMessage(hDlg,IDC_AGENTrangeObst,WM_GETTEXT,10,(LPARAM)buffer);
          obstRange = strtod(buffer,0);
          SendDlgItemMessage(hDlg,IDC_AGENTrangeGoal,WM_GETTEXT,10,(LPARAM)buffer);
          goalRange = strtod(buffer,0);
          environmentInterface::setEnvSettings(goalReward,dirReward,obstReward,
                                               crashPenalty);
          environmentInterface::setAgentSettings(goalRange, obstRange,
                                                 sensorRange, timeConst,
                                                 antWidth);
          resetRequest = 1;

          if (wParam == IDC_AGENTsystemButton) dialogBoxRequest = ENV_SETTINGS;
          else if (wParam == IDC_AGENTnetworkButton) dialogBoxRequest = NETWORK_SETTINGS;

          EndDialog(hDlg,TRUE);
          return TRUE;
        case IDCANCEL:
          EndDialog(hDlg,TRUE);
          return TRUE;
      }
      break;
  }

  return FALSE;
}

//============================================================================
//============================================================================
#ifdef NETWORK
#pragma argsused
BOOL FAR PASCAL userInterface::networkSettings(HWND hDlg, WORD wMessage,
                                               WPARAM wParam, LPARAM lParam)
{
  char buffer[10];
  static unsigned seed;
  static int geneticInterval, maxCount,
             maxMsgBoardLength,
             classListLength, numConditions, BBAenabled,  BBAstyle,
             elitismEnabled, producerTaxInterval, CDOenabled, CEOenabled,
             TCOenabled, maxActionsToPost,
             actionPassingEnabled, classifierPassingEnabled,
             classifierTxInterval, maxClassifiersToTx, maxClassifiersToRx,
             actionTxInterval, maxActionsToTx, maxActionsToRx, netOn;
  static double bidConstant, sexualProb, mutationProb, startStrength,
                headTax, bidTax, producerTax, eliteThresh, strengthCap, bidCap,
                TxStrenThresh, RxStrenThresh, TxBidThresh, RxBidThresh;

  switch (wMessage)
  {
    case WM_INITDIALOG:
      classifierSystem::getSettings(&seed, &geneticInterval, &maxCount, &netOn,
           &maxMsgBoardLength,
           &classListLength, &numConditions, &BBAenabled,  &BBAstyle,
           &elitismEnabled, &producerTaxInterval, &CDOenabled, &CEOenabled,
           &TCOenabled, &maxActionsToPost, &bidConstant, &sexualProb,
           &mutationProb, &startStrength, &headTax, &bidTax, &producerTax,
           &eliteThresh, &strengthCap, &bidCap,
           &actionPassingEnabled, &classifierPassingEnabled, &classifierTxInterval,
           &maxClassifiersToTx, &maxClassifiersToRx, &actionTxInterval,
           &maxActionsToTx, &maxActionsToRx, &TxStrenThresh, &RxStrenThresh,
           &TxBidThresh, &RxBidThresh);

      SendDlgItemMessage(hDlg,IDC_NETWORKenabled,BM_SETCHECK,(WPARAM)netOn,0);
      SendDlgItemMessage(hDlg,IDC_NETWORKactionEnabled,BM_SETCHECK,(WPARAM)actionPassingEnabled,0);
      SendDlgItemMessage(hDlg,IDC_NETWORKclassEnabled,BM_SETCHECK,(WPARAM)classifierPassingEnabled,0);

      sprintf(buffer,"%d",classifierTxInterval);
      SendDlgItemMessage(hDlg,IDC_NETWORKclassTXinterval,WM_SETTEXT,10,(LPARAM)buffer);
      sprintf(buffer,"%d",maxClassifiersToTx);
      SendDlgItemMessage(hDlg,IDC_NETWORKclassMaxToTX,WM_SETTEXT,10,(LPARAM)buffer);
      sprintf(buffer,"%.2f",TxStrenThresh);
      SendDlgItemMessage(hDlg,IDC_NETWORKclassTXstrenThresh,WM_SETTEXT,10,(LPARAM)buffer);
      sprintf(buffer,"%d",maxClassifiersToRx);
      SendDlgItemMessage(hDlg,IDC_NETWORKclassMaxToRX,WM_SETTEXT,10,(LPARAM)buffer);
      sprintf(buffer,"%.2f",RxStrenThresh);
      SendDlgItemMessage(hDlg,IDC_NETWORKclassRXstrenThresh,WM_SETTEXT,10,(LPARAM)buffer);

      sprintf(buffer,"%d",actionTxInterval);
      SendDlgItemMessage(hDlg,IDC_NETWORKactionTXint,WM_SETTEXT,10,(LPARAM)buffer);
      sprintf(buffer,"%d",maxActionsToTx);
      SendDlgItemMessage(hDlg,IDC_NETWORKactionMaxToTX,WM_SETTEXT,10,(LPARAM)buffer);
      sprintf(buffer,"%.2f",TxBidThresh);
      SendDlgItemMessage(hDlg,IDC_NETWORKactionTXbidThresh,WM_SETTEXT,10,(LPARAM)buffer);
      sprintf(buffer,"%d",maxActionsToRx);
      SendDlgItemMessage(hDlg,IDC_NETWORKactionMaxToRX,WM_SETTEXT,10,(LPARAM)buffer);
      sprintf(buffer,"%.2f",RxBidThresh);
      SendDlgItemMessage(hDlg,IDC_NETWORKactionRXbidThresh,WM_SETTEXT,10,(LPARAM)buffer);
      return TRUE;

    case WM_COMMAND:
      switch (wParam)
      {
        case IDOK:
        case IDC_NETWORKagentButton:
        case IDC_NETWORKsystemButton:
          netOn = (int) SendDlgItemMessage(hDlg,IDC_NETWORKenabled,BM_GETCHECK,0,0);
          actionPassingEnabled = (int) SendDlgItemMessage(hDlg,IDC_NETWORKactionEnabled,BM_GETCHECK,0,0);
          classifierPassingEnabled = (int) SendDlgItemMessage(hDlg,IDC_NETWORKclassEnabled,BM_GETCHECK,0,0);

          SendDlgItemMessage(hDlg,IDC_NETWORKclassTXinterval,WM_GETTEXT,10,(LPARAM)buffer);
          classifierTxInterval = atoi(buffer);
          SendDlgItemMessage(hDlg,IDC_NETWORKclassMaxToTX,WM_GETTEXT,10,(LPARAM)buffer);
          maxClassifiersToTx = atoi(buffer);
          SendDlgItemMessage(hDlg,IDC_NETWORKclassTXstrenThresh,WM_GETTEXT,10,(LPARAM)buffer);
          TxStrenThresh = strtod(buffer,0);
          SendDlgItemMessage(hDlg,IDC_NETWORKclassMaxToRX,WM_GETTEXT,10,(LPARAM)buffer);
          maxClassifiersToRx = atoi(buffer);
          SendDlgItemMessage(hDlg,IDC_NETWORKclassRXstrenThresh,WM_GETTEXT,10,(LPARAM)buffer);
          RxStrenThresh = strtod(buffer,0);

          SendDlgItemMessage(hDlg,IDC_NETWORKactionTXint,WM_GETTEXT,10,(LPARAM)buffer);
          actionTxInterval = atoi(buffer);
          SendDlgItemMessage(hDlg,IDC_NETWORKactionMaxToTX,WM_GETTEXT,10,(LPARAM)buffer);
          maxActionsToTx = atoi(buffer);
          SendDlgItemMessage(hDlg,IDC_NETWORKactionTXbidThresh,WM_GETTEXT,10,(LPARAM)buffer);
          TxBidThresh = strtod(buffer,0);
          SendDlgItemMessage(hDlg,IDC_NETWORKactionMaxToRX,WM_GETTEXT,10,(LPARAM)buffer);
          maxActionsToRx = atoi(buffer);
          SendDlgItemMessage(hDlg,IDC_NETWORKactionRXbidThresh,WM_GETTEXT,10,(LPARAM)buffer);
          RxBidThresh = strtod(buffer,0);

          if (actionPassingEnabled)
            maxMsgBoardLength = maxActionsToPost + maxActionsToRx;
          else
            maxMsgBoardLength = maxActionsToPost;
          classifierSystem::setSettings(seed, geneticInterval, maxCount, netOn,
               maxMsgBoardLength,
               classListLength, numConditions, BBAenabled,  BBAstyle,
               elitismEnabled, producerTaxInterval, CDOenabled, CEOenabled,
               TCOenabled, maxActionsToPost, bidConstant, sexualProb,
               mutationProb, startStrength, headTax, bidTax, producerTax,
               eliteThresh, strengthCap, bidCap,
               actionPassingEnabled, classifierPassingEnabled, classifierTxInterval,
               maxClassifiersToTx, maxClassifiersToRx, actionTxInterval,
               maxActionsToTx, maxActionsToRx, TxStrenThresh, RxStrenThresh,
               TxBidThresh, RxBidThresh);

          resetRequest = 1;

          if (wParam == IDC_NETWORKagentButton) dialogBoxRequest = AGENT_SETTINGS;
          else if (wParam == IDC_NETWORKsystemButton) dialogBoxRequest = ENV_SETTINGS;

          EndDialog(hDlg,TRUE);
          return TRUE;
        case IDCANCEL:
          EndDialog(hDlg,TRUE);
          return TRUE;
      }
      break;
  }

  return FALSE;
}
#endif

//============================================================================
//============================================================================
#pragma argsused
BOOL FAR PASCAL userInterface::speedBar(HWND hDlg, WORD wMessage, WPARAM wParam,
                                        LPARAM lParam)
{
  switch(wMessage)
  {
    case WM_INITDIALOG:
      return TRUE;

    case WM_COMMAND:
      switch(wParam)
      {
        case IDC_SPEEDBARrun:      runStartRequest = 1;  return 1;
        case IDC_SPEEDBARstep:     stepRequest = 1; return 1;
        case IDC_SPEEDBARstop:     stopRequest = 1; return 1;
        case IDC_SPEEDBARreset:    resetRequest = 1; return 1;
        case IDC_SPEEDBARbatch:    batchRequest = 1; return 1;
        case IDC_SPEEDBARagent1:   newAgentDisplayRequest = 1; return 1;
        case IDC_SPEEDBARagent2:   newAgentDisplayRequest = 2; return 1;
        case IDC_SPEEDBARagent3:   newAgentDisplayRequest = 3; return 1;
        case IDC_SPEEDBARagent4:   newAgentDisplayRequest = 4; return 1;
        case IDC_SPEEDBARagent5:   newAgentDisplayRequest = 5; return 1;
        case IDC_SPEEDBARagent6:   newAgentDisplayRequest = 6; return 1;
        case IDC_SPEEDBARagent7:   newAgentDisplayRequest = 7; return 1;
        case IDC_SPEEDBARagent8:   newAgentDisplayRequest = 8; return 1;
      }
      break;
  }

  return FALSE;
}

//============================================================================
//============================================================================
/*  This function recalculates all of the screen boxes rectangles and moves the
    toolbar when called.  The function is usually called only when a repaint
    of the entire client area is requested.  (e.g. window initialization or
    resizing or axes flag change).

    The XXBox variables contain the coordinates of the actual boxes drawn on the
    screen.  The XXRect variables contain the coordinates of the update
    regions for boxes (i.e. the interior of the boxes.)  These coordinates
    are used for clipping and for signalling a region update.

*/

// Field rectangle location & axis labeling spacing.  cy and cx are the y and
// x dimensions of the current font.
//
// Top:     space
//          cy
// Bottom:  tic  + space + label + space + title + space
//          cy/2 + cy/2  + cy    + cy/2  + cy    + cy/2
// Left:    tic  + space + largest label      + space + title + space
//          cy/2 + cy/2  + max(length(label)) + cy/2  + cx    + cy/2
// Right:   space
//          cy

void userInterface::calcDimensions(int firstCallHere)
{
  // Update the axis dimensions
  task.getAxesRanges(&x1Copy,&x2Copy,&y1Copy,&y2Copy);

  // Get general info about the agents
  float dum1,dum2,dum3,dum4,agentWidth;
  environmentInterface::getAgentSettings(&dum1,&dum2,&dum3,&dum4,&agentWidth);

  // If this is not the first call to this function, destroy the previous
  // region handles.
  if (!firstCallHere)
    deleteRegionObjects();

  // Temporary rectangles representing the clip regions to be registered.
  RECT envRect, fieldRect, msgBoardRect, classListRect, sensorsRect,
       statusRect, classListScrollRect, msgBoardScrollRect;

  // Get the DC
  HDC hDC = GetDC(hWnd);

  // Get the client area dimensions
  RECT clientAreaRect;
  GetClientRect(hWnd,&clientAreaRect);

  // Determine the height of the current font and base the spacing between
  // rectangles and the border on that height.
  SIZE sz;
  G.getTextSize(hDC,"X",&sz);
  int space = int(1.5*float(sz.cy)),  border = sz.cy / 2;

  // Determine the height of the speed bar and move bar.
  RECT speedBarRect;
  GetWindowRect(hSpeedBar,&speedBarRect);
  int speedBarSpace = speedBarRect.bottom - speedBarRect.top;
  SetWindowPos(hSpeedBar,HWND_TOP,0,0,clientAreaRect.right, speedBarSpace,
               SWP_NOMOVE | SWP_NOREDRAW);

  hSpeedBarRegion = CreateRectRgn(0,0,clientAreaRect.right,speedBarSpace);

  // Width is the non-whitepsace area which will be divided between the
  // environment and the message board.  Height is the non-whitespace area that
  // will be divided between the environment and the status window.  Env size
  // is the square dimension of the environment.
  int width = clientAreaRect.right - space - 2*border,
      height = clientAreaRect.bottom - space - border - speedBarSpace,
      envSize;

  if (width<height) envSize = int(float(width)*.75);
  else envSize = int(float(height)*.75);

  // Determine the environment rectangle coordinates.
  envBox.left = border;
  envBox.top = speedBarSpace + space;
  envBox.right = envBox.left + envSize;
  envBox.bottom = envBox.top + envSize;

  envRect = envBox;
  InflateRect(&envRect,-1,-1);

  hEnvRegion = CreateRectRgn(envRect.left, envRect.top, envRect.right,
                             envRect.bottom);
  hEnvBox = CreateRectRgn(envBox.left, envBox.top-space, envBox.right,
                          envBox.bottom);

  // Determine the field rectangle dimensions.
  if (axesOn)
  {
    char temp1[20], temp2[20];
    sprintf(temp1,"%d",y1Copy);  sprintf(temp2,"%d",y2Copy);
    if (strlen(temp1) > strlen(temp2))
      G.getTextSize(hDC,temp1,&sz);
    else
      G.getTextSize(hDC,temp2,&sz);

    fieldBox.left = envBox.left + sz.cy*2 + sz.cx;
    G.getTextSize(hDC,"Y",&sz);
    fieldBox.left += sz.cx;
    fieldBox.right = envBox.right - sz.cy;
    fieldBox.top = envBox.top + sz.cy;
    fieldBox.bottom = envBox.bottom - sz.cy*4;
  }
  else
    fieldBox = envBox;

  fieldRect = fieldBox;
  InflateRect(&fieldRect,-1,-1);

  hFieldRegion = CreateRectRgn(fieldRect.left,fieldRect.top,fieldRect.right,
                               fieldRect.bottom);

  // Set plotting constants
  dx = (float) (fieldBox.right - fieldBox.left) / (float) (x2Copy - x1Copy);
  dy = (float) (fieldBox.bottom - fieldBox.top) / (float) (y2Copy - y1Copy);

  // Determine the width of the ant, goal, and obstacles based on the
  // environment dimensions.
  if (objectsToScale)
    antWidth = goalWidth = obstWidth = agentWidth*dx;
  else
    antWidth = goalWidth = obstWidth = (fieldBox.right-fieldBox.left) / 40;

  // Determine the message board rectangle coordinates.
  msgBoardBox.left = envBox.right + space;
  msgBoardBox.top = envBox.top;
  msgBoardBox.right = clientAreaRect.right - border;
  msgBoardBox.bottom = msgBoardBox.top + int(float(envSize-2*space)*.40);

  msgBoardRect = msgBoardBox;
  InflateRect(&msgBoardRect,-1,-1);

  HFONT hOldFont = (HFONT) SelectObject(hDC,G.getFont(G.FIXED));
  G.getTextSize(hDC,"X",&sz);
  fixedTextHeight = sz.cy;
  fixedTextWidth = sz.cx;
  SelectObject(hDC,hOldFont);

  maxMsgBoardLength = (msgBoardRect.bottom - msgBoardRect.top) / fixedTextHeight;
  normalizeIndices(&msgBoardStart, &msgBoardEnd, messageBoard::size(),
                   maxMsgBoardLength);

  if (messageBoard::size() > maxMsgBoardLength)
  {
    msgBoardScrollOn = 1;
    SetScrollRange(hMsgBoardScrollBar,SB_CTL,0,
                   messageBoard::size() - maxMsgBoardLength, FALSE);
    msgBoardRect.right -= scrollBarWidth+1;

    msgBoardScrollRect.left = msgBoardBox.right-scrollBarWidth-1;
    msgBoardScrollRect.top = msgBoardBox.top+1;
    msgBoardScrollRect.right = msgBoardScrollRect.left + scrollBarWidth;
    msgBoardScrollRect.bottom = msgBoardScrollRect.top +
                                (msgBoardBox.bottom-msgBoardBox.top-2);

    SetWindowPos(hMsgBoardScrollBar, HWND_TOP,
                 msgBoardScrollRect.left, msgBoardScrollRect.top,
                 scrollBarWidth, (msgBoardBox.bottom-msgBoardBox.top-2),
                 SWP_NOREDRAW);

    hMsgBoardScrollRegion = CreateRectRgn(msgBoardScrollRect.left,
                                          msgBoardScrollRect.top,
                                          msgBoardScrollRect.right,
                                          msgBoardScrollRect.bottom);
  }
  else
    msgBoardScrollOn = 0;

  hMsgBoardRegion = CreateRectRgn(msgBoardRect.left, msgBoardRect.top,
                                  msgBoardRect.right, msgBoardRect.bottom);
  hMsgBoardBox = CreateRectRgn(msgBoardBox.left, msgBoardBox.top-space,
                               msgBoardBox.right, msgBoardBox.bottom);

  // Determine the sensor rectangle coordinates.
  sensorsBox.left = msgBoardBox.left;
  sensorsBox.top = msgBoardBox.bottom + space;
  sensorsBox.right = msgBoardBox.right;
  sensorsBox.bottom = sensorsBox.top + int(float(envSize-2*space)*.10);

  sensorsRect = sensorsBox;
  InflateRect(&sensorsRect,-1,-1);
  hSensorsRegion = CreateRectRgn(sensorsRect.left, sensorsRect.top,
                                 sensorsRect.right, sensorsRect.bottom);
  hSensorsBox = CreateRectRgn(sensorsBox.left, msgBoardBox.bottom+1,
                              sensorsBox.right, sensorsBox.bottom);

  // Determine the classifier list rectangle coordinates.
  classListBox.left = msgBoardBox.left;
  classListBox.top = sensorsBox.bottom + space;
  classListBox.right = msgBoardBox.right;
  classListBox.bottom = envBox.bottom;

  classListRect = classListBox;
  InflateRect(&classListRect,-1,-1);

  maxClassListLength = (classListRect.bottom - classListRect.top) / fixedTextHeight;
  normalizeIndices(&classListStart, &classListEnd, classifierList::size(),
                   maxClassListLength);

  if (classifierList::size() > maxClassListLength)
  {
    classListScrollOn = 1;
    SetScrollRange(hClassListScrollBar,SB_CTL, 0,
                   classifierList::size() - maxClassListLength, FALSE);
    classListRect.right -= scrollBarWidth+1;

    classListScrollRect.left = classListBox.right-scrollBarWidth-1;
    classListScrollRect.top = classListBox.top+1;
    classListScrollRect.right = classListScrollRect.left + scrollBarWidth;
    classListScrollRect.bottom = classListScrollRect.top +
                                 (classListBox.bottom-classListBox.top-2);

    SetWindowPos(hClassListScrollBar, HWND_TOP, classListScrollRect.left,
                 classListScrollRect.top,
                 scrollBarWidth, (classListBox.bottom-classListBox.top-2),
                 SWP_NOREDRAW);

    hClassListScrollRegion = CreateRectRgn(classListScrollRect.left,
                                           classListScrollRect.top,
                                           classListScrollRect.right,
                                           classListScrollRect.bottom);
  }
  else classListScrollOn = 0;

  hClassListRegion = CreateRectRgn(classListRect.left, classListRect.top,
                                   classListRect.right, classListRect.bottom);
  hClassListBox = CreateRectRgn(classListBox.left, sensorsBox.bottom+1,
                                classListBox.right, classListBox.bottom);

  // Determine the status rectangle coordinates.
  statusBox.left = envBox.left;
  statusBox.top = envBox.bottom + space;
  statusBox.right = msgBoardBox.right;
  statusBox.bottom = clientAreaRect.bottom - border;

  statusRect = statusBox;
  InflateRect(&statusRect,-1,-1);

  hStatusRegion = CreateRectRgn(statusRect.left, statusRect.top,
                                statusRect.right, statusRect.bottom);

  hStatusBox = CreateRectRgn(statusBox.left, envBox.bottom+1, statusBox.right,
                             statusBox.bottom);

  maxStatusLength = (statusRect.bottom - statusRect.top) / fixedTextHeight;

  // Enable a speedbar button for each agent

  int numAgents = task.getNumAgents();
  for (int i=0; i<MAX_AGENTS; i++)
  {
    HWND handle = GetDlgItem(hSpeedBar,IDC_SPEEDBARagent1+i);
    if (i<numAgents) ShowWindow(handle,SW_SHOW);
    else ShowWindow(handle,SW_HIDE);
  }

  // Return the DC
  ReleaseDC(hWnd,hDC);
}

//============================================================================
//============================================================================
// Normalizes *start and *end to fit within max given the current value of
// *start.  len is the maximum index.

void userInterface::normalizeIndices(int *start, int *end, int len, int max)
{
  if (len <= max)
  {
    *start = 0;
    *end = len-1;
  }
  else
  {
    if (*start < 0) *start = 0;
    *end = *start + max-1;
    if (*end >= len) { *end = len-1;  *start = *end - max+1; }
  }
}

//============================================================================
//============================================================================

void userInterface::repaintWindow(int updateAgent, int updateEnv,
                                  int updateInfo, int updateStatusOnly,
                                  int updateBoxes)
{
  int directCall = 0;

  // Determine if this is a direct call (i.e. WM_PAINT message bypassed)
  if (updateAgent || updateInfo || updateEnv || updateStatusOnly ||
      updateBoxes)
    directCall = 1;

  // Get the DC
  PAINTSTRUCT ps;
  HDC hDC;
  if (directCall) hDC = GetDC(hWnd);
  else hDC = BeginPaint(hWnd,&ps);

  // Make the brush null so that no erasing is done in the rectangles which
  // are drawn.  All erasing is done explicitly.
  HBRUSH hOldBrush = (HBRUSH) SelectObject(hDC,GetStockObject(NULL_BRUSH));

  // These update items are never updated on a direct call
  if (!directCall)
  {
    RECT tempRect;

    // Determine client area dimensions
    RECT rc;
    GetClientRect(hWnd,&rc);

    // Get speedBar dimensions.
    RECT speedBarRect;
    GetClientRect(hSpeedBar,&speedBarRect);

    // Turn on the speedbar
    SelectClipRgn(hDC,hSpeedBarRegion);
    if (RectVisible(hDC,&ps.rcPaint))
      ShowWindow(hSpeedBar,SW_SHOW);

    // Message board box
    SelectClipRgn(hDC,hMsgBoardBox);
    if (RectVisible(hDC,&ps.rcPaint))
    {
      SetRect(&tempRect,msgBoardBox.left,speedBarRect.bottom,msgBoardBox.right,
              msgBoardBox.top);
      G.centerText(hDC,tempRect,"Message Board");
      G.drawRectangle(hDC, msgBoardBox, antColor[agent]);
    }

    // Message board scroll bar
    if (msgBoardScrollOn)
    {
      SelectClipRgn(hDC,hMsgBoardScrollRegion);
      if (RectVisible(hDC,&ps.rcPaint))
      {
        SetScrollPos(hMsgBoardScrollBar,SB_CTL,msgBoardStart,TRUE);
        ShowWindow(hMsgBoardScrollBar,SW_SHOW);
      }
    }

    // Sensor box
    SelectClipRgn(hDC,hSensorsBox);
    if (RectVisible(hDC,&ps.rcPaint))
    {
      SetRect(&tempRect,sensorsBox.left,msgBoardBox.bottom,sensorsBox.right,
              sensorsBox.top);
      G.centerText(hDC,tempRect,"Sensors");
      G.drawRectangle(hDC, sensorsBox, antColor[agent]);
    }

    // Classifier List box
    SelectClipRgn(hDC,hClassListBox);
    if (RectVisible(hDC,&ps.rcPaint))
    {
      SetRect(&tempRect,classListBox.left, sensorsBox.bottom, classListBox.right,
              classListBox.top);
      G.centerText(hDC,tempRect,"Classifier List");
      G.drawRectangle(hDC, classListBox, antColor[agent]);
    }

    // Classifier list scroll bar
    if (classListScrollOn)
    {
      SelectClipRgn(hDC,hClassListScrollRegion);
      if (RectVisible(hDC, &ps.rcPaint))
      {
        SetScrollPos(hClassListScrollBar,SB_CTL,classListStart,TRUE);
        ShowWindow(hClassListScrollBar,SW_SHOW);
      }
    }

    // Status box
    SelectClipRgn(hDC,hStatusBox);
    if (RectVisible(hDC,&ps.rcPaint))
    {
      SetRect(&tempRect,statusBox.left, envBox.bottom, statusBox.right,
              statusBox.top);
      G.centerText(hDC,tempRect,"Status");
      G.drawRectangle(hDC, statusBox, antColor[agent]);
    }

    // Environment Box
    SelectClipRgn(hDC,hEnvBox);
    if (RectVisible(hDC,&ps.rcPaint))
    {
      SetRect(&tempRect,envBox.left,speedBarRect.bottom,envBox.right,envBox.top);
      G.centerText(hDC,tempRect,"Agent Environment");
      G.drawRectangle(hDC,envBox);
    }
  } // end if(!directCall)

  // Used for multiple-agent display.  Box color matches agent color
  if (updateBoxes)
  {
    G.drawRectangle(hDC, msgBoardBox, antColor[agent]);
    G.drawRectangle(hDC, sensorsBox, antColor[agent]);
    G.drawRectangle(hDC, classListBox, antColor[agent]);
    G.drawRectangle(hDC, statusBox, antColor[agent]);
  }

  // Environment object variables
  coord huge *antPts;
  coord *obstPts, goalPt;
  int antCount, numObst,
      numAgents = task.getNumAgents();
  float huge *antAngles;
  float goalDist, goalAngle, obstDist, obstAngle;

  // Repaint all or part of the environment, field, and objects
  SelectClipRgn(hDC,hEnvRegion);
  if (directCall || RectVisible(hDC,&ps.rcPaint))
  {
    if (updateAgent)  // only done on direct call
    {
      SelectClipRgn(hDC,hFieldRegion);

      for (int j=0; j<numAgents; j++)
      {
        if (task.agentDone(j)) continue;
        task.getAgent(j, &antPts, &antAngles, &antCount, &goalDist, &goalAngle,
                      &obstDist, &obstAngle);

        for (int i=lastAgentIndexDisp; i<antCount; i++)
          drawAnt(hDC,antPts[i],antAngles[i],antColor[j]);
      }

      lastAgentIndexDisp = antCount-1;
    }

    if (updateEnv || !directCall)
    {
      if (updateEnv) G.fillRectangle(hDC,envBox,G.WHITE);

      if (axesOn)
      {
        G.drawRectangle(hDC,fieldBox);
        labelAxes(hDC,x1Copy,x2Copy,5,"X" ,y1Copy,y2Copy,5,"Y");
      }

      SelectClipRgn(hDC,hFieldRegion);
      task.getObjects(&obstPts,&numObst,&goalPt);
      drawObstacles(hDC,obstPts,numObst);
      drawGoal(hDC,goalPt);
      for (int j=0; j<numAgents; j++)
      {
        task.getAgent(j, &antPts, &antAngles, &antCount, &goalDist, &goalAngle,
                      &obstDist, &obstAngle);

        for (int i=0; i<antCount; i++)
          drawAnt(hDC,antPts[i],antAngles[i],antColor[j]);
      }
      lastAgentIndexDisp = antCount-1;
    }
  }

  // Update the message board listing and scroll bar

  SelectClipRgn(hDC,hMsgBoardRegion);
  if (updateInfo || (!directCall && RectVisible(hDC,&ps.rcPaint)))
    displayMessageBoard(hDC);

  // Update the sensor box

  SelectClipRgn(hDC,hSensorsRegion);
  if (updateInfo || (!directCall && RectVisible(hDC,&ps.rcPaint)))
    displaySensors(hDC);

  // Update the classifier list and scroll bar

  SelectClipRgn(hDC,hClassListRegion);
  if (updateInfo || (!directCall && RectVisible(hDC, &ps.rcPaint)))
    displayClassifierList(hDC);

  // Always update the status box listing on a direct call or when it is
  // visible.
  SelectClipRgn(hDC,hStatusRegion);
  if (directCall || RectVisible(hDC, &ps.rcPaint)) displayStatus(hDC);

  // Restore the old brush
  SelectObject(hDC,hOldBrush);

  // Return the DC
  if (directCall) ReleaseDC(hWnd,hDC);
  else EndPaint(hWnd,&ps);
}

//============================================================================
//============================================================================

void userInterface::createMetaFile()
{

// CreateEnhMetaFile
HDC hdcMeta;

hdcMeta = CreateMetaFile("test.wmf");
SetMapMode(hdcMeta,MM_ANISOTROPIC);
SetWindowOrg(hdcMeta,0,0);
MoveTo(hdcMeta, 10, 10);
LineTo(hdcMeta, 100, 100);
CloseMetaFile(hdcMeta);


/*
HMETAFILE hmf = GetEnhMetaFile("dg.wmf");
HDC hDC = GetDC(hWnd);
RECT rc;
SetRect(&rc,0,0,640,480);
PlayEnhMetaFile(hDC, hmf, &rc);
//DeleteMetaFile(hmf);
ReleaseDC(hWnd,hDC);
*/
/*
HDC hdcMeta;
HMETAFILE hmf;
void FAR *metaPtr;

HGLOBAL hMeta = GlobalAlloc(GMEM_SHARE,sizeof(METAFILEPICT));
metaPtr = GlobalLock(hMeta);

hdcMeta = CreateMetaFile(NULL);
SetMapMode(hdcMeta,MM_ANISOTROPIC);
SetWindowOrg(hdcMeta,0,0);
MoveTo(hdcMeta, 10, 10);
LineTo(hdcMeta, 400, 400);
hmf = CloseMetaFile(hdcMeta);

((METAFILEPICT *) metaPtr)->mm = MM_ANISOTROPIC;
((METAFILEPICT *) metaPtr)->xExt = 0;
((METAFILEPICT *) metaPtr)->yExt = 0;
((METAFILEPICT *) metaPtr)->hMF = hmf;

OpenClipboard(hWnd);
EmptyClipboard();
SetClipboardData(CF_METAFILEPICT,hMeta);
CloseClipboard();
//HDC hDC = GetDC(hWnd);
//PlayMetaFile(hDC, hmf);
//DeleteMetaFile(hmf);
//ReleaseDC(hWnd,hDC);

*/
/*
  HDC hDC = CreateMetaFile("test.wmf");

  // Make the brush null so that no erasing is done in the rectangles which
  // are drawn.  All erasing is done explicitly.
  HBRUSH hOldBrush = (HBRUSH) SelectObject(hDC,GetStockObject(NULL_BRUSH));

  // Environment Box
  RECT tempRect;
  SetRect(&tempRect,envBox.left,0,envBox.right,envBox.top);
  G.centerText(hDC,tempRect,"Agent Environment");
  G.drawRectangle(hDC,envBox);

  // Environment object variables
  coord huge *antPts;
  coord *obstPts, goalPt;
  int antCount, numObst,
      numAgents = task.getNumAgents();
  float huge *antAngles;
  float goalDist, goalAngle, obstDist, obstAngle;

  // Repaint all or part of the environment, field, and objects
  if (axesOn)
  {
    G.drawRectangle(hDC,fieldBox);
    labelAxes(hDC,x1Copy,x2Copy,5,"X" ,y1Copy,y2Copy,5,"Y");
  }

  task.getObjects(&obstPts,&numObst,&goalPt);
  drawObstacles(hDC,obstPts,numObst);
  drawGoal(hDC,goalPt);
  for (int j=0; j<numAgents; j++)
  {
    task.getAgent(j, &antPts, &antAngles, &antCount, &goalDist, &goalAngle,
                  &obstDist, &obstAngle);

    for (int i=0; i<antCount; i++)
      drawAnt(hDC,antPts[i],antAngles[i],antColor[j]);
  }

  // Restore the old brush
  SelectObject(hDC,hOldBrush);

  // Close the meta file
  CloseMetaFile(hDC);*/
}

//============================================================================
//============================================================================
void userInterface::labelAxes(HDC hDC, int x1, int x2, int numXTics, char *xLabel,
                              int y1, int y2, int numYTics, char *yLabel)
{
 char label[20];
 int numTics,x,y;
 SIZE sz;

 numTics = numXTics - 1;

 for (int i = x1; i<=x2; i+=(x2-x1)/numTics)
 {
   sprintf(label,"%d",i);
   G.getTextSize(hDC,label,&sz);

   if (i==x2)
     x = fieldBox.right-1;
   else
     x = xCoord(i);

// Bottom:  tic  + space + label + space + title + space
//          cy/2 + cy/2  + cy    + cy/2  + cy    + cy/2
   G.drawLine(hDC, x, fieldBox.bottom, x, fieldBox.bottom+sz.cy/2 );
   if (i==x2) G.printText(hDC, x - sz.cx, fieldBox.bottom + sz.cy, label );
   else G.printText(hDC, x - sz.cx/2, fieldBox.bottom + sz.cy, label );
 }

 RECT tempRect;
 SetRect(&tempRect,fieldBox.left,fieldBox.bottom+sz.cy*5/2,
         fieldBox.right,fieldBox.bottom+sz.cy*7/2);
 G.centerText(hDC,tempRect,xLabel);

 numTics = numYTics - 1;

 for (i = y1; i <= y2; i+=(y2-y1)/numTics)
  {
   sprintf(label,"%d ",i);
   G.getTextSize(hDC,label,&sz);

   if (i==y2)
     y = fieldBox.top;
   else
     y = yCoord(i);

// Left:    tic  + space + largest label      + space + title + space
//          cy/2 + cy/2  + max(length(label)) + cy/2  + cx    + cy/2
   G.drawLine(hDC, fieldBox.left-sz.cy/2, y, fieldBox.left, y );
   G.printText(hDC, fieldBox.left - sz.cx - sz.cy, y - sz.cy/2, label );
  }

  G.getTextSize(hDC,yLabel,&sz);
  SetRect(&tempRect,envBox.left + sz.cy/2, fieldBox.top,
          envBox.left + sz.cy/2 + sz.cx, fieldBox.bottom);
  G.centerText(hDC,tempRect,yLabel);
}

//============================================================================
//============================================================================
// Draws an ant at the given point and angle.  In addition, a line is drawn
// indicating which direction the ant is "looking."
void userInterface::drawAnt(HDC hDC, coord antPt, float antAngle, int color)
{
 int     x1 = xCoord(antPt.getX()),
         y1 = yCoord(antPt.getY());
 coord pt2 = antPt + float(antWidth/2)/dx * coord(cos(antAngle),sin(antAngle));
// complex pt2 = antPt + float(antWidth/2)/dx * exp( complex(0,1) * antAngle );
 int     x2 = xCoord(pt2.getX()),
         y2 = yCoord(pt2.getY());

 G.fillCircle(hDC,x1,y1,antWidth/2,color);
 G.drawLine(hDC,x1,y1,x2,y2);
}

//============================================================================
//============================================================================
// Draws obstacles at the given points.  Note that the line color is changed
// to the obstacle color since a black outline would overwrite adjacent obstacle
// rectangles.
void userInterface::drawObstacles(HDC hDC, coord *obstPts, int numObst)
{
 int space = obstWidth / 2;
 HPEN hOldPen = (HPEN) SelectObject(hDC,G.getPen(obstacleColor));
 for (int i = 0; i < numObst; i++)
 {
   int x = xCoord(obstPts[i].getX()), y = yCoord(obstPts[i].getY());
   G.fillRectangle(hDC,x-space,y-space,x+space,y+space,obstacleColor);
 }
 SelectObject(hDC,hOldPen);
}

//============================================================================
//============================================================================
// Draws the goal at the given location.
void userInterface::drawGoal(HDC hDC, coord goalPt)
{
 G.fillCircle(hDC, xCoord(goalPt.getX()), yCoord(goalPt.getY()), goalWidth/2,
              goalColor);
}

/*
    This function creates an array of messages to be displayed in the
    message board rectangle based on the contents of the messageBoard
    class.
*/
void userInterface::displayMessageBoard(HDC hDC)
{
 HFONT hOldFnt = (HFONT) SelectObject(hDC,G.getFont(G.FIXED));
 RECT msgBoardRect = G.getRegion(hMsgBoardRegion);
 COLORREF oldColor;
 char *msgStr, outputStr[80];

 if (infoIsOld) oldColor = SetTextColor(hDC,RGB(192,192,192));

 int actualLength = task.getMsgBoardCurrentSize(agent);

 try { msgStr = new char[message::size()+1]; }
 catch(xalloc) { error(0,"userInterface::displayMessageBoard"); }

 for (int i=msgBoardStart; i<=msgBoardEnd; i++)
 {
#ifdef NETWORK
   if (i<actualLength)
   {
     message msg; int sup; double bid; unsigned long ID;
     task.getMsgBoardPosting(agent,i,msg,&sup,&ID,&bid);
     msg.msgToStr(msgStr);
     sprintf(outputStr,"%s %4d %4lu %6.2f",msgStr,sup,ID,bid);
   }
   else
   {
     for (int j=0; j<message::size(); j++) msgStr[j] = 'X';  msgStr[j] = '\0';
     sprintf(outputStr,"%s XXXX XXXX XXXXXX",msgStr);
   }
#else
   if (i<actualLength)
   {
     message msg; int sup; double bid;
     task.getMsgBoardPosting(agent,i,msg,&sup,&bid);
     msg.msgToStr(msgStr);
     sprintf(outputStr,"%s %4d %6.2f",msgStr,sup,bid);
   }
   else
   {
     for (int j=0; j<message::size(); j++) msgStr[j] = 'X';  msgStr[j] = '\0';
     sprintf(outputStr,"%s XXXX XXXXXX",msgStr);
   }
#endif

   int y = msgBoardRect.top + (i-msgBoardStart)*fixedTextHeight;
   G.printText(hDC,msgBoardRect.left,y,outputStr);
 }

 delete [] msgStr;

 SelectObject(hDC,hOldFnt);
 if (infoIsOld) SetTextColor(hDC,oldColor);
}

//============================================================================
//============================================================================
/*
   Manually displaying text.

   In calc function:
     Determine height of characters in font.  Determine how many lines can
     be put in the window based on the window's current size.  Store this
     number.

   In display function:
     Set clipping region.  Switch to the fixed font.
     Display x lines.

   In control function:
     When scroll bar is clicked, set new starting position.
*/

void userInterface::displayClassifierList(HDC hDC)
{
 HFONT hOldFnt = (HFONT) SelectObject(hDC,G.getFont(G.FIXED));
 RECT classListRect = G.getRegion(hClassListRegion);
 COLORREF oldColor;
 char *condStr, *actStr, temp[80];

 if (infoIsOld) oldColor = SetTextColor(hDC,RGB(192,192,192));

 try
 { condStr = new char[message::size()+1];
   actStr = new char[message::size()+1];
 }
 catch(xalloc) { error(0,"userInterface::displayClassifierList"); }

 for (int i=classListStart; i<=classListEnd; i++)
 {
#ifdef NETWORK
   message cond, act;  double stren, spec, sup, ruleBid;
   int match, sel, supplier, elite, ruleNum;  unsigned long ID;

   task.getClassifier(agent,i,cond,act,&stren,&spec,&sup,&ruleBid,&match,&sel,
                     &supplier,&ID,&elite,&ruleNum);
   cond.msgToStr(condStr);
   act.msgToStr(actStr);
//   sprintf(temp,"%c %c %4d %s %s %4.2f %6.2f %4d %4ld %6.2f %6.2f",
//           match ? '*' : ' ', sel ? '*' : ' ', ruleNum, condStr, actStr,
//           spec, sup, supplier, ID, ruleBid, stren);
   sprintf(temp,"%c%c%c %4d %s %s %6.2f %4d %4lu %6.2f %6.2f",
           match ? '*' : ' ', sel ? '*' : ' ', elite ? '*' : ' ',
           ruleNum, condStr, actStr, sup, supplier, ID, ruleBid, stren);
   int y = classListRect.top + (i-classListStart)*fixedTextHeight;
   G.printText(hDC,classListRect.left,y,temp);
#else
   message cond, act;  double stren, spec, sup, ruleBid;
   int match, sel, supplier, ruleNum, elite;

   task.getClassifier(agent,i,cond,act,&stren,&spec,&sup,&ruleBid,&match,&sel,
                     &supplier,&ruleNum);
   cond.msgToStr(condStr);
   act.msgToStr(actStr);
//   sprintf(temp,"%c %c %4d %s %s %4.2f %6.2f %4d %6.2f %6.2f",
//           match ? '*' : ' ', sel ? '*' : ' ', ruleNum, condStr, actStr,
//           spec, sup, supplier, ruleBid, stren);
   sprintf(temp,"%c%c%c %4d %s %s %4.2f %6.2f %4d %6.2f",
           match ? '*' : ' ', sel ? '*' : ' ', elite ? '*' : ' ',
           ruleNum, condStr, actStr, spec, sup, supplier, stren);
   int y = classListRect.top + (i-classListStart)*fixedTextHeight;
   G.printText(hDC,classListRect.left,y,temp);
#endif
 }

 delete [] condStr;
 delete [] actStr;

 SelectObject(hDC,hOldFnt);
 if (infoIsOld) SetTextColor(hDC,oldColor);
}

//============================================================================
//============================================================================

void userInterface::displaySensors(HDC hDC)
{
 HFONT hOldFnt = (HFONT) SelectObject(hDC,G.getFont(G.FIXED));
 RECT sensorsRect = G.getRegion(hSensorsRegion);
 COLORREF oldColor;
 if (infoIsOld) oldColor = SetTextColor(hDC,RGB(192,192,192));

 message sensorMsg;
 sensorMsg = task.getAgentSensorMsg(agent);
 char *sensorStr;

 try { sensorStr = new char[sensorMsg.size()+1]; }
 catch(xalloc) { error(0,"userInterface::displaySensors"); }

 sensorMsg.msgToStr(sensorStr);
 G.printText(hDC,sensorsRect.left,sensorsRect.top,sensorStr);

 delete [] sensorStr;

 SelectObject(hDC,hOldFnt);
 if (infoIsOld) SetTextColor(hDC,oldColor);
}

//============================================================================
//============================================================================
void userInterface::displayStatus(HDC hDC)
{
 static int lastDisplay = infoIsOld;   // infoIsOld = 1 means iteration display only
 int numItems;                         // number of items to display
 char temp[13][80];

 // Clear the status box is information status has changed.
 if (lastDisplay != infoIsOld) G.fillRectangle(hDC,statusBox,G.WHITE);
 lastDisplay = infoIsOld;

 if (!infoIsOld)
 {
   int clock = task.readClock(agent);
   numItems = 13;

   int numCrossovers, numSexual, numMutations, numCDOs, numCEOs,
       antCount, numCrashes;
   coord huge *antPts;
   float huge *antAngles;
   float goalDist, goalAngle, obstDist, obstAngle;

   task.readStats(agent, &numCrossovers, &numSexual, &numMutations, &numCDOs,
                  &numCEOs, &numCrashes);
   task.getAgent(agent, &antPts, &antAngles, &antCount, &goalDist, &goalAngle,
                 &obstDist, &obstAngle);

   sprintf(temp[0], "Seed:              %4u",classifierSystem::getSeed());
   sprintf(temp[1], "x loc:           %6.1f",antPts[antCount-1].getX());
   sprintf(temp[2], "y loc:           %6.1f",antPts[antCount-1].getY());
   sprintf(temp[3], "Direction:      %+7.2f",degrees(antAngles[antCount-1]));
   sprintf(temp[4], "Distance:        %6.1f",goalDist);
   sprintf(temp[5], "Angle:          %+7.2f",degrees(goalAngle));
   sprintf(temp[6], "Iteration:         %4d",clock);
   sprintf(temp[7], "Sexual Crossover:  %4d",numSexual);
   sprintf(temp[8], "Asexual Crossover: %4d",numCrossovers-numSexual);
   sprintf(temp[9], "Mutations:         %4d",numMutations);
   sprintf(temp[10],"CDOs:              %4d",numCDOs);
   sprintf(temp[11],"CEOs:              %4d",numCEOs);
   sprintf(temp[12],"Crashes:           %4d",numCrashes);
 }
 else
 {
   int clock = task.readGlobalClock();
   if (simState == batchStop || simState == batchRun)
   {
     if ((clock % 10) == 0)
     {
       numItems = 2;
       sprintf(temp[0], "Sim #              %4d",simNum);
       sprintf(temp[1], "Iteration:         %4d",clock);
     }
     else return;
   }
   else
   {
     if ((clock % 10) == 0)
     {
       numItems = 1;
       sprintf(temp[0], "Iteration:         %4d",clock);
     }
     else return;
   }
 }

 HFONT hOldFnt = (HFONT) SelectObject(hDC,G.getFont(G.FIXED));
 RECT statusRect = G.getRegion(hStatusRegion);

 // This can be updated to dynamically list the status items based on the
 // screen scaling.
 int columnSpace = 30*fixedTextWidth, col = 0, statusStart = 0;

 for (int i=0; i<numItems; i++)
 {
   if (i==6) col = 1;
   int x = statusRect.left + col*columnSpace,
       y = statusRect.top + (i-(col*6)-statusStart)*fixedTextHeight;
   G.printText(hDC,x,y,temp[i]);
 }

 SelectObject(hDC,hOldFnt);
}

//============================================================================
//============================================================================
void userInterface::saveFileAs()
{
  char dummy[256],simFileTitle[256];

  if (selectFile(simFile,dummy,simFileTitle,"SIM","Save Simulation File",
                 IDS_SIMFILTER, OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY, 1))
  {
    save(simFile);
    sprintf(windowTitle,"Borg : %s",simFileTitle);
    SetWindowText(hWnd,windowTitle);
  }
}

//============================================================================
//============================================================================
void userInterface::save(char *filename)
{
  SetCursor(LoadCursor(NULL,IDC_WAIT));

  char verStr[4] = "2.1";

  remove(filename);
  FILE *fptr = fopen(filename,"wb");

  fwrite(verStr,sizeof(char),4,fptr);
  task.saveStatic(fptr);
  task.save(fptr);

  fclose(fptr);

  SetCursor(LoadCursor(NULL,IDC_ARROW));
}

//============================================================================
//============================================================================
void userInterface::openFile()
{
  char dummy[256], simFileTitle[256];

  // Display the Open file box
  if (selectFile(simFile,dummy,simFileTitle,"SIM","Open Simulation File",
                 IDS_SIMFILTER, OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST |
                 OFN_HIDEREADONLY, 0))
  {
    load(simFile);
    sprintf(windowTitle,"Borg : %s",simFileTitle);
    SetWindowText(hWnd,windowTitle);
    simState = normalStop;
  }
}

//============================================================================
//============================================================================
int userInterface::load(char *filename)
{
  char verStr[4];

  FILE *fptr = fopen(filename,"rb");
  if (!fptr) return 0;

  SetCursor(LoadCursor(NULL,IDC_WAIT));

  fread(verStr,sizeof(char),4,fptr);
  if (strcmp(verStr,"2.1") != 0) error(12,"");
  else
  {
    task.loadStatic(fptr);
    task.load(fptr);
    calcDimensions();
    updateScreen(RESET_UPDATE);
  }

  fclose(fptr);

  SetCursor(LoadCursor(NULL,IDC_ARROW));

  return 1;
}

//============================================================================
//============================================================================
void userInterface::readINI()
{
  char buf[20];
  GetPrivateProfileString("display","Environment","On",buf,20,".\\borg.ini");
  if (strcmp(buf,"On") == 0) antOn = 1;
  else antOn = 0;
  GetPrivateProfileString("display","Info","On",buf,20,".\\borg.ini");
  if (strcmp(buf,"On") == 0) infoOn = 1;
  else infoOn = 0;
  GetPrivateProfileString("display","Axes","On",buf,20,".\\borg.ini");
  if (strcmp(buf,"On") == 0) axesOn = 1;
  else axesOn = 0;
  GetPrivateProfileString("display","Objects","Scaled",buf,20,".\\borg.ini");
  if (strcmp(buf,"Scaled") == 0) objectsToScale = 1;
  else objectsToScale = 0;
}

//============================================================================
//============================================================================
void userInterface::writeINI()
{
  WritePrivateProfileString("display", "Environment", antOn ? "On" : "Off",
                            ".\\borg.ini");
  WritePrivateProfileString("display", "Info", infoOn ? "On" : "Off",
                            ".\\borg.ini");
  WritePrivateProfileString("display", "Axes", axesOn ? "On" : "Off",
                            ".\\borg.ini");
  WritePrivateProfileString("display", "Objects", objectsToScale ? "Scaled" :
                            "NotScaled", ".\\borg.ini");
}

//============================================================================
//============================================================================
int userInterface::displayError(int num, char *str)
{
  char errorStr[300];
  int fatal = 1;

  switch(num)
  {
    case 0:  sprintf(errorStr,"Memory allocation error in %s\n\n"
                              "Application will terminate",str);
             break;
    case 1:  sprintf(errorStr,"Array out of range error in %s\n\n"
                              "Application will terminate",str);
             break;
    case 2:  sprintf(errorStr,"Infinite Loop in %s\n\n"
                              "Application will terminate",str);
             break;
    case 3:  sprintf(errorStr,"Incompatible file type\n\nOpen File Cancelled");
             fatal = 0;
             break;

    case 10: sprintf(errorStr,"Window creation failture in %s\n\n"
                              "Application will terminate",str);
             break;
    case 11: sprintf(errorStr,"Process creation failture in %s\n\n"
                              "Application will terminate",str);
             break;
    case 12: sprintf(errorStr,"Incompatible file type\n\nOpen File Cancelled");
             fatal = 0;
             break;
    case 13: sprintf(errorStr,"Exit batch mode before executing a file "
                              "walk-through\n\nRequest cancelled");
             fatal = 0;
             break;
    default: sprintf(errorStr,"%s",str);
             break;
  }

  if (fatal)
  {
    MessageBeep(-1);
    MessageBox(hWnd,errorStr,"Fatal Error",MB_SYSTEMMODAL | MB_OK);
    exit(1);
  }
  else
  {
    MessageBeep(-1);
    MessageBox(hWnd,errorStr,"Error",MB_APPLMODAL | MB_ICONEXCLAMATION | MB_OK);
  }

  return 0;
}

//============================================================================
//============================================================================
int userInterface::selectFile(char *file, char *fileNoExt, char *fileTitle,
                              const char *defaultExt,
                              const char *windowTitle,
                              UINT filterID, DWORD flags, int boxFlag)
{
  OPENFILENAME ofn;       // common dialog box structure
  char szFile[128],       // filename string
       szFileTitle[128],  // file-title string
       szFilter[128],     // filter string
       chReplace;         // string separator for szFilter
  int i, cbString,        // integer count variables
      result;             // Return value

  szFile[0] = '\0';

  cbString = LoadString(hInst, filterID, szFilter, sizeof(szFilter));

  // Replace the | character in the fileter string with the null character

  chReplace = szFilter[cbString - 1];
  for (i = 0; szFilter[i] != '\0'; i++)
    if (szFilter[i] == chReplace)
      szFilter[i] = '\0';

  // Initialize the OPENFILENAME structure

  memset(&ofn, 0, sizeof(OPENFILENAME));
  ofn.lStructSize = sizeof(OPENFILENAME);
  ofn.hwndOwner = hWnd;
  ofn.lpstrFilter = szFilter;
  ofn.nFilterIndex = 1;
  ofn.lpstrFile = szFile;
  ofn.nMaxFile = sizeof(szFile);
  ofn.lpstrFileTitle = szFileTitle;
  ofn.nMaxFileTitle = sizeof(szFileTitle);
  ofn.lpstrInitialDir = NULL;
  ofn.lpstrTitle = windowTitle;
  ofn.Flags = flags;
  ofn.lpstrDefExt = defaultExt;

  // Display the Open file box

  if (boxFlag == 0) result = GetOpenFileName(&ofn);
  else result = GetSaveFileName(&ofn);

  if (result)
  {
    strcpy(file,ofn.lpstrFile);
    strcpy(fileNoExt,ofn.lpstrFile);  fileNoExt[ofn.nFileExtension-1] = '\0';
    strcpy(fileTitle,ofn.lpstrFileTitle);
  }

  return result;
}



/*
   This function is called to initiate a batch run.  The user must select a
   batch file to run.  This function then calls the runBatch function to
   load and begin the first batch.
*/

//============================================================================
//============================================================================

#pragma argsused
void userInterface::saveStats(char *filename)
{
  int numCrossovers, numSexual, numMutations, numCDOs, numCEOs,
      geneticInterval, maxCount, numCrashes,
      numAgents = task.getNumAgents();
  unsigned seed;

#ifdef NETWORK
  int netOn;
  classifierSystem::getSettings(&seed, &geneticInterval, &maxCount, &netOn);
#else
  classifierSystem::getSettings(&seed, &geneticInterval, &maxCount);
#endif

  FILE *fptr = fopen(filename,"a+");

  for (int i=0; i<numAgents; i++)
  {
    task.readStats(i, &numCrossovers, &numSexual, &numMutations, &numCDOs,
                   &numCEOs, &numCrashes);
    fprintf(fptr,"%4d   %4ld   %4u     %4d       %4d       %4d       %4d       "
                 "%4d       %4d      %4d\n",
            simNum,task.getAgentID(i),seed,task.readClock(i),numSexual,
            numCrossovers-numSexual,numMutations, numCDOs,numCEOs,numCrashes);
  }

  fclose(fptr);
}

//============================================================================
//============================================================================
void userInterface::startBatch()
{
  char batchFileTitle[128], buffer[80];

  // Get the batch filename
  if (!selectFile(batchFile,outputFile,batchFileTitle,"B","Select Batch File",
                  IDS_BATCHFILTER, OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST |
                  OFN_HIDEREADONLY, 0))
    return;

  // Make the output filename the same as the batch filename except with an
  // "O" extension.  Delete any such filename already on disk.
  strcat(outputFile,".O");
  remove(outputFile);
  FILE *fptr = fopen(outputFile,"a+");
  fprintf(fptr,"                    Number of   Sexual     Asexual    Number of  Number of  Number of  Number of\n"
               "Sim #  Agent  Seed  Iterations  Crossover  Crossover  Mutations  CDO's      CEO's      Crashes\n\n");
  fclose(fptr);

  // Save current simulation work and indicate batch simulation in window
  strcpy(tempFile,"XXXXXX");
  mktemp(tempFile);
  save(tempFile);
  sprintf(buffer,"Borg : Batch Simulation Mode, %s",batchFileTitle);
  SetWindowText(hWnd,buffer);

  // Start the batch simulations
  runBatch(1);
}

/*
   This function is called to read the .b file and start the next simulation in
   the batch.  If this is not the first call to the function (i.e. the start of
   a batch), the previous simulation is saved before starting a new one.
*/

//============================================================================
//============================================================================

#define MAX_CHARS_PER_LINE  300
void userInterface::runBatch(int firstRun)
{
  static char filename[13];
  static fpos_t filePos;
  char temp[MAX_CHARS_PER_LINE+1], BBAenabledStr, softResetStr, netOnStr,
       classPassOnStr, actPassOnStr, CDOstr, CEOstr, eliteStr;

  unsigned seed;
  float goalRange, obstRange, sensorRange, timeConst, antWidth,
        goalReward, dirReward, obstReward, crashPenalty;
  int numAgents, envType,
      geneticInterval, maxCount, maxMsgBoardLength,
      classListLength, numConditions, BBAenabled,  BBAstyle,
      elitismEnabled, producerTaxInterval, CDOenabled, CEOenabled,
      TCOenabled, maxActionsToPost, actionPassingEnabled,
      classifierPassingEnabled, classifierTxInterval,
      maxClassifiersToTx, maxClassifiersToRx, actionTxInterval,
      maxActionsToTx, maxActionsToRx, netOn;
  double bidConstant, sexualProb, mutationProb, startStrength, headTax, bidTax,
        producerTax, eliteThresh, strengthCap, bidCap, TxStrenThresh,
        RxStrenThresh, TxBidThresh, RxBidThresh;

  // Open the batch file.
  FILE *fptr = fopen(batchFile,"r");

  // On first run, load the starting simulation number.  On subsequent runs,
  // save the newly-completed simulation and restore the file pointer to the
  // next line of simulation information.
  if (firstRun)
  {
    do
    {
      fgets(temp,MAX_CHARS_PER_LINE,fptr);
    } while(temp[0] == ';');
    sscanf(temp,"%d",&simNum);
  }
  else
  {
    save(filename);
    saveStats(outputFile);
    fsetpos(fptr,&filePos);
    ++simNum;
  }

  // Load next batch simulation
  do
  {
    fgets(temp,MAX_CHARS_PER_LINE,fptr);
  } while(temp[0] == ';');

  if (temp[0] == '*')       // Done
  {
    MessageBeep(-1);
    MessageBox(hWnd,"Batch simulation completed.","Batch Run",
               MB_APPLMODAL | MB_ICONINFORMATION);
    simState = normalStop;
    SetWindowText(hWnd,windowTitle);
    load(tempFile);
    remove(tempFile);
  }
  else
  {
    // Load the current settings.  Not all settings are changed.
    classifierSystem::getSettings(&seed, &geneticInterval, &maxCount, &netOn,
         &maxMsgBoardLength,
         &classListLength, &numConditions, &BBAenabled,  &BBAstyle,
         &elitismEnabled, &producerTaxInterval, &CDOenabled, &CEOenabled,
         &TCOenabled, &maxActionsToPost, &bidConstant, &sexualProb,
         &mutationProb, &startStrength, &headTax, &bidTax, &producerTax,
         &eliteThresh, &strengthCap, &bidCap,
         &actionPassingEnabled, &classifierPassingEnabled, &classifierTxInterval,
         &maxClassifiersToTx, &maxClassifiersToRx, &actionTxInterval,
         &maxActionsToTx, &maxActionsToRx, &TxStrenThresh, &RxStrenThresh,
         &TxBidThresh, &RxBidThresh);

    // Read new parameters
    sscanf(temp,
           "%u %d %d %c %c %c %c %c %lf %lf %d %d %d %d %lf %lf %lf %lf %lf %lf "  // system
           " %f %f %f %f %f %f %f %f %f"                           // agent
           " %c %c %d %d %d %lf %lf %c %d %d %d %lf %lf",          // network
           &seed, &numAgents, &envType, &BBAenabledStr, &softResetStr,
           &CDOstr, &CEOstr, &eliteStr, &eliteThresh, &headTax, &maxCount,
           &classListLength, &maxActionsToPost, &geneticInterval, &bidConstant,
           &startStrength, &strengthCap, &bidCap, &sexualProb, &mutationProb,

           &goalReward, &dirReward, &obstReward, &crashPenalty,
           &goalRange, &obstRange, &sensorRange, &timeConst, &antWidth,

           &netOnStr, &classPassOnStr, &classifierTxInterval,
           &maxClassifiersToTx, &maxClassifiersToRx, &TxStrenThresh,
           &RxStrenThresh, &actPassOnStr, &actionTxInterval,
           &maxActionsToTx, &maxActionsToRx, &TxBidThresh, &RxBidThresh);
    if (BBAenabledStr == 'Y') BBAenabled = 1;
    else BBAenabled = 0;
    if (netOnStr == 'Y') netOn = 1;
    else netOn = 0;
    if (classPassOnStr == 'Y') classifierPassingEnabled = 1;
    else classifierPassingEnabled = 0;
    if (actPassOnStr == 'Y') actionPassingEnabled = 1;
    else actionPassingEnabled = 0;
    if (CDOstr == 'Y') CDOenabled = 1;
    else CDOenabled = 0;
    if (CEOstr == 'Y') CEOenabled = 1;
    else CEOenabled = 0;
    if (eliteStr == 'Y') elitismEnabled = 1;
    else elitismEnabled = 0;
    if (envType == 0) envType = task.STANDARD;
    else envType = task.CONCAVE_OBST;

    // Set the maximum length of the message board
    if (netOn && actionPassingEnabled)
      maxMsgBoardLength = maxActionsToPost + maxActionsToRx;
    else
      maxMsgBoardLength = maxActionsToPost;

    // Make the simulation filename
    sprintf(filename,"%d.sim",simNum);

    // Set the number of agents and the environment type
    task.setNumAgents(numAgents);
    task.setEnvType(envType);

    // Set new parameters
    environmentInterface::setEnvSettings(goalReward, dirReward, obstReward,
                                         crashPenalty);
    environmentInterface::setAgentSettings(goalRange, obstRange, sensorRange,
                                           timeConst, antWidth);
    classifierSystem::setSettings(seed, geneticInterval, maxCount, netOn,
         maxMsgBoardLength,
         classListLength, numConditions, BBAenabled,  BBAstyle,
         elitismEnabled, producerTaxInterval, CDOenabled, CEOenabled,
         TCOenabled, maxActionsToPost, bidConstant, sexualProb,
         mutationProb, startStrength, headTax, bidTax, producerTax,
         eliteThresh, strengthCap, bidCap,
         actionPassingEnabled, classifierPassingEnabled, classifierTxInterval,
         maxClassifiersToTx, maxClassifiersToRx, actionTxInterval,
         maxActionsToTx, maxActionsToRx, TxStrenThresh, RxStrenThresh,
         TxBidThresh, RxBidThresh);

    // Reset system
    simState = batchStop;
    if (softResetStr=='Y') task.softReset();
    else task.reset();
    updateScreen(RESET_UPDATE);
    runStartRequest = 1;
  }

  // Save the file position
  fgetpos(fptr,&filePos);
  fclose(fptr);
}

//============================================================================
//============================================================================
// agent, env, info, statusOnly
void PASCAL userInterface::updateScreen(int control)
{
  switch(control)
  {
    case START_UPDATE:  // Info update with grey if info not enabled
      infoIsOld = 1-infoOn;
      repaintWindow(0,0,(1-infoOn),0,0);
      break;
    case STOP_UPDATE:   // Agent and info update if not already enabled
      SetCursor(LoadCursor(NULL,IDC_WAIT));
      infoIsOld = 0;
      repaintWindow((1-antOn),0,(1-infoOn),0,0);
      SetCursor(LoadCursor(NULL,IDC_ARROW));
      break;
    case RUN_UPDATE:    // Agent and info update if enabled, status only if
                        // info not enabled
      repaintWindow(antOn,0,infoOn,(1-infoOn),0);
      break;
    case STEP_UPDATE:   // Agent and info update
      SetCursor(LoadCursor(NULL,IDC_WAIT));
      infoIsOld = 0;
      repaintWindow(1,0,1,0,0);
      SetCursor(LoadCursor(NULL,IDC_ARROW));
      break;
    case RESET_UPDATE:  // Environment and info update
      calcDimensions();
      ShowWindow(hSpeedBar,SW_HIDE);
      ShowWindow(hMsgBoardScrollBar,SW_HIDE);
      ShowWindow(hClassListScrollBar,SW_HIDE);
      InvalidateRect(hWnd,NULL,1);
      UpdateWindow(hWnd);
      break;
    case BOX_UPDATE:    // Update the boxes (for multiple agent use)
      repaintWindow(0,0,0,0,1);
      break;
  }
}

/*
   Update flag explanation:

   updateAgentOnly :  Updates the current agent position only.  No environment
                      or previous agent positions update.
   updateInfoOnly :   Update the info boxe text only.  No boxes or titles are
                      redrawn.
   refreshInfo :      Refreshes the status display with the default status
                      information.

   Display flag explanation:

   antOn:   Agent display at each time step enabled.
   infoOn:  Info display at each time step.  Note that even if the info is
            turned off, the status area will still display an iteration
            count.
*/

/*
    Used to initiate a file walk-through.  File walk-through is a quick way
    to view a large number of simulations (say, from a batch run).
    The current work is saved, and then the user steps through a set of
    files in order.  The user can only look at the simulations and cannot
    run or reset any of them in this mode.
*/

//============================================================================
//============================================================================
void userInterface::fileWalkSelect()
{
  char fileTitle[128], dummy1[128], dummy2[128], *file, *ext;

  if (selectFile(fileWalkPath,dummy1,fileTitle,"SIM","File Walk Mode : Select First File",
                 IDS_WALKFILTER, OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST |
                 OFN_HIDEREADONLY, 0))
  {
    // extract path from 'file'
    int i = strlen(fileWalkPath)-1;
    while(fileWalkPath[i] != '\\') --i;
    fileWalkPath[i+1] = '\0';

    // determine whether file is a SIM or P file
    file = fileTitle;
    i = 0; while(fileTitle[i] != '.') i++;
    fileTitle[i] = '\0';
    ext = fileTitle + i + 1;
    if (strcmp(ext,"SIM") == 0) fileWalkType = 0;
    else fileWalkType = 1;

    // set the starting file and get ending file
    if (fileWalkType)  // *.p file
    {
      sscanf(file,"%d_%d",&fileWalkStart,&fileWalkStartNum);

      if (selectFile(dummy1,dummy2,fileTitle,"P","File Walk Mode : Select Last File",
                     IDS_BINPOSFILTER, OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST |
                     OFN_HIDEREADONLY, 0))
      {
        // extract filename from 'fileTitle'
        file = fileTitle;
        i = 0; while(fileTitle[i] != '.') i++;
        fileTitle[i] = '\0';
        sscanf(file,"%d_%d",&fileWalkEnd,&fileWalkEndNum);

        walkRequest = 1;
      }
    }
    else
    {
      fileWalkStart = atoi(file);

      if (selectFile(dummy1,dummy2,fileTitle,"SIM","File Walk Mode : Select Last File",
                     IDS_SIMFILTER, OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST |
                     OFN_HIDEREADONLY, 0))
      {
        // extract filename from 'fileTitle'
        file = fileTitle;
        i = 0; while(fileTitle[i] != '.') i++;
        fileTitle[i] = '\0';
        fileWalkEnd = atoi(file);

        walkRequest = 1;
      }
    }
  }
}

//============================================================================
//============================================================================
void userInterface::startWalk()
{
  // Save current simulation work and indicate file walk mode
  strcpy(tempFile,"XXXXXX");
  mktemp(tempFile);
  save(tempFile);
  simState = fileWalk;

  fileWalkCont(1);
}

//============================================================================
//============================================================================

FILE *userInterface::findNext(int *series, int *num, int end, char *path,
                              char *currentFile)
{
  char file[128];
  FILE *fptr;

  // loop thru files
  while(*series<=end)
  {
    sprintf(file,"%s%d_%d.p",path,*series,*num);
    fptr = fopen(file,"rb");
    if (fptr)
    {
      sprintf(currentFile,"%d_%d.p",*series,*num);
      (*num)++;
      return fptr;
    }
    else
    {
      (*series)++;
      *num = 1;
    }
  }

  return 0;
}

//============================================================================
//============================================================================

void userInterface::fileWalkCont(int firstCall)
{
  char filename[128], fileTitle[13], buffer[80];
  static int currentFile, currentNum;

  if (firstCall) { currentFile = fileWalkStart; currentNum = fileWalkStartNum; }

  if (fileWalkType)  // *.p files
  {
    if (currentFile == fileWalkEnd && currentNum > fileWalkEndNum) { endWalk(); return; }
    FILE *fptr = findNext(&currentFile,&currentNum,fileWalkEnd,fileWalkPath,fileTitle);
    if (!fptr) { endWalk(); return; }

    SetCursor(LoadCursor(NULL,IDC_WAIT));
    task.loadAgentPosBinary(fptr);
    SetCursor(LoadCursor(NULL,IDC_ARROW));
    updateScreen(RESET_UPDATE);

    fclose(fptr);
  }
  else
  {
    do
    {
      if (currentFile > fileWalkEnd) { endWalk(); return; }

      sprintf(fileTitle,"%d.SIM",currentFile);
      sprintf(filename,"%s%s",fileWalkPath,fileTitle);
      ++currentFile;
    } while (!load(filename));
  }

  sprintf(buffer,"Borg : File Walk Mode, %s",fileTitle);
  SetWindowText(hWnd,buffer);
}

//============================================================================
//============================================================================
void userInterface::endWalk()
{
  SetWindowText(hWnd,windowTitle);
  simState = normalStop;
  load(tempFile);
  remove(tempFile);
  return;
}

//============================================================================
//============================================================================

void userInterface::loadRules()
{
  char dummy[128], ruleFileTitle[128];

  if (selectFile(ruleFile,dummy,ruleFileTitle,"RUL","Load Rules",
                 IDS_RULEFILTER, OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST |
                 OFN_HIDEREADONLY, 0))
  {
    SetCursor(LoadCursor(NULL,IDC_WAIT));
    FILE *fptr = fopen(ruleFile,"r");
    task.loadRules(fptr);
    fclose(fptr);
    SetCursor(LoadCursor(NULL,IDC_ARROW));
    updateScreen(STEP_UPDATE);
  }
}

//============================================================================
//============================================================================

void userInterface::saveRules()
{
  char dummy[128], ruleFileTitle[128];

  if (selectFile(ruleFile,dummy,ruleFileTitle,"RUL","Save Rules",
                 IDS_RULEFILTER, OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY, 1))
  {
    SetCursor(LoadCursor(NULL,IDC_WAIT));
    FILE *fptr = fopen(ruleFile,"w");
    task.saveRules(fptr);
    fclose(fptr);
    SetCursor(LoadCursor(NULL,IDC_ARROW));
  }
}

void userInterface::saveAgentPositions()
{
 SetCursor(LoadCursor(NULL,IDC_WAIT));
 task.saveAgentPositions();
 SetCursor(LoadCursor(NULL,IDC_ARROW));
}

void userInterface::savePositionsBin()
{
  char dummy[128], fileTitle[128];

  if (selectFile(binPosFile,dummy,fileTitle,"P","Save Rules",
                 IDS_BINPOSFILTER, OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY, 1))
  {
    SetCursor(LoadCursor(NULL,IDC_WAIT));
    FILE *fptr = fopen(binPosFile,"wb");
    task.saveAgentPosBinary(fptr);
    fclose(fptr);
    SetCursor(LoadCursor(NULL,IDC_ARROW));
  }
}

void userInterface::loadPositionsBin()
{
  char dummy[128], fileTitle[128];

  if (selectFile(binPosFile,dummy,fileTitle,"P","Load Rules",
                 IDS_BINPOSFILTER, OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST |
                 OFN_HIDEREADONLY, 0))
  {
    SetCursor(LoadCursor(NULL,IDC_WAIT));
    FILE *fptr = fopen(binPosFile,"rb");
    task.loadAgentPosBinary(fptr);
    fclose(fptr);
    SetCursor(LoadCursor(NULL,IDC_ARROW));
    updateScreen(RESET_UPDATE);
  }
}

//============================================================================
//============================================================================
//============================================================================
//============================================================================
//============================================================================
//============================================================================

void userInterface::makeBitmap()
{
    HBITMAP hBmp;
    HDC     hDC, hDCdest;

    hDC = GetDC(hWnd);
//    if ((GetDeviceCaps(hdc,RASTERCAPS) & RC_BITBLT)==0) error(0,"no can do");

    hDCdest = CreateCompatibleDC(hDC);

    if (!hDC || !hDCdest) error(0,"Cannot create compatible DC's");

//    hBmp = CreateCompatibleBitmap(hDC, 100, 100);
//    HBITMAP hBmpOld = (HBITMAP) SelectObject(hDCdest,hBmp);
//    if (!hBmpOld) error(0,"Count not select bitmap2");

//    if (!BitBlt(hDCdest, 0, 0, 100, 100, hDC, 0, 0, SRCCOPY)) error(0,"copy failed");

//    SelectObject(hDCdest,hBmpOld);
    hBmp = LoadBitmap(NULL,MAKEINTRESOURCE(OBM_UPARROW));
    if (hBmp == NULL) error(0,"Cannot load bitmap");
    HBITMAP hBmpOld = (HBITMAP) SelectObject(hDCdest,hBmp);
    BITMAP bmp;
    if (!GetObject(hBmp, sizeof(BITMAP), (LPSTR)&bmp)) error(0,"GetObject");
    if (!BitBlt(hDC, 0, 0, bmp.bmWidth, bmp.bmHeight, hDCdest, 0, 0, SRCCOPY)) error(0,"copy failed");
    SelectObject(hDCdest,hBmpOld);

    PBITMAPINFO pbmi = CreateBitmapInfoStruct(hBmp);
//    CreateBMPFile("test.bmp",pbmi,hBmp,hDCdest);
    CreateBMPFile("test.bmp",pbmi,hBmp,hDC);

    ReleaseDC(hWnd,hDC);
    DeleteDC(hDCdest);
}

PBITMAPINFO userInterface::CreateBitmapInfoStruct(HBITMAP hBmp) {
    BITMAP bmp;
    PBITMAPINFO pbmi;
    WORD    cClrBits;

    /* Retrieve the bitmap's color format, width, and height. */

    if (!GetObject(hBmp, sizeof(BITMAP), (LPSTR)&bmp))
        error(0,"GetObject");


    /* Convert the color format to a count of bits. */

    cClrBits = (WORD)(bmp.bmPlanes * bmp.bmBitsPixel);


    if (cClrBits == 1)
        cClrBits = 1;
    else if (cClrBits <= 4)
        cClrBits = 4;
    else if (cClrBits <= 8)
        cClrBits = 8;
    else if (cClrBits <= 16)
        cClrBits = 16;
    else if (cClrBits <= 24)
        cClrBits = 24;
    else
        cClrBits = 32;

    /*
     * Allocate memory for the BITMAPINFO structure. (This structure
     * contains a BITMAPINFOHEADER structure and an array of RGBQUAD data
     * structures.)
     */

    if (cClrBits != 24)

         pbmi = (PBITMAPINFO) LocalAlloc(LPTR,
                    sizeof(BITMAPINFOHEADER) +
                    sizeof(RGBQUAD) * cClrBits);

    /*
     * There is no RGBQUAD array for the 24-bit-per-pixel format.
     */

    else
         pbmi = (PBITMAPINFO) LocalAlloc(LPTR,
                    sizeof(BITMAPINFOHEADER));



    /* Initialize the fields in the BITMAPINFO structure. */

    pbmi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);

    pbmi->bmiHeader.biWidth = bmp.bmWidth;
    pbmi->bmiHeader.biHeight = bmp.bmHeight;
    pbmi->bmiHeader.biPlanes = bmp.bmPlanes;
    pbmi->bmiHeader.biBitCount = bmp.bmBitsPixel;


    /* If the bitmap is not compressed, set the BI_RGB flag. */

    pbmi->bmiHeader.biCompression = BI_RGB;

    /*
     * Compute the number of bytes in the array of color
     * indices and store the result in biSizeImage.
     */

    pbmi->bmiHeader.biSizeImage = (pbmi->bmiHeader.biWidth + 7) /8

                                  * pbmi->bmiHeader.biHeight
                                  * cClrBits;

    /*
     * Set biClrImportant to 0, indicating that all of the
     * device colors are important.
     */

    pbmi->bmiHeader.biClrImportant = 0;

    return pbmi;

}

void userInterface::CreateBMPFile(LPSTR pszFile, PBITMAPINFO pbi,
                  HBITMAP hBMP, HDC hDC)
 {

//    HANDLE hf;                  /* file handle */
    BITMAPFILEHEADER hdr;       /* bitmap file-header */
    PBITMAPINFOHEADER pbih;     /* bitmap info-header */
    LPBYTE lpBits;              /* memory pointer */
//    DWORD dwTotal;              /* total count of bytes */

//    DWORD cb;                   /* incremental count of bytes */
    BYTE *hp;                   /* byte pointer */
//    DWORD dwTmp;


    pbih = (PBITMAPINFOHEADER) pbi;

    lpBits = (LPBYTE) GlobalAlloc(GMEM_FIXED, pbih->biSizeImage);
    if (!lpBits)
         error(0,"GlobalAlloc");

    /*
     * Retrieve the color table (RGBQUAD array) and the bits
     * (array of palette indices) from the DIB.

     */

//    if (!GetBitmapBits(hBMP,pbih->biSizeImage,lpBits))
    long numLines = pbi->bmiHeader.biHeight;
    if (!GetDIBits(hDC, hBMP, 0, (unsigned) numLines,
                   lpBits, pbi, DIB_RGB_COLORS))
        error(0,"GetDIBits");

    /* Create the .BMP file. */

    FILE *fptr = fopen(pszFile,"wb");
    if (!fptr) error(0,"Could not open file.");

    hdr.bfType = 0x4d42;        /* 0x42 = "B" 0x4d = "M" */

    /* Compute the size of the entire file. */

    hdr.bfSize = (DWORD) (sizeof(BITMAPFILEHEADER) +
                 pbih->biSize + pbih->biClrImportant
                 * sizeof(RGBQUAD) + pbih->biSizeImage);


    hdr.bfReserved1 = 0;
    hdr.bfReserved2 = 0;

    /* Compute the offset to the array of color indices. */

    hdr.bfOffBits = (DWORD) sizeof(BITMAPFILEHEADER) +
                    pbih->biSize + pbih->biClrImportant
                    * sizeof (RGBQUAD);

    /* Copy the BITMAPFILEHEADER into the .BMP file. */

    fwrite(&hdr,sizeof(BITMAPFILEHEADER),1,fptr);

    /* Copy the BITMAPINFOHEADER and RGBQUAD array into the file. */

    fwrite(&pbih,(size_t) (sizeof(BITMAPINFOHEADER) + pbih->biClrImportant * sizeof (RGBQUAD)),
           1, fptr);

    /* Copy the array of color indices into the .BMP file. */

    fwrite(&hp,(size_t) pbih->biSizeImage,1,fptr);
/*
    dwTotal = cb = pbih->biSizeImage;

    hp = lpBits;
    while (cb > MAXWRITE)  {
            fwrite(&hp, (int) MAXWRITE, 1, fptr);
            cb-= MAXWRITE;
            hp += MAXWRITE;
    }

    fwrite(&hf, (int) cb, 1, fptr);
*/
    /* Close the .BMP file. */

    fclose(fptr);

}


