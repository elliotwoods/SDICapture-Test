#include <GL/glew.h>

#include "RenderTarget.h"

RenderTarget::RenderTarget(void)
{
    m_uiBufferId        = 0;
    m_uiBufferWidth     = 0;
    m_uiBufferHeight    = 0;
    m_nBufferFormat     = 0;
}


RenderTarget::~RenderTarget(void)
{
    deleteBuffer();
}


bool RenderTarget::createBuffer(unsigned int uiWidth, unsigned int uiHeight, int nBufferFormat, int nExtFormat, int nType)
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    if (m_uiBufferId == 0)
    {
        m_uiBufferWidth  = uiWidth;
        m_uiBufferHeight = uiHeight;
        m_nBufferFormat  = nBufferFormat;
        m_nExtFormat     = nExtFormat;
        m_nType          = nType;

        // Setup texture to be used as color attachment
        glGenTextures(1, &m_uiColorTex);

        glBindTexture(GL_TEXTURE_2D, m_uiColorTex);

        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
    
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    	
        glTexImage2D(GL_TEXTURE_2D, 0, m_nBufferFormat, m_uiBufferWidth, m_uiBufferHeight, 0, m_nExtFormat, m_nType, 0);

        // Create FBO with color and depth attachment
        glGenFramebuffers(1,  &m_uiBufferId);
        glGenRenderbuffers(1, &m_uiDepthBuffer);

        glBindFramebuffer(GL_FRAMEBUFFER, m_uiBufferId);

        glBindRenderbuffer(GL_RENDERBUFFER, m_uiDepthBuffer);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, m_uiBufferWidth, m_uiBufferHeight);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_uiColorTex, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,  GL_RENDERBUFFER, m_uiDepthBuffer);

        glBindRenderbuffer(GL_RENDERBUFFER, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindTexture(GL_TEXTURE_2D, 0);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE)
        {  
            return true;
        }
    }
    
    return false;
}


void RenderTarget::deleteBuffer()
{
    if (m_uiColorTex)
        glDeleteTextures(1, &m_uiColorTex);

    if (m_uiDepthBuffer)
        glDeleteRenderbuffers(1, &m_uiDepthBuffer);

    if (m_uiBufferId)
        glDeleteFramebuffers(1, &m_uiBufferId);

    m_uiBufferId = 0;
}


void RenderTarget::bind(GLenum nTarget)
{
    if (m_uiBufferId)
    {
        glBindFramebuffer(nTarget, m_uiBufferId);
    }
}


void RenderTarget::unbind()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}


void RenderTarget::draw()
{
    int nViewport[4];

    glGetIntegerv(GL_VIEWPORT, nViewport);

    float left, right, bottom, top;

    left  = -(float)nViewport[2]/2.0f;
    right = -left;
    bottom = -(float)nViewport[3]/2.0f;
    top    = -bottom;

    glEnable(GL_TEXTURE_2D);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();

    glOrtho(left, right, bottom, top, -1.0f, 1.0f);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glDisable(GL_DEPTH_TEST);

    glBindTexture(GL_TEXTURE_2D, m_uiColorTex);

    // Draw quad with color attachment texture 
    glBegin(GL_QUADS);

    glTexCoord2f(0.0f, 0.0f);
    glVertex3f(left, bottom, 0.0f);

    glTexCoord2f(1.0f, 0.0f);
    glVertex3f(right, bottom, 0.0f);

    glTexCoord2f(1.0f, 1.0f);
    glVertex3f(right, top, 0.0f);

    glTexCoord2f(0.0f, 1.0f);
    glVertex3f(left, top, 0.0f);

    glEnd();

    glBindTexture(GL_TEXTURE_2D, 0);

    glPopMatrix();

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();

    glMatrixMode(GL_MODELVIEW);

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_TEXTURE_2D);
}


unsigned int RenderTarget::getBufferHeight()
{
    if (m_uiBufferId)
    {
        return m_uiBufferHeight;
    }

    return 0;
}


unsigned int RenderTarget::getBufferWidth()
{
    if (m_uiBufferId)
    {
        return m_uiBufferWidth;
    }

    return 0;
}


int RenderTarget::getBufferFormat()
{
    if (m_uiBufferId)
    {
        return m_nBufferFormat;
    }

    return 0;
}