#define STRICT
#include<windows.h>
#include<bwcc.h>
#include"task.h"
#include"ui.h"
#include"borg.h"

graphics G;        // instantiation of the graphics handling functions
taskEnvironment task;
userInterface UI;  // instantiation of the user interface, should occur last

/*
   Windows program entry point.  This procedure handles message processing
   for all instances of this program.  The application is only initialized
   once using InitApplication().  Each instance is initialized using
   InitInstance().
*/

int PASCAL WinMain(HINSTANCE hinstCurrent, HINSTANCE hinstPrevious,
                          LPSTR lpszCmdLine, int nCmdShow)
{ return UI.WinMain(hinstCurrent,hinstPrevious,lpszCmdLine,nCmdShow); }

#pragma argsused
int PASCAL userInterface::WinMain(HINSTANCE hinstCurrent, HINSTANCE hinstPrevious,
                                  LPSTR lpszCmdLine, int nCmdShow)
{
 MSG msg;

 BWCCGetVersion();

 if (!hinstPrevious)
	if (!UI.initApplication(hinstCurrent)) return FALSE;

 if (!UI.initInstance(hinstCurrent, nCmdShow)) return FALSE;

 while (GetMessage(&msg, NULL, NULL, NULL))
 {
	TranslateMessage(&msg);
	DispatchMessage(&msg);
 }

 return msg.wParam;
}


