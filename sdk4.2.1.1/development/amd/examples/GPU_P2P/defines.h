
#pragma once

#define GL_EXTERNAL_VIRTUAL_MEMORY_AMD          0x9160

#define GL_BUS_ADDRESSABLE_MEMORY_AMD           0x9168
#define GL_EXTERNAL_PHYSICAL_MEMORY_AMD         0x9169

typedef GLvoid (APIENTRY * PFNGLWAITMARKERAMDPROC)          (GLuint buffer, GLuint marker);
typedef GLvoid (APIENTRY * PFNGLWRITEMARKERAMDPROC)         (GLuint buffer, GLuint marker, GLuint64 offset);
typedef GLvoid (APIENTRY * PFNGLMAKEBUFFERSRESIDENTAMDPROC) (GLsizei count, GLuint* buffers, GLuint64* baddrs, GLuint64* maddrs);
typedef GLvoid (APIENTRY * PFNGLBUFFERBUSADDRESSAMDPROC)    (GLenum target, GLsizeiptr size, GLuint64 baddrs, GLuint64 maddrs);


extern PFNGLWAITMARKERAMDPROC          glWaitMarkerAMD;
extern PFNGLWRITEMARKERAMDPROC         glWriteMarkerAMD;
extern PFNGLMAKEBUFFERSRESIDENTAMDPROC glMakeBuffersResidentAMD;
extern PFNGLBUFFERBUSADDRESSAMDPROC    glBufferBusAddressAMD;


#define OneSecond 1000*1000*1000L

#define NUM_BUFFERS 2


typedef struct
{
    unsigned int       uiTransferId;
    unsigned long long ullBufferBusAddress;
    unsigned long long ullMarkerBusAddress;
} FrameData;