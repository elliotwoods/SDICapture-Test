
#pragma once


#include "defines.h"

class SyncedBuffer;


class GLSink
{
public:

    GLSink();
    ~GLSink();

    void            initGL();
    void            resize(unsigned int w, unsigned int h);
    bool            createDownStream(unsigned int w, unsigned int h, int nIntFormat, int nExtFormat, int nType);

    void            draw();

    void            release();

    unsigned long long* getBufferBusAddress()   { return m_pBufferBusAddress; };
    unsigned long long* getMarkerBusAddress()   { return m_pMarkerBusAddress; };

    SyncedBuffer*   getInputBuffer() { return (SyncedBuffer*) m_pInputBuffer; };

private:

    
    unsigned int            m_uiWindowWidth;
    unsigned int            m_uiWindowHeight;

    unsigned int            m_uiTextureWidth;
    unsigned int            m_uiTextureHeight;
    unsigned int            m_uiTextureSize;
    unsigned int            m_uiTexture;

    float                   m_fAspectRatio;

    unsigned int            m_uiBufferSize;
    unsigned int            m_uiBufferIdx;
    unsigned int*           m_pUnPackBuffer;
    unsigned long long*     m_pBufferBusAddress;
    unsigned long long*     m_pMarkerBusAddress;

    int                     m_nIntFormat;
    int                     m_nExtFormat;
    int                     m_nType;

    unsigned int            m_uiQuad;

    SyncedBuffer*           m_pInputBuffer;                        
};
