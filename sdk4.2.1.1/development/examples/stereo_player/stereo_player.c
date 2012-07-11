/*
//    Part of the DVS SDK (http://www.dvs.de)
//
//    stereo_player - Demonstrates the usage of the Render and FIFO API.
//
*/
#include "../common/dpxformat.h"
#include "stereo_player.h"
#include "tags.h"

extern dvs_mutex gRunningLock;
extern int gRunning;

/*
// helper function to read frame
*/
int readframe(threadInfo_t * pThreadInfo, videoBuffer_t * pbuffer, frameInfo_t * pframeInfo, int src, int framenr)
{
  int res     = SV_OK;
  int running = TRUE;
  int bDummy  = TRUE;
  int size    = 0;
  char *pfilename              = NULL;
  char filename[MAX_PATHNAME];
  sv_storageinfo outputInfo    = {0};
  settings_t * pSettings       = pThreadInfo->pSettings;

  switch(src) {
  case 0:
    pfilename = pSettings->SrcOneFileName;
    bDummy = pSettings->bDummySrcOne;
    break;
  case 1:
    pfilename = pSettings->SrcTwoFileName;
    bDummy = pSettings->bDummySrcTwo;
    break;
  default:
    running = FALSE;
  }

  if(running && bDummy) {
    // get the actual output settings
    res = sv_storage_status(pSettings->pSv, 0, NULL, &outputInfo, sizeof(outputInfo), SV_STORAGEINFO_COOKIEISJACK);
    if(res != SV_OK) {
      printf("readframe: sv_storage_status failed! res: %d '%s'\n", res, sv_geterrortext(res));
      running = FALSE; 
    }

    pframeInfo->dataoffset  = 0;
    pframeInfo->xsize       = outputInfo.xsize;
    pframeInfo->ysize       = (pSettings->fifoFlags & SV_FIFO_FLAG_FIELD) ? (outputInfo.ysize / 2) : outputInfo.ysize;
    pframeInfo->nbits       = outputInfo.nbits;
    pframeInfo->storagemode = (pSettings->fifoFlags & SV_FIFO_FLAG_FIELD) ? (outputInfo.videomode | SV_MODE_STORAGE_FRAME) : outputInfo.videomode; // handle as frame;
    dpxformat_dpxtype(outputInfo.videomode, &pframeInfo->dpxType, &pframeInfo->nbits, NULL);
    pframeInfo->size        = dpxformat_framesize_noheader(pframeInfo->xsize, pframeInfo->ysize, 
                                                           pframeInfo->dpxType, pframeInfo->nbits, NULL);
  }

  if(running && !bDummy) {
    sprintf(filename, pfilename, pSettings->startFrame + framenr);    
    // Read image data and needed properties (e.g. xsize, ysize, dpxtype, nbits, ...) from dpx file.
    size = dpxformat_readframe_noheader(filename, pbuffer->pAligned, pbuffer->bufferSize, 
                                        &pframeInfo->dataoffset, &pframeInfo->xsize, &pframeInfo->ysize,
                                        &pframeInfo->dpxType, &pframeInfo->nbits, NULL);
    if(!size) {
      printf("readframe: dpxformat_readframe_noheader failed! file: '%s', size: %d\n", filename, size);   
      running = FALSE;
    }

    pframeInfo->size        = dpxformat_framesize_noheader(pframeInfo->xsize, pframeInfo->ysize, 
                                                           pframeInfo->dpxType, pframeInfo->nbits, NULL);
    pframeInfo->storagemode = dpxformat_getstoragemode(pframeInfo->dpxType, pframeInfo->nbits);

  }
  return running;
}


/*
// help menu
*/
void help(void)
{
  printf("stereo_player [options] dpx_source_one [dpx_source_two] #start #nframes\n");
  printf("                    dpx_source_<xxx> syntax is printf syntax\n");
  printf("                    The special filename 'dummy' omits disk i/o\n");
  printf(" options:\n");
  printf("         -l          Loop forever\n");
  printf("         -v          Verbose\n");
  printf("\n");
  printf("         -ll         Low latency mode\n");
  printf("         -f=#        FIFO depth (default: 10)\n");
  printf("         -t=#        Number of render threads (default: 10)\n");
  printf("         -o=#        Output option:\n");
  printf("                       0: No stereo, output frames on jack A\n");
  printf("                       1: Output even frames on jack A and odd frames on jack B\n");
  printf("                       2: Output on separate jacks\n");
  printf("                       3: Output on jack A, side by side\n");
  printf("                       4: Output on jack A, on top of each other\n");
  printf("                       5: Output on jack A, line by line (even lines: right, odd line left)\n");
  printf("              Note: mode 3 to 5 will change the aspect ratio of the image\n");
  printf("\n");
  printf("         -a=#        Timing analyses:\n");
  printf("                       0: Output statistics\n");
  printf("                       1: Output mean value on frame basis\n");
  printf("                       2: Output duration on frame basis\n");
  printf("                       3: Output waiting timing on frame basis\n");
  printf("\n");
  printf("         note:\n");
  printf("           - The output will be scaled to the present output format\n");
}


int parseCommandlineParameter(int argc, char ** argv, settings_t *pSettings)
{
  int error = FALSE;
  int i;

  // minimum of four parameter 
  if(argc < 4) {
    return TRUE;
  }

  // set pSettings to zero 
  memset(pSettings, 0, sizeof(settings_t));
  
  // default thread count
  pSettings->threadCount = 10;
  pSettings->fifoDepth   = 10;

  while(argv[1][0] == '-') {
    switch(argv[1][1]) {
    case 'a':
      if(argv[1][2] == '=') {
        pSettings->timingAnalyses = atoi(&argv[1][3]);
      } else {
        error = TRUE;
      }
      break;
    case 'f':
      if(argv[1][2] == '=') {
        pSettings->fifoDepth = atoi(&argv[1][3]);
      } else {
        error = TRUE;
      }
      break;
    case 't':
      if(argv[1][2] == '=') {
        pSettings->threadCount = atoi(&argv[1][3]);
      } else {
        error = TRUE;
      }
      break;
    case 'o':
      if(argv[1][2] == '=') {
        pSettings->OutputMode = atoi(&argv[1][3]);
      } else {
        error = TRUE;
      }
      break;
    case 'v':
      pSettings->verbose = TRUE;
      break;
    case 'l':
      if(argv[1][2] == 'l') {
        pSettings->bLowLatency = TRUE;
      } else {
        pSettings->loopMode = TRUE;
      }
      break;
    default:
      error = TRUE;
    }
    argv++; argc--;
  } // end of while loop

  if(pSettings->verbose) {
    for(i = 0; i < argc; i++) {
      printf("%s ", argv[i]);
    }
    printf("\n");
  }

  if( (argc == 4) && 
      ((pSettings->OutputMode == STEREO_OUTPUTMODE_MONO) || 
       (pSettings->OutputMode == STEREO_OUTPUTMODE_EVEN_ODD_FRAME)) ) {

    // only one source file needed for output mode
    pSettings->bSecondSource = FALSE;
    
    // read source one file name
    strcpy(pSettings->SrcOneFileName, argv[1]);
    argv++; argc--;
    // set source two file name to ""
    pSettings->SrcTwoFileName[0] =0;

    // read start frame and frame count
    pSettings->startFrame = atoi(argv[1]);
    pSettings->frameCount = atoi(argv[2]);
  } else if((argc == 5) && 
      ((pSettings->OutputMode == STEREO_OUTPUTMODE_INDEPENDENT) || 
       (pSettings->OutputMode == STEREO_OUTPUTMODE_LEFT_RIGHT)  ||
       (pSettings->OutputMode == STEREO_OUTPUTMODE_TOP_BOTTOM)  ||
       (pSettings->OutputMode == STEREO_OUTPUTMODE_EVEN_ODD_LINE)) ) {

    // only one source file needed for output mode
    pSettings->bSecondSource = TRUE;
    
    // read source one file name
    strcpy(pSettings->SrcOneFileName, argv[1]);
    argv++; argc--;
    // read source two file name
    strcpy(pSettings->SrcTwoFileName, argv[1]);
    argv++; argc--;

    // read start frame and frame count
    pSettings->startFrame = atoi(argv[1]);
    pSettings->frameCount = atoi(argv[2]);
  } else {
    error = TRUE;
  }

  // one or two outputs needed for current setting?
  if((pSettings->OutputMode == STEREO_OUTPUTMODE_EVEN_ODD_FRAME) || 
     (pSettings->OutputMode == STEREO_OUTPUTMODE_INDEPENDENT)) {
    pSettings->bSecondOutput = TRUE;
  } else {
    pSettings->bSecondOutput = FALSE;
  }

  if( pSettings->OutputMode == STEREO_OUTPUTMODE_EVEN_ODD_FRAME ) {
    // even frame numbers are considered as source one
    // odd  frame numbers are considered as source two
    pSettings->bSecondSource = TRUE;
    strcpy(pSettings->SrcTwoFileName, pSettings->SrcOneFileName);

    // set increment to 2
    setFrameCountInc(2);

    // start frame and frame count need to be even numbers
    if( (pSettings->startFrame%2) || (pSettings->frameCount%2) ) {
      error = TRUE;
    }
  }

  // use of dummy file buffer?
  pSettings->bDummySrcOne = strcmp(pSettings->SrcOneFileName, "dummy") ? FALSE : TRUE;
  pSettings->bDummySrcTwo = strcmp(pSettings->SrcTwoFileName, "dummy") ? FALSE : TRUE;

  return error;
}

/*
// Signal handler
*/
void signalHandler(int signal) 
{
  printf("closing application...\n");
  gRunning = FALSE;
}

/*
// Here are the render threads created.
*/
int startPlayer(settings_t * pSettings)
{
  int res   = SV_OK;
  int running = TRUE;
  int error   = FALSE;
  int mode    = 0;
  int i;
  threadInfo_t * pThreadInfo;
  sv_fifo_memory fifoMemoryMode;
  
  // Allocate some space for each thread's threadInfo_t struct.
  pThreadInfo = (threadInfo_t *) malloc(pSettings->threadCount * sizeof(threadInfo_t));
  memset(pThreadInfo, 0, pSettings->threadCount * sizeof(threadInfo_t));

  // Open a handle to the DVS video device.
  res = sv_openex(&pSettings->pSv, "", SV_OPENPROGRAM_DEMOPROGRAM, SV_OPENTYPE_DEFAULT, 0, 0);
  if(!pSettings->pSv || (res != SV_OK)) {
    printf("startPlayer: sv_openex() failed! res: %d '%s'\n", res, sv_geterrortext(res));
    running = FALSE;
  }

  if(running) {
    // Get the one and only needed/allowed sv_render_handle pointer.
    res = sv_render_open(pSettings->pSv, &pSettings->pSvRh);
    if(!pSettings->pSvRh || (res != SV_OK)) {
      printf("startPlayer: sv_render_open() failed! res: %d '%s'\n", res, sv_geterrortext(res));
      running = FALSE;
    }
  }
  
  if(running) {
    res = sv_jack_query(pSettings->pSv, 0, SV_QUERY_MODE_CURRENT, 0, &mode);
    if(res != SV_OK) {
      printf("startPlayer: sv_jack_query() failed! res: %d '%s'\n", res, sv_geterrortext(res));
      running = FALSE;  
    }
    if(running && !(mode & SV_MODE_STORAGE_FRAME) ) {
      pSettings->fifoFlags |= SV_FIFO_FLAG_FIELD;
    }
  }

  if(running) {
    memset(&fifoMemoryMode, 0, sizeof(fifoMemoryMode));
    fifoMemoryMode.mode = SV_FIFO_MEMORYMODE_FIFO_NONE;
    res = sv_fifo_memorymode(pSettings->pSv, &fifoMemoryMode);
    if(res != SV_OK) {
      printf("startPlayer: sv_fifo_memorymode() failed! res: %d '%s'\n", res, sv_geterrortext(res));
      running = FALSE;
    }
  }

  if(running) {
    // open output fifo for jack A
    res = sv_fifo_init(pSettings->pSv, &pSettings->pFifoJackA, JACK_ID_OUTA, 0, SV_FIFO_DMA_OFF, (SV_FIFO_FLAG_VIDEOONLY | pSettings->fifoFlags), 0);
    if(!pSettings->pFifoJackA || (res != SV_OK)) {
      printf("startPlayer: sv_fifo_init(JACK_ID_OUTA) failed! res: %d '%s'\n", res, sv_geterrortext(res));
      running = FALSE;
    }
  }

  if(running && pSettings->bSecondOutput) {
    // open output fifo for jack B
    res = sv_fifo_init(pSettings->pSv, &pSettings->pFifoJackB, JACK_ID_OUTB, 0, SV_FIFO_DMA_OFF, (SV_FIFO_FLAG_VIDEOONLY | pSettings->fifoFlags), 0);
    if(!pSettings->pFifoJackB || (res != SV_OK)) {
      printf("startPlayer: sv_fifo_init(JACK_ID_OUTB) failed! res: %d '%s'\n", res, sv_geterrortext(res));
      running = FALSE;
    }
  }

  if(running) {
    initThreadMutex();
    
    // Create the working threads.
    for(i = 0; i < pSettings->threadCount; i++) {
      pThreadInfo[i].id        = i;
      pThreadInfo[i].pSettings = pSettings;
      pThreadInfo[i].result    = 0;
      initThreadCond(&pThreadInfo[i]);

      dvs_thread_create(&pThreadInfo[i].handle, playerThread, (void*) &pThreadInfo[i], &pThreadInfo[i].exit);
    
	    // Wait until every thread is running.
      dvs_cond_wait(&pThreadInfo[i].running, &gRunningLock, FALSE);
	}

    // Wait until thread termination and free thread handles.
    for(i = 0; i < pSettings->threadCount; i++) {
      error |= dvs_thread_exitcode(&pThreadInfo[i].handle, &pThreadInfo[i].exit);
      closeThreadCond(&pThreadInfo[i]);
    }
  }

  /*
  // Clean-up.
  */
  closeThreadMutex();
    
  if(pThreadInfo) {
    free(pThreadInfo);
  }
  if(pSettings->pFifoJackA) {
    // wait until all frames are displayed before closing the fifo
    sv_fifo_wait(pSettings->pSv, pSettings->pFifoJackA);
    sv_fifo_free(pSettings->pSv, pSettings->pFifoJackA);
  }
  if(pSettings->pFifoJackB) {
    // wait until all frames are displayed before closing the fifo
    sv_fifo_wait(pSettings->pSv, pSettings->pFifoJackB);
    sv_fifo_free(pSettings->pSv, pSettings->pFifoJackB);
  }
  if(pSettings->pSvRh) {
    sv_render_close(pSettings->pSv, pSettings->pSvRh);
  }
  if(pSettings->pSv) {
    sv_close(pSettings->pSv);
  }
  
  return error;
}


/*
// Main entry function.
*/
int main(int argc, char ** argv)
{
  int error   = FALSE;

  settings_t   gSettings = { 0 };
  settings_t * pSettings = &gSettings;

  signal(SIGTERM, signalHandler);
  signal(SIGINT,  signalHandler);
  
  error = parseCommandlineParameter(argc, argv, pSettings);
  // print help if parsing fails
  if(error) {
    help();
  }

  if(!error) {
     do {
      if(!error) {
        // Reset globals to default values
        initGlobals();

        // Start the rendering.
        error = startPlayer(pSettings);
        if(!error) {
          printStatistics();
        }
      }
    } while(pSettings->loopMode && gRunning && !error);   
  }

  return error;
}
