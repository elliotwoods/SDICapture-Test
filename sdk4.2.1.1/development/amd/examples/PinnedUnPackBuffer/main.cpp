
#include <windows.h>

#include "GL/glew.h"
#include "GL/wglew.h"

#include "../../../header/dvs_setup.h"
#include "../../../header/dvs_clib.h"
#include "../../../header/dvs_direct.h"
#include "PinnedUnPackBuffer.h"

HWND			ghWnd;
HDC				ghDC;
HGLRC			ghCtx;
MSG				gMsg;

int gWidth  = 1920;
int gHeight = 1080;
PinnedUnPackBuffer*  pBuffer = NULL;
bool running = false;

bool OpenWindow(LPCSTR cClassName, LPCSTR cWindowName);
void CloseWindow();
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

// io threads
int recordAvailable = 0;
int recordCurrent = 0;
int displayAvailable = 0;
int displayCurrent = 0;
const int numBuffers = 1;
sv_storageinfo storageinfo;

DWORD WINAPI displayThread(void * vparams);
DWORD WINAPI recordThread(void * vparams);
void startThread(HANDLE * t, LPTHREAD_START_ROUTINE func, sv_direct_handle * dh);
void stopThread(HANDLE t);

typedef struct {
  sv_handle * sv;
  sv_direct_handle * dh;
} thread_params;

#define LOG_MAX_QUEUE 32
#define LOG_MAX_STRLEN 256

char log_queue[LOG_MAX_QUEUE][LOG_MAX_STRLEN];
char cache_log_queue[LOG_MAX_QUEUE][LOG_MAX_STRLEN];
int qcount = 0, ccount = 0;
HANDLE gLogLock = INVALID_HANDLE_VALUE;
FILE * gLogfile = NULL;

void addLog(const char * message);
void flushLogs();
void closeLog();

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR lpCmdLine, int nShowCmd)
{
	WNDCLASSEX		wndclass;
	const LPCSTR  cClassName  = "OGL";
	const LPCSTR	cWindowName = "Pinned Memory Sample";
  
  HANDLE recordThreadHandle = NULL, displayThreadHandle = NULL;
  thread_params recordThreadParams, displayThreadParams;
  
  sv_handle * sv = NULL;
  sv_direct_handle * dh_in = NULL, * dh_out = NULL;
  int res = SV_OK;

  // need high res Sleep()
  timeBeginPeriod(1);

	// Register WindowClass
	wndclass.cbSize         = sizeof(WNDCLASSEX);
	wndclass.style          = CS_OWNDC;
	wndclass.lpfnWndProc    = WndProc;
	wndclass.cbClsExtra     = 0;
	wndclass.cbWndExtra     = 0;
	wndclass.hInstance      = (HINSTANCE)GetModuleHandle(NULL);
	wndclass.hIcon		    = LoadIcon(NULL, IDI_APPLICATION);
	wndclass.hCursor        = LoadCursor(NULL, IDC_ARROW);
	wndclass.hbrBackground  = NULL;
	wndclass.lpszMenuName   = NULL;
	wndclass.lpszClassName  = cClassName;
	wndclass.hIconSm		= LoadIcon(NULL, IDI_APPLICATION);

  if(!RegisterClassEx(&wndclass)) {
		return FALSE;
  }

  pBuffer = new PinnedUnPackBuffer;

  res = sv_openex(&sv, "", SV_OPENPROGRAM_DEMOPROGRAM, SV_OPENTYPE_DEFAULT, 0, 0);
  if(res != SV_OK) {
    MessageBox(NULL, sv_geterrortext(res), "sv_openex()", 0);
  }

  if(OpenWindow(cClassName, cWindowName)) 
  {
    int videomode;

    if(res == SV_OK) {
      res = sv_jack_option_set(sv, 1, SV_OPTION_ALPHAGAIN, 0);
      if(res != SV_OK) {
        MessageBox(NULL, sv_geterrortext(res), "sv_jack_option_set(SV_OPTION_ALPHAGAIN)", 0);
      }
    }

    if(res == SV_OK) {
      res = sv_jack_option_set(sv, 1, SV_OPTION_ALPHAOFFSET, 0x10000);
      if(res != SV_OK) {
        MessageBox(NULL, sv_geterrortext(res), "sv_jack_option_set(SV_OPTION_ALPHAOFFSET)", 0);
      }
    }

    // raster information
    res = sv_storage_status(sv, 0, NULL, &storageinfo, sizeof(sv_storageinfo), SV_STORAGEINFO_COOKIEISJACK);
    if(res != SV_OK) {
      printf("sv_storage_status():%d (%s)\n", res, sv_geterrortext(res));
      return 1;
    } else {
      gWidth = storageinfo.xsize;
      gHeight = storageinfo.ysize;
    }

    if(res == SV_OK) {
      res = sv_direct_init(sv, &dh_in, "AMD/OPENGL", 1, numBuffers, 0);
      if(res != SV_OK) {
        MessageBox(NULL, sv_geterrortext(res), "sv_direct_init(in)", 0);
      }
    }

    if(res == SV_OK) {
      res = sv_direct_init(sv, &dh_out, "AMD/OPENGL", 0, numBuffers, 0);
      if(res != SV_OK) {
        MessageBox(NULL, sv_geterrortext(res), "sv_direct_init(out)", 0);
      }
    }

    if(res == SV_OK) {
      pBuffer->resize(gWidth, gHeight);
      running = pBuffer->init(sv, dh_in, dh_out, numBuffers);
    }

    if(running) {
      // create logging mutex
      gLogLock = CreateMutex(NULL, FALSE, "PUBLogMutex");

      // start io threads
      recordThreadParams.sv = sv;
      recordThreadParams.dh = dh_in;
      startThread(&recordThreadHandle, recordThread, &recordThreadParams);

      displayThreadParams.sv = sv;
      displayThreadParams.dh = dh_out;
      startThread(&displayThreadHandle, displayThread, &displayThreadParams);
    }

    while(running) {
      if(PeekMessage(&gMsg, NULL, 0, 0, PM_REMOVE)) {
        if(gMsg.message == WM_QUIT) {
          running = false;
        } else {
          TranslateMessage(&gMsg);
          DispatchMessage(&gMsg);
        }
      }

      flushLogs();
      if(recordAvailable == recordCurrent) {
        continue;
      }

      res = sv_direct_sync(sv, dh_in, recordCurrent % numBuffers, 0);
      if(res == SV_OK) {
        pBuffer->draw(recordCurrent % numBuffers);
        recordCurrent++;
        pBuffer->output(displayAvailable % numBuffers);
        
        res = sv_direct_sync(sv, dh_out, displayAvailable % numBuffers, 0);
        if(res == SV_OK) {
          displayAvailable++;
        }
      }

      SwapBuffers(ghDC);
    }

    CloseWindow();
  }

  stopThread(recordThreadHandle);
  stopThread(displayThreadHandle);
  if(gLogLock) {
    CloseHandle(gLogLock);
  }
  closeLog();

  sv_direct_free(sv, dh_in);
  sv_direct_free(sv, dh_out);
  sv_close(sv);
  delete pBuffer;
  UnregisterClass(cClassName, hInst);    

  timeEndPeriod(1);

  return WM_QUIT;
}

DWORD WINAPI recordThread(void * vparams)
{
  thread_params * params = (thread_params *) vparams;
  int dropped = 0;
  int res = SV_OK;

  while(running) {
    sv_direct_bufferinfo bufferinfo = { sizeof(sv_direct_bufferinfo) };

    res = sv_direct_record(params->sv, params->dh, recordAvailable % numBuffers, 0, &bufferinfo);
    if(res == SV_OK) {
      recordAvailable++;
    
      {
        unsigned int vsync_duration = (1000000 / storageinfo.fps);
        uint64 record_go = ((uint64)bufferinfo.clock_high << 32) | bufferinfo.clock_low;
        uint64 dma_ready = ((uint64)bufferinfo.dma.clock_ready_high << 32) | bufferinfo.dma.clock_ready_low;
        sv_direct_info info = { sizeof(sv_direct_bufferinfo) };
        char message[256];

        res = sv_direct_status(params->sv, params->dh, &info);
        if(res == SV_OK) {        
          sprintf(message, "%d> record:\t vsync->dmadone: %5d dmastart->vsync:%5d dropped:%d %c\n", recordAvailable % numBuffers, (int)(dma_ready - record_go - vsync_duration), bufferinfo.dma.clock_distance, info.dropped, (info.dropped > dropped) ? '!' : ' ');
          addLog(message);
          dropped = info.dropped;
        }
      }
    }
  }

  return 0;
}

DWORD WINAPI displayThread(void * vparams)
{
  thread_params * params = (thread_params *) vparams;
  int dropped = 0;
  int res = SV_OK;

  while(running) {
    sv_direct_bufferinfo bufferinfo = { sizeof(sv_direct_bufferinfo) };

    while((displayAvailable == displayCurrent) && running) {
      Sleep(1);
    }

    res = sv_direct_display(params->sv, params->dh, displayCurrent % numBuffers, 0, &bufferinfo);
    if(res == SV_OK) {
      displayCurrent++;

      {
        unsigned int vsync_duration = (1000000 / storageinfo.fps);
        sv_direct_info info = { sizeof(sv_direct_bufferinfo), 0, 0 };
        char message[256];

        res = sv_direct_status(params->sv, params->dh, &info);
        if(res == SV_OK) {
          sprintf(message, "%d> display:\t vsync->dmastart:%5d dmastart->vsync:%5d dropped:%d %c\n", displayCurrent % numBuffers, (int)(vsync_duration - bufferinfo.dma.clock_distance), bufferinfo.dma.clock_distance, info.dropped, (info.dropped > dropped) ? '!' : ' ');
          addLog(message); 
          dropped = info.dropped;
        }
      }
    }
  }

  return 0;
}

void addLog(const char * message)
{
  if(gLogLock == INVALID_HANDLE_VALUE) return;
  WaitForSingleObject(gLogLock, INFINITE);
  
  if(qcount < LOG_MAX_QUEUE) {
    strncpy(log_queue[qcount], message, LOG_MAX_STRLEN); 
    log_queue[qcount][LOG_MAX_STRLEN-1] = 0;
    qcount++;
  }

  ReleaseMutex(gLogLock);
}

void flushLogs()
{
  ccount = 0;

  if(gLogLock == INVALID_HANDLE_VALUE) return;
  WaitForSingleObject(gLogLock, INFINITE);
  if(qcount) {
    memcpy(cache_log_queue, log_queue, LOG_MAX_QUEUE * LOG_MAX_STRLEN);
    ccount = qcount;
    qcount = 0;
  }
  ReleaseMutex(gLogLock);

  if(!ccount) 
    return;

  if(!gLogfile) {
    gLogfile = fopen("publog.txt", "w");
  }

  if(gLogfile) {
    int i;
    for(i = 0; i < ccount; i++) {
      fprintf(gLogfile, cache_log_queue[i]);
    }

    fflush(gLogfile);
  }
}

void closeLog()
{
  if(gLogfile) {
    fclose(gLogfile);
  }
}

void startThread(HANDLE * t, LPTHREAD_START_ROUTINE func, sv_direct_handle * dh)
{
  if(t) {
    *t = CreateThread(0, 0, func, dh, 0, NULL);
  }
}

void stopThread(HANDLE t)
{
  if(t) {
    running = false;
    WaitForSingleObject(t, 1000);
    CloseHandle(t);
  }
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  switch(uMsg) {
  case WM_CHAR:
    if ((char)wParam == VK_ESCAPE) {
      PostQuitMessage(0);
    }
    return 0;

  case WM_CREATE:
    return 0;

  case WM_SIZE:
    gWidth  = LOWORD(lParam);
    gHeight = HIWORD(lParam);

    pBuffer->resize(gWidth, gHeight);
    return 0;

  case WM_KEYDOWN:
    switch(wParam) {
    case VK_RIGHT:
      pBuffer->changeRotation(+1.0f, 0.0f, 0.0f);
      break;
    case VK_LEFT:
      pBuffer->changeRotation(-1.0f, 0.0f, 0.0f);
      break;
    case VK_DOWN:
      pBuffer->changeRotation(0.0f, +1.0f, 0.0f);
      break;
    case VK_UP:
      pBuffer->changeRotation(0.0f, -1.0f, 0.0f);
      break;
    case VK_END:
      pBuffer->changeRotation(0.0f, 0.0f, +1.0f);
      break;
    case VK_HOME:
      pBuffer->changeRotation(0.0f, 0.0f, -1.0f);
      break;

    default: break;
    }
    return 0;

  case WM_DESTROY:
    PostQuitMessage(0);
    return 0;
  }

  return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

bool OpenWindow(LPCSTR cClassName, LPCSTR cWindowName )
{
  static PIXELFORMATDESCRIPTOR pfd;
  MONITORINFO mi = { sizeof(mi) };
	int mPixelFormat;
  
  if(GetMonitorInfo(MonitorFromWindow(ghWnd, MONITOR_DEFAULTTOPRIMARY), &mi)) {
    gWidth = mi.rcMonitor.right - mi.rcMonitor.left;
    gHeight = mi.rcMonitor.bottom - mi.rcMonitor.top;
	  ghWnd = CreateWindow(cClassName, 
						             cWindowName,
						             WS_VISIBLE | WS_POPUP | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
						             mi.rcMonitor.left,
						             mi.rcMonitor.top,
						             gWidth,
						             gHeight,
						             NULL,
						             NULL,
						             (HINSTANCE)GetModuleHandle(NULL),
						             NULL);
  }

  if(!ghWnd) {
		return FALSE;
  }

	pfd.nSize           = sizeof(PIXELFORMATDESCRIPTOR); 
	pfd.nVersion        = 1; 
	pfd.dwFlags         = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL  | PFD_DOUBLEBUFFER ;
	pfd.iPixelType      = PFD_TYPE_RGBA; 
	pfd.cColorBits      = 24; 
	pfd.cRedBits        = 8; 
	pfd.cRedShift       = 0; 
	pfd.cGreenBits      = 8; 
	pfd.cGreenShift     = 0; 
	pfd.cBlueBits       = 8; 
	pfd.cBlueShift      = 0; 
	pfd.cAlphaBits      = 8;
	pfd.cAlphaShift     = 0; 
	pfd.cAccumBits      = 0; 
	pfd.cAccumRedBits   = 0; 
	pfd.cAccumGreenBits = 0; 
	pfd.cAccumBlueBits  = 0; 
	pfd.cAccumAlphaBits = 0; 
	pfd.cDepthBits      = 24; 
	pfd.cStencilBits    = 8; 
	pfd.cAuxBuffers     = 0; 
	pfd.iLayerType      = PFD_MAIN_PLANE;
	pfd.bReserved       = 0; 
	pfd.dwLayerMask     = 0;
	pfd.dwVisibleMask   = 0; 
	pfd.dwDamageMask    = 0;

	ghDC = GetDC(ghWnd);
  if(!ghDC) {
		return FALSE;
  }
	
  mPixelFormat = ChoosePixelFormat(ghDC, &pfd);
  if(!mPixelFormat) {
		return FALSE;
  }
	SetPixelFormat(ghDC, mPixelFormat, &pfd);

	ghCtx = wglCreateContext(ghDC);
  wglMakeCurrent(ghDC, ghCtx);

  if(!glewInit() == GLEW_OK) {
    return FALSE;
  }

  if(WGLEW_ARB_create_context) {
		wglDeleteContext(ghCtx);

		int attribs[] = {
				 WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
				 WGL_CONTEXT_MINOR_VERSION_ARB, 2,
         WGL_CONTEXT_PROFILE_MASK_ARB , WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB,
#ifdef DEBUG
				 WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_DEBUG_BIT_ARB,
#endif
				 0
    }; 

		ghCtx = wglCreateContextAttribsARB(ghDC, 0, attribs);
		if(ghCtx) {
			wglMakeCurrent(ghDC, ghCtx);
    } else {
			return FALSE;
    }
  }

	wglMakeCurrent(ghDC, ghCtx);
#if 0
  // reduces gpu performance
  wglSwapIntervalEXT(1); // vsync to fix opengl window tearing
#endif

  ShowWindow(ghWnd, SW_SHOW);
  UpdateWindow(ghWnd);

	return TRUE;
}


void CloseWindow()
{
	if(ghCtx) {
		wglMakeCurrent(ghDC, NULL);
		wglDeleteContext(ghCtx);
		ReleaseDC(ghWnd, ghDC);
		DestroyWindow(ghWnd);
	}
}
