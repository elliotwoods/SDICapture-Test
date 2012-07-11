#define _CRT_SECURE_NO_WARNINGS 1
#ifdef WIN32
#include <windows.h>
#include <GL/glew.h>
#include <GL/wglew.h>
#else
#include <stdint.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/glx.h>
#include <GL/glxext.h>
#endif
#include <GL/glu.h>
#include <math.h>
#include <assert.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>

#define STRETCH_TEST 0

extern "C" {
#include "../../../header/dvs_setup.h"
#include "../../../header/dvs_clib.h"
#include "../../../header/dvs_direct.h"
#include "../../../header/dvs_thread.h"
}

sv_handle * sv;
sv_direct_handle * dh_out, * dh_in;
sv_storageinfo storageinfo;

// GL_ARB_copy_buffer
#define GL_COPY_READ_BUFFER  0x8F36
#define GL_COPY_WRITE_BUFFER 0x8F37

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
int WIDTH      = 1920;
int HEIGHT     = 1080;

#ifdef WIN32
static HWND hWnd;
static HDC currentDC = 0;
HDC hDC;
HGLRC hGLRC;
#else
Display * dpy;
Window win;
GLXContext ctx;
#endif

GLuint frameBufferObject = 0;
GLuint glBuffers[numBuffers][2];
sv_direct_timecode timecodes[numBuffers][2];

void initGL(void);
void * recordThread(void * arg);
void * displayThread(void * arg);
static int verbose = 0;
static int running = TRUE;

typedef struct {
  sv_direct_handle * dh;
  dvs_cond finish;
} thread_param;

void signal_handler(int signal)
{
  (void)signal;
  running = FALSE;
}

#ifdef WIN32
void setupPixelformat(HDC hDC)
{
    PIXELFORMATDESCRIPTOR pfd = {
        sizeof(PIXELFORMATDESCRIPTOR),	/* size of this pfd */
        1,				/* version num */
        PFD_DRAW_TO_WINDOW |		/* support window */
        PFD_DOUBLEBUFFER |              /* double buffered */
        PFD_SUPPORT_OPENGL,		/* support OpenGL */
        PFD_TYPE_RGBA,			/* color type */
        8,				/* 8-bit color depth */
        0, 0, 0, 0, 0, 0,		/* color bits (ignored) */
        0,				/* no alpha buffer */
        0,				/* alpha bits (ignored) */
        0,				/* no accumulation buffer */
        0, 0, 0, 0,			/* accum bits (ignored) */
        0,				/* depth buffer (filled below)*/
        0,				/* no stencil buffer */
        0,				/* no auxiliary buffers */
        PFD_MAIN_PLANE,			/* main layer */
        0,				/* reserved */
        0, 0, 0,			/* no layer, visible, damage masks */
    };
    int SelectedPixelFormat;
    BOOL retVal;

    SelectedPixelFormat = ChoosePixelFormat(hDC, &pfd);
    if (SelectedPixelFormat == 0) {
        fprintf(stderr, "ChoosePixelFormat failed\n");
    }

    retVal = SetPixelFormat(hDC, SelectedPixelFormat, &pfd);
    if (retVal != TRUE) {
        fprintf(stderr, "SetPixelFormat failed\n");
    }
}

LRESULT APIENTRY WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    GLuint res = 0;
    switch (message) {
    case WM_CREATE:
        hDC = GetDC(hWnd);
        setupPixelformat(hDC);

        hGLRC = wglCreateContext(hDC);
        wglMakeCurrent(hDC, hGLRC);
        res = glGetError();
        initGL();
        assert (GL_NO_ERROR == glGetError());
        return 0;
    case WM_DESTROY:
        if (hGLRC) {
            wglMakeCurrent(hDC, hGLRC);

            glMatrixMode(GL_PROJECTION);
            glLoadIdentity();
            glMatrixMode(GL_MODELVIEW);

            wglMakeCurrent(NULL, NULL);
            wglDeleteContext(hGLRC);
        }
        ReleaseDC(hWnd, hDC);
        PostQuitMessage(0);
        return 0;
    case WM_SIZE:
        //resize(LOWORD(lParam), HIWORD(lParam));
        return 0;
    case WM_CHAR:
        switch ((int)wParam) {
        case VK_ESCAPE:
            DestroyWindow(hWnd);
            return 0;
        default:
            break;
        }
        break;
    default:
        break;
    }

    /* Deal with any unprocessed messages */
    return DefWindowProc(hWnd, message, wParam, lParam);
}
#endif

#ifndef WIN32
static Bool WaitForNotify(Display * d, XEvent * e, char *arg) {
  return (e->type == MapNotify) && (e->xmap.window == (Window) arg);
}
#endif

void initWindow(void)
{
#ifdef WIN32
    WNDCLASS wndClass;
    static char *className = "GLinterop";
    HINSTANCE hInstance = GetModuleHandle(NULL);
    RECT rect;
    DWORD dwStyle = WS_OVERLAPPED | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;

    /* Define and register the window class */
    wndClass.style = CS_OWNDC;
    wndClass.lpfnWndProc = WndProc;
    wndClass.cbClsExtra = 0;
    wndClass.cbWndExtra = 0;
    wndClass.hInstance = hInstance,
    wndClass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
    wndClass.hbrBackground = NULL;
    wndClass.lpszMenuName = NULL;
    wndClass.lpszClassName = className;
    RegisterClass(&wndClass);

    /* Figure out a default size for the window */
    SetRect(&rect, 0, 0, WIDTH, HEIGHT);
    AdjustWindowRect(&rect, dwStyle, FALSE);

    /* Create a window of the previously defined class */
    hWnd = CreateWindow(
        className, "dummy", dwStyle,
        rect.left, rect.top, rect.right, rect.bottom,
        NULL, NULL, hInstance, NULL);
#else
    int width = WIDTH;
    int height = HEIGHT;
    int screen;
    XVisualInfo *vi;
    XSetWindowAttributes swa;
    XEvent event;
    Colormap cmap;
    unsigned long mask;
    GLXFBConfig *configs, config;
    int numConfigs;
    int config_list[] = { GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT, 
			  GLX_DOUBLEBUFFER, GL_TRUE,
			  GLX_RENDER_TYPE, GLX_RGBA_BIT,
			  GLX_RED_SIZE, 8,
			  GLX_GREEN_SIZE, 8,
			  GLX_BLUE_SIZE, 8,
			  GLX_FLOAT_COMPONENTS_NV, GL_FALSE,
			  None };	

    // Notify Xlib that the app is multithreaded.
    XInitThreads();
    // Open X display
    dpy = XOpenDisplay(NULL);
    if(!dpy) {
        printf("Error: could not open display");
        return;
    }

    // Get screen.
    screen = XDefaultScreen(dpy);
    // Find required framebuffer configuration
    configs = glXChooseFBConfig(dpy, screen, config_list, &numConfigs);
    if(!configs) {
      printf("CreatePBuffer(): Unable to find a matching FBConfig.");
      return;
    }

    // Find a config with the right number of color bits.
    int i;
    for (i = 0; i < numConfigs; i++) {
      int attr;
      if (glXGetFBConfigAttrib(dpy, configs[i], GLX_RED_SIZE, &attr)) {
        printf("glXGetFBConfigAttrib(GLX_RED_SIZE) failed!");
        return;
      }
      if (attr != 8)
        continue;
      if (glXGetFBConfigAttrib(dpy, configs[i], GLX_GREEN_SIZE, &attr)) {
        printf("glXGetFBConfigAttrib(GLX_GREEN_SIZE) failed!");
        return;
      }
      if (attr != 8)
        continue;
      if (glXGetFBConfigAttrib(dpy, configs[i], GLX_BLUE_SIZE, &attr)) {
        printf("glXGetFBConfigAttrib(GLX_BLUE_SIZE) failed!");
        return;
      }
      if (attr != 8)
        continue;
      if (glXGetFBConfigAttrib(dpy, configs[i], GLX_ALPHA_SIZE, &attr)) {
        printf("glXGetFBConfigAttrib(GLX_ALPHA_SIZE) failed");
        return;
      }
      if (attr != 8)
        continue;
      break;
    }
  
    if(i == numConfigs) {
      printf("No 8-bit FBConfigs found.");
      return;
    }
  
    config = configs[i];
    
    // Don't need the config list anymore so free it.
    XFree(configs);
    configs = NULL;

    // Create a context for the onscreen window.
    ctx = glXCreateNewContext(dpy, config, GLX_RGBA_TYPE, 0, true);
    // Get visual from FB config.
    if((vi = glXGetVisualFromFBConfig(dpy, config)) == NULL) {
      printf("Couldn't find visual for onscreen window.");
      return;
    }

    // Create color map.
    if(!(cmap = XCreateColormap(dpy, RootWindow(dpy, vi->screen), vi->visual, AllocNone))) {
      printf("XCreateColormap failed!");
      return;
    }
  
    // Create window.
    swa.colormap = cmap;
    swa.border_pixel = 0;
    swa.background_pixel = 1;
    swa.event_mask = ExposureMask | StructureNotifyMask | KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask ;
    mask = CWBackPixel | CWBorderPixel | CWColormap | CWEventMask;
    win = XCreateWindow(dpy, RootWindow(dpy, vi->screen), 0, 0, width, height, 0, vi->depth, InputOutput, vi->visual, mask, &swa);
    
    // Map window.
    XMapWindow(dpy, win);
    XIfEvent(dpy, &event, WaitForNotify, (char *) win);
    
    // Set window colormap.
    XSetWMColormapWindows(dpy, win, &win, 1);
  
    // Connect the context to the window.
    if(!(glXMakeCurrent(dpy, win, ctx))) {
      printf("glXMakeCurrent failed!");
      return;
    }

    // Don't lock the capture/draw loop to the graphics vsync.  
    //glXSwapIntervalSGI(0);

    XFlush(dpy);
#endif
}

#ifndef WIN32
void renderToWindow(int target)
{
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  // Reset view parameters
  glViewport(0, 0, WIDTH, HEIGHT);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  gluOrtho2D(-1.0, 1.0, -1.0, 1.0);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  // Draw contents of each video texture
  glEnable(GL_TEXTURE_2D);
  	
  // Bind texture object 	
  glBindTexture(GL_TEXTURE_2D, target);
  
  // Draw textured quad in lower left quadrant of graphics window.
  glBegin(GL_QUADS);	
  glTexCoord2f(0.0, 0.0); glVertex2f(1, 1);
  glTexCoord2f(0.0, 1.0);  glVertex2f(1, -1); 
  glTexCoord2f(1.0, 1.0);  glVertex2f(-1, -1); 
  glTexCoord2f(1.0, 0.0); glVertex2f(-1, 1);
  glEnd();
	
  glXSwapBuffers(dpy, win);

  glBindTexture(GL_TEXTURE_2D, 0);  
  glDisable(GL_TEXTURE_2D);
}
#endif

void initGL(void)
{
#ifdef WIN32
    // Init glew
    glewInit();
    if (!glewIsSupported("GL_VERSION_2_0 "
                         "GL_ARB_pixel_buffer_object "
                         "GL_EXT_framebuffer_object "
            )) {
        fprintf(stderr, "Support for necessary OpenGL extensions missing.\n");
    }
    assert(GL_NO_ERROR == glGetError());
#endif

    // Hack to detect the presence of G7x. While GL_EXT_geometry_shader4 
    // is not required for this test, it's an easy way to check if the HW 
    // is GL 3.0 capable or not.
    if (!strstr((const char *)glGetString(GL_EXTENSIONS), "GL_EXT_geometry_shader4")) {
        printf("Found unsupported config for interop.");
    }
    assert(GL_NO_ERROR == glGetError());
}

#define GL_RGBA8UI                                          0x8D7C
void allocGLTextures(GLuint *bufs, unsigned int numBuffers, GLenum type)
{
    for (unsigned int i = 0; i < numBuffers; i++)
    {
        glBindTexture(GL_TEXTURE_2D, bufs[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, type, WIDTH, HEIGHT, 0, type, GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST );
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST );
        assert(GL_NO_ERROR == glGetError());
    }
}

bool initBuffers()
{
    for (int buffer = 0; buffer < numBuffers; buffer++) {
        for (int i = DVPINPUT; i <= DVPOUTPUT; i++) {
            glGenTextures(1, &glBuffers[buffer][i]);
            // Allocate storage for the textures
            allocGLTextures(&glBuffers[buffer][i], 1, GL_RGBA);

            // Initialize timecode buffers.
            timecodes[buffer][i].size = sizeof(sv_direct_timecode);
        }
    }

    return true;
}

void cleanupBuffers() {
    for (int buffer = 0; buffer < numBuffers; buffer++) {
        for (int i = DVPINPUT; i <= DVPOUTPUT; i++) {
            glDeleteTextures(1, &glBuffers[buffer][i]);
        }
    }
}

void renderSomethingGL(int target, int source)
{
#if STRETCH_TEST
    static int counter = 0;
    static int inc     = 1;
#endif
    int width  = WIDTH;
    int height = HEIGHT;
    if(!frameBufferObject) {
        glGenFramebuffersEXT(1, &frameBufferObject);
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, frameBufferObject);
    } else {
        glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, frameBufferObject);
    }

    //Attach 2D texture to this FBO
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, target, 0);
    assert(GL_NO_ERROR == glGetError());

    glPushMatrix();
    glClearColor(0.0f,0.0f,0.0f,1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glBindTexture(GL_TEXTURE_2D, source);
    glEnable(GL_TEXTURE_2D);

    glViewport(0, 0, width, height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, width, 0, height, -1, 1);
    glMatrixMode(GL_MODELVIEW);

    glBegin(GL_QUADS);
    glTexCoord2f(0, 0);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f); 
    glVertex2i(0, 0);

#if STRETCH_TEST
    glTexCoord2f(0, (50 + counter) / 100.0f);
#else
    glTexCoord2f(0, 1.0f);
#endif
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f); 
    glVertex2i(0, height);

    glTexCoord2f(1.0f, 1.0f);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f); 
    glVertex2i(width, height);

#if STRETCH_TEST
    glTexCoord2f((50 + counter) / 100.0f, 0.0f);
#else
    glTexCoord2f(1.0f, 0);
#endif
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f); 
    glVertex2i(width, 0);
    glEnd();

#if STRETCH_TEST
    counter += inc; 
    if(counter > 100) {
      inc = -1;
    } else if(counter <= 0) {
      inc =  1;
    }
#endif

    glPopMatrix();
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, 0, 0);
    glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
}

void initApp() 
{
#ifdef WIN32
    static SIZE_T  dwMin = 0, dwMax = 0;
    HANDLE hProcess = OpenProcess( PROCESS_QUERY_INFORMATION | PROCESS_SET_QUOTA, FALSE, GetCurrentProcessId());
    if (!hProcess) {
        printf("OpenProcess failed (%d)\n", GetLastError() );
    }
    // Retrieve the working set size of the process.
    if (!dwMin && !GetProcessWorkingSetSize(hProcess, &dwMin, &dwMax)) {
        printf("GetProcessWorkingSetSize failed (%d)\n", GetLastError());
    }
    // 128MB extra
    if (!SetProcessWorkingSetSize(hProcess, (128 * 0x100000) + dwMin, (128 * 0x100000) + (dwMax - dwMin))) {
         printf("SetProcessWorkingSetSize failed (%d)\n", GetLastError());
    }
    CloseHandle(hProcess);
#endif

    initBuffers();
}

void closeApp() {
    cleanupBuffers();
}

int main(int argc, char *argv[])
{
    printf("OGL fifo example v0.1\n");
    signal(SIGTERM, signal_handler);
    signal(SIGINT,  signal_handler);

    int numFrames = -1;
    int flags = 0;
    int res = SV_OK;
    
    dvs_thread recordThreadHandle;
    dvs_thread displayThreadHandle;
    thread_param tparam[2];

    for(int i = 1; i < argc; i++) {
        if(!strcmp(argv[i], "field")) {
            flags = SV_DIRECT_FLAG_FIELD;
            HEIGHT /= 2;
        } else if(!strcmp(argv[i], "verbose")) {
            // increase verbose level
            verbose++;
        }
    }

    initWindow();
    initApp(); 

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
    res = sv_direct_init(sv, &dh_out, "NVIDIA/OPENGL", 0, numBuffers, flags);
    if(res != SV_OK) {
        printf("sv_direct_init(out):%d (%s)\n", res, sv_geterrortext(res));
        return 1;
    }

    // input
    res = sv_direct_init(sv, &dh_in, "NVIDIA/OPENGL", 1, numBuffers, flags);
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
                res = sv_direct_bind_opengl(sv, dh, buffer, glBuffers[buffer][i]);
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
    tparam[0].dh = dh_in; tparam[1].dh = dh_out;
    dvs_thread_create(&recordThreadHandle, recordThread, &tparam[0], &tparam[0].finish);
    dvs_thread_create(&displayThreadHandle, displayThread, &tparam[1], &tparam[1].finish);

    int recordCurrent = 0;
    int displayCurrent = 0;
    
    // first record go
    dvs_cond_broadcast(&record_go[recordCurrent % numBuffers], &record_go_mutex[recordCurrent % numBuffers], FALSE);
    for(int frame = 0; running && ((numFrames == -1) || (frame < numFrames)); frame++)
    {
        int res = SV_OK;
        dvs_cond_wait(&record_ready[recordCurrent % numBuffers], &record_ready_mutex[recordCurrent % numBuffers], FALSE);
        if(frame > 0) { 
            dvs_cond_wait(&display_ready[displayCurrent % numBuffers], &display_ready_mutex[displayCurrent % numBuffers], FALSE);
        }

        res = sv_direct_sync(sv, dh_in, recordCurrent % numBuffers, 0);
        if(res != SV_OK) {
            printf("sv_direct_sync(in):%d (%s)\n", res, sv_geterrortext(res));
        } else {
            renderSomethingGL(glBuffers[displayCurrent % numBuffers][DVPOUTPUT], glBuffers[recordCurrent % numBuffers][DVPINPUT]);
#ifndef WIN32
            renderToWindow(glBuffers[displayCurrent % numBuffers][DVPOUTPUT]);
#endif

            if(verbose > 1) {
                printf("%d> timecodes: dvitc[0]:%08x/%08x dvitc[1]:%08x/%08x ltc:%08x/%08x vtr:%08x/%08x\n", recordCurrent % numBuffers,
                  timecodes[recordCurrent % numBuffers][DVPINPUT].dvitc_tc[0],
                  timecodes[recordCurrent % numBuffers][DVPINPUT].dvitc_ub[0],
                  timecodes[recordCurrent % numBuffers][DVPINPUT].dvitc_tc[1],
                  timecodes[recordCurrent % numBuffers][DVPINPUT].dvitc_ub[1],
                  timecodes[recordCurrent % numBuffers][DVPINPUT].ltc_tc,
                  timecodes[recordCurrent % numBuffers][DVPINPUT].ltc_ub,
                  timecodes[recordCurrent % numBuffers][DVPINPUT].vtr_tc,
                  timecodes[recordCurrent % numBuffers][DVPINPUT].vtr_ub
                );
            }

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

#ifndef WIN32
        {
          XEvent event;          
          if(XCheckWindowEvent(dpy, win, ~0L, &event)) {
            // quit on Esc
            if(event.type == KeyPress && event.xkey.keycode == 9) {
               running = FALSE;
               break;
            }
          }
        }
#endif
    }

    printf("stopping threads\n");
    dvs_thread_exitcode(&recordThreadHandle, &tparam[0].finish);
    dvs_thread_exitcode(&displayThreadHandle, &tparam[1].finish);

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
#ifndef WIN32
    XCloseDisplay(dpy);
#endif
    printf("done.\n");
    return 0;
}

void * recordThread(void * arg) 
{
  thread_param * param = (thread_param *) arg;
  sv_direct_handle * dh = param->dh;
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
        uint64 record_go = ((uint64)bufferinfo.clock_high << 32) | bufferinfo.clock_low;
        uint64 dma_ready = ((uint64)bufferinfo.dma.clock_ready_high << 32) | bufferinfo.dma.clock_ready_low;
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

  dvs_thread_exit(&res, &param->finish);
  return 0;
}

void * displayThread(void * arg) 
{
  thread_param * param = (thread_param *) arg;
  sv_direct_handle * dh = param->dh;
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

  dvs_thread_exit(&res, &param->finish);
  return 0;
}
