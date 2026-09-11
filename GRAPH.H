#ifndef GRAPH_H
#define GRAPH_H

struct regionListType
{
  RECT rc;
  int clear;
  int invalidated;
};

class graphics
{
   enum {numBrushes = 9 , numPens = 9, numFonts = 2};
   int graphicsInitialized;
   HBRUSH hBrushes[numBrushes];
   HPEN hPens[numPens];
   HFONT hFonts[numFonts];

   int maxRegions, regionCount;
   regionListType *regionList;

 public:
   enum { RED = 0, GREEN = 1, BLUE, PURPLE, YELLOW, CYAN, GRAY, WHITE, BLACK };
   enum { FIXED = 0, SYSTEM = 1 };
   graphics();
   ~graphics();
   void drawLine(HDC,int,int,int,int);
   void drawLineTo(HDC,int,int);
   void moveTo(HDC,int,int);
   void drawRectangle(HDC,int,int,int,int);
   void drawRectangle(HDC hDC, RECT &rc);
   void drawRectangle(HDC hDC, RECT &rc, int color);
   void fillRectangle(HDC,int,int,int,int,int);
   void fillRectangle(HDC,RECT &,int);
   void drawCircle(HDC,int,int,int);
   void fillCircle(HDC,int,int,int,int);
   HBRUSH getBrush(int color);
   HPEN getPen(int color);
   HFONT getFont(int font);
   COLORREF setTextColor(HDC hDC, int color);
   void setTextColor(HDC hDC, COLORREF color);
   void printText(HDC,int,int,char*);
   void centerText(HDC hDC, RECT &rc, char *text);
   void getTextSize(HDC hDC, char *str, SIZE *sz);
   void invalidateRgn(HWND hWnd, HRGN hRgn, int clear)
   { RECT temp;  GetRgnBox(hRgn,&temp);  InvalidateRect(hWnd,&temp,clear); }
   RECT getRegion(HRGN hRgn)
   { RECT temp;  GetRgnBox(hRgn,&temp);  return temp; }

};

extern graphics G;    // global graphics object

#endif
