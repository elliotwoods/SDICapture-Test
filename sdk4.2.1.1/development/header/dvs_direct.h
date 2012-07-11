#ifndef _DVS_DIRECT_H
#define _DVS_DIRECT_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef CBLIBINT_H
typedef void sv_direct_handle;
#endif

#if !defined(__gl_h_) && !defined(__GL_H__) && !defined(__X_GL_H)
typedef unsigned int GLuint;
#endif

#if !defined(__cuda_cuda_h__)
#if defined(__x86_64) || defined(AMD64) || defined(_M_AMD64)
typedef unsigned long long CUdeviceptr;
#else
typedef unsigned int CUdeviceptr;
#endif
typedef struct CUarray_st *CUarray; 
#endif

#if defined(WIN32)
#include <windows.h>

#ifndef DIRECT3D_VERSION
interface DECLSPEC_UUID("0CFBAF3A-9FF6-429a-99B3-A2796AF8B89B") IDirect3DSurface9;
typedef interface IDirect3DSurface9                             IDirect3DSurface9;
#endif

#ifndef __ID3D10Resource_INTERFACE_DEFINED__
typedef void ID3D10Resource;
#endif

#ifndef __ID3D11Resource_INTERFACE_DEFINED__
typedef void ID3D11Resource;
#endif

#endif

#if !defined(DOCUMENTATION)

typedef struct {
  int size;                ///< Size of the structure (needs to be filled by the
                           ///< caller).
  int available;           ///< For an input this element is the number of filled/
                           ///< recorded buffers that can be fetched by
                           ///< sv_direct_record() without blocking. For an output this
                           ///< element is the number of buffers that can be queued to
                           ///< the driver without blocking.
  int dropped;             ///< Number of buffers that were dropped since calling the 
                           ///< function sv_direct_init().
  int when;                ///< Buffer tick.
  int clock_high;          ///< Buffer clock (upper 32 bits).
  int clock_low;           ///< Buffer clock (lower 32 bits).
  int pad[16];             ///< Reserved for future use.
} sv_direct_info;

typedef struct {
  int size;                ///< Size of the structure (needs to be filled by the
                           ///< caller).
  int ltc_tc;              ///< Analog LTC timecode without bit masking.
  int ltc_ub;              ///< Analog LTC user bytes.
  int vtr_tc;              ///< VTR timecode.
  int vtr_ub;              ///< VTR user bytes.
  int vitc_tc[2];          ///< Analog VITC timecode.
  int vitc_ub[2];          ///< Analog VITC user bytes.
  int film_tc[2];          ///< Analog film timecode.
  int film_ub[2];          ///< Analog film user bytes.
  int prod_tc[2];          ///< Analog production timecode.
  int prod_ub[2];          ///< Analog production user bytes.
  int dvitc_tc[2];         ///< Digital/ANC VITC timecode.
  int dvitc_ub[2];         ///< Digital/ANC VITC user bytes.
  int dfilm_tc[2];         ///< Digital/ANC film timecode (RP201).
  int dfilm_ub[2];         ///< Digital/ANC film user bytes (RP201).
  int dprod_tc[2];         ///< Digital/ANC production timecode (RP201).
  int dprod_ub[2];         ///< Digital/ANC production user bytes (RP201).
  int dltc_tc;             ///< Digital/ANC LTC timecode.
  int dltc_ub;             ///< Digital/ANC LTC user bytes.
  int gpi;                 ///< GPI information of the buffer.
  int pad[16];             ///< Reserved for future use.
} sv_direct_timecode;

typedef struct {
  int field;               ///< Field index (0 or 1) of this ANC packet. Fields have
                           ///< to be returned in order.
  int linenr;              ///< Line number of this ANC packet.
  int did;                 ///< Data ID of this ANC packet.
  int sdid;                ///< Secondary data ID of this ANC packet.
  int bvanc;               ///< Position this packet in VANC(1) or HANC(0).
  unsigned char data[256]; ///< Payload buffer.
  unsigned int datasize;   ///< Payload buffer size.
  int pad[8];              ///< Reserved for future use.
} sv_direct_ancpacket;
typedef int (*sv_direct_anc_callback)(void * userdata, int bufferindex, sv_direct_ancpacket * packet);

typedef struct {
  int size;                ///< Size of the structure (needs to be filled by the
                           ///< caller).
  int when;                ///< Current tick.
  int clock_high;          ///< Current clock time (upper 32 bits).
  int clock_low;           ///< Current clock time (lower 32 bits).
  struct {
    int clock_distance;    ///< Distance from starting the DMA transfer (between video
                           ///< board and system memory) until the following vsync. This
                           ///< value can be seen as an indication if the respective
                           ///< sv_direct_display() or sv_direct_record() function has
                           ///< been called too late.
    int clock_go_high;     ///< Clock time of the starting of the DMA (upper 32 bits).
    int clock_go_low;      ///< Clock time of the starting of the DMA (lower 32 bits).
    int clock_ready_high;  ///< Clock time of the finishing of the DMA (upper 32 bits).
    int clock_ready_low;   ///< Clock time of the finishing of the DMA (upper 32 bits).
    int pad[8];            ///< Reserved for future use.
  } dma;
  int pad[16];             ///< Reserved for future use.
} sv_direct_bufferinfo;

#endif /* !DOCUMENTATION */

#define SV_DIRECT_FLAG_DISCARD     0x00000001
#define SV_DIRECT_FLAG_FIELD       0x00000002
#define SV_DIRECT_FLAG_DONTBLOCK   0x00000004

export int sv_direct_init(sv_handle * sv, sv_direct_handle ** pdh, char * mode, int jack, int buffercount, int flags);
export int sv_direct_initex(sv_handle * sv, sv_direct_handle ** pdh, char * mode, int jack, int buffercount, int flags, void * data);
export int sv_direct_free(sv_handle * sv, sv_direct_handle * dh);
export int sv_direct_status(sv_handle * sv, sv_direct_handle * dh, sv_direct_info * pinfo);

export int sv_direct_bind_anc_callback(sv_handle * sv, sv_direct_handle * dh, int bufferindex, sv_direct_anc_callback cb, int * cberror, void * userdata);
export int sv_direct_bind_buffer(sv_handle * sv, sv_direct_handle * dh, int bufferindex, char * addr, int size);
export int sv_direct_bind_timecode(sv_handle * sv, sv_direct_handle * dh, int bufferindex, sv_direct_timecode * ptc);
export int sv_direct_bind_opengl(sv_handle * sv, sv_direct_handle * dh, int bufferindex, GLuint texture);
export int sv_direct_bind_cuda_devptr(sv_handle * sv, sv_direct_handle * dh, int bufferindex, CUdeviceptr devPtr);
export int sv_direct_bind_cuda_array(sv_handle * sv, sv_direct_handle * dh, int bufferindex, CUarray array);
export int sv_direct_bind_physical(sv_handle * sv, sv_direct_handle * dh, int bufferindex, uint64 * paddr, int * psize, uint64 * pmarker, int * pmarkersize);
export int sv_direct_write_marker(sv_handle * sv, sv_direct_handle * dh, int bufferindex, int value);
export int sv_direct_unbind(sv_handle * sv, sv_direct_handle * dh, int bufferindex);

#if defined(WIN32)
export int sv_direct_bind_d3d9surface(sv_handle * sv, sv_direct_handle * dh, int bufferindex, IDirect3DSurface9 * surface);
export int sv_direct_bind_d3d10resource(sv_handle * sv, sv_direct_handle * dh, int bufferindex, ID3D10Resource * resource);
export int sv_direct_bind_d3d11resource(sv_handle * sv, sv_direct_handle * dh, int bufferindex, ID3D11Resource * resource);
#endif

export int sv_direct_record(sv_handle * sv, sv_direct_handle * dh, int bufferindex, int flags, sv_direct_bufferinfo * pinfo);
export int sv_direct_display(sv_handle * sv, sv_direct_handle * dh, int bufferindex, int flags, sv_direct_bufferinfo * pinfo);
export int sv_direct_sync(sv_handle * sv, sv_direct_handle * dh, int bufferindex, int flags);

#ifdef __cplusplus
}
#endif

#endif
