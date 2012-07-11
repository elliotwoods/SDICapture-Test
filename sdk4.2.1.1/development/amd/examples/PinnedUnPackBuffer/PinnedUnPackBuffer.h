
#pragma once
#include "../../../header/dvs_clib.h"
#include "../../../header/dvs_direct.h"
#include <stdio.h>

class RenderTarget;

class PinnedUnPackBuffer {

public:

  PinnedUnPackBuffer();
  ~PinnedUnPackBuffer();

  bool init(sv_handle * sv, sv_direct_handle * dh_in, sv_direct_handle * dh_out, int buffercount);
  void resize(unsigned int w, unsigned int h);
  bool draw(int bufferindex);
  bool output(int bufferindex);

  void changeRotation(float x, float y, float z);

private:
  unsigned int mWidth;
  unsigned int mHeight;
  float mRotX, mRotY, mRotZ;

  GLuint mUIQuad;
  GLuint mUIRQuad;
  GLuint mUITexture;

  GLuint mTextureInput[2];
  GLuint mTextureOutput[2];
  RenderTarget * mRenderTarget;
};