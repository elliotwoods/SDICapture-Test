/*
//    Part of the DVS (http://www.dvs.de) *DVS*VERSION* SDK 
//
//    proxy - Captures the outgoing downscaled video signal and shows this on
//    the monitor.
//
//    This example works only under Windows
*/


#ifndef DVS_SDK_PROXY
#define DVS_SDK_PROXY

#include <windows.h>
#include <windowsx.h>
#include <vfw.h>
#include "resource.h"

#define PROXY_MAXBUFFERS 4

#define WINDOW_XBORDER 40
#define WINDOW_YBORDER 40


#include "..\..\header\dvs_setup.h"
#include "..\..\header\dvs_clib.h"
#include "..\..\header\dvs_fifo.h"

#include "..\common\convert2bgra.h"
#include "..\common\analyzer.h"
#include "..\common\dvs_support.h"

typedef struct {
  HINSTANCE hInstance;
  HWND      hWnd;
  HANDLE    hEventRecorded;
  HANDLE    hThreadRecord;
  HANDLE    hThreadDisplay;
  DWORD     pidDisplay;
  DWORD     pidRecord;
  HWND      hSlideH;
  HWND      hSlideV;

  sv_handle * sv;
  int         running;
  int         pause;
  int         field;
  int         clearbuffer;
  int         apilevel;

  struct {
    CRITICAL_SECTION mutex;
    struct {
      CRITICAL_SECTION mutex;
      int              bufferid;
      int              available;
    } record, display;
  } mutex;
  struct {
    char * buffer;
    int	xsize;
    int	ysize;
    int scale;
    int zoom;
    int ystart;
    int xstart;
    int planesmask;
    int showalpha;
  } display;

  struct {
    int	nbuffer;        /* Current buffer         */
    struct {
      char * buffer;         /* Aligned buffers        */
      char * buffer_org;     /* Unaligned buffers      */
      sv_capturebuffer bufferinfo;
    } table[PROXY_MAXBUFFERS];
  } record;

  struct {
    int storagemode;
    int xsize;
    int ysize;
    int buffersize;
    int timecode;
    int framenumber;
    int buffertype;
    int updated;
  } proxy;

  struct {
    char buffer[256];
    int  update;
    int  notempty;
    int  tick;
    int  dropped;
  } info;

  struct {
    int operation;
  } analyzer;

  char error[128];

  struct {
    int acache[2048];
    int vcache[2048];
    int cachepos;
  } avdelay;

} proxy_handle;

#define PLANEMASK_RED     1
#define PLANEMASK_GREEN   2
#define PLANEMASK_BLUE    4


#endif  /* DVS_SDK_PROXY */
