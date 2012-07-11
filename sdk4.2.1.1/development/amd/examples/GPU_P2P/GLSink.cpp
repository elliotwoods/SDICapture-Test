

#include <windows.h>
#include <assert.h>
#include <GL/glew.h>
#include <GL/wglew.h>

#include "defines.h"
#include "FormatInfo.h"
#include "SyncedBuffer.h"
#include "GLSink.h"



GLSink::GLSink()
{
    m_uiWindowWidth  = 0;
    m_uiWindowHeight = 0;

    m_uiTextureWidth  = 0;
    m_uiTextureHeight = 0;
    m_uiTexture       = 0;
    m_uiTextureSize   = 0;

    m_nIntFormat = GL_RGB8;
    m_nExtFormat = GL_RGB;
    m_nType      = GL_UNSIGNED_BYTE;

    m_uiBufferSize  = 0;
    m_uiBufferIdx   = 0;
    m_pUnPackBuffer = NULL;

    m_uiQuad = 0;
 
    m_fAspectRatio = 1.0f;

    m_pUnPackBuffer     = NULL;
    m_pBufferBusAddress = NULL;
    m_pMarkerBusAddress = NULL;
    m_pInputBuffer      = NULL;
}


GLSink::~GLSink()
{
    // make sure now other thread is blocked
    release();

    if (m_pUnPackBuffer)
    {
        delete [] m_pUnPackBuffer;
    }

    if (m_pBufferBusAddress)
        delete [] m_pBufferBusAddress;

    if (m_pMarkerBusAddress)
        delete [] m_pMarkerBusAddress;

    if (m_pInputBuffer)
        delete m_pInputBuffer;

}


void GLSink::initGL()
{
    const float pArray [] = { -0.5f,  0.5f, 0.0f,       0.0f, 0.0f,
                               0.5f,  0.5f, 0.0f,       1.0f, 0.0f,
                               0.5f, -0.5f, 0.0f,       1.0f, 1.0f,
                              -0.5f, -0.5f, 0.0f,       0.0f, 1.0f 
                            };
    
    glClearColor(0.0f, 0.2f, 0.8f, 1.0f);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);

    glShadeModel(GL_SMOOTH);

    glPolygonMode(GL_FRONT, GL_FILL);
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

    // create quad that is used to map SDi texture to
    glGenBuffers(1, &m_uiQuad);
    glBindBuffer(GL_ARRAY_BUFFER, m_uiQuad);

    glBufferData(GL_ARRAY_BUFFER, 20*sizeof(float), pArray, GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, m_uiQuad);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);

    glVertexPointer(3, GL_FLOAT,   5*sizeof(float), 0);
    glTexCoordPointer(2, GL_FLOAT, 5*sizeof(float), (char*)NULL + 3*sizeof(float));

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // Activate Sync to VBlank to avoid tearing
    wglSwapIntervalEXT(1);
}


void GLSink::resize(unsigned int w, unsigned int h)
{
    m_uiWindowWidth  = w;
    m_uiWindowHeight = h;

    glViewport(0, 0, w, h);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    glOrtho(-(double)w/(double)h*0.5, (double)w/(double)h*0.5, -0.5, 0.5, -1.0, 1.0);
    
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}


bool GLSink::createDownStream(unsigned int w, unsigned int h, int nIntFormat, int nExtFormat, int nType)
{
    m_uiTextureWidth  = w;
    m_uiTextureHeight = h;

    m_nIntFormat = nIntFormat;
    m_nExtFormat = nExtFormat;
    m_nType      = nType;

    m_fAspectRatio = (float)w/(float)h;

    m_uiBufferSize = FormatInfo::getInternalFormatSize(m_nIntFormat) * m_uiTextureWidth * m_uiTextureHeight;

    if (m_uiBufferSize == 0)
        return false;

    // check if format is supported
    if (FormatInfo::getExternalFormatSize(m_nExtFormat, m_nType) == 0)
        return false;

    glBindBuffer(GL_BUS_ADDRESSABLE_MEMORY_AMD, 0);

    // Create texture that will be used to store frames from remote device
    glGenTextures(1, &m_uiTexture);

    glBindTexture(GL_TEXTURE_2D, m_uiTexture);

    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

    glTexImage2D(GL_TEXTURE_2D, 0, m_nIntFormat, m_uiTextureWidth, m_uiTextureHeight, 0, m_nExtFormat, m_nType, NULL);

    glBindTexture(GL_TEXTURE_2D, 0);

    m_uiTextureSize = m_uiTextureWidth*m_uiTextureHeight*FormatInfo::getExternalFormatSize(m_nExtFormat, m_nType);


    m_pUnPackBuffer     = new unsigned int[NUM_BUFFERS];
    m_pBufferBusAddress = new unsigned long long[NUM_BUFFERS];
    m_pMarkerBusAddress = new unsigned long long[NUM_BUFFERS];

    glGenBuffers(NUM_BUFFERS, m_pUnPackBuffer);

    // Create buffers
    for (unsigned int i = 0; i < NUM_BUFFERS; i++)
    {
        glBindBuffer(GL_BUS_ADDRESSABLE_MEMORY_AMD, m_pUnPackBuffer[i]);
        glBufferData(GL_BUS_ADDRESSABLE_MEMORY_AMD, m_uiBufferSize, 0, GL_DYNAMIC_DRAW);
    }

    // Call makeResident when all BufferData calls were submitted. This call will make the
    // buffers resident in local visible memory.
    glMakeBuffersResidentAMD(NUM_BUFFERS, m_pUnPackBuffer, m_pBufferBusAddress, m_pMarkerBusAddress);
    
    // Create synchronization buffers
    m_pInputBuffer = new SyncedBuffer;

    m_pInputBuffer->createSyncedBuffer(NUM_BUFFERS);

    for (unsigned int i = 0; i < NUM_BUFFERS; i++)
    {
        FrameData* pFrameData = new FrameData;

        pFrameData->uiTransferId        = 0;
        pFrameData->ullBufferBusAddress = m_pBufferBusAddress[i];
        pFrameData->ullMarkerBusAddress = m_pMarkerBusAddress[i];

        m_pInputBuffer->setBufferMemory(i, (void*)pFrameData);
    }

    return true;
}


void GLSink::draw()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Update stream 
    glBindTexture(GL_TEXTURE_2D, m_uiTexture);

    FrameData* pFrame = NULL;

    // block until new frame is available
    m_uiBufferIdx = m_pInputBuffer->getBufferForReading((void*&)pFrame);

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, m_pUnPackBuffer[m_uiBufferIdx]);

    glWaitMarkerAMD(m_pUnPackBuffer[m_uiBufferIdx], pFrame->uiTransferId);

    // Copy pinned mem to texture
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_uiTextureWidth, m_uiTextureHeight, m_nExtFormat, m_nType, NULL);

    // Insert fence to determine when we can release the buffer
    GLsync Fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

    glPushMatrix();
   
    // Scale quad to the AR of the incoming texture
    glScalef(m_fAspectRatio, 1.0f, 1.0f);
   
    // Draw quad with mapped texture
    glBindBuffer(GL_ARRAY_BUFFER, m_uiQuad);
    glDrawArrays(GL_QUADS, 0, 4);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    
    glPopMatrix();

    glBindTexture(GL_TEXTURE_2D, 0);

    if (glIsSync(Fence))
    {
        glClientWaitSync(Fence, GL_SYNC_FLUSH_COMMANDS_BIT, OneSecond);
        glDeleteSync(Fence);
    }

    m_pInputBuffer->releaseReadBuffer();
}


void GLSink::release()
{
    if (m_pInputBuffer)
        m_pInputBuffer->releaseReadBuffer();

}
