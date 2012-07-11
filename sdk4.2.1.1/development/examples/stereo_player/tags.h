#ifndef TAGS_H
#define TAGS_H

/* stereo_player.c        */ int  main(int argc, char ** argv);
/* stereo_player.c        */ int  parseCommandlineParameter(int argc, char ** argv, settings_t *pSettings);
/* stereo_player.c        */ void help(void);
/* stereo_player.c        */ int  startPlayer(settings_t * pSettings);
/* stereo_player.c        */ void signalHandler(int signal);
/* stereo_player.c        */ int  readframe(threadInfo_t * pThreadInfo, videoBuffer_t * pbuffer, frameInfo_t * pframeInfo, int src, int framenr);
/* stereo_player_thread.c */ void * playerThread(void * p);
/* stereo_player_thread.c */ int  playerExecute(threadInfo_t * pThreadInfo, videoBuffer_t * pSourceBuffer, frameInfo_t * pframeInfo);
/* stereo_player_thread.c */ int  playerDisplay(threadInfo_t * pThreadInfo, sv_render_image **ppDestRenderImage, int frameNo);
/* stereo_player_thread.c */ int  playerRender(threadInfo_t * pThreadInfo, sv_render_image **ppSrcRenderImage, sv_render_image **ppDestRenderImage, frameInfo_t * pframeInfo);
/* stereo_player_thread.c */ int  playerDMA(threadInfo_t * pThreadInfo, videoBuffer_t * pSourceBuffer, sv_render_image **ppSrcRenderImage, int size, int frameNo);
/* stereo_player_thread.c */ int  initRenderImage(threadInfo_t * pThreadInfo, sv_render_image **ppRenderImage, frameInfo_t * pframeInfo);
/* stereo_player_thread.c */ int  allocateAlignedBuffer(videoBuffer_t * pVideoBuffer, unsigned int size, unsigned int alignment);
/* stereo_player_thread.c */ void closeThreadCond(threadInfo_t * pThreadInfo);
/* stereo_player_thread.c */ int  initThreadCond(threadInfo_t * pThreadInfo);
/* stereo_player_thread.c */ void closeThreadMutex(void);
/* stereo_player_thread.c */ void initThreadMutex(void);
/* stereo_player_thread.c */ void initGlobals(void);
/* stereo_player_thread.c */ void setFrameCountInc(int inc);
/* stereo_player_thread.c */ int  getNextFrame(void);
/* stereo_player_thread.c */ void printStatistics(void);
/* stereo_player_thread.c */ uint64 getCurrentTime(settings_t *pSettings);
/* stereo_player_thread.c */ void timingAnalysesThread(threadInfo_t * pThreadInfo, int frameNo);
/* stereo_player_thread.c */ void calcStatistics(statistic_t *pterm, int value);

#endif
