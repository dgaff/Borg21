#ifndef UI_H
#define UI_H

#include"graph.h"

class userInterface
{
    // Windows parameters
    HINSTANCE hInst;          // Holds current instance handle of program
    HWND hWnd;                // Holds current window handle
    UINT wMessage;            // Holds current message
    WPARAM wParam;            // Holds current w message parameter
    LPARAM lParam;            // Holds current l message parameter
    HMENU hMenu;              // Holds main menu handle
    FARPROC speedBarProc;     // Speed bar process pointer
    HWND hSpeedBar;           // Speed bar window handle
    HWND hClassListScrollBar; // Classifier list box scroll bar handle
    HWND hMsgBoardScrollBar;  // Message board list box scroll bar handle

    // Region handles used to flag region repainting
    HRGN hEnvRegion, hFieldRegion,
         hMsgBoardRegion, hSensorsRegion, hClassListRegion, hStatusRegion,
         hClassListScrollRegion, hMsgBoardScrollRegion,
         hEnvBox, hMsgBoardBox, hClassListBox, hSensorsBox,
         hStatusBox, hSpeedBarRegion;

    // Updating states for updateScreen()
    enum { START_UPDATE, STOP_UPDATE, RUN_UPDATE, STEP_UPDATE, RESET_UPDATE,
           BOX_UPDATE };

    // Filenames
    char simFile[128], batchFile[128], outputFile[128], tempFile[13],
         fileWalkPath[128], ruleFile[128], binPosFile[128];

    // Main window title
    char windowTitle[80];

    // Flags & parameters
    int agent,
        stopRequest, resetRequest,           // Action requests from menu selection
        batchRequest, runRequest,
        newAgentDisplayRequest,
        dialogBoxRequest,
        resetEnvRequest,
        runStartRequest,
        stepRequest,
        newRequest,
        walkRequest,
        simState,                            // State of simulation cycle
        simNum,                              // Simulation number for batch mode
        axesOn,                              // Axis draw flag
        antOn,                               // Ant draw during iteration
        infoOn,                              // Info display durring iteration
        objectsToScale,                      // Display objects to scale flag
        infoIsOld,                           // Flag to indicate use of grey in info listings
        lastAgentIndexDisp,                  // Last ant position displayed on screen
        windowMinimized,                     // Window is minimized
        *antColor, obstacleColor, goalColor, // Draw colors
        msgBoardStart, msgBoardEnd,          // Start and end of message display
        maxMsgBoardLength,                   // Maximum # of message that will fit
                                             //   in message board box
        classListStart, classListEnd,        // Start and end of classifier list
        maxClassListLength,                  // Maximum # of classifiers that will
                                             //   fit in classifier box
        maxStatusLength,                     // Maximum lines available in status box
        fixedTextHeight,                     // Height of fixed font text
        fixedTextWidth,                      // Width of fixed font text
        classListScrollOn,                   // Class list scroll bar on flag
        msgBoardScrollOn,                    // Msg board scroll bar on flag
        scrollBarWidth,                      // Width of vertical scroll bar
        fileWalkStart, fileWalkEnd,          // Simulation file walk through
        fileWalkStartNum, fileWalkEndNum,    // used with *.p files
        fileWalkType;                        // 0=='*.sim' 1=='*.p'

    // Coord's of boxes to be drawn in the client area.
    RECT envBox, fieldBox, msgBoardBox, classListBox, sensorsBox, statusBox;

    // Environment object widths in pixels.  These widths are for display
    // purposes and do not represent the actual width in axis coordinates
    // (as given in the environment class).  These widths are set to ensure
    // that the user will be able to see the objects on a small screen.
    int antWidth, goalWidth, obstWidth;

    // Axes information
    int x1Copy, x2Copy, y1Copy, y2Copy;   // Copy of the playing field axis
                                          //   range from environment class.
    float dx,dy;                          // Pixel-to-data ratios for plotting.

    // Cleanup for HRGN objects
    void deleteRegionObjects();

    // Functions accessed by simulation menu and speed bar.  The screen updates
    // are for display refreshing.  Actual real-time updating is done in the
    // WM_TIMER message case.

 public:
    enum { goal, normalRun, normalStop, batchRun, batchStop, fileWalk };
    enum { DISP_SETTINGS = 1, ENV_SETTINGS, AGENT_SETTINGS, NETWORK_SETTINGS,
           ABOUT };
    userInterface();
    ~userInterface() { delete [] antColor; }
    int PASCAL WinMain(HINSTANCE, HINSTANCE, LPSTR, int);
    LRESULT FAR PASCAL mainWndProc(HWND, UINT, WPARAM, LPARAM);
    BOOL FAR PASCAL speedBar(HWND, WORD, WPARAM, LPARAM);
    BOOL FAR PASCAL displaySettings(HWND, WORD, WPARAM, LPARAM);
    BOOL FAR PASCAL envSettings(HWND, WORD, WPARAM, LPARAM);
    BOOL FAR PASCAL agentSettings(HWND, WORD, WPARAM, LPARAM);
#ifdef NETWORK
    BOOL FAR PASCAL networkSettings(HWND, WORD, WPARAM, LPARAM);
#endif
    BOOL FAR PASCAL about(HWND, WORD, WPARAM, LPARAM);
    BOOL FAR PASCAL fileWalkSetup(HWND, WORD, WPARAM, LPARAM);

    BOOL initApplication(HINSTANCE);
    BOOL initInstance(HINSTANCE, int);
    void cleanUpInstance();
    void processSize();
    int processMenu();
    void PASCAL processTimer();
    void processScroll();
    int listScroll(int *start, int *end, int len, int max);
    void calcDimensions(int firstCallHere = 0);
    void normalizeIndices(int *start, int *end, int len, int max);
    void repaintWindow(int updateAgent=0, int updateEnv=0, int updateInfo=0,
                       int updateStatusOnly=0, int updateBoxes=0);
    void createMetaFile();
    void drawAnt(HDC hDC, coord antPt, float antAngle, int color);
    void drawObstacles(HDC hDC, coord *obstPts, int numObst);
    void drawGoal(HDC hDC, coord goalPt);
    void labelAxes(HDC hDC, int x1, int x2, int numXTics, char *xLabel,
                   int y1, int y2, int numYTics, char *yLabel);
    int xCoord(double x) { return fieldBox.left + int((x-double(x1Copy))*dx); }
    int yCoord(double y) { return fieldBox.bottom-1 - int((y-double(y1Copy))*dy); }

    void displayMessageBoard(HDC hDC);
    void displayClassifierList(HDC hDC);
    void displaySensors(HDC hDC);
    void displayStatus(HDC hDC);
    void saveFileAs();
    void saveFile() { save(simFile); }
    void save(char *filename);
    void openFile();
    int load(char *filename);
    void loadRules();
    void saveRules();
    void readINI();
    void writeINI();
    void PASCAL updateScreen(int control);
    int displayError(int num, char *str);
    void saveStats(char *filename);
    int selectFile(char *file, char *fileNoExt, char *fileTitle,
                   const char *defaultExt, const char *windowTitle,
                   UINT filterID, DWORD flags, int boxFlag);
    void startBatch();
    void runBatch(int firstRun=0);
    void fileWalkSelect();
    void startWalk();
    void fileWalkCont(int firstCall=0);
    void endWalk();
    FILE *findNext(int *series,int *num,int end,char *path,char *currentFile);
    void saveAgentPositions();
    void savePositionsBin();
    void loadPositionsBin();
    void makeBitmap();
    void CreateBMPFile(LPSTR pszFile, PBITMAPINFO pbi, HBITMAP hBMP, HDC hDC);
    PBITMAPINFO CreateBitmapInfoStruct(HBITMAP hBmp);
};

extern userInterface UI;

#endif
