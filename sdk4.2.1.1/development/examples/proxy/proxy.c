/*
//
//    Part of the DVS (http://www.dvs.de) *DVS*VERSION* SDK 
//
//    proxy - Captures the outgoing downscaled video signal and shows this on
//    the monitor.
//
//    This example works only under Windows
*/

#include "proxy.h"

int proxy_reset(proxy_handle * handle, int scale);
int proxy_resize(proxy_handle * handle, int xsize, int ysize);

void __cdecl Trace(char * string, ...)
{
  va_list va;
  char    ach[1024];
  
  va_start(va, string);

  wvsprintf(ach, string, va);

  va_end(va);

  OutputDebugString(ach);
}

int __cdecl Error(char * string, ...)
{
  va_list va;
  char    ach[1024];
  int res;
  
  va_start(va, string);

  wvsprintf(ach, string, va);

  va_end(va);

  OutputDebugString(ach);

  res = MessageBox(NULL, ach, "Proxy Error", MB_TASKMODAL | MB_ABORTRETRYIGNORE);

  switch(res) {
  case IDABORT:
    PostQuitMessage(0);
    break;
  case IDRETRY:
    return TRUE;
  case IDIGNORE:
  default:;
  }

  return FALSE;
}

#define ANALYZER_AVSYNC  98
#define ANALYZER_AVDELAY 99

void analyzer_avdelay(proxy_handle * handle, unsigned char * pdest, int xsize, int ysize, unsigned char * vbuffer, unsigned char * abuffer, int audiosize, int avsync)
{
  unsigned char * p = vbuffer;
  int * pint;
  int arraylength = 16;
  int adata[256];
  int vdata[256];
  int slice;
  int i,x,y;
  int luma;
  int pos;
  int nexty;
  int startpos;

  if(avsync) {
    arraylength = 64;
  }
  slice = (xsize + arraylength - 1) / arraylength;

  if(!handle->pause) {
    memset(vdata, 0, sizeof(vdata));
    memset(adata, 0, sizeof(adata));

    // Video
    for(y = 0; y < ysize; y++) {
      for(x = 0; x < xsize; x+=slice) {
        luma = 0;
        for(i = x; (i < xsize) && (i < x + slice); i++, p+=2) {
          luma += p[1];
        }
        vdata[x/slice] += luma / slice;
      }
    }
    vdata[arraylength-1] = -20 * ysize;

    // Audio
    for(luma = 0, pint = (int*)abuffer, y = x = 0; (x < audiosize) && (y < arraylength); x+=64, y++) {
      luma += *pint;
      if(luma < 0) luma = -luma;
      if(luma > 1<<30) luma = 0xf0; else
      if(luma > 1<<28) luma = 0xe0; else
      if(luma > 1<<26) luma = 0xd0; else
      if(luma > 1<<24) luma = 0xc0; else
      if(luma > 1<<22) luma = 0xb0; else
      if(luma > 1<<20) luma = 0x90; else
      if(luma > 1<<16) luma = 0x70; else
      if(luma > 1<<14) luma = 0x50; else
      if(luma > 1<<12) luma = 0x30; else
      if(luma > 1<<10) luma = 0x10; else
      luma = 0;

      adata[y] += luma;

      pint += 16 *((audiosize / 64) / arraylength);
    }

    // Cache
    for(x = 0; x < arraylength; x++) {
      handle->avdelay.vcache[handle->avdelay.cachepos] = vdata[x] / ysize;
      handle->avdelay.acache[handle->avdelay.cachepos] = adata[x];
      handle->avdelay.cachepos++;
      if(handle->avdelay.cachepos >= arraysize(handle->avdelay.vcache)) {
        handle->avdelay.cachepos = 0;
      }
    }
  }

  // Video
  memset(pdest, 0, 4 * xsize * ysize);

  if(avsync) { 
    // Find avsync position
    pos = handle->avdelay.cachepos;
    for(y = x = 0, startpos = -1; (startpos == -1) && (x < arraysize(handle->avdelay.vcache)); x++) {
      if(--pos < 0) {
        pos = arraysize(handle->avdelay.vcache) - 1;
      }
      if((handle->avdelay.vcache[pos] > 100) && (x > xsize/2)) {
        startpos = pos + xsize / 2;
        if(startpos >= arraysize(handle->avdelay.vcache)) {
          startpos -= arraysize(handle->avdelay.vcache);
        }
      }
    }
    if(startpos < 0) {
      startpos = handle->avdelay.cachepos;
    }
  } else {
    startpos = handle->avdelay.cachepos;
  } 

  pos = startpos;
  for(y = x = 0; (x < arraysize(handle->avdelay.vcache)) && (x < xsize); x++) {
    nexty = 50 + handle->avdelay.vcache[pos--]; 
    if(pos < 0) {
      pos = arraysize(handle->avdelay.vcache) - 1;
    }
    if((y > 0) && (y < ysize)) {
      pdest[4*(xsize*y+x)+1] = 0xff;
    }
    if(nexty > y) {
      for(;(y < nexty) && (y < ysize); y++) {
        pdest[4*(xsize*y+x)+1] = 0xff;
      }
    } else {
      for(;(y > nexty) && (y < ysize) && (y > 0); y--) {
        pdest[4*(xsize*y+x)+1] = 0xff;
      }
    }
  }

  // Audio
  pos = startpos;
  for(y = x = 0; (x < arraysize(handle->avdelay.vcache)) && (x < xsize); x++) {
    nexty = 50 + handle->avdelay.acache[pos--]; 
    if(pos < 0) {
      pos = arraysize(handle->avdelay.vcache) - 1;
    }
    if((y > 0) && (y < ysize)) {
      pdest[4*(xsize*y+x)  ] = 0xff;
      pdest[4*(xsize*y+x)+2] = 0xff;
    }
    if(nexty > y) {
      for(;(y < nexty) && (y < ysize); y++) {
        pdest[4*(xsize*y+x)  ] = 0xff;
        pdest[4*(xsize*y+x)+2] = 0xff;
      }
    } else {
      for(;(y > nexty) && (y < ysize) && (y > 0); y--) {
        pdest[4*(xsize*y+x)  ] = 0xff;
        pdest[4*(xsize*y+x)+2] = 0xff;
      }
    }
  }
}

void convert_tobgra_lut(unsigned char * psource, int lutsize, unsigned char * pdest, int xsize, int ysize)
{
  unsigned int * plut;
  int x,y,nexty,c;
  int lutcomponents = 4;
  int range = 0x10000;
  int bgra[4] = {2,1,0,3};

  memset(pdest, 0, 4 * xsize * ysize);
  
  for(c = 0; c < lutcomponents; c++) {
    plut = (unsigned int *)(psource + bgra[c]*lutsize);
    for(x = y = 0; x < xsize; x++) {
      nexty = (ysize * plut[x]) / range;
      pdest[4*(xsize*y+x)+c] = 0xff;
      if(nexty > y) {
        for(;(y < nexty) && (y < ysize); y++) {
          pdest[4*(xsize*y+x)+c] = 0xff;
        }
      } else {
        for(;(y > nexty) && (y > 0); y--) {
          pdest[4*(xsize*y+x)+c] = 0xff;
        }
      }
    }
  }
}

/*
//  Paint the RGB bitmap to the computer monitor
*/
int proxy_display_draw(proxy_handle * handle, HANDLE hDrawDIB, BITMAPINFOHEADER * pbmiHeader, int eyemode)
{
  HDC hdcWindow;
  int res;
  int xdst  = 7;                 
  int ydst  = 7;
  int dxdst = handle->proxy.xsize * handle->display.scale / 0x100; 
  int dydst = handle->proxy.ysize * handle->display.scale / 0x100;
  int xsrc  = handle->display.xstart; 
  int ysrc  = handle->display.ystart;
  int dxsrc = handle->proxy.xsize / handle->display.zoom;
  int dysrc = handle->proxy.ysize / handle->display.zoom;

  hdcWindow = GetDC(handle->hWnd);
  if(hdcWindow == NULL) {
    Error("proxy_display_draw: GetDC(handle->hWnd) failed = %d", GetLastError());
    return FALSE;
  }

  switch(eyemode) {
  case SV_EYEMODE_LEFT:
    dxdst = dxdst / 2;
    break;
  case SV_EYEMODE_RIGHT:
    xdst  = 7 + dxdst / 2;
    dxdst = dxdst / 2;
    break;
  }

  res = DrawDibDraw(hDrawDIB, hdcWindow,
                    xdst, ydst, dxdst, dydst,
                    pbmiHeader, 
                    handle->display.buffer, 
                    xsrc, ysrc, dxsrc, dysrc,
                    DDF_SAME_DRAW ); 

  if(!res && handle->running){
    Error("proxy_display_draw: DrawDibDraw(handle->display.hDrawDIB,..) failed = %d", GetLastError());
    return FALSE;
   }

  res = ReleaseDC(handle->hWnd, hdcWindow);
  if(!res) {
    Error("proxy_display_draw: ReleaseDC(hdcWindow) failed = %d", GetLastError());
    return FALSE;
  }

  return TRUE;
} 


void proxy_status(proxy_handle * handle, char * text)
{
  char buffer[256];

  strcpy(handle->info.buffer, text); 
  handle->info.update     = TRUE;
  handle->info.notempty   = text[0] != 0;

  if(handle->info.notempty) {
    wsprintf(buffer, "Proxy - %s", text);
  } else {
    wsprintf(buffer, "Proxy");
  }
          
  SetWindowText(handle->hWnd, buffer);
}


int proxy_status_display(proxy_handle * handle)
{
  char string[512];

  if(handle->error[0]) {
    wsprintf(string, "Error: %s", handle->error); handle->error[0] = 0;
  } else {
    wsprintf(string, "Resolution %dx%d %s/%s %08x %d %d", handle->proxy.xsize, handle->proxy.ysize, sv_support_colormode2string_mode(handle->proxy.storagemode), sv_support_bit2string_mode(handle->proxy.storagemode),handle->proxy.timecode, handle->proxy.framenumber, handle->proxy.buffertype);
  }

  proxy_status(handle, string);

  return TRUE;
}

/*
//  Thread which reads video from card's memory  
*/
DWORD proxy_record_thread(void * voidhandle)
{
  proxy_handle * handle = voidhandle;
  sv_capturebuffer * pbufferinfo;
  char *             pbuffer;
  int res               = SV_OK;
  int running           = TRUE;
  int i                 = 0;
  int apilevel;
  int apiflags;

  // allocate buffers
  if(running) {
    for(i = 0; i < PROXY_MAXBUFFERS; i++) {
      if((handle->record.table[i].buffer_org = malloc(handle->proxy.buffersize + 0x1000)) == NULL) {
        running = FALSE;
      } else {
        handle->record.table[i].buffer = (void *)(((uintptr)handle->record.table[i].buffer_org + 0xfff) & ~0xfff); 
      }
    }
    if(!running) {
      Error("proxy_record_thread: malloc(%d) failed", handle->proxy.buffersize);
    }
  }
  
  while(running && handle->running) {
    EnterCriticalSection(&handle->mutex.record.mutex);

    pbuffer     = handle->record.table[handle->record.nbuffer].buffer;
    pbufferinfo = &handle->record.table[handle->record.nbuffer].bufferinfo;

    if(running && !handle->pause) {
      if(handle->clearbuffer) {
        memset((char*)handle->record.table[handle->record.nbuffer].buffer, 0, handle->proxy.buffersize);
      }

      do {
        // Set function level to use.
        // 1 - original
        // 2 - timecode returns.
        // 3 - stereo returns
        // 4 - audio
        if(handle->apilevel) {
          apilevel = handle->apilevel;
        } else {
          apilevel = 4;
        }

        apiflags = SV_CAPTURE_FLAG_WAITFORNEXT | SV_CAPTURE_FLAG_AUDIO;
        if(handle->field) {
          apiflags |= SV_CAPTURE_FLAG_ONLYFIELD;
        }

        // get last ready proxy buffer
        res = sv_captureex(handle->sv, (char*)pbuffer, handle->proxy.buffersize, &handle->record.table[handle->record.nbuffer].bufferinfo, apilevel, apiflags, NULL);

        if(res == SV_ERROR_NOTREADY) {
          Sleep(5);
        } else if(res != SV_OK) {
          Error("proxy_record_thread: sv_capture() failed = %d '%s'", res, sv_geterrortext(res));
          running = FALSE;
        }
      } while(handle->running && running && (res == SV_ERROR_NOTREADY));

      if(pbufferinfo->buffertype == 3) {
        pbufferinfo->storagemode = pbufferinfo->ysize;
        pbufferinfo->ysize       = 512;
      }
    }

    if(handle->pause) {
      // avoid drop counter increment during pause
      handle->info.tick = 0;
    }

    // do drop detection
    if(handle->info.tick) {
      handle->info.dropped += (pbufferinfo->tick - handle->info.tick) - 1;
    }
    handle->info.tick = pbufferinfo->tick;

    EnterCriticalSection(&handle->mutex.mutex);
    handle->mutex.record.available = TRUE;
    handle->mutex.record.bufferid  = handle->record.nbuffer;
    if(!handle->pause) {
      do {
        if(++handle->record.nbuffer >= PROXY_MAXBUFFERS) {
          handle->record.nbuffer = 0;
        }
      } while(handle->record.nbuffer == handle->mutex.display.bufferid);
    }
    LeaveCriticalSection(&handle->mutex.mutex);

    LeaveCriticalSection(&handle->mutex.record.mutex);

    res = SetEvent(handle->hEventRecorded);
    if(!res) {
      running = Error("proxy_record_ready: SetEvent(hEventRecorded) failed");
    }
  }

  handle->running = FALSE;
  SetEvent(handle->hEventRecorded); // Release the display thread.

  ExitThread(res); 
}

/*
//  Thread which converts and paints the video to the screen
*/
DWORD proxy_display_thread(proxy_handle * handle)
{
  BITMAPINFOHEADER    bmiHeader;
  HDC                 hdcMem     = NULL;
  HBITMAP             hBitmap    = NULL;
  HDRAWDIB	      hDrawDIB   = NULL;
  sv_capturebuffer *  pbufferinfo;
  unsigned char *     buffer;

  int res     = 0;
  int running = TRUE;
  int resize = FALSE;

  while(running && handle->running) {
    res = WaitForSingleObject(handle->hEventRecorded, INFINITE);
    if(res != WAIT_OBJECT_0)  {
      running = Error("proxy_display_thread: WaitForSingleObject(handle->hEventRecorded)=%d gle=%d", res, GetLastError());
    }

    // When there is a new buffer to display, update buffers counter
    res = ResetEvent(handle->hEventRecorded);
    if(!res)  {
      running = Error("proxy_display_thread: ResetEvent(handle->hEventRecorded) failed = %d", GetLastError());
    }

    if(running && handle->running) {
      // If no buffer is record yet skip this run
      if(!handle->mutex.record.available) {
        Sleep(10);
        continue;
      }
    }

    if(running && handle->running) {
      EnterCriticalSection(&handle->mutex.display.mutex);

      EnterCriticalSection(&handle->mutex.mutex);
      handle->mutex.display.bufferid = handle->mutex.record.bufferid;  
      LeaveCriticalSection(&handle->mutex.mutex);

      pbufferinfo = &handle->record.table[handle->mutex.display.bufferid].bufferinfo;
      buffer      = (unsigned char *)handle->record.table[handle->mutex.display.bufferid].buffer;

      if((pbufferinfo->xsize != handle->proxy.xsize) || (pbufferinfo->ysize != handle->proxy.ysize)) {
        resize = TRUE;
      }

      if(resize) {
        resize = FALSE;

        if(hDrawDIB) {
          res = DrawDibEnd(hDrawDIB);
          if(!res)  {
            Error("proxy_display_exit: DrawDibEnd(hDrawDIB) failed = %d", GetLastError());
          }
      
          res = DrawDibClose(hDrawDIB);
          if(!res)  {
            Error("proxy_display_exit: DrawDibClose(handle->display.hDrawDIB) failed = %d", GetLastError());
          }
          hDrawDIB = NULL;
        }
    
        if(hBitmap) {
          DeleteObject(hBitmap); hBitmap = NULL;
        }
      
        if(hdcMem) {
          DeleteDC(hdcMem); hdcMem = NULL;
        }

        res = proxy_resize(handle, pbufferinfo->xsize, pbufferinfo->ysize);
        if(res != SV_OK) {
          Error("proxy_display_thread: resize failed = %d", res);
          running = FALSE;
        }

        if(running) {
          memset(&bmiHeader, 0, sizeof(bmiHeader));
          bmiHeader.biSize             = sizeof(BITMAPINFOHEADER);
          bmiHeader.biHeight           = handle->proxy.ysize;
          bmiHeader.biWidth            = handle->proxy.xsize;
          bmiHeader.biBitCount         = 32;
          bmiHeader.biCompression      = BI_RGB;
          bmiHeader.biSizeImage        = bmiHeader.biWidth * bmiHeader.biHeight * bmiHeader.biBitCount / 8; // Div by 8 we have bytes
          bmiHeader.biXPelsPerMeter    = 0;
          bmiHeader.biYPelsPerMeter    = 0;
          bmiHeader.biClrUsed          = 0;
          bmiHeader.biClrImportant     = 0;
          bmiHeader.biPlanes           = 1;
        }
      
        if(running) {
          hdcMem  = CreateCompatibleDC(NULL);
          if(!hdcMem) {
            Error("proxy_display_thread: CreateCompatibleDC(NULL) failed = %d", GetLastError());
            running = FALSE;
          }
        }
      
        if(running) {
          hBitmap = CreateDIBSection(hdcMem, (BITMAPINFO *)&bmiHeader, DIB_RGB_COLORS, (void**)&handle->display.buffer, NULL, 0 );  
          if(!hBitmap) {
            Error("proxy_display_thread: CreateDIBSection(hdcMem,...) failed = %d", GetLastError());
            running = FALSE;
          }
        }
      
        if(running) {
          hDrawDIB = DrawDibOpen();
      
          if(hDrawDIB == NULL)  {
            Error("proxy_display_thread: DrawDibOpen() failed = %d", GetLastError());
            running = FALSE;
          }
        }
      
        if(running) {
          res = DrawDibBegin(hDrawDIB, NULL, 
  	        handle->proxy.xsize, handle->proxy.ysize, 
  	        &bmiHeader,
  	        handle->proxy.xsize, handle->proxy.ysize, 
  	        DDF_ANIMATE); 
          if(!res ) {
            Error("proxy_display_thread: DrawDibBegin(...) failed = %d", GetLastError());
            running = FALSE;
          }
        }
      }

      if(running && handle->running) {
        if(pbufferinfo->buffertype == 3) {
          convert_tobgra_lut(buffer, pbufferinfo->lineoffset, (unsigned char*)handle->display.buffer, pbufferinfo->xsize, pbufferinfo->ysize);
        } else if(handle->analyzer.operation == ANALYZER_AVDELAY) {
          analyzer_avdelay(handle, (unsigned char*)handle->display.buffer, handle->proxy.xsize, handle->proxy.ysize, buffer, (unsigned char*)(buffer + pbufferinfo->audiooffset), pbufferinfo->audiosize, FALSE);
        } else if(handle->analyzer.operation == ANALYZER_AVSYNC) {
          analyzer_avdelay(handle, (unsigned char*)handle->display.buffer, handle->proxy.xsize, handle->proxy.ysize, buffer, (unsigned char*)(buffer + pbufferinfo->audiooffset), pbufferinfo->audiosize, TRUE);
        } else if(!convert_tobgra(buffer, (unsigned char*)handle->display.buffer, pbufferinfo->xsize, pbufferinfo->ysize, 1, pbufferinfo->lineoffset, pbufferinfo->storagemode, 0, handle->display.showalpha, 0)) {
          wsprintf(handle->error, "convert_tobgra failed");
        }

        handle->proxy.storagemode = pbufferinfo->storagemode;
        handle->proxy.buffertype  = pbufferinfo->buffertype;
        handle->proxy.timecode    = pbufferinfo->timecode;
        handle->proxy.framenumber = pbufferinfo->framenumber;
        handle->proxy.updated     = TRUE;

        if(handle->display.planesmask && !handle->display.showalpha) {
          unsigned int   mask   = (unsigned int)~0;
          int            pixels = handle->proxy.xsize * handle->proxy.ysize;
          unsigned int * ptr    = (unsigned int*)handle->display.buffer;
          int            i;
  
          if(handle->display.planesmask & PLANEMASK_RED) {
            mask &= ~0x00ff0000;
          }
          if(handle->display.planesmask & PLANEMASK_GREEN) {
            mask &= ~0x0000ff00;
          }
          if(handle->display.planesmask & PLANEMASK_BLUE) {
            mask &= ~0x000000ff;
          }
  
          for(i = 0; i < pixels; i++) {
            *(ptr++) &= mask;
          }
        } else if(handle->analyzer.operation) {
          analyzer_buffer dest;
          analyzer_buffer source;
          analyzer_options options = {0};
          options.operation   = handle->analyzer.operation;

          dest.pbuffer   = (unsigned char *)handle->display.buffer;
          dest.xsize     = handle->proxy.xsize;
          dest.ysize     = handle->proxy.ysize;
          source.pbuffer = (unsigned char *)handle->display.buffer;
          source.xsize   = handle->proxy.xsize;
          source.ysize   = handle->proxy.ysize;

          analyzer(&options, &dest, &source);
        }
  
        if(!proxy_display_draw(handle, hDrawDIB, &bmiHeader, pbufferinfo->eyemode)) {
          running = FALSE;
        }
      }
  
      EnterCriticalSection(&handle->mutex.mutex);
      handle->mutex.display.bufferid = -1;
      LeaveCriticalSection(&handle->mutex.mutex);
  
      LeaveCriticalSection(&handle->mutex.display.mutex);
    } 
  }

  if(hDrawDIB) {
    res = DrawDibEnd(hDrawDIB);
    if(!res)  {
      Error("proxy_display_exit: DrawDibEnd(hDrawDIB) failed = %d", GetLastError());
    }

    res = DrawDibClose(hDrawDIB);
    if(!res)  {
      Error("proxy_display_exit: DrawDibClose(handle->display.hDrawDIB) failed = %d", GetLastError());
    }
    hDrawDIB = NULL;
  }

  if(hBitmap) {
    DeleteObject(hBitmap); hBitmap = NULL;
  }

  if(hdcMem) {
    DeleteDC(hdcMem); hdcMem = NULL;
  }

  ExitThread(res); 
}

/*
//  Manages the correct ending of all threads and finalizes proxy structures
*/
void proxy_exit(proxy_handle * handle)
{  
  int res;
  int i;
  DWORD dwExitCode;

  handle->running = FALSE;

  do {
    res = GetExitCodeThread(handle->hThreadDisplay, &dwExitCode);
    if(!res) {
      Error("proxy_exit: GetExitCodeThread(handle->hThreadDisplay,) failed = %d", GetLastError());
    }
    if(dwExitCode == STILL_ACTIVE) {
      Sleep(50);
    }
  } while(res && (dwExitCode == STILL_ACTIVE)); 

  do {
    res = GetExitCodeThread(handle->hThreadRecord, &dwExitCode);
    if(!res) {
      Error("proxy_exit: GetExitCodeThread(handle->hThreadRecord,) failed = %d", GetLastError());
    }
    if(dwExitCode == STILL_ACTIVE) {
      Sleep(50);
    } else{
      for(i = 0; i < PROXY_MAXBUFFERS; i++) {  // When it is sure that displaythread has finished
        if(handle->record.table[i].buffer_org) {  // we free the pointers.
          free(handle->record.table[i].buffer_org);
        }
      }
    }
  } while(res && (dwExitCode == STILL_ACTIVE)); 

  if(handle->hEventRecorded) {
    CloseHandle(handle->hEventRecorded);
    handle->hEventRecorded = NULL;
  }

  if(handle->sv) {
    sv_close(handle->sv);
    handle->sv = NULL;
  }

  DeleteCriticalSection(&handle->mutex.mutex);
  DeleteCriticalSection(&handle->mutex.display.mutex);
  DeleteCriticalSection(&handle->mutex.record.mutex);
}


/*
//  Manages changes caused by scales and zooms
*/
int proxy_reset(proxy_handle * handle, int scale)
{
  RECT           rect;

  EnterCriticalSection(&handle->mutex.display.mutex);

  EnterCriticalSection(&handle->mutex.mutex);   
  handle->mutex.display.bufferid = 0;
  LeaveCriticalSection(&handle->mutex.mutex);

  handle->display.ysize = handle->proxy.ysize * scale / 0x100;
  handle->display.xsize = handle->proxy.xsize * scale / 0x100;

  GetWindowRect(handle->hWnd, &rect);

  ShowWindow(handle->hSlideH, SW_HIDE);
  ShowWindow(handle->hSlideV, SW_HIDE);
  ShowWindow(handle->hWnd,    SW_HIDE);

  MoveWindow(handle->hWnd, rect.left, rect.top, handle->display.xsize + 40, handle->display.ysize + 86, TRUE);
  GetClientRect(handle->hWnd, &rect);
  MoveWindow(handle->hSlideH, rect.left, rect.top + handle->display.ysize + 8, handle->display.xsize + 12, 25, TRUE);
  MoveWindow(handle->hSlideV, rect.left + handle->display.xsize + 8, rect.top + 3, 25, handle->display.ysize + 12, TRUE);
     
  ShowWindow(handle->hWnd   , SW_SHOW);
  ShowWindow(handle->hSlideH, SW_SHOW);
  ShowWindow(handle->hSlideV, SW_SHOW); 

  LeaveCriticalSection(&handle->mutex.display.mutex);

  return TRUE;
}


int proxy_resize(proxy_handle * handle, int xsize, int ysize)
{
  handle->proxy.xsize     = xsize;
  handle->proxy.ysize     = ysize;

  handle->display.scale  = 0x100;

  if(handle->proxy.xsize > 2048) {
    handle->display.scale /= 4;
  } else if(handle->proxy.xsize > 1024) {
    handle->display.scale /= 2;
  }

  handle->display.xsize  = handle->proxy.xsize * handle->display.scale / 0x100;
  handle->display.ysize  = handle->proxy.ysize * handle->display.scale / 0x100;

  handle->display.zoom       = 1;
  handle->display.planesmask = 0;

  // Windows Display resizing.
  proxy_reset(handle, handle->display.scale);

  return SV_OK;
}


/*
//  Thread and proxy structures initialization
*/
int proxy_init(proxy_handle * handle) 
{
  int		 res;

  handle->hEventRecorded = CreateEvent(NULL, TRUE, FALSE, NULL);
  if(handle->hEventRecorded == NULL) {
    Error("proxy_init: CreateEvent(handle->hEventRecorded) failed= %d", GetLastError());
    return FALSE;
  }

  handle->proxy.buffersize  = 4096 * 2160 * 6;

  InitializeCriticalSection(&handle->mutex.mutex);

  InitializeCriticalSection(&handle->mutex.display.mutex);
  InitializeCriticalSection(&handle->mutex.record.mutex);

  handle->mutex.record.bufferid   = 0;
  handle->mutex.display.bufferid  = 0;

  // open device for calling sv_capture() only - other applications still may open the device as usual
  res = sv_openex(&handle->sv, "", SV_OPENPROGRAM_DEMOPROGRAM, SV_OPENTYPE_CAPTURE, 0, 0);
  if(res != SV_OK) {
    Error("proxy_init: Error '%s' opening video device", sv_geterrortext(res));
    return FALSE;
  } 

  res = proxy_resize(handle, 512, 384);
  if(res != SV_OK) {
    sv_close(handle->sv);
    return FALSE;
  }

  // Environmental vars. init

  handle->running        = TRUE;
  handle->display.scale  = 0x100;

  if(handle->proxy.xsize > 1024) {
    handle->display.scale /= 2;
  }

  handle->display.xsize  = handle->proxy.xsize * handle->display.scale / 0x100;
  handle->display.ysize  = handle->proxy.ysize * handle->display.scale / 0x100;

  handle->info.tick      = 0;
  handle->info.dropped   = 0;

  handle->hSlideH = GetDlgItem(handle->hWnd, IDC_SLIDERH);
  handle->hSlideV = GetDlgItem(handle->hWnd, IDC_SLIDERV);
  
  // Thread init

  handle->hThreadRecord = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)proxy_record_thread, handle, 0, &handle->pidRecord);
  if(handle->hThreadRecord == NULL) {
    Error("proxy_init: CreateThread(record) failed = %d", GetLastError());
    handle->running = FALSE;
  }

  handle->hThreadDisplay = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)proxy_display_thread, handle, 0, &handle->pidDisplay);
  if(handle->hThreadDisplay == NULL) {
    Error("proxy_init: CreateThread(display) failed = %d", GetLastError());
    handle->running = FALSE;
  } else{
    SetThreadPriority(handle->hThreadDisplay, THREAD_PRIORITY_IDLE);
  }

  if(!handle->running) {
    proxy_exit(handle);
  }

  return TRUE;
}

/*
//  Handles zooming updates 
*/
void process_zoom(proxy_handle * handle, int zoom)
{
  EnterCriticalSection(&handle->mutex.display.mutex);

  handle->display.zoom = zoom;

  if(zoom == 1) {
    handle->display.xstart = 0;
    handle->display.ystart = 0;
    EnableWindow(handle->hSlideH, FALSE);
    EnableWindow(handle->hSlideV, FALSE); 
  } else {
    EnableWindow(handle->hSlideH, TRUE);
    EnableWindow(handle->hSlideV, TRUE);
  }

  LeaveCriticalSection(&handle->mutex.display.mutex);
}


/*
//  Set menu checkmarks
*/
void proxy_update_menus(proxy_handle * handle)
{
  HMENU hMenu = GetMenu(handle->hWnd);

  /*
  //  Analyzer Menu
  */
  CheckMenuItem(hMenu, ID_ANALYZER_DISABLED,  MF_UNCHECKED);
  CheckMenuItem(hMenu, ID_ANALYZER_HISTOGRAM, MF_UNCHECKED);
  CheckMenuItem(hMenu, ID_ANALYZER_RGBPARADE, MF_UNCHECKED);
  switch(handle->analyzer.operation) {
  case ANALYZER_HISTOGRAM:
    CheckMenuItem(hMenu, ID_ANALYZER_HISTOGRAM, MF_UNCHECKED);
    break;
  case ANALYZER_RGBPARADE:
    CheckMenuItem(hMenu, ID_ANALYZER_RGBPARADE, MF_UNCHECKED);
    break;
  default:
    CheckMenuItem(hMenu, ID_ANALYZER_DISABLED,  MF_UNCHECKED);
  }

  /*
  //  Planes Menu
  */
  if(handle->display.planesmask & PLANEMASK_RED) {
    CheckMenuItem(hMenu, ID_PLANES_RED, MF_UNCHECKED);
  } else {
    CheckMenuItem(hMenu, ID_PLANES_RED, MF_CHECKED);
  }
  if(handle->display.planesmask & PLANEMASK_GREEN) {
    CheckMenuItem(hMenu, ID_PLANES_GREEN, MF_UNCHECKED);
  } else {
    CheckMenuItem(hMenu, ID_PLANES_GREEN, MF_CHECKED);
  }
  if(handle->display.planesmask & PLANEMASK_BLUE) {
    CheckMenuItem(hMenu, ID_PLANES_BLUE, MF_UNCHECKED);
  } else {
    CheckMenuItem(hMenu, ID_PLANES_BLUE, MF_CHECKED);
  }
  if(handle->display.showalpha) {
    CheckMenuItem(hMenu, ID_PLANES_ALPHA, MF_CHECKED);
  } else {
    CheckMenuItem(hMenu, ID_PLANES_ALPHA, MF_UNCHECKED);
  }


  /*
  //  Scale Menu
  */
  CheckMenuItem(hMenu, ID_SCALE_2_1, MF_UNCHECKED);
  CheckMenuItem(hMenu, ID_SCALE_1_1, MF_UNCHECKED);
  CheckMenuItem(hMenu, ID_SCALE_1_2, MF_UNCHECKED);
  CheckMenuItem(hMenu, ID_SCALE_1_4, MF_UNCHECKED);
  CheckMenuItem(hMenu, ID_SCALE_1_8, MF_UNCHECKED);
  switch(handle->display.scale) {
  case 0x200:
    CheckMenuItem(hMenu, ID_SCALE_2_1, MF_CHECKED);
    break;
  case 0x100:
    CheckMenuItem(hMenu, ID_SCALE_1_1, MF_CHECKED);
    break;
  case 0x80:
    CheckMenuItem(hMenu, ID_SCALE_1_2, MF_CHECKED);
    break;
  case 0x40:
    CheckMenuItem(hMenu, ID_SCALE_1_4, MF_CHECKED);
    break;
  case 0x20:
    CheckMenuItem(hMenu, ID_SCALE_1_8, MF_CHECKED);
    break;
  }

  /*
  //  Zoom Menu
  */
  CheckMenuItem(hMenu, ID_ZOOM_1X,  MF_UNCHECKED);
  CheckMenuItem(hMenu, ID_ZOOM_2X,  MF_UNCHECKED);
  CheckMenuItem(hMenu, ID_ZOOM_4X,  MF_UNCHECKED);
  CheckMenuItem(hMenu, ID_ZOOM_8X,  MF_UNCHECKED);
  CheckMenuItem(hMenu, ID_ZOOM_16X, MF_UNCHECKED);
  CheckMenuItem(hMenu, ID_ZOOM_32X, MF_UNCHECKED);
  switch(handle->display.zoom) {
  case 1:
    CheckMenuItem(hMenu, ID_ZOOM_1X, MF_CHECKED);
    break;
  case 2:
    CheckMenuItem(hMenu, ID_ZOOM_2X, MF_CHECKED);
    break;
  case 4:
    CheckMenuItem(hMenu, ID_ZOOM_4X, MF_CHECKED);
    break;
  case 8:
    CheckMenuItem(hMenu, ID_ZOOM_8X, MF_CHECKED);
    break;
  case 16:
    CheckMenuItem(hMenu, ID_ZOOM_16X, MF_CHECKED);
    break;
  case 32:
    CheckMenuItem(hMenu, ID_ZOOM_32X, MF_CHECKED);
    break;
  }


  /*
  //  Apilevel Menu
  */
  CheckMenuItem(hMenu, ID_APILEVEL_DEFAULT, MF_UNCHECKED);
  CheckMenuItem(hMenu, ID_APILEVEL_1,       MF_UNCHECKED);
  CheckMenuItem(hMenu, ID_APILEVEL_2,       MF_UNCHECKED);
  CheckMenuItem(hMenu, ID_APILEVEL_3,       MF_UNCHECKED);
  switch(handle->apilevel) {
  case 0:
    CheckMenuItem(hMenu, ID_APILEVEL_DEFAULT, MF_CHECKED);
    break;
  case 1:
    CheckMenuItem(hMenu, ID_APILEVEL_1, MF_CHECKED);
    break;
  case 2:
    CheckMenuItem(hMenu, ID_APILEVEL_2, MF_CHECKED);
    break;
  case 3:
    CheckMenuItem(hMenu, ID_APILEVEL_3, MF_CHECKED);
    break;
  }

  /*
  //  Misc
  */
  if(handle->pause) {
    CheckMenuItem(hMenu, ID_PAUSE, MF_CHECKED);
  } else {
    CheckMenuItem(hMenu, ID_PAUSE, MF_UNCHECKED);
  }
  if(handle->field) {
    CheckMenuItem(hMenu, ID_FIELD, MF_CHECKED);
  } else {
    CheckMenuItem(hMenu, ID_FIELD, MF_UNCHECKED);
  }
  if(handle->clearbuffer) {
    CheckMenuItem(hMenu, ID_OPTIONS_CLEARBUFFER, MF_CHECKED);
  } else {
    CheckMenuItem(hMenu, ID_OPTIONS_CLEARBUFFER, MF_UNCHECKED);
  }
}

void proxy_redraw(proxy_handle * handle)
{
  SetEvent(handle->hEventRecorded);
}

#define TIMERID 54321

/*
//  Event handler for the proxy dialog
*/
DWORD APIENTRY proxy_dlgproc(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam) 
{ 
  static proxy_handle * handle = NULL;
  int command;
 
  switch(message) {
  case WM_INITDIALOG:
    handle       = (proxy_handle *) lparam;
    handle->hWnd = hWnd;
    proxy_init(handle);
    proxy_update_menus(handle);
    SetTimer(hWnd, TIMERID, 100, NULL);
    return FALSE;

  case WM_COMMAND:
    command = GET_WM_COMMAND_ID(wparam, lparam);

    switch(command) {
    case ID_ANALYZER_DISABLED:
      handle->analyzer.operation = 0;
      proxy_redraw(handle);
      break;
    case ID_ANALYZER_HISTOGRAM:
      handle->analyzer.operation = ANALYZER_HISTOGRAM;
      proxy_redraw(handle);
      break;
    case ID_ANALYZER_RGBPARADE:
      handle->analyzer.operation = ANALYZER_RGBPARADE;
      proxy_redraw(handle);
      break;
    case ID_ANALYZER_AVDELAY:
      handle->analyzer.operation = ANALYZER_AVDELAY;
      proxy_redraw(handle);
      break;
    case ID_ANALYZER_AVSYNC:
      handle->analyzer.operation = ANALYZER_AVSYNC;
      proxy_redraw(handle);
      break;

    case ID_PAUSE:
      handle->pause = !handle->pause;
      break;
    case ID_FIELD:
      handle->field = !handle->field;
      break;


    case ID_OPTIONS_CLEARBUFFER:
      handle->clearbuffer = !handle->clearbuffer;
      break;

    case ID_PLANES_RED:
      if(handle->display.planesmask & PLANEMASK_RED) {
        handle->display.planesmask &= ~PLANEMASK_RED;
      } else {
        handle->display.planesmask |=  PLANEMASK_RED;
      }
      proxy_redraw(handle);
      break;
    case ID_PLANES_GREEN:
      if(handle->display.planesmask & PLANEMASK_GREEN) {
        handle->display.planesmask &= ~PLANEMASK_GREEN;
      } else {
        handle->display.planesmask |=  PLANEMASK_GREEN;
      }
      proxy_redraw(handle);
      break;
    case ID_PLANES_BLUE:
      if(handle->display.planesmask & PLANEMASK_BLUE) {
        handle->display.planesmask &= ~PLANEMASK_BLUE;
      } else {
        handle->display.planesmask |=  PLANEMASK_BLUE;
      }
      proxy_redraw(handle);
      break;
    case ID_PLANES_ALPHA:
      handle->display.showalpha = !handle->display.showalpha;
      break;

    case ID_APILEVEL_DEFAULT:
      handle->apilevel = 0;
      break;
    case ID_APILEVEL_1:
      handle->apilevel = 1;
      break;
    case ID_APILEVEL_2:
      handle->apilevel = 2;
      break;
    case ID_APILEVEL_3:
      handle->apilevel = 3;
      break;

    case ID_SCALE_2_1:
      handle->display.scale = 0x200;
      handle->display.zoom  = 1;
      proxy_reset(handle, handle->display.scale);
      proxy_redraw(handle);
      break;
    case ID_SCALE_1_1:
      handle->display.scale = 0x100;
      handle->display.zoom  = 1;
      proxy_reset(handle, handle->display.scale);
      proxy_redraw(handle);
      break;
    case ID_SCALE_1_2:
      handle->display.scale = 0x80;
      handle->display.zoom  = 1;
      proxy_reset(handle, handle->display.scale);
      proxy_redraw(handle);
      break;
    case ID_SCALE_1_4:
      handle->display.scale = 0x40;
      handle->display.zoom  = 1;
      proxy_reset(handle, handle->display.scale);
      proxy_redraw(handle);
      break;
    case ID_SCALE_1_8: 
      handle->display.scale = 0x20;
      handle->display.zoom  = 1;
      proxy_reset(handle, handle->display.scale);
      proxy_redraw(handle);
      break;

    case ID_ZOOM_1X:
      process_zoom(handle, 1);
      proxy_redraw(handle);
      break;
    case ID_ZOOM_2X:
      process_zoom(handle, 2);
      proxy_redraw(handle);
      break;
    case ID_ZOOM_4X:
      process_zoom(handle, 4);
      proxy_redraw(handle);
      break;
    case ID_ZOOM_8X:
      process_zoom(handle, 8);
      proxy_redraw(handle);
      break;
    case ID_ZOOM_16X:
      process_zoom(handle, 16);
      proxy_redraw(handle);
      break;
    case ID_ZOOM_32X:
      process_zoom(handle, 32);
      proxy_redraw(handle);
      break;

    case IDOK:
    case IDCANCEL:
    case ID_EXIT:
      proxy_exit(handle);
      EndDialog(hWnd, TRUE);
      break;

    default:
      return FALSE;
    }
    proxy_update_menus(handle);
    return TRUE;

  case WM_TIMER:
    if(wparam == TIMERID) {
      if(handle->proxy.updated) {
        proxy_status_display(handle);
        handle->proxy.updated = FALSE;
      }
      return TRUE;
    }
    break;

  case WM_HSCROLL:
    switch(LOWORD(wparam)) {
    case SB_THUMBPOSITION:
    case SB_THUMBTRACK:
      handle->display.xstart = (int) ((HIWORD(wparam)) * (handle->proxy.xsize - (handle->proxy.xsize / handle->display.zoom)) / 100);    
      proxy_redraw(handle);
      break;
    }
    break;

  case WM_VSCROLL:
    switch(LOWORD(wparam)) {
    case SB_THUMBPOSITION:
    case SB_THUMBTRACK:
      handle->display.ystart = (int) ((HIWORD(wparam)) * (handle->proxy.ysize - (handle->proxy.ysize / handle->display.zoom)) / 100);  
      proxy_redraw(handle);
      break;
    }
    break;

  case WM_ERASEBKGND:
    proxy_redraw(handle);
    return FALSE; 

  default: 
    return FALSE;
  }

  return FALSE;
}

proxy_handle global_proxy;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
  int res = TRUE;
  proxy_handle * handle;
    
  handle = &global_proxy;

  memset(handle, 0, sizeof(proxy_handle));

  handle->hInstance = hInstance;

  res = DialogBoxParam(hInstance, MAKEINTRESOURCE(IDD_PROXY), NULL, (DLGPROC)proxy_dlgproc, (LPARAM)handle);
  if(res == -1) {
    Error("WinMain: DialogBox failed = %d", GetLastError());
  }

  return 0;
}
