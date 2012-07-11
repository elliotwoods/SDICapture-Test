/*
//
//    Part of the DVS (http://www.dvs.de) *DVS*VERSION* SDK 
//
//    preview - Captures the incomming video signal and shows this on the monitor.
//
//    This example works only under Windows
*/

/*
// Main WIN32 file
*/
#include "preview.h"

int __cdecl Error(char * string, ...)
{
  va_list va;
  char    ach[1024];
  int res;
  
  va_start(va, string);

  wvsprintf(ach, string, va);

  va_end(va);

  OutputDebugString(ach);

  res = MessageBox(NULL, ach, "Preview Error", MB_TASKMODAL | MB_ABORTRETRYIGNORE);

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
  

int preview_display_text(preview_handle * handle, char * string)
{
  HDC hDC;
  int res;
  RECT rect;

  hDC = GetDC(handle->hWnd);
  if(hDC == NULL) {
    Error("preview_display_text: GetDC(handle->hWnd) failed = %d", GetLastError());
    return FALSE;
  }

  GetClientRect(handle->hWnd, &rect);

  SetBkMode(hDC, TRANSPARENT);          
  SetTextColor(hDC, RGB(100, 100, 100));

  DrawText(hDC, string, (int)strlen(string), &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

  if(!OffsetRect(&rect, 2, 2)){
    return FALSE;
  }

  SetTextColor(hDC, RGB(255, 255, 255));
  DrawText(hDC, string, (int)strlen(string), &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
  
  SetTextColor(hDC, RGB(0, 0, 0));
  SetBkMode(hDC, OPAQUE);


  res = ReleaseDC(handle->hWnd, hDC);
  if(!res) {
    Error("preview_display_text: ReleaseDC(hDC) failed = %d", GetLastError());
    return FALSE;
  }

  return TRUE;
} 

/*
//  Paint the RGB bitmap to the computer monitor
*/

int preview_display_draw(preview_handle * handle, HANDLE hDrawDIB, BITMAPINFOHEADER * pbmiHeader)
{
  HDC hdcWindow;
  int res;

  hdcWindow = GetDC(handle->hWnd);
  if(hdcWindow == NULL) {
    Error("preview_display_draw: GetDC(handle->hWnd) failed = %d", GetLastError());
    return FALSE;
  }

  res = DrawDibDraw(hDrawDIB, hdcWindow,
                    7, 10, 
                    handle->display.xsize, 
                    handle->display.ysize,
                    pbmiHeader, 
                    handle->display.bufferRGB, 
                    handle->display.xstart, handle->display.ystart, 
                    handle->storage.xsize / handle->display.zoom, 
                    handle->storage.ysize / handle->display.zoom,
                    DDF_SAME_DRAW ); 

  if(!res && handle->running){
    Error("preview_display_draw: DrawDibDraw(handle->display.hDrawDIB,..) failed = %d", GetLastError());
    return FALSE;
   }

  res = ReleaseDC(handle->hWnd, hdcWindow);
  if(!res) {
    Error("preview_display_draw: ReleaseDC(hdcWindow) failed = %d", GetLastError());
    return FALSE;
  }

  return TRUE;
} 




void preview_status(preview_handle * handle, char * text)
{
  char buffer[256];

  strcpy(handle->info.buffer, text); 
  handle->info.count      = 0; 
  handle->info.update     = TRUE;
  handle->info.notempty   = text[0] != 0;

  if(handle->info.notempty) {
    wsprintf(buffer, "Preview - %s", text);
  } else {
    wsprintf(buffer, "Preview");
  }
          
  SetWindowText(handle->hWnd, buffer);
}


int preview_status_display(preview_handle * handle)
{
  int running = TRUE;
  int res;

  res = sv_query(handle->sv, SV_QUERY_VIDEOINERROR, 0, &handle->status.videoinerror);
  if(res != SV_OK) {
    running = Error("preview_record_thread: sv_query(sv, SV_QUERY_VIDEOINPUT) failed %d '%s'", res, sv_geterrortext(res));
  }
  res = sv_query(handle->sv, SV_QUERY_AUDIOINERROR, 0, &handle->status.audioinerror);
  if(res != SV_OK) {
    running = Error("preview_record_thread: sv_query(sv, SV_QUERY_VIDEOINPUT) failed %d '%s'", res, sv_geterrortext(res));
  }
  res = sv_query(handle->sv, SV_QUERY_AUDIO_AIVCHANNELS, 0, &handle->status.aivchannels);
  if((res != SV_OK) && (res != SV_ERROR_NOTIMPLEMENTED) && (res != SV_ERROR_WRONG_HARDWARE)) {
    running = Error("preview_record_thread: sv_query(sv, SV_QUERY_AUDIO_AIVCHANNELS) failed %d '%s'", res, sv_geterrortext(res));
  }
  res = sv_query(handle->sv, SV_QUERY_AUDIO_AESCHANNELS, 0, &handle->status.aivchannels);
  if((res != SV_OK) && (res != SV_ERROR_NOTIMPLEMENTED) && (res != SV_ERROR_WRONG_HARDWARE)) {
    running = Error("preview_record_thread: sv_query(sv, SV_QUERY_AUDIO_AESCHANNELS) failed %d '%s'", res, sv_geterrortext(res));
  }
 
  if(handle->status.videoinerror != handle->window.videoinerror) {
    if(handle->status.videoinerror == SV_OK) {
      preview_status(handle, "Video Input Connected"); 
    } else {
      preview_status(handle, "Missing Video Input"); 
    }
    handle->window.videoinerror = handle->status.videoinerror; 
    handle->window.audioinerror = handle->status.audioinerror; 
  } else if(handle->status.audioinerror != handle->window.audioinerror) {
    if(handle->status.audioinerror == SV_OK) {
      preview_status(handle, "Audio Input Connected"); 
    } else {
      preview_status(handle, "Missing Audio Input"); 
    }
    handle->window.audioinerror = handle->status.audioinerror; 
  }

  if(handle->status.audioinerror == SV_OK) {
    handle->record.noaudioinput = FALSE;
  }
  if(handle->status.videoinerror == SV_OK) {
    handle->record.novideoinput = FALSE;
  }

  if(handle->status.aivchannels != handle->window.aivchannels) {
    handle->window.aivchannels = handle->status.aivchannels;
  }
  if(handle->status.aeschannels != handle->window.aeschannels) {
    handle->window.aeschannels = handle->status.aeschannels;
  }

  return running;
}

/*
//  Thread which reads video from card's memory  
*/
DWORD preview_record_thread(void * voidhandle)
{
  preview_handle * handle      = voidhandle;
  sv_fifo * pfifo              = NULL;
  sv_fifo_buffer * pbuffer     = NULL;
  sv_fifo_configinfo config    = { 0 };
  int res                      = 0;
  int running                  = TRUE;
  int i                        = 0;
  int pollcount                = 0;
  int fifoflags;
  char * anc = NULL;
  char * anc_org = NULL;
  int ancstreamer = FALSE;

  if(running) {
    if(sv_option_get(handle->sv, SV_OPTION_ANCCOMPLETE, &ancstreamer) == SV_OK) {
      ancstreamer = (ancstreamer == SV_ANCCOMPLETE_STREAMER) ? TRUE : FALSE;
    }
  }

  // Initializes FIFO DMA for reading

  if(running) {
    res = sv_fifo_init(handle->sv, &pfifo, TRUE, FALSE, TRUE, ancstreamer ? SV_FIFO_FLAG_ANC : 0, 0);    
    if(res != SV_OK)  {
      Error("preview_record_thread: sv_fifo_init(sv) failed %d '%s'", res, sv_geterrortext(res));
      running = FALSE;
    } 
  }

  if(running) {
    res = sv_fifo_configstatus(handle->sv, pfifo, &config);
    if(res != SV_OK)  {
      Error("preview_record_thread: sv_fifo_configstatus(sv) failed %d '%s'", res, sv_geterrortext(res));
      running = FALSE;
    }   
    handle->pfifo = pfifo;
  }

  anc_org = anc = malloc(config.ancbuffersize + config.dmaalignment);
  if(config.dmaalignment) {
    anc = (void *)(((uintptr)anc_org + (config.dmaalignment-1)) & ~(config.dmaalignment-1));
  }

  handle->record.fieldsize = handle->storage.fieldsize[0] + handle->storage.fieldsize[1] + 0x10000;

  if(running) {
    for(i = 0; i < PREVIEW_MAXBUFFERS; i++) {
      if((handle->record.table[i].buffer_org = malloc(handle->record.fieldsize + 0x1000)) == NULL) {
        running = FALSE;
      } else {
        handle->record.table[i].buffer = (void *)(((uintptr)handle->record.table[i].buffer_org + 0xfff) & ~0xfff); 
      }
    }
    if(!running) {
      Error("preview_record_thread: malloc(%d) failed", handle->record.fieldsize);
    }
  }
  
  if(running) {
    res = sv_fifo_start(handle->sv, pfifo);
    if(res != SV_OK) {
      Error("preview_record_thread: sv_fifo_start(sv) failed %d '%s'", res, sv_geterrortext(res));
      running = FALSE;
    }   
  }
 
  while(running && handle->running) {
    if(pollcount++ > 10) {
      preview_status_display(handle);
      pollcount = 0;
    }

    fifoflags = SV_FIFO_FLAG_FLUSH;
    if(handle->record.noaudioinput) {
      fifoflags |= SV_FIFO_FLAG_VIDEOONLY;
    }
    fifoflags |= SV_FIFO_FLAG_NODMAADDR;
    if(ancstreamer) {
      fifoflags |= SV_FIFO_FLAG_ANC;
    }

    res = sv_fifo_getbuffer(handle->sv, pfifo, &pbuffer, NULL, fifoflags); 
    switch(res) {
    case SV_ERROR_NOCARRIER:
    case SV_ERROR_INPUT_VIDEO_NOSIGNAL:
    case SV_ERROR_INPUT_VIDEO_RASTER:
    case SV_ERROR_INPUT_KEY_NOSIGNAL:
    case SV_ERROR_INPUT_KEY_RASTER:
      if(!handle->record.novideoinput && !handle->pollnocarrier) {
        preview_status(handle, "No Video Input");
        handle->record.novideoinput  = TRUE;
      }
      Sleep(10);
      break;
    case SV_ERROR_INPUT_AUDIO_NOAESEBU:
    case SV_ERROR_INPUT_AUDIO_FREQUENCY:
    case SV_ERROR_INPUT_AUDIO_NOAIV:
      preview_status(handle, "No Audio Input");
      handle->record.noaudioinput = TRUE;
      res = SV_OK;
      break;
    case SV_ERROR_DISPLAYONLY:
      if(!handle->record.novideoinput && !handle->pollnocarrier) {
        preview_status(handle, "Display Only Raster");
        handle->record.novideoinput = TRUE;
      }
      Sleep(10);
      break;
    case SV_ERROR_VSYNCPASSED:
    case SV_ERROR_VSYNCFUTURE:
      if(!handle->record.novideoinput && !handle->pollnocarrier) {
        preview_status(handle, "Signal not stable");
        handle->record.novideoinput  = TRUE;
      }
      Sleep(10);
      break;
    case SV_OK:
      if(handle->info.notempty) {
        if(handle->info.count > 25) {
          preview_status(handle, "");
        }
      }
      break;
    default:
      preview_status(handle, sv_geterrortext(res));
    }

    handle->info.count++;
  
    if(pbuffer && (res == SV_OK)) {
      pbuffer->video[0].addr = handle->record.table[handle->record.nbuffer].buffer;
      pbuffer->video[0].size = handle->record.fieldsize;

      if(pbuffer->anc[0].size) {
        pbuffer->anc[0].addr = anc;
      }

      if(pbuffer->anc[1].size) {
        pbuffer->anc[1].addr = anc + pbuffer->anc[0].size;
      }

      res = sv_fifo_putbuffer(handle->sv, pfifo, pbuffer, NULL);
      if(running && (res != SV_OK))  {
        running = Error("preview_record_thread: sv_fifo_putbuffer(sv) failed %d '%s'", res, sv_geterrortext(res));
      } 

      if(handle->display.viewanc && config.ancbuffersize) {
        memcpy(handle->record.table[handle->record.nbuffer].buffer, anc, config.ancbuffersize);
      }

      EnterCriticalSection(&handle->mutex.mutex);
      handle->mutex.LastRecordedBuffer = handle->record.nbuffer;
      LeaveCriticalSection(&handle->mutex.mutex);

      if(++handle->record.nbuffer >= PREVIEW_MAXBUFFERS) {
        handle->record.nbuffer = 0;
      } 

      res = SetEvent(handle->hEventRecorded); 
      if(!res) {
        running = Error("preview_record_ready: SetEvent(hEventRecorded) failed");
      }  
    }

    if(handle->running && handle->pause) {
      res = sv_fifo_stop(handle->sv, pfifo, 0);
      if(res != SV_OK) {
        Error("preview_record_thread: sv_fifo_stop(sv) failed %d '%s'", res, sv_geterrortext(res));
        running = FALSE;
      }
      while(running && handle->running && handle->pause) {
        Sleep(10);
      }
      res = sv_fifo_start(handle->sv, pfifo);
      if(res != SV_OK) {
        Error("preview_record_thread: sv_fifo_start(sv) failed %d '%s'", res, sv_geterrortext(res));
        running = FALSE;
      }
    }
  }


  if(pfifo) {
    res = sv_fifo_free(handle->sv, pfifo);
    if(running && (res != SV_OK))  {
      Error("preview_record_thread: sv_fifo_exit(sv) failed %d '%s'", res, sv_geterrortext(res));
    }
  }

  if(anc_org) {
    free(anc_org);
  }

  handle->running = FALSE;
  SetEvent(handle->hEventRecorded); // Release the display thread.

#ifdef WIN32
  ExitThread(res); 
#else
  return FALSE;
#endif
}

/*
//  Thread which converts and paints the video to the screen
*/

DWORD preview_display_thread(preview_handle * handle)
{
  BITMAPINFOHEADER	bmiHeader;
  HDC			hdcMem     = NULL;
  HBITMAP		hBitmap    = NULL;
  HDRAWDIB		hDrawDIB   = NULL;

  int res     = 0;
  int running = TRUE;

  memset(&bmiHeader, 0, sizeof(bmiHeader));
  bmiHeader.biSize             = sizeof(BITMAPINFOHEADER);
  bmiHeader.biHeight           = handle->storage.storageysize;
  bmiHeader.biWidth            = handle->storage.storagexsize;
  bmiHeader.biBitCount         = 32;
  bmiHeader.biCompression      = BI_RGB;
  bmiHeader.biSizeImage	       = bmiHeader.biWidth * bmiHeader.biHeight * bmiHeader.biBitCount / 8; // Div by 8 we have bytes
  bmiHeader.biXPelsPerMeter    = 0;
  bmiHeader.biYPelsPerMeter    = 0;
  bmiHeader.biClrUsed          = 0;
  bmiHeader.biClrImportant     = 0;
  bmiHeader.biPlanes           = 1;

  if(running) {
    hdcMem  = CreateCompatibleDC(NULL);
    if(!hdcMem) {
      Error("preview_display_thread: CreateCompatibleDC(NULL) failed = %d", GetLastError());
      running = FALSE;
    }
  }

  if(running) {
    hBitmap = CreateDIBSection(hdcMem, (BITMAPINFO *)&bmiHeader, DIB_RGB_COLORS, (void**)&handle->display.bufferRGB, NULL, 0 );  
    if(!hBitmap) {
      Error("preview_display_thread: CreateDIBSection(hdcMem,...) failed = %d", GetLastError());
      running = FALSE;
    }
  }

  if(running) {
    hDrawDIB = DrawDibOpen();

    if(hDrawDIB == NULL)  {
      Error("preview_display_thread: DrawDibOpen() failed = %d", GetLastError());
      running = FALSE;
    }
  }

  if(running) {
    res = DrawDibBegin(hDrawDIB, NULL, 
	    handle->storage.xsize, handle->storage.ysize, 
	    &bmiHeader,
	    handle->storage.storagexsize, handle->storage.storageysize, 
	    DDF_BUFFER); 
    if(!res ) {
      Error("preview_display_thread: DrawDibBegin(...) failed = %d", GetLastError());
      running = FALSE;
    }
  }

  while(running && handle->running) {

    res = WaitForSingleObject(handle->hEventRecorded, INFINITE);
    if(res != WAIT_OBJECT_0)  {
      running = Error("preview_display_thread: WaitForSingleObject(handle->hEventRecorded)=%d gle=%d", res, GetLastError());
    }

    // When there is a new buffer to display, update buffers counter

    res = ResetEvent(handle->hEventRecorded);
    if(!res)  {
      running = Error("preview_display_thread: ResetEvent(handle->hEventRecorded) failed = %d", GetLastError());
    }

    EnterCriticalSection(&handle->mutex.mutex);
      handle->mutex.CurrentDisplayBuffer = handle->mutex.LastRecordedBuffer;  
    LeaveCriticalSection(&handle->mutex.mutex);

    // Do the color conversion depending on the color mode

    if(handle->record.novideoinput) {
      preview_display_text(handle, "No Carrier");
    } else if(running && handle->running) {
      int draw = convert_tobgra((unsigned char *)handle->record.table[handle->mutex.CurrentDisplayBuffer].buffer, (unsigned char*)handle->display.bufferRGB, handle->storage.storagexsize, handle->storage.storageysize, handle->storage.interlace, handle->storage.lineoffset[0], handle->storage.videomode, handle->storage.bottom2top, handle->display.viewalpha, handle->display.fields);

      if(draw) {
        if(!preview_display_draw(handle, hDrawDIB, &bmiHeader)) {
          running = FALSE;
        }
      } else {
        preview_display_text(handle, "Pixelformat not supported");
      }
    }

    EnterCriticalSection(&handle->mutex.mutex);
      handle->mutex.CurrentDisplayBuffer = -1;  
    LeaveCriticalSection(&handle->mutex.mutex);
  }

  if(hDrawDIB) {
    res = DrawDibEnd(hDrawDIB);
    if(!res)  {
      Error("preview_display_exit: DrawDibEnd(hDrawDIB) failed = %d", GetLastError());
    }

    res = DrawDibClose(hDrawDIB);
    if(!res)  {
      Error("preview_display_exit: DrawDibClose(handle->display.hDrawDIB) failed = %d", GetLastError());
    }
  }

  if(hBitmap) {
    DeleteObject(hBitmap);
  }

  if(hdcMem) {
    DeleteDC(hdcMem);
  }

#ifdef WIN32
  ExitThread(res); 
#else
  return FALSE;
#endif
}

/*
//  Manages the correct ending of all threads and finalizes preview structures
*/

void preview_exit(preview_handle * handle)
{  
  int res;
  int i;
  DWORD dwExitCode;

  handle->running = FALSE;


  do {
    res = GetExitCodeThread(handle->hThreadDisplay, &dwExitCode);
    if(!res) {
      Error("preview_exit: GetExitCodeThread(handle->hThreadDisplay,) failed = %d", GetLastError());
    }
    if(dwExitCode == STILL_ACTIVE) {
      Sleep(50);
    }
  } while(res && (dwExitCode == STILL_ACTIVE)); 

  do {
    res = GetExitCodeThread(handle->hThreadRecord, &dwExitCode);
    if(!res) {
      Error("preview_exit: GetExitCodeThread(handle->hThreadRecord,) failed = %d", GetLastError());
    }
    if(dwExitCode == STILL_ACTIVE) {
      Sleep(50);
    } else{
      for(i = 0; i < PREVIEW_MAXBUFFERS; i++) {  // When it is sure that displaythread has finished
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
}

/*
//  Update menus checkboxes
*/
void preview_updatemenus(preview_handle * handle)
{
  HMENU hMenu = GetMenu(handle->hWnd);

  if(handle->display.viewalpha) {
    CheckMenuItem(hMenu, ID_OPTIONS_VIEWALPHA, MF_CHECKED);
  } else {
    CheckMenuItem(hMenu, ID_OPTIONS_VIEWALPHA, MF_UNCHECKED);
  }

  if(handle->display.viewanc) {
    CheckMenuItem(hMenu, ID_OPTIONS_VIEWANC, MF_CHECKED);
  } else {
    CheckMenuItem(hMenu, ID_OPTIONS_VIEWANC, MF_UNCHECKED);
  }

  CheckMenuItem(hMenu, ID_FIELDS_FRAME,  MF_UNCHECKED);
  CheckMenuItem(hMenu, ID_FIELDS_FIELD1, MF_UNCHECKED);
  CheckMenuItem(hMenu, ID_FIELDS_FIELD2, MF_UNCHECKED);
  switch(handle->display.fields) {
  case 0:
    CheckMenuItem(hMenu, ID_FIELDS_FRAME,  MF_CHECKED);
    break;
  case 1:
    CheckMenuItem(hMenu, ID_FIELDS_FIELD1, MF_CHECKED);
    break;
  case 2:
    CheckMenuItem(hMenu, ID_FIELDS_FIELD2, MF_CHECKED);
    break;
  }

  CheckMenuItem(hMenu, ID_SCALE1, MF_UNCHECKED);
  CheckMenuItem(hMenu, ID_SCALE2, MF_UNCHECKED);
  CheckMenuItem(hMenu, ID_SCALE4, MF_UNCHECKED);
  CheckMenuItem(hMenu, ID_SCALE8, MF_UNCHECKED);
  switch(handle->display.scale) {
  case 1:
    CheckMenuItem(hMenu, ID_SCALE1, MF_CHECKED);
    break;
  case 2:
    CheckMenuItem(hMenu, ID_SCALE2, MF_CHECKED);
    break;
  case 4:
    CheckMenuItem(hMenu, ID_SCALE4, MF_CHECKED);
    break;
  case 8:
    CheckMenuItem(hMenu, ID_SCALE8, MF_CHECKED);
    break;
  }

  CheckMenuItem(hMenu, ID_ZOOM_1X, MF_UNCHECKED);
  CheckMenuItem(hMenu, ID_ZOOM_2X, MF_UNCHECKED);
  CheckMenuItem(hMenu, ID_ZOOM_4X, MF_UNCHECKED);
  CheckMenuItem(hMenu, ID_ZOOM_8X, MF_UNCHECKED);
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
  }
}

/*
//  Manages changes caused by scales and zooms
*/

int preview_resize(preview_handle * handle, int xsize, int ysize)
{
  RECT           rect;

  EnterCriticalSection(&handle->mutex.mutex);   

  handle->mutex.LastRecordedBuffer   = 0;
  handle->mutex.CurrentDisplayBuffer = 0;

  if(xsize && ysize) {
    handle->display.ysize = xsize;
    handle->display.xsize = ysize;
  }

  GetWindowRect(handle->hWnd, &rect);

  ShowWindow(handle->hSlideH, SW_HIDE);
  ShowWindow(handle->hSlideV, SW_HIDE);
  ShowWindow(handle->hWnd,    SW_HIDE);

  if(!xsize || !ysize) {
    MoveWindow(handle->hWnd, rect.left, rect.top, PREVIEW_XBORDER + handle->display.xsize, PREVIEW_YBORDER + handle->display.ysize, TRUE);
  }
  GetClientRect(handle->hWnd, &rect);
  MoveWindow(handle->hSlideH, rect.left, rect.top + handle->display.ysize + 11, handle->display.xsize + 12, 25, TRUE);
  MoveWindow(handle->hSlideV, rect.left + handle->display.xsize + 8, rect.top + 3 , 25, handle->display.ysize + 15, TRUE);
     
  ShowWindow(handle->hWnd   , SW_SHOW);
  ShowWindow(handle->hSlideH, SW_SHOW);
  ShowWindow(handle->hSlideV, SW_SHOW); 

  RedrawWindow(handle->hWnd, NULL, NULL, RDW_FRAME);

  LeaveCriticalSection(&handle->mutex.mutex);

  return TRUE;
}


void preview_resizewindow(preview_handle * handle)
{
  RECT           rect;

  GetWindowRect(handle->hWnd, &rect);
  MoveWindow(handle->hWnd, rect.left, rect.top, rect.left + PREVIEW_XBORDER + handle->storage.xsize / handle->display.scale, rect.top + PREVIEW_YBORDER + handle->storage.ysize / handle->display.scale, TRUE);
}

/*
//  Thread and preview structures initialization
*/

int preview_init(preview_handle * handle) 
{
  int res;
  int videomode;

  handle->hEventRecorded = CreateEvent(NULL, TRUE, FALSE, NULL);
  if(handle->hEventRecorded == NULL) {
    Error("preview_init: CreateEvent(handle->hEventRecorded) failed= %d", GetLastError());
    return FALSE;
  }

  InitializeCriticalSection(&handle->mutex.mutex);

  handle->mutex.LastRecordedBuffer   = 0;
  handle->mutex.CurrentDisplayBuffer = 0;
 
  res = sv_openex(&handle->sv, "", SV_OPENPROGRAM_DEMOPROGRAM, SV_OPENTYPE_INPUT, 0, 0);
  if(res != SV_OK) {
    Error("preview: Error '%s' opening video device", sv_geterrortext(res));
    return FALSE;
  } 

  res = sv_jack_option_get(handle->sv, 1, SV_OPTION_VIDEOMODE, &videomode);
  if(res != SV_OK) {
    Error("preview: Error '%s' sv_jack_option_get(SV_OPTION_VIDEOMODE)", sv_geterrortext(res));
    return FALSE;
  }
  
  res = sv_jack_option_set(handle->sv, 1, SV_OPTION_VIDEOMODE, videomode | SV_MODE_STORAGE_FRAME);
   if(res != SV_OK) {
    Error("preview: Error '%s' sv_jack_option_set(SV_OPTION_VIDEOMODE)", sv_geterrortext(res));
    return FALSE;
  }

  res = sv_storage_status(handle->sv, 1, NULL, &handle->storage, sizeof(handle->storage), SV_STORAGEINFO_COOKIEISJACK);
  if(res != SV_OK) {
    Error("preview_init: sv_storage_status(sv, ...) failed = %d", res);
    return FALSE;
  }

  if(handle->storage.lineoffset[0] < 0) {
    handle->storage.lineoffset[0] = -handle->storage.lineoffset[0];
  }

  // Environmental vars. init

  handle->running        = TRUE;
  if(handle->storage.storagexsize <= 1024) {
    handle->display.scale  = 1;
  } else if(handle->storage.storagexsize <= 2048) {
    handle->display.scale  = 2;
  } else {
    handle->display.scale  = 4;
  }

  handle->display.ysize  = handle->storage.storageysize / handle->display.scale;
  handle->display.xsize  = handle->storage.storagexsize / handle->display.scale;

  handle->display.zoom   = 1;  
  handle->test           = 0;

  handle->hSlideH = GetDlgItem(handle->hWnd, IDC_SLIDERH);
  handle->hSlideV = GetDlgItem(handle->hWnd, IDC_SLIDERV);
  

  // Set initial values for menu checkboxes.
  preview_updatemenus(handle);
  
  // Windows Display resizing.
  preview_resizewindow(handle);


  // Thread init
  handle->hThreadRecord = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)preview_record_thread, handle, 0, &handle->pidRecord);
  if(handle->hThreadRecord == NULL) {
    Error("preview_init: CreateThread(record) failed = %d", GetLastError());
    handle->running = FALSE;
  }

  handle->hThreadDisplay = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)preview_display_thread, handle, 0, &handle->pidDisplay);
  if(handle->hThreadDisplay == NULL) {
    Error("preview_init: CreateThread(display) failed = %d", GetLastError());
    handle->running = FALSE;
  } else{
    SetThreadPriority(handle->hThreadDisplay, THREAD_PRIORITY_IDLE);
  }

  if(!handle->running) {
    preview_exit(handle);
  }

  return TRUE;
}

/*
//  Handles zooming updates 
*/

void process_zoom(int zoom, preview_handle * handle)
{
  EnterCriticalSection(&handle->mutex.mutex);
  switch(zoom){
  case ID_ZOOM_1X:
    handle->display.zoom   = 1;
    handle->display.xstart = 0;
    handle->display.ystart = 0;
    EnableWindow(handle->hSlideH, FALSE);
    EnableWindow(handle->hSlideV, FALSE); 
    break;
  case ID_ZOOM_2X:
    handle->display.zoom   = 2;
    EnableWindow(handle->hSlideH, TRUE);
    EnableWindow(handle->hSlideV, TRUE);
    break;
  case ID_ZOOM_4X:
    handle->display.zoom   = 4;
    EnableWindow(handle->hSlideH, TRUE);
    EnableWindow(handle->hSlideV, TRUE);
    break;
  case ID_ZOOM_8X:
    handle->display.zoom   = 8;
    EnableWindow(handle->hSlideH, TRUE);
    EnableWindow(handle->hSlideV, TRUE);
    break;
  }

  LeaveCriticalSection(&handle->mutex.mutex);
}

/*
//  Event handler for the preview dialog
*/

DWORD APIENTRY preview_dlgproc(HWND hWnd, UINT message, WPARAM wparam, LPARAM lparam) 
{ 
  static preview_handle * handle = NULL;
  int command;
  char buffer[256];
 
  switch(message) {
  case WM_INITDIALOG:
    handle       = (preview_handle *) lparam;
    handle->hWnd = hWnd;
    preview_init(handle);
    return FALSE;

  case WM_COMMAND:
    command = GET_WM_COMMAND_ID(wparam, lparam);

    switch(command) {
      break;

    case ID_FILE_POLLNOCARRIER:
      handle->pollnocarrier = !handle->pollnocarrier;
      break;

    case ID_OPTIONS_VIEWALPHA:
      handle->display.viewalpha = !handle->display.viewalpha;
      break;
    case ID_OPTIONS_VIEWANC:
      handle->display.viewanc = !handle->display.viewanc;
      break;
    case ID_FIELDS_FRAME:
      handle->display.fields = 0;
      break;
    case ID_FIELDS_FIELD1:
      handle->display.fields = 1;
      break;
    case ID_FIELDS_FIELD2:
      handle->display.fields = 2;
      break;
    case ID_PAUSE:
      handle->pause = !handle->pause;
      break;

    case ID_ZOOM_1X:
      process_zoom(ID_ZOOM_1X, handle);
      break;
    case ID_ZOOM_2X:
      process_zoom(ID_ZOOM_2X, handle);
      break;
    case ID_ZOOM_4X:
      process_zoom(ID_ZOOM_4X, handle);
      break;
    case ID_ZOOM_8X:
      process_zoom(ID_ZOOM_8X, handle);
      break;

    case ID_SCALE1:
      handle->display.scale = 1;
      handle->display.zoom  = 1;     
      preview_resizewindow(handle);
      break;

    case ID_SCALE2:
      handle->display.scale = 2;
      handle->display.zoom  = 1;
      preview_resizewindow(handle);
      break;

    case ID_SCALE4:
      handle->display.scale = 4;
      handle->display.zoom  = 1;
      preview_resizewindow(handle);
      break;

    case ID_SCALE8: 
      handle->display.scale = 8;
      handle->display.zoom  = 1;
      preview_resizewindow(handle);
      break;

    case ID_STATUS:
      wsprintf(buffer, " Fps = %d\n Fullrange = %d\n Interlaced = %d\n Nbits = %d\n RGBformat = %d\n Xsize = %d  Ysize = %d\n Colormode = %d\n VideoMode = %d\n Linesize = %d\n Fieldsize %d\n",
                        handle->storage.fps, 
                        handle->storage.fullrange, 
                        handle->storage.interlace,
                        handle->storage.nbits,
                        handle->storage.rgbformat,
                        handle->storage.storagexsize, 
                        handle->storage.storageysize,
                        handle->storage.colormode,
                        handle->storage.videomode,
                        handle->storage.linesize,
                        handle->storage.fieldsize[0]
                       ); 
      MessageBox(handle->hWnd, buffer, "Storage Status", MB_OK);
      break;

    case IDOK:
    case IDCANCEL:
    case ID_EXIT:
      preview_exit(handle);
      EndDialog(hWnd, TRUE);
      break;

    default:
      return FALSE;
    }

    preview_updatemenus(handle);

    return TRUE;

  case WM_HSCROLL:
    if(HIWORD(wparam) != 0){
       handle->display.xstart = (int) ((HIWORD(wparam)) * ( handle->storage.storagexsize - (handle->storage.storagexsize / handle->display.zoom ) ) / 100);    
    }
    break;

  case WM_VSCROLL:
    if(HIWORD(wparam) != 0){
      handle->display.ystart = (int) ((HIWORD(wparam)) * ( handle->storage.storageysize - (handle->storage.storageysize / handle->display.zoom ) ) / 100);  
    }
    break;
  
  case WM_SIZE: 
    {
      int xsize = HIWORD(lparam) - PREVIEW_XBORDER;
      int ysize = LOWORD(lparam) - PREVIEW_YBORDER;

      if((handle->display.xsize != xsize) || (handle->display.ysize != ysize)) {
        preview_resize(handle, xsize, ysize);
      }
    }
    return FALSE; 

  default: 
    return FALSE;
  }

  return FALSE;
}

preview_handle global_preview;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
  int res = TRUE;
  preview_handle * handle;
    
  handle = &global_preview;

  memset(handle, 0, sizeof(preview_handle));

  handle->hInstance = hInstance;

  res = DialogBoxParam(hInstance, MAKEINTRESOURCE(IDD_PREVIEW), NULL, (DLGPROC)preview_dlgproc, (LPARAM)handle);
  if(res == -1) {
    Error("WinMain: DialogBox failed = %d", GetLastError());
  }

  return 0;
}
