/*
// Part of the DVS SDK (http://www.dvs.de)
//
// stereo_player - Demonstrates the Usage of the Render and FIFO API
//
// The stereo_player example demonstrates how to render an image with the Render API and 
// afterwards display it with the FIFO API without transferring the rendered image back to 
// the system memory. It also shows how to create a stereo image from an image couple 
// (right eye/left eye). 
// The following stereo options are available for the outputs:
//
// 0) No stereo, output frames on jack A
//               +---------------+
//               |               |
//               |      1(n)     |  ==> OUTA                             ==> OUTB
//               |               |
//               +---------------+
//
// 1) Output even frames on jack A and odd frames on jack B 
//               +---------------+                    +---------------+
//               |               |                    |               |
//               |    1(even)    |  ==> OUTA          |     1(odd)    |  ==> OUTB
//               |               |                    |               |
//               +---------------+                    +---------------+
//
// 2) Output on separate jacks
//               +---------------+                    +---------------+
//               |               |                    |               |
//               |      1(n)     |  ==> OUTA          |      2(n)     |  ==> OUTB
//               |               |                    |               |
//               +---------------+                    +---------------+
//
// 3) Output on jack A, side by side
//               +-------+-------+
//               |       |       |
//               | 1(n)  |  2(n) |  ==> OUTA                             ==> OUTB
//               |       |       |
//               +-------+-------+
//
// 4) Output on jack A, on top of each other 
//               +---------------+
//               |      1(n)     |
//               +---------------+  ==> OUTA                             ==> OUTB
//               |      2(n)     |
//               +---------------+
//
// 5) Output on jack A, line by line (even lines: right, odd line left) 
//               +-1(n)----------+
//               +----------2(n)-+
//               +-1(n)----------+  ==> OUTA                             ==> OUTB
//               +----------2(n)-+
//               +---------------+
//
// The control flow is illustrated in the following figure. The startPlayer() function creates 
// a number of threads. Each thread loads an image or image couple from the storage (playerExecute()), 
// transfers it to the video board (playerDMA()), renders it to the selected stereo output format 
// (playerRender()) and outputs it at the output jack (playerDisplay()). After that it processes 
// the next image or finishes.
//
// Conrol Flow:
//
//  main  startPlayer  playerThread  playerExecute  playerDMA   playerRender  playerDisplay
//   |         |            |             |              |           |             |
//   +-------->| n Threads  |             |              |           |             |
//   | \       +----------->|             |              |           |             |
//   |  \      |            +------------>|              |           |             |
//   |  |      |            | \           + - - - - - - - - - - - - - - - - - - - >|
//   |  |      |            |  \          |          low latency option            |
//   |  |      |            |  |          |< - - - - - - - - - - - - - - - - - - - +
//   |  |      |            |  |          |              |           |             |
//   |  |      |            |  |          +------------->|           |             |
//   |  |      |            |  |          |<-------------+           |             |
//   |  |Loop? |            |  |n Frames  |              |           |             |
//   |  |      |            |  |          +------------------------->|             |
//   |  |      |            |  |          |<-------------------------+             |
//   |  |      |            |  |          |              |           |             |
//   |  |      |            |  |          +--------------------------------------->|
//   |  |      |            |  /          |         default timing option          |
//   |  |      |            | /           |<---------------------------------------+
//   |  /      |            |<------------+              |           |             |
//   | /       |<-----------+             |              |           |             |
//   |<--------|            |             |              |           |             |
//   |         |            |             |              |           |             |
//
// The stereo_player example has two different timing options:
// 1) The default timing has at least three interrupts delay. It provides a kind of error 
//    concealing in cases when the data can not be provided in time, for example because the 
//    DMA (see timings below) or the rendering take temporarily more time than expected. 
//    In this case the last good picture will be repeated. 
// 2) The low latency timing has at least two interrupts delay but it does not provide an 
//    error concealing. In cases where the DMA or rendering are too slow it may happen that 
//    a picture is displayed that contains parts of two different pictures.  
//
// The following figures illustrate the different options:
//
// Overall Timing:
//
// a) Default Timing (DMA and render within one vsync)
//     n-3                 n-2                 n-1                  n                  n+1
//      |                   |                   |                   |                   |
//      |_____  __________  |                   |                   |___________________|
// n ---|_DMA_||__RENDER__|=|-------------------|-------------------|______DISPLAY______|-----
//      |                   GetBuffer/PutBuffer |                   |                   |
//      |                   |                   |                   |                   |
//      |                   |<==================== OVERALL TIME =======================>|
//      |                   |_____  __________  |                   |                   |_____
// n+1 -|-------------------|_DMA_||__RENDER__|=|-------------------|-------------------|__DIS
//      |                   |                   GetBuffer/PutBuffer |                   |
//      |                   |                   |                   |                   |
//
// b) Default Timing (DMA and render more than one vsync)
//     n-3                 n-2                 n-1                  n                  n+1
//      |                   |                   |                   |                   |
//    _____                 |                   |                   |___________________|
// n -ER___|================|-------------------|-------------------|______DISPLAY______|-----
//      |                   GetBuffer/PutBuffer |                   |                   |
//      |                   |                   |                   |                   |
//      |<=============================== OVERALL TIME ================================>|
//      |_______  _____________                 |                   |                   |_____
// n+1 -|__DMA__||____RENDER___|================|-------------------|-------------------|__DIS
//      |                   |                   GetBuffer/PutBuffer |                   |
//
//
// 1) Low Latency Timing (DMA and render within one vsync)
//     n-2                 n-1                  n                  n+1                 n+2
//      |                   |                   |                   |                   |
//      |_____  __________  |                   |___________________|                   |
// n ---|_DMA_||__RENDER__|=|-------------------|______DISPLAY______|-------------------|-----
//      GetBuffer/PutBuffer |                   |                   |                   |
//      |                   |                   |                   |                   |
//      |                  <============= OVERALL TIME ============>|                   |
//      |                   |_____  __________  |                   |___________________|
// n+1 -|-------------------|_DMA_||__RENDER__|=|-------------------|______DISPLAY______|-----
//      |                   GetBuffer/PutBuffer |                   |                   |
//
// 2)Low Latency Timing (DMA and render more than one vsync)
//     n-2                 n-1                  n                  n+1                 n+2
//      |                   |                   |                   |                   |
//      |<=============================== OVERALL TIME ================================>|
//      |_______  _____________                 |___________________|                   |
// n ---|__DMA__||____RENDER___|================|______DISPLAY______|-------------------|-----
//      GetBuffer/PutBuffer |                   |                   |                   |
//      |                   |                   |                   |                   |
//      |                   |_______  _____________                 |___________________|
// n+1 -|-------------------|__DMA__||____RENDER___|================|______DISPLAY______|-----
//      |                   GetBuffer/PutBuffer |                   |                   |
//      |                   |                   |                   |                   |
//      |                   |   <================== OVERALL TIME ======================>|
//      |                   |                   |_______  _____________                 |_____
// n+2 -|-------------------|-------------------|__DMA__||____RENDER___|================|__DIS
//      |                   |                   GetBuffer/PutBuffer |                   |
//      |                   |                   |                   |                   |
//
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>

#ifdef WIN32
  #include <windows.h>
#else
  #ifdef linux
    #define __USE_LARGEFILE64
  #endif
  #include <unistd.h>
#endif


#include "../../header/dvs_setup.h"
#include "../../header/dvs_clib.h"
#include "../../header/dvs_fifo.h"
#include "../../header/dvs_thread.h"
#include "../../header/dvs_render.h"

#define MAX_PATHNAME    512

#define MAX_BUFFERSIZE  (4096 * 3112 * 4)

/*
//  Defines for stereo option
*/
#define STEREO_OUTPUTMODE_MONO           0
#define STEREO_OUTPUTMODE_EVEN_ODD_FRAME 1
#define STEREO_OUTPUTMODE_INDEPENDENT    2
#define STEREO_OUTPUTMODE_LEFT_RIGHT     3
#define STEREO_OUTPUTMODE_TOP_BOTTOM     4
#define STEREO_OUTPUTMODE_EVEN_ODD_LINE  5

#define TIMING_ANALYSES_NONE             0
#define TIMING_ANALYSES_STATISTICS       1
#define TIMING_ANALYSES_PROCESS          2
#define TIMING_ANALYSES_WAIT             3

#define JACK_ID_OUTA                     0
#define JACK_ID_INA                      1
#define JACK_ID_OUTB                     2
#define JACK_ID_INB                      3

#define JACK_OUTPUT_TOLERANCE            20 // us

/*
// stucture to save the settings/params for the program
*/
typedef struct {
  sv_handle * pSv;
  sv_render_handle * pSvRh;
  sv_fifo * pFifoJackA;
  sv_fifo * pFifoJackB;

  int loopMode;
  int verbose;
  int threadCount;
  int startFrame;
  int frameCount;
  int OutputMode;
  int timingAnalyses;
  int fifoDepth;
  int fifoFlags;
  int bLowLatency;
  
  int bSecondSource;
  int bSecondOutput;
  
  char SrcOneFileName[MAX_PATHNAME];
  char SrcTwoFileName[MAX_PATHNAME];

  int bDummySrcOne;
  int bDummySrcTwo;
    
} settings_t;

/*
// structure to hold the thread information 
*/
typedef struct {
  int id;
  int result;

  dvs_thread handle;
  dvs_cond exit;
  dvs_cond running;
  dvs_mutex * pFrameLock;

  sv_render_context * pSvRcOne;
  sv_render_context * pSvRcTwo;

  settings_t * pSettings;

  sv_overlapped  DmaOvlapOne;
  sv_overlapped  DmaOvlapTwo;
  sv_overlapped  RenOvlapOne;
  sv_overlapped  RenOvlapTwo;
  
  struct {
    uint64 startTime;
    uint64 startFileRead;
    uint64   endFileRead;
    uint64 startDmaTransfer;
    uint64   endDmaTransfer;
    uint64 startRenderProcess;
    uint64   endRenderProcess;
    uint64 startFifoOutput;
    uint64   endFifoOutput;
    uint64 endTime;
  } time;
 
} threadInfo_t;

typedef struct {
  int xsize;           // x size [pixel]
  int ysize;           // y size [pixel]
  int nbits;           // number of bits per pixel
  int dpxType;
  int lineoffset;      // Offset [byte] from line to line
  int dataoffset;      // offset [byte] to the start of the data 
  int pixelsize_num;   // Size of 1 pixel, numerator/denominator
  int pixelsize_denom; // Size of 1 pixel, numerator/denominator
  int size;            // size of the image in byte
  int storagemode;     // Storage mode (SV_MODE_<xxx>)
  int frameNo;         // actual frame number
} frameInfo_t;


typedef struct {
  char * pBuffer;
  char * pAligned;
  unsigned int bufferSize;
} videoBuffer_t;

typedef struct {
  char name[50];
  int count;
  dvs_mutex mutex;
} sync_t;

typedef struct {
  uint64 sum;
  int count;
  int min;
  float mean;
  int max;
} statistic_t;

typedef struct {
  statistic_t overall;
  statistic_t fileread;
  statistic_t dma;  
  statistic_t render;
  statistic_t fifo;
  statistic_t wait;
} timing_t;

