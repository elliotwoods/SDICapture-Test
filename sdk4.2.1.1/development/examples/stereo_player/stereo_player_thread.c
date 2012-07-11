/*
//    Part of the DVS SDK (http://www.dvs.de)
//
//    stereo_player - Demonstrates the usage of the Render and FIFO API.
//
*/
#include "stereo_player.h"
#include "tags.h"

int gRunning        = TRUE;
int gbFifoRunning   = FALSE;
int gDropped        = 0;
int gFrameCountInc  = 1;    // used for gFrameNo and the sync counters
int gFrameNo        = 0;

dvs_mutex gRunningLock;
dvs_mutex gFifoLock;
dvs_mutex gDroppedLock;
dvs_mutex gFrameNoLock;
dvs_mutex gRenderFrameNoLock;
dvs_mutex gDisplayFrameNoLock;
dvs_mutex gTimingLock;

sync_t gtReadFileSync;
sync_t gtMallocSync;
sync_t gtDmaSync;
sync_t gtRenderSync;
sync_t gtDisplaySync;

timing_t gtTiming;

/*
// Helper function for making sure that all frames will be rendered.
*/
int getNextFrame(void)
{
  int local;
  dvs_mutex_enter(&gFrameNoLock);
  local = gFrameNo;
  gFrameNo+=gFrameCountInc;
  dvs_mutex_leave(&gFrameNoLock);
  return local;
}

/*
// Helper function to set the increment from one fame to the next
// global default after reset is 1
*/
void setFrameCountInc(int inc)
{
  gFrameCountInc = inc;
}

/*
//  Init gloable functions
*/
void initGlobals(void)
{
  gbFifoRunning       = FALSE;
  gDropped            = 0;
  gFrameNo            = 0;
  
  gtReadFileSync.count= 0;
  gtMallocSync.count  = 0;
  gtDmaSync.count     = 0;
  gtRenderSync.count  = 0;
  gtDisplaySync.count = 0;
  
  sprintf(gtReadFileSync.name,   "FileRead Sync");
  sprintf(gtMallocSync.name,     "Malloc Sync");
  sprintf(gtDmaSync.name,        "DMA Sync");
  sprintf(gtRenderSync.name,     "Render Sync");
  sprintf(gtDisplaySync.name,    "Fifo Sync");
  
  memset(&gtTiming, 0, sizeof(timing_t));
}

/*
// Init all mutex used by thread context
*/
void initThreadMutex(void)
{
  dvs_mutex_init(&gRunningLock);
  dvs_mutex_init(&gFifoLock);
  dvs_mutex_init(&gDroppedLock);
  dvs_mutex_init(&gFrameNoLock);
  dvs_mutex_init(&gTimingLock);

  dvs_mutex_init(&gtReadFileSync.mutex);
  dvs_mutex_init(&gtMallocSync.mutex);
  dvs_mutex_init(&gtDmaSync.mutex);
  dvs_mutex_init(&gtRenderSync.mutex);
  dvs_mutex_init(&gtDisplaySync.mutex);
}

/*
// close all mutex used by thread context
*/
void closeThreadMutex(void)
{
  dvs_mutex_free(&gRunningLock);
  dvs_mutex_free(&gFifoLock);
  dvs_mutex_free(&gDroppedLock);
  dvs_mutex_free(&gFrameNoLock);
  dvs_mutex_free(&gTimingLock); 
  
  dvs_mutex_free(&gtReadFileSync.mutex);
  dvs_mutex_free(&gtMallocSync.mutex);
  dvs_mutex_free(&gtDmaSync.mutex);
  dvs_mutex_free(&gtRenderSync.mutex);
  dvs_mutex_free(&gtDisplaySync.mutex);
  }

/*
// Init all variables used by thread context
*/
int initThreadCond(threadInfo_t * pThreadInfo)
{
  int result = SV_OK;
  
  dvs_cond_init(&pThreadInfo->running);

  memset(&pThreadInfo->RenOvlapOne, 0, sizeof(sv_overlapped));
  memset(&pThreadInfo->RenOvlapTwo, 0, sizeof(sv_overlapped));
  memset(&pThreadInfo->DmaOvlapOne, 0, sizeof(sv_overlapped));
  memset(&pThreadInfo->DmaOvlapTwo, 0, sizeof(sv_overlapped));

#ifdef WIN32
  pThreadInfo->RenOvlapOne.overlapped.hEvent = CreateEvent(0, FALSE, 0, NULL);
  pThreadInfo->RenOvlapTwo.overlapped.hEvent = CreateEvent(0, FALSE, 0, NULL);
  pThreadInfo->DmaOvlapOne.overlapped.hEvent = CreateEvent(0, FALSE, 0, NULL);
  pThreadInfo->DmaOvlapTwo.overlapped.hEvent = CreateEvent(0, FALSE, 0, NULL);
#endif

  return result;
}

/*
// close all variables used by thread context
*/
void closeThreadCond(threadInfo_t * pThreadInfo)
{
  dvs_cond_free(&pThreadInfo->running);

#ifdef WIN32
  CloseHandle(pThreadInfo->RenOvlapOne.overlapped.hEvent);
  CloseHandle(pThreadInfo->RenOvlapTwo.overlapped.hEvent);
  CloseHandle(pThreadInfo->DmaOvlapOne.overlapped.hEvent);
  CloseHandle(pThreadInfo->DmaOvlapTwo.overlapped.hEvent);
#endif
}

/*
// Helper function for timing analyses
*/
uint64 getCurrentTime(settings_t *pSettings)
{
  uint64 time      = 0;
  uint32 clockhigh = 0;
  uint32 clocklow  = 0;
  int    tick      = 0;

  // get the current time from driver
  sv_currenttime(pSettings->pSv, SV_CURRENTTIME_CURRENT, &tick, &clockhigh, &clocklow); 
  // generate 64bit time
  time = (((uint64)clockhigh)<<32) + (uint64)clocklow;
  return time;
}
void timingAnalysesThread(threadInfo_t * pThreadInfo, int frameNo)
{
  settings_t * pSettings = pThreadInfo->pSettings;
  int waitdma, waitrender, waitfifo, waitoutput;

  int overall    = (int)(pThreadInfo->time.endTime           - pThreadInfo->time.startTime);
  int fileread   = (int)(pThreadInfo->time.endFileRead       - pThreadInfo->time.startFileRead);
  int dma        = (int)(pThreadInfo->time.endDmaTransfer    - pThreadInfo->time.startDmaTransfer);
  int render     = (int)(pThreadInfo->time.endRenderProcess  - pThreadInfo->time.startRenderProcess);
  int fifo       = (int)(pThreadInfo->time.endFifoOutput     - pThreadInfo->time.startFifoOutput);
  
  if(pSettings->bLowLatency) {
    waitfifo   = (int)(pThreadInfo->time.startFifoOutput   - pThreadInfo->time.endFileRead);
    waitdma    = (int)(pThreadInfo->time.startDmaTransfer  - pThreadInfo->time.endFifoOutput);
    waitrender = (int)(pThreadInfo->time.startRenderProcess- pThreadInfo->time.endDmaTransfer);
    waitoutput = (int)(pThreadInfo->time.endTime           - pThreadInfo->time.endRenderProcess);
  } else {
    waitdma    = (int)(pThreadInfo->time.startDmaTransfer  - pThreadInfo->time.endFileRead);
    waitrender = (int)(pThreadInfo->time.startRenderProcess- pThreadInfo->time.endDmaTransfer);
    waitfifo   = (int)(pThreadInfo->time.startFifoOutput   - pThreadInfo->time.endRenderProcess);
    waitoutput = (int)(pThreadInfo->time.endTime           - pThreadInfo->time.endFifoOutput);
  }

  if((waitoutput < 0) || (pThreadInfo->time.endTime == 0)) waitoutput = 0;
 
  // lock this section
  dvs_mutex_enter(&gTimingLock); 
  calcStatistics(&gtTiming.overall, overall);
  calcStatistics(&gtTiming.fileread,fileread);
  calcStatistics(&gtTiming.dma,     dma);
  calcStatistics(&gtTiming.render,  render);
  calcStatistics(&gtTiming.fifo,    fifo);
  calcStatistics(&gtTiming.wait,    (waitdma + waitrender + waitfifo + waitoutput));
 
  switch(pSettings->timingAnalyses) {
  case TIMING_ANALYSES_STATISTICS:
    printf("Frame(%d) Mean[us]: %6d(T) %6d(F) %6d(D) %6d(R) %6d(O) %6d(W)\n",
      frameNo, (int)gtTiming.overall.mean, (int)gtTiming.fileread.mean, (int)gtTiming.dma.mean, (int)gtTiming.render.mean, (int)gtTiming.fifo.mean, (int)gtTiming.wait.mean);
    
    break;
  case TIMING_ANALYSES_PROCESS:
    printf("Frame(%d) Duration[us]: %6d(T) %6d(F) %6d(D) %6d(R) %6d(O)\n",
            frameNo, overall, fileread, dma, render, fifo);
    break;
  case TIMING_ANALYSES_WAIT:
    printf("Frame(%d) Wait[us]: %6d(D) %6d(R) %6d(F) %6d(O)\n",
            frameNo, waitdma, waitrender, waitfifo, waitoutput);
    break;
  default: // TIMING_ANALYSES_NONE 
    break; // do nothing
  }
  dvs_mutex_leave(&gTimingLock); 
}
void calcStatistics(statistic_t *pterm, int value)
{
  // check min
  if(pterm->min == 0)    { pterm->min = value; }  // init mean
  if(value < pterm->min) { pterm->min = value; }
  
  // calculate mean
  if(pterm->mean == 0) {
    pterm->sum   = 0;
    pterm->count = 1;
    pterm->mean  = value;     // init mean
  } else {
    pterm->mean = (float)((pterm->mean * pterm->count) + value) / (float)(pterm->count+1);
    pterm->count++;
    pterm->sum += value;
  }
  
  // check max
  if(pterm->max < value) { pterm->max = value; }
}
void printStatistics(void)
{
  printf("\n");
  printf("Overall:  (min/mean/max)[us] %6d / %6d/ %6d\n",gtTiming.overall.min, (int)gtTiming.overall.mean, gtTiming.overall.max);
  printf("FileRead: (min/mean/max)[us] %6d / %6d/ %6d\n",gtTiming.fileread.min, (int)gtTiming.fileread.mean, gtTiming.fileread.max);
  printf("DMA:      (min/mean/max)[us] %6d / %6d/ %6d\n",gtTiming.dma.min, (int)gtTiming.dma.mean, gtTiming.dma.max);
  printf("Render:   (min/mean/max)[us] %6d / %6d/ %6d\n",gtTiming.render.min, (int)gtTiming.render.mean, gtTiming.render.max);
  printf("Fifo:     (min/mean/max)[us] %6d / %6d/ %6d\n",gtTiming.fifo.min, (int)gtTiming.fifo.mean, gtTiming.fifo.max);
  printf("Wait:     (min/mean/max)[us] %6d / %6d/ %6d\n",gtTiming.wait.min, (int)gtTiming.wait.mean, gtTiming.wait.max);
  printf("Drop:                        %6d\n", gDropped);
  printf("\n");
}


/*
// Helper function to start a synchronized section
*/
int startSync(settings_t *pSettings, sync_t * ptSync, int count, int id)
{
  int timeout = 0;
  
  /*
  // To make sure that the frames are processed in the correct order a counter
  // for the frame number is used. The task sleeps until its frame
  // number is reached.
  // The function returns with the mutex locked. Release mutex with endSync() after the 
  // critical section.
  */
  timeout = 1000;
  do {
    dvs_mutex_enter(&ptSync->mutex);
    if( count == ptSync->count ) {
      // hold the mutex until end of render manipulation 
      break;
    } else {
      dvs_mutex_leave(&ptSync->mutex);
      sv_usleep(pSettings->pSv, 200); // Wait 0.2 milliseconds until the next retry.
    }
  } while( --timeout > 0 );

  if( timeout <= 0 ) {
    printf("startSync(%d): Timeout for %s\n", id, ptSync->name);
    return FALSE;
  }

  return TRUE;
}

/*
// Helper function to finish a synchronized section
*/
int endSync(sync_t *ptSync)
{
  ptSync->count+=gFrameCountInc;  
  dvs_mutex_leave(&ptSync->mutex);
  return 0;
}

/*
// Helper function to allocate and align buffers.
*/
int allocateAlignedBuffer(videoBuffer_t * pVideoBuffer, unsigned int size, unsigned int alignment)
{
  int result = TRUE;

  if(result) {    
    pVideoBuffer->bufferSize = (size + (alignment - 1)) & ~(alignment - 1);
    pVideoBuffer->pBuffer    = (char *) malloc(((pVideoBuffer->bufferSize + alignment) + (alignment - 1)) & ~(uintptr)(alignment - 1));
    pVideoBuffer->pAligned   = (char *)(((uintptr)pVideoBuffer->pBuffer + (alignment - 1)) & ~(uintptr)(alignment - 1));
    if(!pVideoBuffer->pAligned) {
      result = FALSE;
      printf("allocateAlignedBuffer(): Unable to allocate video buffer!\n");
    }
  }

  if(result) {
    memset(pVideoBuffer->pAligned, 0x00, pVideoBuffer->bufferSize);
  }

  return result;
}

/*
// Helper function to malloc/realloc the sv_render_image
*/
int initRenderImage(threadInfo_t * pThreadInfo, sv_render_image **ppRenderImage, frameInfo_t * pframeInfo)
{
  int                     res     = SV_OK;
  int                     timeout;
  sv_render_bufferlayout  bufferLayout;
  sv_handle             * pSv     = pThreadInfo->pSettings->pSv;
  sv_render_handle      * pSvRh   = pThreadInfo->pSettings->pSvRh;
  
  // Set the properties of the buffer's layout.
  memset(&bufferLayout, 0, sizeof(sv_render_bufferlayout));
  bufferLayout.v1.xsize       = pframeInfo->xsize;
  bufferLayout.v1.ysize       = pframeInfo->ysize;
  bufferLayout.v1.storagemode = pframeInfo->storagemode;
  bufferLayout.v1.lineoffset  = pframeInfo->lineoffset;
  bufferLayout.v1.dataoffset  = pframeInfo->dataoffset;
  
  if(!(*ppRenderImage)) {
    /*
    // Try to allocate space on the DVS video board for the first source buffer.
    // This may fail due to memory fragmentation, so retry up to 100 times only.
    */
    timeout = 100;
    do {
      res = sv_render_malloc(pSv, pSvRh, ppRenderImage, 1, sizeof(sv_render_bufferlayout), &bufferLayout);
      if(res == SV_ERROR_MALLOC) {
        sv_usleep(pSv, 2000); // Wait 2 milliseconds until the next retry.
      }
    } while((res == SV_ERROR_MALLOC) && (--timeout > 0));

    if((res != SV_OK) && (timeout <= 0)) {
      res  = SV_ERROR_TIMEOUT;
    }
  }else{
    res = sv_render_realloc(pSv, pSvRh, *ppRenderImage, 1, sizeof(sv_render_bufferlayout), &bufferLayout);
  }
  return res;
}

/*
// Helper function transfers two render images at the same time
// from the system memory to the video board memory.
//
// Note: The two images must be of the same size!
*/
int playerDMA(threadInfo_t * pThreadInfo, videoBuffer_t * pSourceBuffer, 
                        sv_render_image **ppSrcRenderImage, int size, int frameNo)
{
  int res1     = SV_OK;
  int res2     = SV_OK;
  int running  = TRUE;
  int dmaOffset, dmaSize;

  sv_handle        * pSv       = pThreadInfo->pSettings->pSv;
  sv_render_handle * pSvRh     = pThreadInfo->pSettings->pSvRh;
  settings_t       * pSettings = pThreadInfo->pSettings;

  // synchronize the dma transfer 
  running = startSync(pSettings, &gtDmaSync, frameNo, pThreadInfo->id);

  // get current time for analyses
  pThreadInfo->time.startDmaTransfer = getCurrentTime(pSettings);

  if(running) {
    // Transfer the source buffer from the system RAM to the DVS video board RAM.
    // This is done in maximum chunk sizes of 4MB.
    dmaOffset = 0;
    while( dmaOffset < size ) {
      dmaSize = size - dmaOffset;
      if(dmaSize > 0x400000) {
        dmaSize = 0x400000;
      }
      
      // start DMA for first source image
      res1 = sv_render_dma( pSv, pSvRh, TRUE, ppSrcRenderImage[0], 
                          &pSourceBuffer[0].pAligned[dmaOffset], 
                          dmaOffset, dmaSize, &pThreadInfo->DmaOvlapOne);
      if((res1 != SV_OK) && (res1 != SV_ACTIVE) ) {
        break;
      }

      // start DMA for second source image
      if( pSettings->bSecondSource ) {
        res2 = sv_render_dma( pSv, pSvRh, TRUE, ppSrcRenderImage[1], 
                            &pSourceBuffer[1].pAligned[dmaOffset], 
                             dmaOffset, dmaSize, &pThreadInfo->DmaOvlapTwo);
        if((res2 != SV_OK) && (res2 != SV_ACTIVE) ) {
          break;
        }     
        // wait for the DMA transfer of second source image
        res2 = sv_memory_dma_ready(pSv, &pThreadInfo->DmaOvlapTwo, res2);    
      }
      // wait for the DMA transfer of first source image
      res1 = sv_memory_dma_ready(pSv, &pThreadInfo->DmaOvlapOne, res1);

      dmaOffset += dmaSize;
    }

    if( (res1 != SV_OK) || (res2 != SV_OK) ) {
      printf("playerExecute(%d): sv_render_dma failed! res: %d '%s'\n", pThreadInfo->id, res1?res1:res2, sv_geterrortext(res1?res1:res2));
      running = FALSE;
    }
  }

    // get current time for analyses
  pThreadInfo->time.endDmaTransfer = getCurrentTime(pSettings);

  // we are done with the dma. 
  // now we can increment dma count
  if(running) {
    endSync(&gtDmaSync);
  }

  return running;
}


/*
// Helper function to render the current frame.
// The following sv_render_push_XXX functions show how the render stack can be prepared.
//
// A stereo image is generated by two render passes from two seperate source buffers into 
// one destination buffer. The different stereo options are configured by different
// destination buffer layouts for the render passes set by sv_render_realloc().
*/
int playerRender(threadInfo_t * pThreadInfo, sv_render_image **ppSrcRenderImage, 
                 sv_render_image **ppDestRenderImage, frameInfo_t * pframeInfo)
{ 
  int res              = SV_OK;
  int running          = TRUE;
  int bSetScaler       = FALSE;
  int bModifiedDestBuf = FALSE;
  int frameNo          = pframeInfo->frameNo;

  settings_t * pSettings       = pThreadInfo->pSettings;
  sv_render_handle  * pSvRh    = pSettings->pSvRh;
  sv_render_context * pSvRcOne = pThreadInfo->pSvRcOne;
  sv_render_context * pSvRcTwo = pThreadInfo->pSvRcTwo;

  sv_render_scaler scale;
  frameInfo_t      tempFrameInfo;
  memset(&scale, 0, sizeof(sv_render_scaler));
  memcpy(&tempFrameInfo, pframeInfo, sizeof(frameInfo_t));

  
  // synchronize the render path
  running = startSync(pSettings, &gtRenderSync, frameNo, pThreadInfo->id);

  // get current time for analyses
  pThreadInfo->time.startRenderProcess = getCurrentTime(pSettings);

  /*
  // Push the source image data on the stack.
  // note: At the moment we can push only one image on the stack.
  //       This might change in future SDK versions.
  */
  if (running) {
    res = sv_render_push_image(pSettings->pSv, pSvRh, pSvRcOne, ppSrcRenderImage[0]);
    if(res != SV_OK) {
      printf("playerRender(%d): sv_render_push_image() failed! res: %d '%s'\n", pThreadInfo->id, res, sv_geterrortext(res));
      running = FALSE;
    }
  }
  
  // setup the different stereo render formats
  if(running) {
    switch(pSettings->OutputMode) {
      case STEREO_OUTPUTMODE_LEFT_RIGHT:
        // left image
        tempFrameInfo.xsize      = pframeInfo->xsize/2;
        // lineoffset and dataoffset not changed
        res = initRenderImage(pThreadInfo,  &ppDestRenderImage[0], &tempFrameInfo);
        bSetScaler = bModifiedDestBuf = TRUE;
        break;
      case STEREO_OUTPUTMODE_TOP_BOTTOM:
        // top image
        tempFrameInfo.ysize      = pframeInfo->ysize/2;
        // lineoffset and dataoffset not changed
        res = initRenderImage(pThreadInfo,  &ppDestRenderImage[0], &tempFrameInfo);      
        bSetScaler = bModifiedDestBuf = TRUE;
        break;
      case STEREO_OUTPUTMODE_EVEN_ODD_LINE:
        // left image stored in odd lines
        tempFrameInfo.ysize      = pframeInfo->ysize/2;
        tempFrameInfo.lineoffset = pframeInfo->lineoffset * 2;  // virtual line is twice as long
        tempFrameInfo.dataoffset = pframeInfo->lineoffset;      // take odd lines
        res = initRenderImage(pThreadInfo,  &ppDestRenderImage[0], &tempFrameInfo);
        bSetScaler = bModifiedDestBuf = TRUE;
        break;
      default:
        // do not modify the destination buffer
        // use auto scaling
        break; 
    }
    if(res != SV_OK) {
      printf("playerRender(%d): sv_render_realloc() failed! res: %d '%s'\n", pThreadInfo->id, res, sv_geterrortext(res));
      running = FALSE;
    }
    if(bSetScaler) {
      scale.v1.xsize = tempFrameInfo.xsize;
      scale.v1.ysize = tempFrameInfo.ysize;
      res = sv_render_push_scaler (pSettings->pSv, pSvRh, pSvRcOne, ppDestRenderImage[0], 
                                   1, sizeof(sv_render_scaler), &scale);
      if(res != SV_OK) {
        printf("playerRender(%d): sv_render_push_scaler() failed! res: %d '%s'\n", pThreadInfo->id, res, sv_geterrortext(res));
        running = FALSE;
      }
    }
  }
  
  // Push a render operator on the stack for the preceding image and the preceding setting(s).
  if (running) {
    res = sv_render_push_render(pSettings->pSv, pSvRh, pSvRcOne, ppDestRenderImage[0]);
    if(res != SV_OK) {
      printf("playerRender(%d): sv_render_push_render() failed! res: %d '%s'\n", pThreadInfo->id, res, sv_geterrortext(res));
      running = FALSE;
    }
  }

  /*
  // Start the render operation.
  // note: This example does use parameter 'poverlapped' which means
  //       that sv_render_issue() is non blocking. 
  //       so waiting with sv_render_ready() is required.
  */
  if (running) {
    res = sv_render_issue(pSettings->pSv, pSvRh, pSvRcOne, &pThreadInfo->RenOvlapOne);
    if( (res != SV_OK) && (res != SV_ACTIVE) ) {
      printf("playerRender(%d): sv_render_issue() failed! res: %d '%s'\n", pThreadInfo->id, res, sv_geterrortext(res));
      running = FALSE;
    }
  }

  // Reset the render context for the next frame that shall be rendered.
  // Do this even if an error happenend because then the render context is in a valid state again.
  res = sv_render_reuse(pSettings->pSv, pSvRh, pSvRcOne);
  if(res != SV_OK) {
    printf("playerRender(%d): sv_render_reuse() failed! res: %d '%s'\n", pThreadInfo->id, res, sv_geterrortext(res));
    running = FALSE;
  }
  
  // second render context needed?
  if(pSettings->bSecondSource) {
    // Push the second source image data on the stack.
    if(running) {
      res = sv_render_push_image(pSettings->pSv, pSvRh, pSvRcTwo, ppSrcRenderImage[1]);
      if(res != SV_OK) {
        printf("playerRender(%d): sv_render_push_image() failed! res: %d '%s'\n", pThreadInfo->id, res, sv_geterrortext(res));
        running = FALSE;
      }
    }

    if(running) {
      switch(pSettings->OutputMode) {
        case STEREO_OUTPUTMODE_LEFT_RIGHT:
          // right image
          tempFrameInfo.xsize      = pframeInfo->xsize/2;
          // tempFrameInfo.lineoffset // not changed
          tempFrameInfo.dataoffset = (tempFrameInfo.xsize * pframeInfo->pixelsize_num) / pframeInfo->pixelsize_denom;
          res = initRenderImage(pThreadInfo,  &ppDestRenderImage[0], &tempFrameInfo);
          bSetScaler = bModifiedDestBuf = TRUE;
          break;
        case STEREO_OUTPUTMODE_TOP_BOTTOM:
          // bottom image
          tempFrameInfo.ysize      = pframeInfo->ysize/2;
          // tempFrameInfo.lineoffset // not changed
          tempFrameInfo.dataoffset = tempFrameInfo.ysize * tempFrameInfo.lineoffset;
          res = initRenderImage(pThreadInfo,  &ppDestRenderImage[0], &tempFrameInfo);
          bSetScaler = bModifiedDestBuf = TRUE;
          break;
        case STEREO_OUTPUTMODE_EVEN_ODD_LINE:
          // right image is stored in even lines
          tempFrameInfo.ysize      = pframeInfo->ysize/2;
          tempFrameInfo.lineoffset = pframeInfo->lineoffset * 2;  // virtual line is twice as long
          tempFrameInfo.dataoffset = 0;
          res = initRenderImage(pThreadInfo,  &ppDestRenderImage[0], &tempFrameInfo);      
          bSetScaler = bModifiedDestBuf = TRUE;
          break;
        default:
          break; // do nothing in this case
      }
      if(res != SV_OK) {
        printf("playerRender(%d): sv_render_realloc() failed! res: %d '%s'\n", pThreadInfo->id, res, sv_geterrortext(res));
        running = FALSE;
      }
      
      // bScaler is only set for output options on output jack A
      if(bSetScaler && !pSettings->bSecondOutput) {
        scale.v1.xsize = tempFrameInfo.xsize;
        scale.v1.ysize = tempFrameInfo.ysize;
        res = sv_render_push_scaler (pSettings->pSv, pSvRh, pSvRcTwo, ppDestRenderImage[0], 
                                     1, sizeof(sv_render_scaler), &scale);
        if(res != SV_OK) {
          printf("playerRender(%d): sv_render_push_scaler() failed! res: %d '%s'\n", pThreadInfo->id, res, sv_geterrortext(res));
          running = FALSE;
        }
      }
    }

    // Push a render operator on the stack for the preceding image and the preceding setting(s).
    if(running) {
      if(pSettings->bSecondOutput) {
        res = sv_render_push_render(pSettings->pSv, pSvRh, pSvRcTwo, ppDestRenderImage[1]);
      }else{
        res = sv_render_push_render(pSettings->pSv, pSvRh, pSvRcTwo, ppDestRenderImage[0]);
      }
      if(res != SV_OK) {
        printf("playerRender(%d): sv_render_push_render() failed! res: %d '%s'\n", pThreadInfo->id, res, sv_geterrortext(res));
        running = FALSE;
      }
    }
  
    // Start the second render operation.
    if(running) {
      res = sv_render_issue(pSettings->pSv, pSvRh, pSvRcTwo, &pThreadInfo->RenOvlapTwo);
      if( (res != SV_OK) && (res != SV_ACTIVE) ) {
        printf("playerRender(%d): sv_render_issue() failed! res: %d '%s'\n", pThreadInfo->id, res, sv_geterrortext(res));
        running = FALSE;
      }
    }
    
    // Reset the render context for the next frame that shall be rendered.
    // Do this even if an error happenend because then the render context is in a valid state again.
    res = sv_render_reuse(pSettings->pSv, pSvRh, pSvRcTwo);
    if(res != SV_OK) {
      printf("playerRender(%d): sv_render_reuse() failed! res: %d '%s'\n", pThreadInfo->id, res, sv_geterrortext(res));
      running = FALSE;
    }    
  }

  // buffer only modified on output options for jack A
  if(running && bModifiedDestBuf && !pSettings->bSecondOutput) {
    // reset the destination buffer th the original setup
    res = initRenderImage(pThreadInfo,  &ppDestRenderImage[0], pframeInfo);
  }

  // we are done manipulating the render context. 
  // now we can increment render frame count
  endSync(&gtRenderSync);
  
  
  // wait for the the end of the render process
  if(running && pSettings->bSecondSource) {
    // Wait for the render context to finish
    res = sv_render_ready(pSettings->pSv, pSvRh, pSvRcTwo, 0, &pThreadInfo->RenOvlapTwo);
    if(res != SV_OK) {
      printf("playerRender(%d): sv_render_reuse() failed! res: %d '%s'\n", pThreadInfo->id, res, sv_geterrortext(res));
      running = FALSE;
    }  
  }

  if(running) {
    // Wait for the render context to finish
    res = sv_render_ready(pSettings->pSv, pSvRh, pSvRcOne, 0, &pThreadInfo->RenOvlapOne);
    if(res != SV_OK) {
      printf("playerRender(%d): sv_render_reuse() failed! res: %d '%s'\n", pThreadInfo->id, res, sv_geterrortext(res));
      running = FALSE;
    }  
  }

  // get current time for analyses
  pThreadInfo->time.endRenderProcess = getCurrentTime(pSettings);

  return running;
 }   
    
    
 /*
// Helper function to display the current frame.
// The following sv_fifo_XXX functions show how the render image is output via FIFO API.
// The render image buffer is put directly to the FIFO API without transfering it to the 
// system memory and back to the video board.
*/
int playerDisplay(threadInfo_t * pThreadInfo, sv_render_image **ppDestRenderImage, int frameNo)
{    
  int res        = SV_OK;
  int running    = TRUE;
  uint64 displayTimeJackA = 0;
  uint64 displayTimeJackB = 0;

  settings_t * pSettings = pThreadInfo->pSettings;
  
  sv_fifo_buffer   * pFifoBuffer;
  sv_fifo_info       fInfo[2];
  sv_fifo_bufferinfo destInfo[2];
  memset(destInfo, 0, 2*sizeof(sv_fifo_bufferinfo));
  memset(fInfo, 0, 2*sizeof(sv_fifo_info));

  // synchronize the display path
  running = startSync(pSettings, &gtDisplaySync, frameNo, pThreadInfo->id);

  // get current time for analyses after sv_fifo_getbuffer() because of blocking if fifo full

  /*
  //  control the fill level of the output fifo
  */
  do {
    res = sv_fifo_status(pSettings->pSv, pSettings->pFifoJackA, &fInfo[0]);
    if(res != SV_OK) {
      printf("playerDisplay: sv_fifo_status() res=%d '%s'\n", res, sv_geterrortext(res));
      running = FALSE; 
    }

    if((fInfo[0].nbuffers - fInfo[0].availbuffers) > pSettings->fifoDepth) {
      res = sv_fifo_vsyncwait(pSettings->pSv, pSettings->pFifoJackA);
      if(res != SV_OK)  {
        printf("sv_fifo_vsyncwait(dst) failed %d '%s'\n", res, sv_geterrortext(res));
        running = FALSE;
      }
    }
  } while(gRunning && ((fInfo[0].nbuffers - fInfo[0].availbuffers) > pSettings->fifoDepth));  


  if(running) {
    // sv_fifo_getbuffer() call blocks if the output FIFO is full
    res = sv_fifo_getbuffer(pSettings->pSv, pSettings->pFifoJackA, &pFifoBuffer, 0, pSettings->fifoFlags);
    if(res != SV_OK) {
      printf("playerDisplay(%d): sv_fifo_getbuffer(jackA) failed! res: %d '%s'\n", pThreadInfo->id, res, sv_geterrortext(res));
      running = FALSE;
    }

    // get current time for analyses
    pThreadInfo->time.startFifoOutput = getCurrentTime(pSettings);
    
    // Put the render image into the pFifoBuffer structure
    pFifoBuffer->dma.addr =     (char*)ppDestRenderImage[0]->bufferoffset;
    pFifoBuffer->dma.size =            ppDestRenderImage[0]->buffersize;
    // If set, an automatic freeing of the internal render buffer will be performed,
    // otherwise it won't:
    pFifoBuffer->storage.bufferid    = ppDestRenderImage[0]->bufferid;
    pFifoBuffer->storage.xsize       = ppDestRenderImage[0]->xsize;
    pFifoBuffer->storage.ysize       = ppDestRenderImage[0]->ysize;
    pFifoBuffer->storage.storagemode = (pSettings->fifoFlags & SV_FIFO_FLAG_FIELD) ? (ppDestRenderImage[0]->storagemode & ~SV_MODE_STORAGE_FRAME) : ppDestRenderImage[0]->storagemode; // handle as field as field
    pFifoBuffer->storage.matrixtype  = ppDestRenderImage[0]->matrixtype;
    pFifoBuffer->storage.lineoffset  = ppDestRenderImage[0]->lineoffset;
    pFifoBuffer->storage.dataoffset  = ppDestRenderImage[0]->dataoffset;

    // Transfer the rendered image to the FIFO API
    res = sv_fifo_putbuffer(pSettings->pSv, pSettings->pFifoJackA, pFifoBuffer, &destInfo[0]);
    displayTimeJackA = (((uint64)destInfo[0].clock_high)<<32) + (uint64)destInfo[0].clock_low;
    if(res != SV_OK) {
      printf("playerDisplay(%d): sv_fifo_putbuffer(jackA) failed! res: %d '%s'\n", pThreadInfo->id, res, sv_geterrortext(res));
      running = FALSE;
    }
  }

  if(running && pSettings->bSecondOutput) {
    // sv_fifo_getbuffer() call blocks if the output FIFO is full during display
    res = sv_fifo_getbuffer(pSettings->pSv, pSettings->pFifoJackB, &pFifoBuffer, 0, pSettings->fifoFlags);
    if(res != SV_OK) {
      printf("playerDisplay(%d): sv_fifo_getbuffer(jackB) failed! res: %d '%s'\n", pThreadInfo->id, res, sv_geterrortext(res));
      running = FALSE;
    }

    // Put the render image into the pFifoBuffer structure
    pFifoBuffer->dma.addr =     (char*)ppDestRenderImage[1]->bufferoffset;
    pFifoBuffer->dma.size =            ppDestRenderImage[1]->buffersize;
    // If set, an automatic freeing of the internal render buffer will be performed,
    // otherwise it won't:
    pFifoBuffer->storage.bufferid    = ppDestRenderImage[1]->bufferid;
    pFifoBuffer->storage.xsize       = ppDestRenderImage[1]->xsize;
    pFifoBuffer->storage.ysize       = ppDestRenderImage[1]->ysize;
    pFifoBuffer->storage.storagemode = (pSettings->fifoFlags & SV_FIFO_FLAG_FIELD) ? (ppDestRenderImage[1]->storagemode & ~SV_MODE_STORAGE_FRAME) : ppDestRenderImage[1]->storagemode; // handle as field as field
    pFifoBuffer->storage.matrixtype  = ppDestRenderImage[1]->matrixtype;
    pFifoBuffer->storage.lineoffset  = ppDestRenderImage[1]->lineoffset;
    pFifoBuffer->storage.dataoffset  = ppDestRenderImage[1]->dataoffset;

    // Transfer the rendered image to the FIFO API
    res = sv_fifo_putbuffer(pSettings->pSv, pSettings->pFifoJackB, pFifoBuffer, &destInfo[1]);
    displayTimeJackB = (((uint64)destInfo[1].clock_high)<<32) + (uint64)destInfo[1].clock_low;
    if(res != SV_OK) {
      printf("playerDisplay(%d): sv_fifo_putbuffer(jackB) failed! res: %d '%s'\n", pThreadInfo->id, res, sv_geterrortext(res));
      running = FALSE;
    }

    if(running) {
      // Check if both fifos have same tick
      if(abs(displayTimeJackA-displayTimeJackB) > 10) {
        printf("playerDisplay(%d): Output Fifo is running asynchronous! (JackA: %d, JackB: %d)\n", pThreadInfo->id, displayTimeJackA, displayTimeJackB);
        running = FALSE;        
      }
    }
  }

  // we are done manipulating the fifo. 
  // now we can increment display frame count
  endSync(&gtDisplaySync);

  // get current time for analyses
  if(displayTimeJackA) {
    pThreadInfo->time.endTime   = displayTimeJackA;
  } else {
    pThreadInfo->time.startTime = 0;
    pThreadInfo->time.endTime   = 0;
  }

  if(running) {
    // save the dropped information with a mutex
    dvs_mutex_enter(&gDroppedLock);
      // status of jack A
      res = sv_fifo_status(pSettings->pSv, pSettings->pFifoJackA, &fInfo[0]);
      if(res != SV_OK) {
          printf("playerDisplay: sv_fifo_status(jackA) res=%d '%s'\n", res, sv_geterrortext(res));
          running = FALSE; 
      }
      if(running && pSettings->bSecondOutput) {
        // status of jack B
        res = sv_fifo_status(pSettings->pSv, pSettings->pFifoJackB, &fInfo[1]);
        if(res != SV_OK) {
            printf("playerDisplay: sv_fifo_status(jackB) res=%d '%s'\n", res, sv_geterrortext(res));
            running = FALSE; 
        }
      }
      if( pSettings->verbose || (fInfo[0].dropped+fInfo[1].dropped)>gDropped ) {
        gDropped = fInfo[0].dropped+fInfo[1].dropped;
        printf("o: avail %02d drop %02d  nbuf %02d  tick %06d  delay %d\n", 
                fInfo[0].availbuffers, gDropped, fInfo[0].nbuffers, 
                fInfo[0].tick, destInfo[0].when - fInfo[0].tick);
      }
    dvs_mutex_leave(&gDroppedLock);
       
    /*
    // Start of output determined by jack A
    */
    dvs_mutex_enter(&gFifoLock);
    if(!gbFifoRunning && ((fInfo[0].nbuffers-fInfo[0].availbuffers) > 0) ) {
      if(pSettings->verbose) {
        printf("playerDisplay: sv_fifo_start()\n");
      }
      // Start the fifo jack A
      res = sv_fifo_start(pSettings->pSv, pSettings->pFifoJackA);
      if(res != SV_OK) {
        printf("playerDisplay: sv_fifo_start() failed = %d '%s'\n", res, sv_geterrortext(res));
        running = FALSE;
      }
      if(pSettings->bSecondOutput) {
        // Start the fifo
        res = sv_fifo_start(pSettings->pSv, pSettings->pFifoJackB);
        if(res != SV_OK) {
          printf("playerDisplay: sv_fifo_start() failed = %d '%s'\n", res, sv_geterrortext(res));
          running = FALSE;
        }
      }
      gbFifoRunning = TRUE;
    }
    dvs_mutex_leave(&gFifoLock);
  }

  // get current time for analyses
  pThreadInfo->time.endFifoOutput = getCurrentTime(pSettings);
  
  return running;
}   
 
/*
// This is the execute function of the player example program used by each thread.
*/
int playerExecute(threadInfo_t * pThreadInfo, videoBuffer_t * pSourceBuffer, frameInfo_t * pframeInfo)
{
  int res             = SV_OK;
  int running         = TRUE;
  int frameNo         = pframeInfo->frameNo;
  
  sv_render_image           * ppSrcRenderImage[2] = {0}, * ppDestRenderImage[2] = {0};
  sv_storageinfo              outputInfo          = {0};
  frameInfo_t                 destFrameInfo       = {0};
 
  settings_t * pSettings    = pThreadInfo->pSettings;
  sv_render_handle  * pSvRh = pSettings->pSvRh;

  // synchronize the malloc
  running = startSync(pSettings, &gtMallocSync, frameNo, pThreadInfo->id);

  if(pSettings->verbose) {
    printf("playerExecute(%d): Start rendering frame %d\n", pThreadInfo->id, frameNo);
  }

  if(running) {
    // malloc source render image
    res = initRenderImage(pThreadInfo,  &ppSrcRenderImage[0], pframeInfo);
    if(res != SV_OK) {
      printf("playerExecute(%d): sv_render_malloc(src) failed! res: %d '%s'\n", pThreadInfo->id, res, sv_geterrortext(res));
      running = FALSE;
    }
  } 
  
  // allocate sv_render_image for second source render image if needed.
  if( (running) && (pSettings->bSecondSource) ) {
    // same frame info as source one
    res = initRenderImage(pThreadInfo,  &ppSrcRenderImage[1], pframeInfo);
    if(res != SV_OK) {
      printf("playerExecute(%d): sv_render_malloc(src) failed! res: %d '%s'\n", pThreadInfo->id, res, sv_geterrortext(res));
      running = FALSE;
    }
  }

  if(running) {
    // get the actual output settings
    res = sv_storage_status(pSettings->pSv, 0, NULL, &outputInfo, sizeof(outputInfo), SV_STORAGEINFO_COOKIEISJACK);
    if(res != SV_OK) {
      printf("playerExecute(%d): sv_storage_status failed! res: %d '%s'\n", pThreadInfo->id, res, sv_geterrortext(res));
      running = FALSE;
    }
  }

  if(running) {
    // Set the properties of the destination buffer's layout.
    destFrameInfo.xsize           = outputInfo.storagexsize;
    destFrameInfo.ysize           = (pSettings->fifoFlags & SV_FIFO_FLAG_FIELD) ? (outputInfo.storageysize / 2) : outputInfo.storageysize;
    destFrameInfo.storagemode     = (pSettings->fifoFlags & SV_FIFO_FLAG_FIELD) ? (outputInfo.videomode | SV_MODE_STORAGE_FRAME) : outputInfo.videomode; // handle field as frame
    destFrameInfo.pixelsize_num   = outputInfo.pixelsize_num;
    destFrameInfo.pixelsize_denom = outputInfo.pixelsize_denom;
    destFrameInfo.lineoffset      = (destFrameInfo.xsize * destFrameInfo.pixelsize_num) / destFrameInfo.pixelsize_denom;
    destFrameInfo.dataoffset      = 0;
    destFrameInfo.frameNo         = frameNo;
    res = initRenderImage(pThreadInfo,  &ppDestRenderImage[0], &destFrameInfo);
    if(res != SV_OK) {
      printf("playerExecute(%d): sv_render_malloc(dest1) failed! res: %d '%s'\n", pThreadInfo->id, res, sv_geterrortext(res));
      running = FALSE;
    }
  }

  if(running && pSettings->bSecondOutput ) {
    // jack B properties of the destination buffer's layout are the same as jack A
    res = initRenderImage(pThreadInfo,  &ppDestRenderImage[1], &destFrameInfo);
    if(res != SV_OK) {
      printf("playerExecute(%d): sv_render_malloc(dest2) failed! res: %d '%s'\n", pThreadInfo->id, res, sv_geterrortext(res));
      running = FALSE;
    }
  }

  if(running) {
    endSync(&gtMallocSync);
  }

  /*
  // For the low latency timing the image will be pushed to the output fifo before the rendering process
  */
  if(running && pSettings->bLowLatency) {
    running = playerDisplay(pThreadInfo, ppDestRenderImage, frameNo);
  }

  /*
  // Transfer images from the system memory to the video board.
  */
  if(running) {
    res = playerDMA(pThreadInfo, pSourceBuffer,ppSrcRenderImage,  pframeInfo->size, frameNo);
  }

  /*
  // Render the frames depending the on stereo option
  */
  if(running) {
    running = playerRender(pThreadInfo, ppSrcRenderImage, ppDestRenderImage, &destFrameInfo);
  }

  /*
  // For normal timing the image is pushed to the output fifo after the rendering
  // For the low latency timing make a safety check for the processing timing.
  */
  if(running && !pSettings->bLowLatency) {
    running = playerDisplay(pThreadInfo, ppDestRenderImage, frameNo);
  } else {
    // Check if frame timing failed
    int delta = (int)(pThreadInfo->time.endRenderProcess - pThreadInfo->time.endTime);
    if( (pThreadInfo->time.endTime) && (delta > 0) ) {
      printf("playerExecute(%d): Frame timing failed! delta[us]: %d\n", pThreadInfo->id, delta);
    }
  }
  
  /* Clean up*/

  // Free the allocated source buffer on the DVS video device to make it available again.
  if (ppSrcRenderImage[0]) {
    res = sv_render_free(pSettings->pSv, pSvRh, ppSrcRenderImage[0]);
    if(res != SV_OK) {
      printf("playerExecute(%d): sv_render_free() failed! res: %d '%s'\n", pThreadInfo->id, res, sv_geterrortext(res));
      running = FALSE;
    }
  }
  if (ppSrcRenderImage[1]) {
    res = sv_render_free(pSettings->pSv, pSvRh, ppSrcRenderImage[1]);
    if(res != SV_OK) {
      printf("playerExecute(%d): sv_render_free() failed! res: %d '%s'\n", pThreadInfo->id, res, sv_geterrortext(res));
      running = FALSE;
    }
  }
  
  /*
  // Free the allocated destination buffer on the DVS video device.
  // By resetting the 'bufferid' value to 0 the function sv_render_free() will only free
  // the internal structure. The virtual destination buffer in the DRAM will be freed
  // by the FIFO Api.
  */
  if (ppDestRenderImage[0]) { // first dest buffer
    ppDestRenderImage[0]->bufferid = ppDestRenderImage[0]->bufferoffset = ppDestRenderImage[0]->buffersize   = 0;
    res = sv_render_free(pSettings->pSv, pSvRh, ppDestRenderImage[0]);
      if(res != SV_OK) {
        printf("playerExecute(%d): sv_render_free() failed! res: %d '%s'\n", pThreadInfo->id, res, sv_geterrortext(res));
        running = FALSE;
      }
  }
  if (ppDestRenderImage[1]) {  // second dest buffer
    ppDestRenderImage[1]->bufferid = ppDestRenderImage[1]->bufferoffset = ppDestRenderImage[1]->buffersize   = 0;
    res = sv_render_free(pSettings->pSv, pSvRh, ppDestRenderImage[1]);
      if(res != SV_OK) {
        printf("playerExecute(%d): sv_render_free() failed! res: %d '%s'\n", pThreadInfo->id, res, sv_geterrortext(res));
        running = FALSE;
      }
  }
  
  if(pSettings->verbose) {
    printf("playerExecute(%d): Finished rendering frame %d\n", pThreadInfo->id, frameNo);
  }

  return running;
}

/*
// This is the stereo_player thread.
// load one or two images into the system memory and start the processing
*/
void * playerThread(void * p)
{	
  int res       = SV_OK;
  int running   = TRUE;
  int alignment = 0;
 
  threadInfo_t     * pThreadInfo = (threadInfo_t *) p;
  settings_t       * pSettings   = pThreadInfo->pSettings;
  sv_handle        * pSv         = pSettings->pSv;
  sv_render_handle * pSvRh       = pSettings->pSvRh;

  frameInfo_t   frameInfo[2];
  videoBuffer_t source[2];
  memset(frameInfo, 0, 2*sizeof(frameInfo_t));
  memset(source, 0, 2*sizeof(videoBuffer_t));

  if(pSettings->verbose) {
    printf("playerThread(%d): Starting\n", pThreadInfo->id);
  }

  // Query the DMA alignment of the DVS video card.
  res = sv_query(pSv, SV_QUERY_DMAALIGNMENT, 0, &alignment);
  if(res != SV_OK) {
    printf("playerThread(%d): sv_query(SV_QUERY_DMAALIGNMENT) failed = %d '%s'\n", pThreadInfo->id, res, sv_geterrortext(res));
    running = FALSE;
  }

  // Allocate source buffer in system memory
  if(running) {
    running = allocateAlignedBuffer(&source[0], MAX_BUFFERSIZE, alignment);
  }
  // second source buffer needed?
  if( (running) && (pSettings->bSecondSource) ) {
    running = allocateAlignedBuffer(&source[1], MAX_BUFFERSIZE, alignment);
  }

  if(running) {
    // Create a render context.
    res = sv_render_begin(pSv, pSvRh, &pThreadInfo->pSvRcOne);
    if(!pThreadInfo->pSvRcOne || (res != SV_OK)) {
      printf("playerThread(%d): sv_render_begin(pSvRcOne) failed! res: %d '%s'\n", pThreadInfo->id, res, sv_geterrortext(res));
      pThreadInfo->pSvRcOne = NULL;
      running = FALSE;
    }
  }
  if(running && pSettings->bSecondSource) {
    // Create a second render context.
    res = sv_render_begin(pSv, pSvRh, &pThreadInfo->pSvRcTwo);
    if(!pThreadInfo->pSvRcTwo || (res != SV_OK)) {
      printf("playerThread(%d): sv_render_begin(pSvRcTwo) failed! res: %d '%s'\n", pThreadInfo->id, res, sv_geterrortext(res));
      pThreadInfo->pSvRcTwo = NULL;
      running = FALSE;
    }
  }

  if(running) {
    // Send message that thread is running.
    dvs_cond_broadcast(&pThreadInfo->running, &gRunningLock, FALSE);

    do {
      // Get the next frame number for this thread.
      frameInfo[0].frameNo = frameInfo[1].frameNo = getNextFrame();
      if(pSettings->OutputMode == STEREO_OUTPUTMODE_EVEN_ODD_FRAME) {
        frameInfo[1].frameNo++;
      }
  
      // Do the rendering.
      if(frameInfo[0].frameNo < pSettings->frameCount) {
        
        // get current time for analyses
        pThreadInfo->time.startTime = getCurrentTime(pSettings);
        
        // synchronize the file read
        running = startSync(pSettings, &gtReadFileSync, frameInfo[0].frameNo, pThreadInfo->id);

        // get current time for analyses
        pThreadInfo->time.startFileRead = getCurrentTime(pSettings);

        // read image
        if(running) {
          running = readframe(pThreadInfo, &source[0], &frameInfo[0], 0, frameInfo[0].frameNo);
        }

        if(running && pSettings->bSecondSource) {
          // read image
          running &= readframe(pThreadInfo, &source[1], &frameInfo[1], 1, frameInfo[1].frameNo);

          // source image one and source image two have the same params?
          if( (frameInfo[0].dataoffset      != frameInfo[1].dataoffset) ||
              (frameInfo[0].xsize           != frameInfo[1].xsize)  ||
              (frameInfo[0].ysize           != frameInfo[1].ysize)  ||
              ((frameInfo[0].dpxType & 0xff)!= (frameInfo[1].dpxType & 0xff))||  // check only low byte
              (frameInfo[0].nbits           != frameInfo[1].nbits)     ) {
            
            printf("playerThread(%d): dpx_source_one and dpx_source_two need equal format!!\n", pThreadInfo->id);
            running = FALSE;
          }
        }

        // get current time for analyses
        pThreadInfo->time.endFileRead = getCurrentTime(pSettings);

        // we are done reading the files. 
        // now we can increment read file count
        endSync(&gtReadFileSync);

        if(running) {
          running = playerExecute(pThreadInfo, source, &frameInfo[0]);
        }

        timingAnalysesThread(pThreadInfo, frameInfo[0].frameNo);
      }
    } while(gRunning && running && (frameInfo[0].frameNo < pSettings->frameCount));
  }

  // Clean-up
  if(source[0].pBuffer) {
    free(source[0].pBuffer);
  }
  if(source[1].pBuffer) {
    free(source[1].pBuffer);
  }

  // Close the render context again.
  if(pThreadInfo->pSvRcOne) {
    res = sv_render_end(pSv, pSvRh, pThreadInfo->pSvRcOne);
    if(res != SV_OK) {
      printf("playerThread(%d): sv_render_end(pSvRcOne) failed! res: %d '%s'\n", pThreadInfo->id, res, sv_geterrortext(res));
    }
  }
  if(pThreadInfo->pSvRcTwo) {
    res = sv_render_end(pSv, pSvRh, pThreadInfo->pSvRcTwo);
    if(res != SV_OK) {
      printf("playerThread(%d): sv_render_end(pSvRcTwo) failed! res: %d '%s'\n", pThreadInfo->id, res, sv_geterrortext(res));
    }
  }

  if(pSettings->verbose) {
    printf("playerThread(%d): Finished\n", pThreadInfo->id);
  }

  pThreadInfo->result = !running;
  dvs_thread_exit(&pThreadInfo->result, &pThreadInfo->exit);
  return 0;
}