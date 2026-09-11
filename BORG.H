#define IDM_FILEsaveMeta	3
#define IDM_FILE	114
#define IDM_FILEsavePositionsBin	113
#define IDM_FILEsavePositions	112
#define IDM_OPTIONSnetwork	303
#define IDC_NETWORKsystemButton	101
#define IDC_NETWORKagentButton	102
#define IDC_NETWORKactionRXbidThresh	117
#define IDC_NETWORKclassRXstrenThresh	112
#define IDC_NETWORKactionMaxToRX	116
#define IDC_NETWORKclassMaxToRX	111
#define IDC_NETWORKactionTXbidThresh	115
#define IDC_NETWORKclassTXstrenThresh	110
#define IDC_NETWORKactionMaxToTX	114
#define IDC_NETWORKclassMaxToTX	109
#define IDC_NETWORKactionTXint	113
#define IDC_NETWORKclassTXinterval	108
#define IDC_NETWORKactionEnabled	107
#define IDC_NETWORKclassEnabled	106
#define IDC_NETWORKenabled	105
#define IDC_GROUPBOX9	103
#define IDC_GROUPBOX10	104
#define DIALOG_2	2
#define IDM_FILEsaveRules	111
#define IDM_FILEloadRules	110
#define DIALOG_1	1
#define IDM_SIMULATEstartBatch	206
#define IDM_SIMULATEresetEnv	205
#define IDM_FILEsave	105
#define IDM_FILEwalk	107
#define IDM_FILEopen	106
#define IDM_FILEnew	2
#define IDC_GROUPBOX5	101
#define IDC_GROUPBOX6	102
#define IDC_AGENTconstDiameter	108
#define IDC_ENVpayoffGoal	116
#define IDC_ENVpayoffDirection	122
#define IDC_ENVpayoffObst	123
#define IDC_ENVpayoffCrash	124
#define IDC_AGENTsystemButton	109
#define IDC_AGENTnetworkButton	110
#define IDC_GROUPBOX2	107
#define IDC_AGENTconstTime	106
#define IDC_AGENTrangeSensor	105
#define IDC_AGENTrangeObst	104
#define IDC_AGENTrangeGoal	103
#define IDC_GROUPBOX1	104
#define IDC_ENVagentButton	131
#define IDC_ENVnetworkButton	130
#define IDC_ENVstandardObst	105
#define IDC_ENVconcaveObst	116
#define IDC_GROUPBOX4	107
#define IDC_GROUPBOX11	133
#define IDC_ENVceoOn	135
#define IDC_ENVtcoOn	136
#define IDC_ENVcdoOn	134
#define IDC_ENVnumAgents	132
#define IDC_GROUPBOX3	106
#define IDC_ENVbbaEnabled	109
#define IDC_ENVheadTax	108
#define IDC_ENVprodTax	124
#define IDC_ENVprodInt	129
#define IDC_ENVbidTax	123
#define IDC_ENVmaxMsgBoard	117
#define IDC_ENVstrengthCap	118
#define IDC_ENVbidCap	119
#define IDC_ENVgeneticsInterval	115
#define IDC_ENVgeneticsMutationProb	114
#define IDC_ENVgeneticsCrossProb	113
#define IDC_ENVsimMaxIterations	112
#define IDC_ENVsimSeed	111
#define IDC_ENVclassStrength	121
#define IDC_ENVactionMask	109
#define IDC_ENVactionPayoffMask	117
#define IDC_ENVmessageLength	118
#define IDC_ENVcondMask	108
#define IDC_ENVclassBid	120
#define IDC_ENVeliteThresh	125
#define IDC_ENVeliteEnabled	126
#define IDC_GROUPBOX7	127
#define IDC_GROUPBOX8	128
#define IDC_ENVnumCond	122
#define IDC_ENVnumClass	110
#define IDM_OPTIONSagent	301
#define IDM_OPTIONSdisplay	1
#define IDC_DISPLAYenv	101
#define IDC_DISPLAYaxesOn	108
#define IDC_DISPLAYaxesOff	109
#define IDC_DISPLAYinfoDuring	107
#define IDC_DISPLAYinfoAfter	106
#define IDC_DISPLAYenvDuring	103
#define IDC_DISPLAYenvAfter	102
#define IDC_DISPLAYaxes	105
#define IDC_DISPLAYobjectToScale	111
#define IDC_DISPLAYobjectNotToScale	112
#define IDC_DISPLAYobject	110
#define IDC_DISPLAYinfo	104
#define IDC_SPEEDBARrun	        101
#define IDC_SPEEDBARstep	      102
#define IDC_SPEEDBARstop	      103
#define IDC_SPEEDBARreset	      104
#define IDC_SPEEDBARbatch	      105
#define IDC_SPEEDBARagent1	    106
#define IDC_SPEEDBARagent2	    107
#define IDC_SPEEDBARagent3	    108
#define IDC_SPEEDBARagent4	    109
#define IDC_SPEEDBARagent5	    110
#define IDC_SPEEDBARagent6	    111
#define IDC_SPEEDBARagent7	    112
#define IDC_SPEEDBARagent8	    113
#define IDC_CLASSLISTscroll     210
#define IDC_MSGBOARDscroll      211

#define IDM_FILEprinterSetup	   101
#define IDM_FILEpageSetup        102
#define IDM_FILEprint            103
#define IDM_FILEsaveAs           104
#define IDM_FILEexit             108
#define IDM_SIMULATEreset        201
#define IDM_SIMULATEstop         202
#define IDM_SIMULATEstep         203
#define IDM_SIMULATErun          204
#define IDM_OPTIONSenvironment   302
#define IDM_HELPabout            304

#define IDS_BATCHFILTER          101
#define IDS_RULEFILTER           102
#define IDS_BINPOSFILTER         103
#define IDS_SIMFILTER            100
#define IDS_WALKFILTER           104

// Hooks for windows functions. WinMain cannot be inlined.

int PASCAL WinMain(HINSTANCE hinstCurrent, HINSTANCE hinstPrevious,
                   LPSTR lpszCmdLine, int nCmdShow);

inline LRESULT _export FAR PASCAL mainWndProcHook(HWND hWnd, UINT message,
                                                  WPARAM wParam, LPARAM lParam)
{ return UI.mainWndProc(hWnd,message,wParam,lParam); }

inline BOOL _export FAR PASCAL aboutHook(HWND hDlg, WORD message, WPARAM wParam,
                                         LPARAM lParam)
{ return UI.about(hDlg,message,wParam,lParam); }

inline BOOL _export FAR PASCAL speedBarHook(HWND hDlg, WORD wMessage,
                                            WPARAM wParam, LPARAM lParam)
{
  return UI.speedBar(hDlg,wMessage,wParam,lParam);
}
inline BOOL _export FAR PASCAL displaySettingsHook(HWND hDlg, WORD message,
                                                   WPARAM wParam, LPARAM lParam)
{ return UI.displaySettings(hDlg,message,wParam,lParam); }

inline BOOL _export FAR PASCAL envSettingsHook(HWND hDlg, WORD wMessage,
                                               WPARAM wParam, LPARAM lParam)
{ return UI.envSettings(hDlg,wMessage,wParam,lParam); }

inline BOOL _export FAR PASCAL agentSettingsHook(HWND hDlg, WORD wMessage,
                                                 WPARAM wParam, LPARAM lParam)
{ return UI.agentSettings(hDlg,wMessage,wParam,lParam); }

inline BOOL _export FAR PASCAL networkSettingsHook(HWND hDlg, WORD wMessage,
                                                   WPARAM wParam, LPARAM lParam)
{ return UI.networkSettings(hDlg,wMessage,wParam,lParam); }
