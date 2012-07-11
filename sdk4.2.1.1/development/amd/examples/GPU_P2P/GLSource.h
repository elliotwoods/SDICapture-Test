

#pragma once


class SyncedBuffer;
class RenderTarget;


class GLSource
{
public:

    GLSource();
    ~GLSource();

    void            initGL();
    void            resize(unsigned int w, unsigned int h);
    bool            createUpStream(unsigned int w, unsigned int h, int nIntFormat, int nExtFormat, int nType);

    bool            setRemoteMemory(unsigned long long* pBufferBusAddress, unsigned long long* pMarkerBusAddress);

    void            draw();

    void            release();

    void            setOutputBuffer(SyncedBuffer* pOutputBuffer);
    SyncedBuffer*   getOutputBuffer() { return (SyncedBuffer*) m_pOutputBuffer; };

private:


    unsigned int        m_uiWindowWidth;
    unsigned int        m_uiWindowHeight;

    unsigned int        m_uiBufferWidth;
    unsigned int        m_uiBufferHeight;

    unsigned int        m_uiBufferSize;
    unsigned int        m_uiBufferIdx;
    unsigned int*       m_pPackBuffer;

    float               m_fLinePosition[2];

    int                 m_nIntFormat;
    int                 m_nExtFormat;
    int                 m_nType;

    unsigned int        m_uiFontBase;

    RenderTarget*       m_pRenderTarget;

    SyncedBuffer*       m_pOutputBuffer;
};
