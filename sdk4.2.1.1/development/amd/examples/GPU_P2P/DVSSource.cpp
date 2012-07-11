
#include <windows.h>
#include <GL/glew.h>
#include <GL/wglew.h>

#include "SyncedBuffer.h"
#include "FormatInfo.h"
#include "RenderTarget.h"
#include "defines.h"

#include "DVSSource.h"

DVSSource::DVSSource()
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


DVSSource::~DVSSource()
{
    release();

    if (m_pPackBuffer)
    {
        delete [] m_pPackBuffer;
    }

    if (m_pRenderTarget)
        delete m_pRenderTarget;
}



void DVSSource::init()
{
  int res = SV_OK;

  res = sv_openex(&sv, "", SV_OPENPROGRAM_DEMOPROGRAM, SV_OPENTYPE_VINPUT, 0, 0);
  if(res != SV_OK) {
    MessageBox(NULL, sv_geterrortext(res), "sv_openex()", 0);
    sv = NULL;
    dh = NULL;
  }

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

  if(res == SV_OK) {
    res = sv_direct_init(sv, &dh, "AMD", 1, NUM_BUFFERS, 0);
    if(res != SV_OK) {
      MessageBox(NULL, sv_geterrortext(res), "sv_direct_init()", 0);
      dh = NULL;
    }
  }

  if(res != SV_OK) {
    if(dh) {
      sv_direct_free(sv, dh);
    }
    if(sv) {
      sv_close(sv);
    }
  }
}


// Resize only the window. Since we are rendering into a FBO the
// projection matrix does not change on a window resize
void DVSSource::resize(unsigned int w, unsigned int h)
{
    m_uiWindowWidth  = w;
    m_uiWindowHeight = h;
}

bool DVSSource::createUpStream(unsigned int w, unsigned int h, int nIntFormat, int nExtFormat, int nType)
{
  // FIXME check size and format

  if(!sv || !dh) {
    return false;
  }

  return true;
}


bool DVSSource::setRemoteMemory(unsigned long long* pBufferBusAddress, unsigned long long* pMarkerBusAddress)
{
  int res = SV_OK;

  // check if we received enough addresses
  if (!pBufferBusAddress || !pMarkerBusAddress)
      return false;

  // assign remote buffer to local buffer objects
  for (int i = 0; i < NUM_BUFFERS; i++)
  {
      int buffersize = m_uiWindowWidth * m_uiWindowHeight * 4; // RGBA 8 Bit
      int markersize = 4;

      res = sv_direct_bind_physical(sv, dh, i, &pBufferBusAddress[i], &buffersize, &pMarkerBusAddress[i], &markersize);
      if(res != SV_OK) {
        MessageBox(NULL, sv_geterrortext(res), "sv_direct_bind_physical()", 0);
        return false;
      }
  }

  return true;    
}



void DVSSource::draw()
{
  int res = SV_OK;
  FrameData* pFrame = NULL;
        
  m_uiBufferIdx = m_pOutputBuffer->getBufferForWriting((void*&)pFrame);

  // update frame data
  ++pFrame->uiTransferId;

  res = sv_direct_record(sv, dh, m_uiBufferIdx, 0, NULL);
  if(res != SV_OK) {
    //MessageBox(NULL, sv_geterrortext(res), "sv_direct_record()", 0);
  }

  res = sv_direct_write_marker(sv, dh, m_uiBufferIdx, pFrame->uiTransferId);
  if(res != SV_OK) {
    //MessageBox(NULL, sv_geterrortext(res), "sv_direct_write_marker()", 0);
  }

  // Mark the buffer as ready to be consumed
  m_pOutputBuffer->releaseWriteBuffer();
}
    

void DVSSource::setOutputBuffer(SyncedBuffer* pOutputBuffer)
{
    if (pOutputBuffer)
    {
        m_pOutputBuffer = pOutputBuffer;
    }
}


void DVSSource::release()
{
    if (m_pOutputBuffer)
        m_pOutputBuffer->releaseWriteBuffer();
}


