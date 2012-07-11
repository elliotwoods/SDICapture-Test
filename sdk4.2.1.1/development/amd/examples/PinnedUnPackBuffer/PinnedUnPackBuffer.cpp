#include <windows.h>
#include "GL/glew.h"

#include "PinnedUnPackBuffer.h"
#include "RenderTarget.h"

#define GL_EXTERNAL_VIRTUAL_MEMORY_AMD 0x9160
#define OneSecond 1000*1000*1000L

PinnedUnPackBuffer::PinnedUnPackBuffer()
{
  mUIQuad = 0;

  mWidth  = 0;
  mHeight = 0;

  mTextureInput[0] = 0;
  mTextureInput[1] = 0;

  mTextureOutput[0] = 0;
  mTextureOutput[1] = 0;

  mRenderTarget = 0;

  mRotX = 0.0f;
  mRotY = 0.0f;
  mRotZ = 0.0f;
}


PinnedUnPackBuffer::~PinnedUnPackBuffer()
{
  if(mRenderTarget) {
    delete mRenderTarget;
  }
}


bool PinnedUnPackBuffer::init(sv_handle * sv, sv_direct_handle * dh_in, sv_direct_handle * dh_out, int buffercount)
{
  float quad[] = { -1.0f, -1.0f, 0.0f,   0.0f, 0.0f,
                    1.0f, -1.0f, 0.0f,   1.0f, 0.0f,
                    1.0f,  1.0f, 0.0f,   1.0f, 1.0f,
                   -1.0f,  1.0f, 0.0f,   0.0f, 1.0f };

  float rquad[]= { -1.0f, -1.0f, 0.0f,   0.0f, 1.0f,
                    1.0f, -1.0f, 0.0f,   1.0f, 1.0f,
                    1.0f,  1.0f, 0.0f,   1.0f, 0.0f,
                   -1.0f,  1.0f, 0.0f,   0.0f, 0.0f };

  int res = SV_OK;

  if((buffercount > 2) || (buffercount < 1)) {
    return false;
  }

  glClearColor(0.3f, 0.3f, 0.3f, 1.0f);

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_TEXTURE_2D);
  glShadeModel(GL_SMOOTH);
  glPolygonMode(GL_FRONT, GL_FILL);
  glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
  glEnableClientState(GL_VERTEX_ARRAY);
  glEnableClientState(GL_TEXTURE_COORD_ARRAY);

  glGenBuffers(1, &mUIQuad);
  glBindBuffer(GL_ARRAY_BUFFER, mUIQuad);
  glBufferData(GL_ARRAY_BUFFER, 20 * sizeof(float), quad, GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  glGenBuffers(1, &mUIRQuad);
  glBindBuffer(GL_ARRAY_BUFFER, mUIRQuad);
  glBufferData(GL_ARRAY_BUFFER, 20 * sizeof(float), rquad, GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glViewport(0, 0, mWidth, mHeight);
  glOrtho(-1.0f, 1.0f, 1.0f, -1.0f, 0, 128);
  
  glGenBuffers(buffercount, mTextureInput);
  glGenBuffers(buffercount, mTextureOutput);

  for(int i = 0; i < buffercount; i++) {
    res = sv_direct_bind_opengl(sv, dh_in, i, mTextureInput[i]);
    if(res != SV_OK) {
      return false;
    }

    res = sv_direct_bind_opengl(sv, dh_out, i, mTextureOutput[i]);
    if(res != SV_OK) {
      return false;
    }
  }

  // Generate a texture into which tha data from teh buffer is dpwnloaded
  glGenTextures(1, &mUITexture);
  glBindTexture(GL_TEXTURE_2D, mUITexture);
  glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, mWidth, mHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glBindTexture(GL_TEXTURE_2D, 0);

  mRenderTarget = new RenderTarget;
  mRenderTarget->createBuffer(mWidth, mHeight, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);

  return true;
}


void PinnedUnPackBuffer::resize(unsigned int w, unsigned int h)
{
  mWidth  = w;
  mHeight = h;

  glViewport(0, 0, w, h);
  glOrtho(-1.0f, 1.0f, -1.0f, 1.0f, 0, 128);
}


bool PinnedUnPackBuffer::draw(int bufferindex)
{
  bool result = true;

  glViewport(0, 0, mWidth, mHeight);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, mTextureInput[bufferindex]);

  // Transfer data from buffer to texture
  glBindTexture(GL_TEXTURE_2D, mUITexture);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, mWidth, mHeight, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

  // draw quad with updated texture
  glBindBuffer(GL_ARRAY_BUFFER, mUIQuad);
  glVertexPointer(3, GL_FLOAT, 5*sizeof(float), 0);
  glTexCoordPointer(2, GL_FLOAT, 5*sizeof(float), ((char*)NULL +3*sizeof(float)));
  glDrawArrays(GL_QUADS, 0, 4);

  glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindTexture(GL_TEXTURE_2D, 0);

  return result;
}

bool PinnedUnPackBuffer::output(int bufferindex)
{
  bool result = true;

  mRenderTarget->bind();
  glViewport(0, 0, mWidth, mHeight);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  glBindTexture(GL_TEXTURE_2D, mUITexture);  
  glBindBuffer(GL_ARRAY_BUFFER, mUIRQuad);
  glVertexPointer(3, GL_FLOAT, 5*sizeof(float), 0);
  glTexCoordPointer(2, GL_FLOAT, 5*sizeof(float), ((char*)NULL +3*sizeof(float)));

  glDrawArrays(GL_QUADS, 0, 4);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindTexture(GL_TEXTURE_2D, 0);

  glBindBuffer(GL_PIXEL_PACK_BUFFER, mTextureOutput[bufferindex]);
  glReadPixels(0, 0, mWidth, mHeight, GL_RGBA, GL_UNSIGNED_BYTE, 0);
  glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

  mRenderTarget->unbind();

  return result;
}

void PinnedUnPackBuffer::changeRotation(float x, float y, float z)
{
  mRotX += x;
  mRotY += y;
  mRotZ += z;
}
