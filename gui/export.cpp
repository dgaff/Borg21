//============================================================================
// Export the field panel as a PNG.
//
// WHAT THIS REPLACES
//
// Two 1995 menu items, neither portable:
//
//   File > Save Meta File     userInterface::createMetaFile() -- opened a
//                             Windows Metafile DC with CreateMetaFile(), replayed
//                             the field drawing into it, and wrote a .WMF.  A
//                             metafile is a recording of GDI calls; there is
//                             nothing to port it to.
//
//   makeBitmap()              built a BITMAPINFO by hand, blitted the client area
//                             into a DDB, and wrote a .BMP a byte at a time
//                             (CreateBitmapInfoStruct / CreateBMPFile).
//
// A PNG of the rendered panel replaces both, and is what a figure actually wants.
//
// HOW
//
// The panel is not drawn into a texture of its own -- it is part of the window's
// draw list, like everything else -- so the pixels exist only after the frame has
// been rendered.  main() therefore calls this between
// ImGui_ImplSDLRenderer3_RenderDrawData() and SDL_RenderPresent(), and the rect
// to cut out is the one guiPanelField() recorded while drawing.
//
// The whole target is read back and cropped on the CPU rather than passing a rect
// to SDL_RenderReadPixels, because the renderer is under a non-unit
// SDL_SetRenderScale on a Retina display and a rect argument would be in logical
// units while the crop wants pixels.  Reading everything and cropping here keeps
// the coordinate space unambiguous.
//============================================================================

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL.h>

#include "borggui.h"
#include <host.h>

// The global error() hook, declared the way GRAPH.CPP declared it in 1995
// ("extern int error(int, const char *);  // from ei.cpp") rather than by pulling
// the whole of EI.H -- Layer 3 has no business including Layer 2's interface.
extern int error(int num, const char *str);

//----------------------------------------------------------------------------
// stb_image_write is third-party (gui/vendor/stb); its warnings are not ours to
// fix and it must not be edited, so they are suppressed for this one include.
//----------------------------------------------------------------------------
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#pragma clang diagnostic ignored "-Wsign-compare"
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#pragma clang diagnostic pop

extern SDL_Renderer *guiRenderer;

//----------------------------------------------------------------------------
int guiExportFieldPNG(const char *path, int x, int y, int w, int h)
{
  char msg[1200];

  if (!guiRenderer || w <= 0 || h <= 0)
  {
    snprintf(msg,sizeof(msg),
             "Nothing to export: the Field panel is not visible.");
    error(BORG_ERR_FILE_WRITE,msg);
    return 0;
  }

  SDL_Surface *full = SDL_RenderReadPixels(guiRenderer,NULL);
  if (!full)
  {
    snprintf(msg,sizeof(msg),"Could not read the rendered frame back: %s",
             SDL_GetError());
    error(BORG_ERR_FILE_WRITE,msg);
    return 0;
  }

  // Clip the requested rectangle to what was actually read.
  if (x < 0) { w += x;  x = 0; }
  if (y < 0) { h += y;  y = 0; }
  if (x + w > full->w) w = full->w - x;
  if (y + h > full->h) h = full->h - y;

  if (w <= 0 || h <= 0)
  {
    SDL_DestroySurface(full);
    snprintf(msg,sizeof(msg),
             "Nothing to export: the Field panel is off screen.");
    error(BORG_ERR_FILE_WRITE,msg);
    return 0;
  }

  //--------------------------------------------------------------------------
  // Normalise to 8-bit RGBA however the backbuffer happened to be formatted,
  // then crop.  SDL_ConvertSurface does the format work; two surfaces rather
  // than one is the price of not having to care what the renderer chose.
  //--------------------------------------------------------------------------
  SDL_Surface *rgba = SDL_ConvertSurface(full,SDL_PIXELFORMAT_RGBA32);
  SDL_DestroySurface(full);

  if (!rgba)
  {
    snprintf(msg,sizeof(msg),"Could not convert the frame to RGBA: %s",
             SDL_GetError());
    error(BORG_ERR_FILE_WRITE,msg);
    return 0;
  }

  unsigned char *out = (unsigned char*)malloc((size_t)w*(size_t)h*4);
  if (!out)
  {
    SDL_DestroySurface(rgba);
    error(0,"guiExportFieldPNG");          // 1995 code 0: allocation failure
    return 0;
  }

  const unsigned char *src = (const unsigned char*)rgba->pixels;
  for (int row = 0; row < h; row++)
    memcpy(out + (size_t)row*(size_t)w*4,
           src + (size_t)(y+row)*(size_t)rgba->pitch + (size_t)x*4,
           (size_t)w*4);

  SDL_DestroySurface(rgba);

  int wrote = stbi_write_png(path,w,h,4,out,w*4);
  free(out);

  if (!wrote)
  {
    snprintf(msg,sizeof(msg),"Could not write PNG '%s'",path);
    error(BORG_ERR_FILE_WRITE,msg);
    return 0;
  }

  return 1;
}
