/*
//    Part of the DVS (http://www.dvs.de) *DVS*VERSION* SDK 
//
//    preview - Captures the incomming video signal and shows this on the monitor.
//
//    This example works only under Windows
*/


#ifndef DVS_SDK_PREVIEW
#define DVS_SDK_PREVIEW

#include <windows.h>
#include <windowsx.h>
#include "resource.h"
#include <vfw.h>

#define PREVIEW_XBORDER 40
#define PREVIEW_YBORDER 40

#define PREVIEW_MAXBUFFERS   4

#include "..\..\header\dvs_setup.h"
#include "..\..\header\dvs_clib.h"
#include "..\..\header\dvs_fifo.h"

#include "..\common\convert2bgra.h"

typedef struct {

  HINSTANCE	hInstance;
  HWND		hWnd;
  HANDLE	hEventRecorded;
  HANDLE	hThreadRecord;
  HANDLE	hThreadDisplay;
  DWORD		pidDisplay;
  DWORD		pidRecord;

  sv_fifo       * pfifo;
  sv_handle     * sv;
  int             test;
  HWND            hSlideH;
  HWND            hSlideV;
  sv_storageinfo  storage;
  int             running;
  int             pause;
  int             pollnocarrier;

  struct {
    CRITICAL_SECTION	mutex;
    int			LastRecordedBuffer;
    int			CurrentDisplayBuffer;
  } mutex;
  struct {
    char *		bufferRGB;
    int			xsize;
    int			ysize;
    int                 scale;
    int                 zoom;
    int                 ystart;
    int                 xstart;
    int                 viewalpha;
    int                 viewanc;
    int                 fields;
    RECT                scaling_zone;
  } display;
  struct {
    int			nbuffer;        /* Current buffer         */
    int			fieldsize;      /* Size of 1 video field  */
    int                 novideoinput;
    int                 noaudioinput;
    int                 carrierquery;
    struct {
      char *		buffer;         /* Aligned buffers        */
      char *		buffer_org;     /* Unaligned buffers      */
    } table[PREVIEW_MAXBUFFERS];
  } record;

  struct {
    int   aivchannels;
    int   aeschannels;
    int   ltc_tc;
    int   ltc_ub;
    int   videoinerror;
    int   audioinerror;
  } status, window;

  struct {
    char buffer[256];
    int  update;
    int  count;
    int  notempty;
  } info;

} preview_handle;


#endif  /* DVS_SDK_PREVIEW */
