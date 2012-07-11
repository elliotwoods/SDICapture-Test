#pragma once

#include <string>
#include <vector>


class GLWindow
{
public:
    GLWindow(const char* strWinName, const char* strClsaaName);
    ~GLWindow();

    // setups ADL and enumerates Dispülays
    bool            init();

    void            setFullScreen(bool bFullScreen);
    void            setInitialPosition(unsigned int uiPosX, unsigned int uiPosY);

    // create a window but does not open it
    bool            create(unsigned int uiWidth, unsigned int uiHeight, unsigned int uiDspIndex = 1);
    // creates an OpenGL context for this window
    bool            createContext();
    void            deleteContext();
    // Openes window
    void            open();

    void            makeCurrent();
    void            swapBuffers();

    HDC             getDC()             { return m_hDC; };
    // returns the number of GPUs in the system
    unsigned int    getNumGPUs()        { return m_uiNumGPU; };
    // returns the number of displays mapped on GPU uiGPU
    unsigned int    getNumDisplaysOnGPU(unsigned int uiGPU);
    // returns the DisplayID of Display n on GPU uiGPU
    // n=0 will return the first display on GPU uiGPU if available
    // n=1 will return the second display on GPU uiGPU if available ...
    unsigned int    getGetDisplayOnGPU(unsigned int uiGPU, unsigned int n=0);

private:

    typedef struct
    {
        unsigned int    uiGPUId;
        unsigned int    uiDisplayId;
        unsigned int    uiDisplayLogicalId;
        unsigned int    uiOriginX;
        unsigned int    uiOriginY;
        unsigned int    uiWidth;
        unsigned int    uiHeight;
        std::string     strDisplayname;
    } DisplayData;


    bool            setupADL();
    DisplayData*    getDisplayData(unsigned int uiDspId);


    HDC                         m_hDC;
    HGLRC                       m_hGLRC;
    HWND                        m_hWnd;

    unsigned int                m_uiWidth;
    unsigned int                m_uiHeight;
    unsigned int                m_uiPosX;
    unsigned int                m_uiPosY;

    bool                        m_bADLReady;
    bool                        m_bFullScreen;

    unsigned int                m_uiNumGPU;
    std::vector<DisplayData*>   m_DisplayInfo;

    std::string                 m_strWinName;
    std::string                 m_strClassName; 
};
