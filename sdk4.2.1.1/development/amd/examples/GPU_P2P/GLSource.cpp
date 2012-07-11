
#include <windows.h>
#include <GL/glew.h>
#include <GL/wglew.h>

#include "SyncedBuffer.h"
#include "FormatInfo.h"
#include "RenderTarget.h"
#include "defines.h"

#include "GLSource.h"


GLSource::GLSource()
{
    m_uiWindowWidth  = 0;
    m_uiWindowHeight = 0;

    m_uiBufferWidth  = 0;
    m_uiBufferHeight = 0;

    m_fLinePosition[0] = 0.5f;
    m_fLinePosition[1] = 0.5f;

    m_nIntFormat = GL_RGB8;
    m_nExtFormat = GL_RGB;
    m_nType      = GL_UNSIGNED_BYTE;

    m_uiBufferSize = 0;
    m_uiBufferIdx  = 0;
    m_pPackBuffer  = NULL;

    m_pRenderTarget = NULL;
    m_pOutputBuffer = NULL;


    m_uiFontBase = 0;
}


GLSource::~GLSource()
{
    release();

    if (m_pPackBuffer)
    {
        delete [] m_pPackBuffer;
    }

    if (m_pRenderTarget)
        delete m_pRenderTarget;
}



void GLSource::initGL()
{
    glClearColor(0.0f, 0.2f, 0.8f, 1.0f);

    glEnable(GL_DEPTH_TEST);

    glShadeModel(GL_SMOOTH);

    glPolygonMode(GL_FRONT, GL_FILL);

    wglSwapIntervalEXT(1);
}



// Resize only the window. Since we are rendering into a FBO the
// projection matrix does not change on a window resize
void GLSource::resize(unsigned int w, unsigned int h)
{
    m_uiWindowWidth  = w;
    m_uiWindowHeight = h;
}


// Create a FBO that will be used as render target and a synchronized PACK buffer to transfer
// FBO content to the other GPU
bool GLSource::createUpStream(unsigned int w, unsigned int h, int nIntFormat, int nExtFormat, int nType)
{
    m_uiBufferWidth  = w;
    m_uiBufferHeight = h;

    m_nIntFormat = nIntFormat;
    m_nExtFormat = nExtFormat;
    m_nType      = nType;

    m_uiBufferSize = FormatInfo::getInternalFormatSize(m_nIntFormat) * m_uiBufferWidth * m_uiBufferHeight;

    if (m_uiBufferSize == 0)
        return false;

    // check if external format is supported
    if (FormatInfo::getExternalFormatSize(m_nExtFormat, m_nType) == 0)
        return false;

    // Create FBO that is used as render target
    m_pRenderTarget = new RenderTarget;
    m_pRenderTarget->createBuffer(m_uiBufferWidth, m_uiBufferHeight, m_nIntFormat, m_nExtFormat, m_nType);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    glOrtho(0, m_uiBufferWidth, 0, m_uiBufferHeight, -1.0, 1.0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // Create P2P PACK Buffer
    m_pPackBuffer = new unsigned int[NUM_BUFFERS];

    // Just generate the buffers the data store will be assigned later through setRemoteMemory.
    glGenBuffers(NUM_BUFFERS, m_pPackBuffer);

    glPixelStorei(GL_PACK_ALIGNMENT, FormatInfo::getAlignment(m_uiBufferWidth, m_nExtFormat, m_nType));

    return true;
}


bool GLSource::setRemoteMemory(unsigned long long* pBufferBusAddress, unsigned long long* pMarkerBusAddress)
{
    // check if we received enough addresses
    if (!pBufferBusAddress || !pMarkerBusAddress)
        return false;

    // assign remote buffer to local buffer objects
    for (int i = 0; i < NUM_BUFFERS; i++)
    {
        glBindBuffer(GL_EXTERNAL_PHYSICAL_MEMORY_AMD, m_pPackBuffer[i]);

        // Assign memory on remote GPU to local buffer object. Addresses for buffer and marker need
        // to be page aligned.
        glBufferBusAddressAMD(GL_EXTERNAL_PHYSICAL_MEMORY_AMD, m_uiBufferSize, pBufferBusAddress[i], pMarkerBusAddress[i]);
    }

    glBindBuffer(GL_EXTERNAL_PHYSICAL_MEMORY_AMD, 0);

    return true;    
}



void GLSource::draw()
{
    // Draw a rotating cube to FBO
    m_pRenderTarget->bind();
    glViewport(0, 0, m_uiBufferWidth, m_uiBufferHeight);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glBegin(GL_LINES);

    glVertex2f(m_fLinePosition[0], 0.0f);
    glVertex2f(m_fLinePosition[0], (float)m_uiBufferHeight);

    glVertex2f(0.0f, m_fLinePosition[1]);
    glVertex2f((float)m_uiBufferWidth, m_fLinePosition[1]);

    glEnd();

    m_fLinePosition[0] += 2.0f;
    m_fLinePosition[1] += 2.0f;

    if (m_fLinePosition[0] > (float)m_uiBufferWidth)
        m_fLinePosition[0] = 0.5f;

    if (m_fLinePosition[1] > (float)m_uiBufferHeight)
        m_fLinePosition[1] = 0.5f;

    FrameData* pFrame = NULL;
        
    m_uiBufferIdx = m_pOutputBuffer->getBufferForWriting((void*&)pFrame);

    // update frame data
    ++pFrame->uiTransferId;

    glBindBuffer(GL_PIXEL_PACK_BUFFER, m_pPackBuffer[m_uiBufferIdx]);

    // Copy local buffer into remote buffer
    glReadPixels(0, 0, m_uiBufferWidth, m_uiBufferHeight, m_nExtFormat, m_nType, NULL);
    //glFinish();

    glWriteMarkerAMD(m_pPackBuffer[m_uiBufferIdx], pFrame->uiTransferId, 0);

    GLsync PackFence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

    m_pRenderTarget->unbind();

    // copy FBO into window
    // comment this out to output drawing only on SDI
    glViewport(0, 0, m_uiWindowWidth, m_uiWindowHeight);
    m_pRenderTarget->draw();

    // Wait until ogl operations finnished before releasing the buffers
    if (glIsSync(PackFence))
    {
        // if Pack buffer is no longer accessed, release buffer and allow other threads
        // to consume it 
        glClientWaitSync(PackFence, GL_SYNC_FLUSH_COMMANDS_BIT, OneSecond);
        glDeleteSync(PackFence);
    }

    // Mark the buffer as ready to be consumed
    m_pOutputBuffer->releaseWriteBuffer();
}
    

void GLSource::setOutputBuffer(SyncedBuffer* pOutputBuffer)
{
    if (pOutputBuffer)
    {
        m_pOutputBuffer = pOutputBuffer;
    }
}


void GLSource::release()
{
    if (m_pOutputBuffer)
        m_pOutputBuffer->releaseWriteBuffer();
}


