#define STRICT
#include<windows.h>
#include<except.h>
#include<string.h>   // for strlen() function
#include"graph.h"

extern int error(int, char *);  // from ei.cpp

graphics::graphics() : graphicsInitialized(0), regionCount(0), maxRegions(20)
{
  graphicsInitialized = 1;

   enum { RED = 0, GREEN = 1, BLUE, PURPLE, YELLOW, CYAN, GRAY, WHITE, BLACK };

  hBrushes[RED]    = CreateSolidBrush(RGB(255,  0,  0));
  hBrushes[GREEN]  = CreateSolidBrush(RGB(  0,255,  0));
  hBrushes[BLUE]   = CreateSolidBrush(RGB(  0,  0,255));
  hBrushes[PURPLE] = CreateSolidBrush(RGB(255,  0,255));
  hBrushes[YELLOW] = CreateSolidBrush(RGB(255,255,  0));
  hBrushes[CYAN]   = CreateSolidBrush(RGB(  0,255,255));
  hBrushes[GRAY]   = CreateSolidBrush(RGB(192,192,192));
  hBrushes[WHITE]  = CreateSolidBrush(RGB(255,255,255));
  hBrushes[BLACK]  = CreateSolidBrush(RGB(  0,  0,  0));

  hPens[RED]    = CreatePen(PS_SOLID,1,RGB(255,  0,  0));
  hPens[GREEN]  = CreatePen(PS_SOLID,1,RGB(  0,255,  0));
  hPens[BLUE]   = CreatePen(PS_SOLID,1,RGB(  0,  0,255));
  hPens[PURPLE] = CreatePen(PS_SOLID,1,RGB(255,  0,255));
  hPens[YELLOW] = CreatePen(PS_SOLID,1,RGB(255,255,  0));
  hPens[CYAN]   = CreatePen(PS_SOLID,1,RGB(  0,255,255));
  hPens[GRAY]   = CreatePen(PS_SOLID,1,RGB(192,192,192));
  hPens[WHITE]  = CreatePen(PS_SOLID,1,RGB(255,255,255));
  hPens[BLACK]  = CreatePen(PS_SOLID,1,RGB(  0,  0,  0));

  hFonts[FIXED]  = (HFONT) GetStockObject(OEM_FIXED_FONT);
  hFonts[SYSTEM] = (HFONT) GetStockObject(SYSTEM_FONT);

  try { regionList = new regionListType[maxRegions]; }
  catch(xalloc) { error(0,"graphics::graphics()"); }

  // check for successful allocation
  for (int i=0; i<numBrushes; i++)
    if (!hBrushes[i]) error(0,"graphics::graphics()");
  for (i=0; i<numPens; i++)
    if (!hPens[i]) error(0,"graphics::graphics()");
  for (i=0; i<numFonts; i++)
    if (!hFonts[i]) error(0,"graphics::graphics()");
}

graphics::~graphics()
{
 for (int i=0; i<numBrushes; i++)
   DeleteObject(hBrushes[i]);
 for (i=0; i<numPens; i++)
   DeleteObject(hPens[i]);
 for (i=0; i<numFonts; i++)
   DeleteObject(hFonts[i]);
 delete [] regionList;

 graphicsInitialized = 0;
}

void graphics::drawLine(HDC hDC, int x1,int y1,int x2,int y2)
{
  MoveTo(hDC,x1,y1);
  LineTo(hDC,x2,y2);
}

void graphics::drawLineTo(HDC hDC, int x, int y)
{
  LineTo(hDC,x,y);
}

void graphics::moveTo(HDC hDC, int x, int y)
{
  MoveTo(hDC,x,y);
}

void graphics::drawRectangle(HDC hDC, int x1,int y1,int x2,int y2)
{
  Rectangle(hDC,x1,y1,x2,y2);
}

void graphics::drawRectangle(HDC hDC, RECT &rc)
{
  Rectangle(hDC,rc.left,rc.top,rc.right,rc.bottom);
}

void graphics::drawRectangle(HDC hDC, RECT &rc, int color)
{
  HPEN hPen = (HPEN) SelectObject(hDC,hPens[color]);
  Rectangle(hDC,rc.left,rc.top,rc.right,rc.bottom);
  SelectObject(hDC,hPen);
}

void graphics::fillRectangle(HDC hDC, int x1,int y1,int x2,int y2,int color)
{
  HBRUSH hOldBrush = (HBRUSH) SelectObject(hDC,hBrushes[color]);
  Rectangle(hDC,x1,y1,x2,y2);
  SelectObject(hDC,hOldBrush);
}

void graphics::fillRectangle(HDC hDC, RECT &rc, int color)
{
  HBRUSH hOldBrush = (HBRUSH) SelectObject(hDC,hBrushes[color]);
  Rectangle(hDC,rc.left,rc.top,rc.right,rc.bottom);
  SelectObject(hDC,hOldBrush);
}

void graphics::drawCircle(HDC hDC, int x, int y, int radius)
{
  Ellipse(hDC,x-radius,y-radius,x+radius,y+radius);
}

void graphics::fillCircle(HDC hDC, int x, int y, int radius, int color)
{
  HBRUSH hOldBrush = (HBRUSH) SelectObject(hDC,hBrushes[color]);
  Ellipse(hDC,x-radius,y-radius,x+radius,y+radius);
  SelectObject(hDC,hOldBrush);
}

HBRUSH graphics::getBrush(int color)
{
  if (color >= numBrushes) error(1,"graphics::getBrush()");
  return hBrushes[color];
}

HPEN graphics::getPen(int color)
{
  if (color >= numPens) error(1,"graphics::getPen()");
  return hPens[color];
}

HFONT graphics::getFont(int font)
{
  if (font >= numFonts) error(1,"graphics::getFont()");
  return hFonts[font];
}

COLORREF graphics::setTextColor(HDC hDC, int color)
{
  COLORREF temp;
  switch(color)
  {
    case BLACK:  temp = SetTextColor(hDC,RGB(  0,  0,  0)); break;
    case RED:    temp = SetTextColor(hDC,RGB(255,  0,  0)); break;
    case GREEN:  temp = SetTextColor(hDC,RGB(  0,255,  0)); break;
    case BLUE:   temp = SetTextColor(hDC,RGB(  0,  0,255)); break;
    case PURPLE: temp = SetTextColor(hDC,RGB(255,  0,255)); break;
    case YELLOW: temp = SetTextColor(hDC,RGB(255,255,  0)); break;
    case WHITE:  temp = SetTextColor(hDC,RGB(255,255,255)); break;
  }

  return temp;
}

void graphics::setTextColor(HDC hDC, COLORREF color)
{
  SetTextColor(hDC,color);
}

void graphics::printText(HDC hDC, int x, int y, char *text)
{
  TextOut(hDC,x,y,text,strlen(text));
}

void graphics::centerText(HDC hDC, RECT &rc, char *text)
{
  DrawText(hDC,text,strlen(text),&rc,
           DT_CENTER | DT_VCENTER | DT_NOCLIP | DT_SINGLELINE);
}

void graphics::getTextSize(HDC hDC, char *str, SIZE *sz)
{
  GetTextExtentPoint(hDC,str,strlen(str),sz);
}


