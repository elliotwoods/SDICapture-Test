#include <windows.h>
#include <string>

#include <GL/glew.h>
#include "GLSource.h"
#include "DVSSource.h"
#include "GLSink.h"
#include "GLWindow.h"


unsigned int g_uiWidth  = 1920;
unsigned int g_uiHeight = 1080;

#define USE_DVS_SOURCE 1

typedef struct
{
    bool        bRunning;
    GLWindow*   pSourceWin;
    GLWindow*   pSinkWin;
    GLSink*     pSink;
#if USE_DVS_SOURCE
    DVSSource*  pSource;
#else
    GLSource*   pSource;
#endif
} ThreadData;


ThreadData* g_pThreadData = NULL;

HANDLE  g_hSinkReady;
HANDLE  g_hSourceReady;
HANDLE  g_hSourceDone;


PFNGLWAITMARKERAMDPROC          glWaitMarkerAMD;
PFNGLWRITEMARKERAMDPROC         glWriteMarkerAMD;
PFNGLMAKEBUFFERSRESIDENTAMDPROC glMakeBuffersResidentAMD;
PFNGLBUFFERBUSADDRESSAMDPROC    glBufferBusAddressAMD;


LRESULT CALLBACK    WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);


DWORD WINAPI SourceThreadFunc(LPVOID lpArgs)
{
    ThreadData* pThreadData = (ThreadData*)lpArgs;
#if !USE_DVS_SOURCE
    GLWindow*   pMyWin      = NULL;

    if (!pThreadData || !pThreadData->pSourceWin)
        return 0;
#else
    if (!pThreadData)
        return 0;
#endif

    WaitForSingleObject(g_hSinkReady, INFINITE);

#if USE_DVS_SOURCE
    DVSSource * pSource = new DVSSource;
    
    pSource->init();
    pSource->resize(g_uiWidth, g_uiHeight);
#else
    pMyWin = pThreadData->pSourceWin;

    pMyWin->createContext();
    pMyWin->makeCurrent();

    GLSource* pSource = new GLSource;

    pSource->initGL();
    pSource->resize(g_uiWidth, g_uiHeight);
#endif

    if (pSource->createUpStream(g_uiWidth, g_uiHeight, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE))
    {
        pThreadData->pSource = pSource;

        // Get input buffer of Sink, this buffer will be used to synchonize
        // Sink and source
        SyncedBuffer* pBuffer = pThreadData->pSink->getInputBuffer();

        // Now the buffers on sink GPU are created and the addresses can be obtained
        unsigned long long* pBufferBusAddress = pThreadData->pSink->getBufferBusAddress();
        unsigned long long* pMarkerBusAddress = pThreadData->pSink->getMarkerBusAddress();

        if (!pBuffer ||!pSource->setRemoteMemory(pBufferBusAddress, pMarkerBusAddress))
        {
            g_pThreadData->bRunning = false;
        }

        pSource->setOutputBuffer(pBuffer);

        ReleaseSemaphore(g_hSourceReady, 1, 0);

        while (g_pThreadData->bRunning)
        {
            pSource->draw();
#if !USE_DVS_SOURCE
            pMyWin->swapBuffers();
#endif
        }    
    }

    g_pThreadData->bRunning = false;

    // Just in case creatUpStrea
    ReleaseSemaphore(g_hSourceReady, 1, 0);

    pSource->release();
    delete pSource;

    // Indicate that the Source is done and does no longer need the
    // input buffer of the sink.
    ReleaseSemaphore(g_hSourceDone, 1, 0);

    return 0;
}


DWORD WINAPI SinkThreadFunc(LPVOID lpArgs)
{
    ThreadData* pThreadData = (ThreadData*)lpArgs;
    GLWindow*   pMyWin      = NULL;

#if USE_DVS_SOURCE
    if (!pThreadData)
        return 0;
#else
    if (!pThreadData || !pThreadData->pSourceWin)
        return 0;
#endif

    pMyWin = pThreadData->pSinkWin;

    pMyWin->createContext();
    pMyWin->makeCurrent();

    GLSink* pSink = new GLSink;

    pSink->initGL();
    pSink->resize(g_uiWidth, g_uiHeight);

    g_pThreadData->pSink = pSink;
    
    if (pSink->createDownStream(g_uiWidth, g_uiHeight, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE))
    {
        //unsigned long long* pBufferBusAddress = pSink->getBufferBusAddress();
        //unsigned long long* pMarkerBusAddress = pSink->getMarkerBusAddress();

        ReleaseSemaphore(g_hSinkReady, 1, NULL);

        WaitForSingleObject(g_hSourceReady, INFINITE);
        
        while (g_pThreadData->bRunning)
        {
            pSink->draw();
            pMyWin->swapBuffers();
        }   
        
    }

    g_pThreadData->bRunning = false;

    pSink->release();

    // Since the Souce is using the input buffer of the Sink as output buffer
    // the Sink can only be deleted once the Source is done.
    WaitForSingleObject(g_hSourceDone, 1000);

    delete pSink;

    return 0;
}



int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR lpCmdLine, int nShowCmd)
{
    WNDCLASSEX      wndclass;
    const LPCSTR    cClassName  = "OGL";
    const LPCSTR    cWindowName = "GPU P2P Copy";

    // Register WindowClass
    wndclass.cbSize         = sizeof(WNDCLASSEX);
    wndclass.style          = CS_OWNDC;
    wndclass.lpfnWndProc    = WndProc;
    wndclass.cbClsExtra     = 0;
    wndclass.cbWndExtra     = 0;
    wndclass.hInstance      = (HINSTANCE)GetModuleHandle(NULL);
    wndclass.hIcon          = LoadIcon(NULL, IDI_APPLICATION); 
    wndclass.hCursor        = LoadCursor(NULL, IDC_ARROW);
    wndclass.hbrBackground  = NULL;
    wndclass.lpszMenuName   = NULL;
    wndclass.lpszClassName  = cClassName;
    wndclass.hIconSm        = LoadIcon(NULL, IDI_APPLICATION);

    if (!RegisterClassEx(&wndclass))
        return WM_QUIT;

#if USE_DVS_SOURCE
    GLWindow* pSourceWin = NULL;
    GLWindow* pSinkWin   = new GLWindow("GL Sink",   cClassName);

    if (!pSinkWin->init())
    {
        MessageBox(NULL, "Could not init GLWindow", cWindowName, MB_ICONERROR | MB_OK);
        return WM_QUIT;
    }

    // Create window on first Dsiplay of GPU 1
    unsigned int uiDsp = pSinkWin->getGetDisplayOnGPU(0, 0);

    pSinkWin->create(g_uiWidth, g_uiHeight, uiDsp);
    pSinkWin->open();

    // dummy context
    pSinkWin->createContext();

    if (glewInit() != GLEW_OK)
        return WM_QUIT;

    if (!glMakeBuffersResidentAMD)
        glMakeBuffersResidentAMD = (PFNGLMAKEBUFFERSRESIDENTAMDPROC) wglGetProcAddress("glMakeBuffersResidentAMD");

    if (!glBufferBusAddressAMD)
        glBufferBusAddressAMD = (PFNGLBUFFERBUSADDRESSAMDPROC) wglGetProcAddress("glBufferBusAddressAMD");

    if (!glWaitMarkerAMD)
        glWaitMarkerAMD = (PFNGLWAITMARKERAMDPROC) wglGetProcAddress("glWaitMarkerAMD");

    if (!glWriteMarkerAMD)
        glWriteMarkerAMD = (PFNGLWRITEMARKERAMDPROC) wglGetProcAddress("glWriteMarkerAMD");

    if (!(glMakeBuffersResidentAMD && glWaitMarkerAMD && glWriteMarkerAMD && glBufferBusAddressAMD))
        return WM_QUIT;

    pSinkWin->deleteContext();
#else
    GLWindow* pSourceWin = new GLWindow("GL Source", cClassName);
    GLWindow* pSinkWin   = new GLWindow("GL Sink",   cClassName);

    if (!pSourceWin->init() || !pSinkWin->init())
    {
        MessageBox(NULL, "Could not init GLWindow", cWindowName, MB_ICONERROR | MB_OK);
        return WM_QUIT;
    }

    if (pSourceWin->getNumGPUs() < 2)
    {
        MessageBox(NULL, "Only 1 GPU detected. You will need 2 GPUs to run this demo.", cWindowName, MB_ICONERROR | MB_OK);
        return WM_QUIT;
    }

    if ((pSourceWin->getNumDisplaysOnGPU(0) == 0) || pSourceWin->getNumDisplaysOnGPU(1) == 0)
    {
        MessageBox(NULL, "Not all GPUs have a Display mapped!", cWindowName, MB_ICONERROR | MB_OK);
        return WM_QUIT;
    }

    // Create window on first Dsiplay of GPU 0
    unsigned int uiDsp = pSourceWin->getGetDisplayOnGPU(0, 0);
    
    pSourceWin->create(g_uiWidth, g_uiHeight, uiDsp);
    pSourceWin->open();

    // crteate dumm context to load extensions
    pSourceWin->createContext();

    if (glewInit() != GLEW_OK)
        return WM_QUIT;

    if (!glMakeBuffersResidentAMD)
        glMakeBuffersResidentAMD = (PFNGLMAKEBUFFERSRESIDENTAMDPROC) wglGetProcAddress("glMakeBuffersResidentAMD");

    if (!glBufferBusAddressAMD)
        glBufferBusAddressAMD = (PFNGLBUFFERBUSADDRESSAMDPROC) wglGetProcAddress("glBufferBusAddressAMD");

    if (!glWaitMarkerAMD)
        glWaitMarkerAMD = (PFNGLWAITMARKERAMDPROC) wglGetProcAddress("glWaitMarkerAMD");

    if (!glWriteMarkerAMD)
        glWriteMarkerAMD = (PFNGLWRITEMARKERAMDPROC) wglGetProcAddress("glWriteMarkerAMD");

    if (!(glMakeBuffersResidentAMD && glWaitMarkerAMD && glWriteMarkerAMD && glBufferBusAddressAMD))
        return WM_QUIT;

    pSourceWin->deleteContext();

    // Create window on first Dsiplay of GPU 1
    uiDsp = pSinkWin->getGetDisplayOnGPU(1, 0);

    pSinkWin->create(g_uiWidth, g_uiHeight, uiDsp);
    pSinkWin->open();
#endif

    g_pThreadData = new ThreadData;

    g_pThreadData->bRunning   = true;
    g_pThreadData->pSourceWin = pSourceWin;
    g_pThreadData->pSinkWin   = pSinkWin;
    g_pThreadData->pSource    = NULL;
    g_pThreadData->pSink      = NULL;

    // Create semaphore to indicate that the init of the Sink is ready
    g_hSinkReady   = CreateSemaphore(NULL, 0, 1, NULL);
    // Create semaphore to indicate that the init of the source is done
    g_hSourceReady = CreateSemaphore(NULL, 0, 1, NULL);
    // Create semaphore to indicate that the Source has terminated
    g_hSourceDone  = CreateSemaphore(NULL, 0, 1, NULL);

    DWORD dwSourceThreadId;
    DWORD dwSinkThreadId;

    HANDLE hSourceThread = CreateThread(NULL, 0, SourceThreadFunc, g_pThreadData, 0, &dwSourceThreadId);
    HANDLE hSinkThread   = CreateThread(NULL, 0, SinkThreadFunc,   g_pThreadData,   0, &dwSinkThreadId);

    // Run message loop
    MSG	    Msg;

    while (GetMessage(&Msg, NULL, 0, 0) && g_pThreadData->bRunning)
    {
	    TranslateMessage(&Msg);
		  DispatchMessage(&Msg);
    }

    g_pThreadData->bRunning = false;

    WaitForSingleObject(hSourceThread, INFINITE);
    WaitForSingleObject(hSinkThread,   INFINITE);

    if(pSourceWin) {
      delete pSourceWin;
    }
    if(pSinkWin) {
      delete pSinkWin;
    }

    return WM_QUIT;
}



LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    static int nLastx = 0;
    static int nLasty = 0;

    switch (uMsg)
    {
        char c;

        case WM_CHAR:
            c = (char)wParam;

            switch (c)
            {
            case VK_ESCAPE:
                PostQuitMessage(0);
		        break;
            }
            return 0;

        case WM_CREATE:
            return 0;

        case WM_SIZE:
            //g_nWidth  = LOWORD(lParam);
            //g_nHeight = HIWORD(lParam);
		    
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

