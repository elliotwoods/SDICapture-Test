#define _CRT_SECURE_NO_WARNINGS 1
#include <windows.h>
#include <d3d9.h>
#include <d3dx9.h>
#include <math.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <assert.h>
#include <signal.h>

extern "C" {
#include "../../../header/dvs_setup.h"
#include "../../../header/dvs_clib.h"
#include "../../../header/dvs_direct.h"
#include "../../../header/dvs_thread.h"
}

sv_handle * sv;
sv_direct_handle * dh_out, * dh_in;
sv_storageinfo storageinfo;

// IO Threads
const int numBuffers = 1;
// Events
dvs_cond  record_ready[numBuffers];
dvs_mutex record_ready_mutex[numBuffers];
dvs_cond  display_ready[numBuffers];
dvs_mutex display_ready_mutex[numBuffers];
dvs_cond  record_go[numBuffers];
dvs_mutex record_go_mutex[numBuffers];
dvs_cond  display_go[numBuffers];
dvs_mutex display_go_mutex[numBuffers];

const int DVPINPUT  = 0;
const int DVPOUTPUT = 1;
int WIDTH  = 1920;
int HEIGHT = 1080;

// Direct3D globals
static HWND             hWnd                    = NULL;
LPDIRECT3D9             g_pD3D                  = NULL;
LPDIRECT3DDEVICE9       g_pd3dDevice            = NULL;
LPDIRECT3DVERTEXBUFFER9 g_pVertexBuffer         = NULL;
LPDIRECT3DVERTEXBUFFER9 g_pQuadVertexBuffer     = NULL;
LPD3DXFONT				      g_pStatsFont            = NULL;
BOOL                    g_bIsD3D9Ex             = FALSE;
BOOL                    g_bNoShareHandlesOnWDDM = FALSE;
IDirect3DTexture9 *     g_renderTargetTexture[numBuffers][2] = { 0 };

#define SAFE_RELEASE(x) if(x){ (x)->Release(); (x) = NULL; }
#define D3DFVF_CUSTOMVERTEX ( D3DFVF_XYZ | D3DFVF_TEX1 )
struct Vertex
{
    float x, y, z;
    float tu, tv;
};
Vertex g_cubeVertices[] =
{
	{-1.0f, 1.0f,-1.0f,  0.0f,0.0f },
	{ 1.0f, 1.0f,-1.0f,  1.0f,0.0f },
	{-1.0f,-1.0f,-1.0f,  0.0f,1.0f },
	{ 1.0f,-1.0f,-1.0f,  1.0f,1.0f },
	
	{-1.0f, 1.0f, 1.0f,  1.0f,0.0f },
	{-1.0f,-1.0f, 1.0f,  1.0f,1.0f },
	{ 1.0f, 1.0f, 1.0f,  0.0f,0.0f },
	{ 1.0f,-1.0f, 1.0f,  0.0f,1.0f },
	
	{-1.0f, 1.0f, 1.0f,  0.0f,0.0f },
	{ 1.0f, 1.0f, 1.0f,  1.0f,0.0f },
	{-1.0f, 1.0f,-1.0f,  0.0f,1.0f },
	{ 1.0f, 1.0f,-1.0f,  1.0f,1.0f },
	
	{-1.0f,-1.0f, 1.0f,  0.0f,0.0f },
	{-1.0f,-1.0f,-1.0f,  1.0f,0.0f },
	{ 1.0f,-1.0f, 1.0f,  0.0f,1.0f },
	{ 1.0f,-1.0f,-1.0f,  1.0f,1.0f },

	{ 1.0f, 1.0f,-1.0f,  0.0f,0.0f },
	{ 1.0f, 1.0f, 1.0f,  1.0f,0.0f },
	{ 1.0f,-1.0f,-1.0f,  0.0f,1.0f },
	{ 1.0f,-1.0f, 1.0f,  1.0f,1.0f },
	
	{-1.0f, 1.0f,-1.0f,  1.0f,0.0f },
	{-1.0f,-1.0f,-1.0f,  1.0f,1.0f },
	{-1.0f, 1.0f, 1.0f,  0.0f,0.0f },
	{-1.0f,-1.0f, 1.0f,  0.0f,1.0f }
};
Vertex g_quadVertices[] = 
{
	{-1.0f, 1.0f,-1.0f,  0.0f,0.0f },
	{ 1.0f, 1.0f,-1.0f,  1.0f,0.0f },
	{-1.0f,-1.0f,-1.0f,  0.0f,1.0f },
	{ 1.0f,-1.0f,-1.0f,  1.0f,1.0f }
};

// Timecode buffers
sv_direct_timecode timecodes[numBuffers][2];

static int verbose = 0;
static int running = TRUE;

void init(void);
typedef struct {
  sv_direct_handle * dh_in;
  sv_direct_handle * dh_out;
} renderthread_data;
DWORD WINAPI renderThread(void * data);
DWORD WINAPI recordThread(void * vdh);
DWORD WINAPI displayThread(void * vdh);
void startThread(HANDLE * t, LPTHREAD_START_ROUTINE func, void * data);
void stopThread(HANDLE t);

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow);

bool initBuffers()
{
  for(unsigned int buffer = 0; buffer < numBuffers; buffer++) {
    for(int i = DVPINPUT; i <= DVPOUTPUT; i++) {
      // Create the render target texture.            
      if(!g_renderTargetTexture[buffer][i]) {
        g_pd3dDevice->CreateTexture(WIDTH, HEIGHT, 0, D3DUSAGE_RENDERTARGET, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &g_renderTargetTexture[buffer][i], NULL);
      }
    }
  }
  return true;
}

void cleanupBuffers() 
{
  for(unsigned int buffer = 0; buffer < numBuffers; buffer++) {
    for(int i = DVPINPUT; i <= DVPOUTPUT; i++) {
      if(g_renderTargetTexture[buffer][i]) {
        g_renderTargetTexture[buffer][i]->Release();
        g_renderTargetTexture[buffer][i] = 0;
      }
    }
  }
}

void init( void )
{
  HRESULT hr = E_FAIL;
  HMODULE libHandle = LoadLibrary("d3d9.dll");
  if(libHandle != NULL) {
	  typedef HRESULT (WINAPI *LPDIRECT3DCREATE9EX)(UINT, IDirect3D9Ex **);
	  LPDIRECT3DCREATE9EX Direct3DCreate9ExPtr = NULL;

	  // On Vista/Win7 use D3DCreate9Ex().
	  Direct3DCreate9ExPtr = (LPDIRECT3DCREATE9EX) GetProcAddress(libHandle, "Direct3DCreate9Ex");
	  if(Direct3DCreate9ExPtr) {
		  hr = Direct3DCreate9ExPtr(D3D_SDK_VERSION, (IDirect3D9Ex **)&g_pD3D);
		  if(hr == S_OK && g_pD3D != NULL) {
			  g_bIsD3D9Ex = TRUE;
		  }
	  }
  }

	if(!g_bIsD3D9Ex) {
		g_pD3D = Direct3DCreate9(D3D_SDK_VERSION);
		if(g_pD3D == NULL) {
			exit(1);
		}
	}

	// Get size of window client rectangle.
	RECT rect;
	GetClientRect(hWnd, &rect);

  D3DPRESENT_PARAMETERS d3dpp;
  ZeroMemory( &d3dpp, sizeof(d3dpp) );
  d3dpp.Windowed = TRUE;
  d3dpp.BackBufferCount = 1;
  d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
	d3dpp.BackBufferWidth = rect.right - rect.left; 
  d3dpp.BackBufferHeight = rect.bottom - rect.top; 
  d3dpp.BackBufferFormat = D3DFMT_A8R8G8B8;
	//d3dpp.Flags = D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;
	d3dpp.hDeviceWindow = hWnd;
	d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
  d3dpp.MultiSampleType = D3DMULTISAMPLE_NONE;

  g_pD3D->CreateDevice( D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hWnd,
                        D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_PUREDEVICE,
                        &d3dpp, &g_pd3dDevice );

	// Create vertext buffer for rotating cube
	g_pd3dDevice->CreateVertexBuffer( 24*sizeof(Vertex),0, D3DFVF_CUSTOMVERTEX,
                                      D3DPOOL_DEFAULT, &g_pVertexBuffer, NULL );
	void *pVertices = NULL;

  g_pVertexBuffer->Lock( 0, sizeof(g_cubeVertices), (void**)&pVertices, 0 );
  memcpy( pVertices, g_cubeVertices, sizeof(g_cubeVertices) );
  g_pVertexBuffer->Unlock();

	// Create vertex buffer for video input background
	g_pd3dDevice->CreateVertexBuffer( 4*sizeof(Vertex),0, D3DFVF_CUSTOMVERTEX,
                                      D3DPOOL_DEFAULT, &g_pQuadVertexBuffer, NULL );
	void *pQuadVertices = NULL;

  g_pQuadVertexBuffer->Lock( 0, sizeof(g_quadVertices), (void**)&pQuadVertices, 0 );
  memcpy( pQuadVertices, g_quadVertices, sizeof(g_quadVertices) );
  g_pQuadVertexBuffer->Unlock();

	// Set render state.  Disable lighting.  Enable depth test.
  g_pd3dDevice->SetRenderState( D3DRS_LIGHTING, FALSE );
  g_pd3dDevice->SetRenderState( D3DRS_ZENABLE, TRUE );

	// Initialize font for onscreen statistics display.
  D3DXCreateFont( g_pd3dDevice, 15, 0, FW_BOLD, 1, FALSE, DEFAULT_CHARSET,
                  OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
				          "Arial", &g_pStatsFont );

  static SIZE_T  dwMin = 0, dwMax = 0;
  HANDLE hProcess = OpenProcess( PROCESS_QUERY_INFORMATION | PROCESS_SET_QUOTA, FALSE, GetCurrentProcessId());

  if(!hProcess) {
    printf("OpenProcess failed (%d)\n", GetLastError() );
  }

  // Retrieve the working set size of the process.
  if(!dwMin && !GetProcessWorkingSetSize(hProcess, &dwMin, &dwMax)) {
    printf("GetProcessWorkingSetSize failed (%d)\n", GetLastError());
  }

  // 128MB extra
  if(!SetProcessWorkingSetSize(hProcess, (128 * 0x100000) + dwMin, (128 * 0x100000) + (dwMax - dwMin))) {
     printf("SetProcessWorkingSetSize failed (%d)\n", GetLastError());
  }

  CloseHandle(hProcess);

	// Initialize buffers for GPU Direct for Video transfers.
	initBuffers();
}

void closeApp() {
  cleanupBuffers();
  SAFE_RELEASE(g_pStatsFont);
  SAFE_RELEASE(g_pVertexBuffer);
  SAFE_RELEASE(g_pQuadVertexBuffer);
  SAFE_RELEASE(g_pd3dDevice);
  SAFE_RELEASE(g_pD3D);
}

void render(IDirect3DTexture9 * dxFrameTexIn, IDirect3DTexture9 * dxFrameTexOut)
{
  static UINT l_uiFrameCount = 0;

	IDirect3DSurface9 *pTextureSurface;
	dxFrameTexOut->GetSurfaceLevel(0, &pTextureSurface);
	// Render to video output device buffer
	g_pd3dDevice->SetRenderTarget(0, pTextureSurface);

	if(SUCCEEDED(g_pd3dDevice->BeginScene())) {
		g_pd3dDevice->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_COLORVALUE(0.5f,0.5f,0.5f,1.0f), 1.0f, 0);

		D3DXMATRIX matProj;
		D3DXMATRIX matWorld;
		D3DXMATRIX matTrans;
		D3DXMATRIX matRot;
		D3DXMATRIX matScale;
		D3DXMATRIX matIdent;

		// Get size of window client rectangle.
		RECT rect;
		GetClientRect(hWnd, &rect);

		// Draw video input background

		// Set orthographic projection
		D3DXMatrixOrthoLH(&matProj, -2.0f, -2.0f, -1.0f, 1.0f);
    g_pd3dDevice->SetTransform(D3DTS_PROJECTION, &matProj);
		D3DXMatrixIdentity(&matIdent);
		g_pd3dDevice->SetTransform(D3DTS_WORLD, &matIdent);
		g_pd3dDevice->SetTransform(D3DTS_VIEW, &matIdent);

		g_pd3dDevice->SetStreamSource(0, g_pQuadVertexBuffer, 0, sizeof(Vertex));
		g_pd3dDevice->SetFVF(D3DFVF_CUSTOMVERTEX);
		g_pd3dDevice->SetTexture(0, dxFrameTexIn);
		g_pd3dDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP,  0, 2);

		// Set perspective project
		D3DXMatrixPerspectiveFovLH(&matProj, D3DXToRadian( 45.0f ), (float)(rect.right - rect.left) / (float)(rect.bottom - rect.top), 0.1f, 100.0f);
    g_pd3dDevice->SetTransform(D3DTS_PROJECTION, &matProj);

		// Draw rotating cube.
    double fCrot = ((double)l_uiFrameCount / storageinfo.fps);
		double fXrot = (10.1f * fCrot);
		double fYrot = (10.2f * fCrot);
		double fZrot = (10.3f * fCrot);

		D3DXMatrixTranslation(&matTrans, 0.0f, 0.0f, 5.0f );
		D3DXMatrixRotationYawPitchRoll(&matRot, D3DXToRadian(fXrot), D3DXToRadian(fYrot), D3DXToRadian(fZrot));

		matWorld = matRot * matTrans;
		g_pd3dDevice->SetTransform( D3DTS_WORLD, &matWorld );
		g_pd3dDevice->SetStreamSource( 0, g_pVertexBuffer, 0, sizeof(Vertex) );
		g_pd3dDevice->SetFVF( D3DFVF_CUSTOMVERTEX );

		// Top
		g_pd3dDevice->SetTexture(0, dxFrameTexIn );
		g_pd3dDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP,  0, 2);
		// Bottom
		g_pd3dDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP,  4, 2);
		g_pd3dDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP,  8, 2);
		g_pd3dDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP, 12, 2);
		g_pd3dDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP, 16, 2);
		g_pd3dDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP, 20, 2);
		
    g_pd3dDevice->EndScene();
	}

	// Blit to back buffer
	LPDIRECT3DSURFACE9 pBackBuffer = NULL;
	if(g_pd3dDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, &pBackBuffer ) == S_OK){
		g_pd3dDevice->SetRenderTarget(0, pBackBuffer);
		g_pd3dDevice->StretchRect(pTextureSurface, NULL, pBackBuffer, NULL, D3DTEXF_LINEAR);

    // Draw framecount
		if(SUCCEEDED(g_pd3dDevice->BeginScene())) {
			RECT rc;
      char s[128]; 
      sprintf(s, "%d", l_uiFrameCount);
			SetRect( &rc, 2, 15, 0, 0 );
			g_pStatsFont->DrawText(NULL, s, -1, &rc, DT_NOCLIP, D3DXCOLOR( 1.0f, 1.0f, 0.0f, 1.0f ));
			g_pd3dDevice->EndScene();
		}
	}

  HRESULT hr;
  IDirect3DSwapChain9* pSwapChain;
  g_pd3dDevice->GetSwapChain(0, &pSwapChain);
  while((hr = pSwapChain->Present(NULL, NULL, NULL, NULL, D3DPRESENT_DONOTWAIT)) == D3DERR_WASSTILLDRAWING) {
    SwitchToThread();
  }

  l_uiFrameCount++;

	SAFE_RELEASE(pSwapChain);
	SAFE_RELEASE(pBackBuffer);
	SAFE_RELEASE(pTextureSurface);
}


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	WNDCLASSEX winClass = { sizeof(WNDCLASSEX), NULL, WndProc, 0L, 0L, hInstance, NULL, NULL, NULL, NULL, "DVP_WINDOW_CLASS", NULL };
  winClass.hCursor = LoadCursor(NULL, IDC_ARROW);

	// Register window class.
	if( !RegisterClassEx(&winClass) )
		return E_FAIL;

	hWnd = CreateWindow( "DVP_WINDOW_CLASS", "DirectAPI Direct3D", WS_OVERLAPPEDWINDOW, 100, 100, 640, 480, GetDesktopWindow(), NULL, GetModuleHandle(NULL), NULL );
	if(hWnd == NULL)
		return E_FAIL;

	init();
  ShowWindow(hWnd, nCmdShow);
  UpdateWindow(hWnd);

  int flags = 0;
  int res = SV_OK;
  HANDLE renderThreadHandle = NULL, recordThreadHandle = NULL, displayThreadHandle = NULL;

  res = sv_openex(&sv, "", SV_OPENPROGRAM_DEMOPROGRAM, SV_OPENTYPE_DEFAULT, 0, 0);
  if(res != SV_OK) {
    printf("sv_openex:%d (%s)\n", res, sv_geterrortext(res));
    return 1;
  }

  // raster information
  res = sv_storage_status(sv, 0, NULL, &storageinfo, sizeof(sv_storageinfo), SV_STORAGEINFO_COOKIEISJACK);
  if(res != SV_OK) {
    printf("sv_storage_status():%d (%s)\n", res, sv_geterrortext(res));
    return 1;
  }

  res = sv_jack_option_set(sv, 1, SV_OPTION_ALPHAGAIN, 0);
  if(res != SV_OK) {
    printf("sv_storage_status():%d (%s)\n", res, sv_geterrortext(res));
    return 1;
  }

  res = sv_jack_option_set(sv, 1, SV_OPTION_ALPHAOFFSET, 0x10000);
  if(res != SV_OK) {
    printf("sv_storage_status():%d (%s)\n", res, sv_geterrortext(res));
    return 1;
  }

  // output
  res = sv_direct_initex(sv, &dh_out, "NVIDIA/DIRECTX9", 0, numBuffers, flags, g_pd3dDevice);
  if(res != SV_OK) {
    printf("sv_direct_init(out):%d (%s)\n", res, sv_geterrortext(res));
    return 1;
  }

  // input
  res = sv_direct_initex(sv, &dh_in, "NVIDIA/DIRECTX9", 1, numBuffers, flags, g_pd3dDevice);
  if(res != SV_OK) {
    printf("sv_direct_init(in):%d (%s)\n", res, sv_geterrortext(res));
    return 1;
  }

  for(int buffer = 0; buffer < numBuffers; buffer++) {
    for (int i = DVPINPUT; i <= DVPOUTPUT; i++) {
      sv_direct_handle * dh = NULL;
      switch(i) {
      case DVPOUTPUT:
        dh = dh_out;
        break;
      case DVPINPUT:
        dh = dh_in;
        break;
      default:;
      }
      if(dh) {
        res = sv_direct_bind_d3d9surface(sv, dh, buffer, (IDirect3DSurface9 *) g_renderTargetTexture[buffer][i]);
        if(res != SV_OK) {
          printf("sv_direct_bind_opengl:%d (%s)\n", res, sv_geterrortext(res));
          return 1;
        }
        res = sv_direct_bind_timecode(sv, dh, buffer, &timecodes[buffer][i]);
        if(res != SV_OK) {
          printf("sv_direct_bind_timecode:%d (%s)\n", res, sv_geterrortext(res));
          return 1;
        }
      }
    }
  }

  // init events
  for(int buffer = 0; buffer < numBuffers; buffer++) {
    dvs_cond_init(&record_ready[buffer]);
    dvs_mutex_init(&record_ready_mutex[buffer]);
    dvs_cond_init(&display_ready[buffer]);
    dvs_mutex_init(&display_ready_mutex[buffer]);
    dvs_cond_init(&record_go[buffer]);
    dvs_mutex_init(&record_go_mutex[buffer]);
    dvs_cond_init(&display_go[buffer]);
    dvs_mutex_init(&display_go_mutex[buffer]);
  }

  printf("starting threads\n");
  renderthread_data rtdata;
  rtdata.dh_in = dh_in;
  rtdata.dh_out = dh_out;
  startThread(&renderThreadHandle, renderThread, &rtdata);
  startThread(&recordThreadHandle, recordThread, dh_in);
  startThread(&displayThreadHandle, displayThread, dh_out);

  // first record go
  dvs_cond_broadcast(&record_go[0], &record_go_mutex[0], FALSE);
    
	while(running) {
    MSG uMsg;
    while(PeekMessage(&uMsg, NULL, 0, 0, PM_NOREMOVE) == TRUE) { 
      if(GetMessage(&uMsg, NULL, 0, 0)) { 
        TranslateMessage(&uMsg); 
        DispatchMessage(&uMsg);
      } else { 
        return TRUE; 
      } 
      Sleep(250);
    }
  }

  stopThread(renderThreadHandle);
  stopThread(recordThreadHandle);
  stopThread(displayThreadHandle);

  for(int buffer = 0; buffer < numBuffers; buffer++) {
    dvs_cond_free(&record_ready[buffer]);
    dvs_mutex_free(&record_ready_mutex[buffer]);
    dvs_cond_free(&display_ready[buffer]);
    dvs_mutex_free(&display_ready_mutex[buffer]);
    dvs_cond_free(&record_go[buffer]);
    dvs_mutex_free(&record_go_mutex[buffer]);
    dvs_cond_free(&display_go[buffer]);
    dvs_mutex_free(&display_go_mutex[buffer]);
  }

  sv_direct_free(sv, dh_in);
  sv_direct_free(sv, dh_out);
  sv_close(sv);

	closeApp();
	DestroyWindow(hWnd);
  UnregisterClass( "MY_WINDOWS_CLASS", winClass.hInstance );

	return S_OK;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  switch(msg) {	
  case WM_KEYDOWN:
    switch(wParam) {
    case VK_ESCAPE:
      running = FALSE;
      break;
    }
    break;
  case WM_SIZE:
    {
      RECT rect;
      GetClientRect(hWnd, &rect);

      // Initialize projection matrix.
      D3DXMATRIX matProj;
      D3DXMatrixPerspectiveFovLH(&matProj, D3DXToRadian( 45.0f ), (float)(rect.right - rect.left) / (float)(rect.bottom - rect.top), 0.1f, 100.0f);
      g_pd3dDevice->SetTransform(D3DTS_PROJECTION, &matProj);
    }
    break;
  default:
    return DefWindowProc( hWnd, msg, wParam, lParam );
  }
  return 0;
}

DWORD WINAPI renderThread(void * data)
{
  sv_direct_handle * dh_in = ((renderthread_data *)data)->dh_in;
  sv_direct_handle * dh_out = ((renderthread_data *)data)->dh_out;
  int recordCurrent = 0;
  int displayCurrent = 0;
  int res = SV_OK;

  while(running) {
    dvs_cond_wait(&record_ready[recordCurrent % numBuffers], &record_ready_mutex[recordCurrent % numBuffers], FALSE);
    if(displayCurrent > 0) dvs_cond_wait(&display_ready[displayCurrent % numBuffers], &display_ready_mutex[displayCurrent % numBuffers], FALSE);

    res = sv_direct_sync(sv, dh_in, recordCurrent % numBuffers, 0);
    if(res != SV_OK) {
      printf("sv_direct_sync(in):%d (%s)\n", res, sv_geterrortext(res));
    } else {
      render(g_renderTargetTexture[recordCurrent % numBuffers][DVPINPUT], g_renderTargetTexture[displayCurrent % numBuffers][DVPOUTPUT]);

      // Simply copy timecodes from input to output.
      memcpy(&timecodes[displayCurrent % numBuffers][DVPOUTPUT], &timecodes[recordCurrent % numBuffers][DVPINPUT], sizeof(sv_direct_timecode));

      // next input buffer
      recordCurrent++;
      dvs_cond_broadcast(&record_go[recordCurrent % numBuffers], &record_go_mutex[recordCurrent % numBuffers], FALSE);

      res = sv_direct_sync(sv, dh_out, displayCurrent % numBuffers, 0);
      if(res != SV_OK) {
        printf("sv_direct_sync(out):%d (%s)\n", res, sv_geterrortext(res));
      } else {
        dvs_cond_broadcast(&display_go[displayCurrent % numBuffers], &display_go_mutex[displayCurrent % numBuffers], FALSE);
        // next output buffer
        displayCurrent++;    
      }
    }
  }

  return 0;
}

DWORD WINAPI recordThread(void * vdh) 
{
  sv_direct_handle * dh = (sv_direct_handle *) vdh;
  sv_direct_bufferinfo bufferinfo = { sizeof(sv_direct_bufferinfo) };
  sv_direct_info info;
  int dropped = 0;
  int res = SV_OK;
  int recordCurrent = 0;

  do {
    if(res == SV_OK) {
      dvs_cond_wait(&record_go[recordCurrent % numBuffers], &record_go_mutex[recordCurrent % numBuffers], FALSE);

      res = sv_direct_record(sv, dh, recordCurrent % numBuffers, SV_DIRECT_FLAG_DISCARD, &bufferinfo);
      if(res != SV_OK) {
        printf("sv_direct_record:%d (%s)\n", res, sv_geterrortext(res));

        switch(res) {
        case SV_ERROR_INPUT_VIDEO_NOSIGNAL:
        case SV_ERROR_INPUT_VIDEO_RASTER:
        case SV_ERROR_INPUT_VIDEO_DETECTING:
          // non-fatal errors
          res = SV_OK;
          break;
        }
      }

      if(verbose) {
        unsigned int vsync_duration = (1000000 / storageinfo.fps / (storageinfo.videomode & SV_MODE_STORAGE_FRAME ? 1 : 2));
        unsigned __int64 record_go = ((unsigned __int64)bufferinfo.clock_high << 32) | bufferinfo.clock_low;
        unsigned __int64 dma_ready = ((unsigned __int64)bufferinfo.dma.clock_ready_high << 32) | bufferinfo.dma.clock_ready_low;
        printf("%d> record:\tvsync->dmadone: %5d dmastart->vsync:%5d\n", recordCurrent % numBuffers, (int)(dma_ready - record_go - vsync_duration), bufferinfo.dma.clock_distance);
      }
    }

    if(res == SV_OK) {
      dvs_cond_broadcast(&record_ready[recordCurrent % numBuffers], &record_ready_mutex[recordCurrent % numBuffers], FALSE);
      // next record buffer
      recordCurrent++;
    }

    if(res == SV_OK) {
      info.size = sizeof(sv_direct_info);
      res = sv_direct_status(sv, dh, &info);
      if(res != SV_OK) {
        printf("sv_direct_status:%d (%s)\n", res, sv_geterrortext(res));
      } else {
        if(info.dropped > dropped) {
          printf("recordThread:  dropped:%d\n", info.dropped);
          dropped = info.dropped;
        }
      }
    }
  } while(running && (res == SV_OK));

  return 0;
}

DWORD WINAPI displayThread(void * vdh) 
{
  sv_direct_handle * dh = (sv_direct_handle *) vdh;
  sv_direct_bufferinfo bufferinfo = { sizeof(sv_direct_bufferinfo) };
  sv_direct_info info;
  int dropped = 0;
  int res = SV_OK;
  int displayCurrent = 0;

  do {
    if(res == SV_OK) {
      dvs_cond_wait(&display_go[displayCurrent % numBuffers], &display_go_mutex[displayCurrent % numBuffers], FALSE);
      
      res = sv_direct_display(sv, dh, displayCurrent % numBuffers, SV_DIRECT_FLAG_DISCARD, &bufferinfo);
      if(res != SV_OK) {
        printf("sv_direct_display:%d (%s)\n", res, sv_geterrortext(res));

        switch(res) {
        case SV_ERROR_VSYNCPASSED:
          // might happen in case of delay by GPU
          res = SV_OK;
          break;
        }
      }

      if(verbose) {
        unsigned int vsync_duration = (1000000 / storageinfo.fps / (storageinfo.videomode & SV_MODE_STORAGE_FRAME ? 1 : 2));
        printf("%d> display:\tvsync->dmastart:%5d dmastart->vsync:%5d\n", displayCurrent % numBuffers, (int)(vsync_duration - bufferinfo.dma.clock_distance), bufferinfo.dma.clock_distance);
      }
    }

    if(res == SV_OK) {
      // next display buffer
      displayCurrent++;
      dvs_cond_broadcast(&display_ready[displayCurrent % numBuffers], &display_ready_mutex[displayCurrent % numBuffers], FALSE);
    }

    if(res == SV_OK) {
      info.size = sizeof(sv_direct_info);
      res = sv_direct_status(sv, dh, &info);
      if(res != SV_OK) {
        printf("sv_direct_status:%d (%s)\n", res, sv_geterrortext(res));
      } else {
        if(info.dropped > dropped) {
          printf("displayThread: dropped:%d\n", info.dropped);
          dropped = info.dropped;
        }
      }
    }
  } while(running && (res == SV_OK));

  return 0;
}

void startThread(HANDLE * t, LPTHREAD_START_ROUTINE func, void * data)
{
  if(t) {
    *t = CreateThread(0, 0, func, data, 0, NULL);
  }
}

void stopThread(HANDLE t)
{
  if(t) {
    running = FALSE;
    WaitForSingleObject(t, 1000);
    CloseHandle(t);
  }
}
