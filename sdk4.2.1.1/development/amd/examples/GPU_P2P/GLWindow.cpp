
#include <windows.h>

#include <GL/glew.h>
#include <GL/wglew.h>

#include "adl_prototypes.h"
#include "GLWindow.h"


using namespace std;


// Handle to ADL DLL
static HINSTANCE hDLL = NULL;

// Memory Allocation function needed for ADL
void* __stdcall ADL_Alloc ( int iSize )
{
    void* lpBuffer = malloc ( iSize );
    return lpBuffer;
}

// Memory Free function needed for ADL
void __stdcall ADL_Free ( void* lpBuffer )
{
    if ( NULL != lpBuffer )
    {
        free ( lpBuffer );
        lpBuffer = NULL;
    }
}


void MyDebugFunc(GLuint id, GLenum category, GLenum severity, GLsizei length, const GLchar* message, GLvoid* userParam)
{
    MessageBox(NULL, message, "GL Debug Message", MB_ICONWARNING | MB_OK);
}


GLWindow::GLWindow(const char* strWinName, const char* strClsaaName)
{
    m_strClassName = strClsaaName;
    m_strWinName   = strWinName;

    m_hDC = NULL;
    m_hGLRC = NULL;
    m_hWnd = NULL;

    m_uiWidth = 800;
    m_uiHeight = 600;

    m_uiPosX = 0;
    m_uiPosY = 0;

    m_uiNumGPU = 0;

    m_bADLReady   = false;
    m_bFullScreen = false;
}


GLWindow::~GLWindow(void)
{
    vector<DisplayData*>::iterator itr;

    for (itr = m_DisplayInfo.begin(); itr != m_DisplayInfo.end(); ++itr)
    {
        if ((*itr))
        {
            delete (*itr);
        }
    }
}


bool GLWindow::init()
{
    int				nNumDisplays = 0;
	int				nNumAdapters = 0;
    int             nCurrentBusNumber = 0;
	LPAdapterInfo   pAdapterInfo = NULL;
    unsigned int    uiCurrentGPUId     = 0;
    unsigned int    uiCurrentDisplayId = 0;

    // load all required ADL functions
    if (!setupADL())
        return false;

    // Determine how many adapters and displays are in the system
	ADL_Adapter_NumberOfAdapters_Get(&nNumAdapters);

	if (nNumAdapters > 0)
	{
		pAdapterInfo = (LPAdapterInfo)malloc ( sizeof (AdapterInfo) * nNumAdapters );
        memset ( pAdapterInfo,'\0', sizeof (AdapterInfo) * nNumAdapters );
	}

	ADL_Adapter_AdapterInfo_Get (pAdapterInfo, sizeof (AdapterInfo) * nNumAdapters);

    // Loop through all adapters 
	for (int i = 0; i < nNumAdapters; ++i)
	{
		int				nAdapterIdx; 
		int				nAdapterStatus;
		
		nAdapterIdx = pAdapterInfo[i].iAdapterIndex;

		ADL_Adapter_Active_Get(nAdapterIdx, &nAdapterStatus);

		if (nAdapterStatus)
		{
			LPADLDisplayInfo	pDisplayInfo = NULL;

			ADL_Display_DisplayInfo_Get(nAdapterIdx, &nNumDisplays, &pDisplayInfo, 0);

			for (int j = 0; j < nNumDisplays; ++j)
			{
				// check if display is connected and mapped
				if (pDisplayInfo[j].iDisplayInfoValue & ADL_DISPLAY_DISPLAYINFO_DISPLAYCONNECTED)
				{
                    DEVMODE DevMode;

					// check if display is mapped on adapter
					if (pDisplayInfo[j].iDisplayInfoValue & ADL_DISPLAY_DISPLAYINFO_DISPLAYMAPPED && pDisplayInfo[j].displayID.iDisplayLogicalAdapterIndex == nAdapterIdx)
					{
                        if (nCurrentBusNumber == 0)
                        {
                            // Found first GPU in the system
                            ++m_uiNumGPU;
                            nCurrentBusNumber = pAdapterInfo[nAdapterIdx].iBusNumber;
                        }
                        else if (nCurrentBusNumber != pAdapterInfo[nAdapterIdx].iBusNumber)
                        {
                            // found new GPU
                            ++m_uiNumGPU;
                            ++uiCurrentGPUId;
                            nCurrentBusNumber = pAdapterInfo[nAdapterIdx].iBusNumber;
                        }

                        ++uiCurrentDisplayId;

                        EnumDisplaySettings(pAdapterInfo[i].strDisplayName, ENUM_CURRENT_SETTINGS, &DevMode);

                        // Found first mapped display
                        DisplayData* pDsp = new DisplayData;

                        pDsp->uiGPUId               = uiCurrentGPUId;
                        pDsp->uiDisplayId           = uiCurrentDisplayId;
                        pDsp->uiDisplayLogicalId    = pDisplayInfo[j].displayID.iDisplayLogicalAdapterIndex;
                        pDsp->uiOriginX             = DevMode.dmPosition.x;
                        pDsp->uiOriginY             = DevMode.dmPosition.y;
                        pDsp->uiWidth               = DevMode.dmPelsWidth;
                        pDsp->uiHeight              = DevMode.dmPelsHeight;
                        pDsp->strDisplayname        = string(pAdapterInfo[i].strDisplayName);

                        m_DisplayInfo.push_back(pDsp);
                    }
                }
            }
        }
    }

    return true;
}


unsigned int GLWindow::getNumDisplaysOnGPU(unsigned int uiGPU)
{
    unsigned int uiNumDsp = 0;

    vector<DisplayData*>::iterator itr;

    for (itr = m_DisplayInfo.begin(); itr != m_DisplayInfo.end(); ++itr)
    {
        if ((*itr)->uiGPUId == uiGPU)
        {
            ++uiNumDsp;
        }
    }

    return uiNumDsp;
}



unsigned int GLWindow::getGetDisplayOnGPU(unsigned int uiGPU, unsigned int n)
{
    unsigned int uiCurrentDsp = 0;

    vector<DisplayData*>::iterator itr;

    for (itr = m_DisplayInfo.begin(); itr != m_DisplayInfo.end(); ++itr)
    {
        if ((*itr)->uiGPUId == uiGPU)
        {
            if (uiCurrentDsp == n)
            {
                return (*itr)->uiDisplayId;
            }

            ++uiCurrentDsp;
        }
    }

    return 0;
}



bool GLWindow::create(unsigned int uiWidth, unsigned int uiHeight, unsigned int uiDspIndex)
{
    RECT        WinSize;
    DWORD		dwExStyle;
	DWORD		dwStyle;
	int			nPixelFormat;

    // Get information for the display with id uiDspId. This is the ID
    // shown by CCC. Use the origin of this display as base position for
    // opening the window. Like this a window can be opened on a particular
    // GPU.
    DisplayData* pDsp = getDisplayData(uiDspIndex);

    if (pDsp)
    {
        m_uiPosX += pDsp->uiOriginX;
        m_uiPosY += pDsp->uiOriginY;
    }

    // Adjust window size so that the ClientArea has the initial size
    // of gWidth and gHeight
    WinSize.bottom = uiHeight; 
    WinSize.left   = 0;
    WinSize.right  = uiWidth;
    WinSize.top    = 0;

    AdjustWindowRect(&WinSize, WS_OVERLAPPEDWINDOW, false);

    m_uiWidth  = WinSize.right  - WinSize.left;
    m_uiHeight = WinSize.bottom - WinSize.top;    

	if (m_bFullScreen)
	{
		dwExStyle = WS_EX_APPWINDOW;								
		dwStyle   = WS_POPUP;										
		//ShowCursor(FALSE);	
	}
	else
	{
		dwExStyle=WS_EX_APPWINDOW | WS_EX_WINDOWEDGE;	
		dwStyle=WS_OVERLAPPEDWINDOW;
	}

	m_hWnd = CreateWindowEx( dwExStyle,
						     m_strClassName.c_str(), 
						     m_strWinName.c_str(),
						     dwStyle,
						     m_uiPosX,
						     m_uiPosY,
						     m_uiWidth,
						     m_uiHeight,
						     NULL,
						     NULL,
						    (HINSTANCE)GetModuleHandle(NULL),
						     NULL);

	if (!m_hWnd)
		return FALSE;

	static PIXELFORMATDESCRIPTOR pfd;

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


	m_hDC = GetDC(m_hWnd);

	if (!m_hDC)
		return false;

	nPixelFormat = ChoosePixelFormat(m_hDC, &pfd);

    if (!nPixelFormat)
		return false;

	SetPixelFormat(m_hDC, nPixelFormat, &pfd);
	

	return true;
}


void GLWindow::open()
{
    ShowWindow(m_hWnd, SW_SHOW);
	SetForegroundWindow(m_hWnd);
	SetFocus(m_hWnd);

	UpdateWindow(m_hWnd);
}


void GLWindow::makeCurrent()
{
    wglMakeCurrent(m_hDC, m_hGLRC);
}


void GLWindow::swapBuffers()
{
    SwapBuffers(m_hDC);
}


bool GLWindow::createContext()
{
    if (!m_hDC)
        return false;

    m_hGLRC = wglCreateContext(m_hDC);

    if (!m_hGLRC)
        return false;

    wglMakeCurrent( m_hDC, m_hGLRC );
   
    if (WGLEW_ARB_create_context)
    {
        wglDeleteContext(m_hGLRC);

        int attribs[] = {
          WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
          WGL_CONTEXT_MINOR_VERSION_ARB, 2,
          WGL_CONTEXT_PROFILE_MASK_ARB , WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB,
#ifdef DEBUG
          WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_DEBUG_BIT_ARB,
#endif
          0
        }; 

        m_hGLRC = wglCreateContextAttribsARB(m_hDC, 0, attribs);

        if (m_hGLRC)
        {
            wglMakeCurrent(m_hDC, m_hGLRC);

            if (GLEW_AMD_debug_output)
                glDebugMessageCallbackAMD((GLDEBUGPROCAMD)&MyDebugFunc, NULL);

            return true;            
        }
    }

    return false;
}


void GLWindow::deleteContext()
{
    wglMakeCurrent(m_hDC, NULL);
    wglDeleteContext(m_hGLRC);
}



// Load ADL library and get function pointers
bool GLWindow::setupADL()
{
    // check if ADL was already loaded
    if (hDLL)
    {
        return true;
    }

	hDLL = LoadLibrary("atiadlxx.dll");

	 if (hDLL == NULL)
       hDLL = LoadLibrary("atiadlxy.dll");

	if (!hDLL)
		return false;

	// Get proc address of needed ADL functions
	ADL_Main_Control_Create = (ADL_MAIN_CONTROL_CREATE)GetProcAddress(hDLL,"ADL_Main_Control_Create");
	if (!ADL_Main_Control_Create)
		return false;

	ADL_Main_Control_Destroy         = (ADL_MAIN_CONTROL_DESTROY)GetProcAddress(hDLL, "ADL_Main_Control_Destroy");
	if (!ADL_Main_Control_Destroy)
		return false;

	ADL_Adapter_NumberOfAdapters_Get = (ADL_ADAPTER_NUMBEROFADAPTERS_GET)GetProcAddress(hDLL,"ADL_Adapter_NumberOfAdapters_Get");
	if (!ADL_Adapter_NumberOfAdapters_Get)
		return false;

	ADL_Adapter_AdapterInfo_Get = (ADL_ADAPTER_ADAPTERINFO_GET)GetProcAddress(hDLL,"ADL_Adapter_AdapterInfo_Get");
	if (!ADL_Adapter_AdapterInfo_Get)
		return false;

	ADL_Display_DisplayInfo_Get = (ADL_DISPLAY_DISPLAYINFO_GET)GetProcAddress(hDLL,"ADL_Display_DisplayInfo_Get");
	if (!ADL_Display_DisplayInfo_Get)
		return false;

	ADL_Adapter_Active_Get = (ADL_ADAPTER_ACTIVE_GET)GetProcAddress(hDLL,"ADL_Adapter_Active_Get");
	if (!ADL_Adapter_Active_Get)
		return false;

	ADL_Display_Position_Get = (ADL_DISPLAY_POSITION_GET)GetProcAddress(hDLL,"ADL_Display_Position_Get");
	if (!ADL_Display_Position_Get)
		return false;

	ADL_Display_Property_Get = (ADL_DISPLAY_PROPERTY_GET)GetProcAddress(hDLL,"ADL_Display_Property_Get");
	if (!ADL_Display_Property_Get)
		return false;

	// Init ADL
	if (ADL_Main_Control_Create(ADL_Alloc, 0) != ADL_OK)
		return false;

	return true;
}



GLWindow::DisplayData* GLWindow::getDisplayData(unsigned int uiDspId)
{
    vector<DisplayData*>::iterator itr;

    for (itr = m_DisplayInfo.begin(); itr != m_DisplayInfo.end(); ++itr)
    {
        if ((*itr)->uiDisplayId == uiDspId)
        {
            return (*itr);
        }
    }

    return NULL;
}

