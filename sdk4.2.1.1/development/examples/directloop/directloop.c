#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>

#include "../../header/dvs_setup.h"
#include "../../header/dvs_clib.h"
#include "../../header/dvs_direct.h"
#include "../../header/dvs_thread.h"

#define NUMBUFFERS  1

#define PATHINPUT   0
#define PATHOUTPUT  1

typedef struct anc_packet_list {
  struct anc_packet_list * next;
  sv_direct_ancpacket packet;
  int valid;
} anc_packet_list_t;

typedef struct {
  // DVS device handles
  sv_handle * sv;
  sv_direct_handle * dh_in;
  sv_direct_handle * dh_out;

  // I/O threads
  int exitcode_in;
  int exitcode_out;
  dvs_cond finish_in;
  dvs_cond finish_out;
  
  // Events
  dvs_cond  record_ready[NUMBUFFERS];
  dvs_mutex record_ready_mutex[NUMBUFFERS];
  dvs_cond  display_ready[NUMBUFFERS];
  dvs_mutex display_ready_mutex[NUMBUFFERS];
  dvs_cond  record_go[NUMBUFFERS];
  dvs_mutex record_go_mutex[NUMBUFFERS];
  dvs_cond  display_go[NUMBUFFERS];
  dvs_mutex display_go_mutex[NUMBUFFERS];

  // Video buffers
  char * nativeBuffers_org[NUMBUFFERS][2];
  char * nativeBuffers[NUMBUFFERS][2];
  int    nativeSize;

  // Timecode buffers
  sv_direct_timecode timecodes[NUMBUFFERS][2];
  anc_packet_list_t ancpackets[NUMBUFFERS][2];

  // Informational and parameters
  sv_storageinfo storageinfo;
  int verbose;
} direct_handle;

void * recordThread(void * arg);
void * displayThread(void * arg);
int anc_record_callback(void * userdata, int bufferindex, sv_direct_ancpacket * inpacket);
int anc_display_callback(void * userdata, int bufferindex, sv_direct_ancpacket * outpacket);
static int running = TRUE;

void signal_handler(int signal)
{
  (void)signal;
  running = FALSE;
}

int initBuffers(direct_handle * handle)
{
  int buffer;
  int i;

  handle->nativeSize = handle->storageinfo.fieldsize[0] + handle->storageinfo.fieldsize[1];

  for (buffer = 0; buffer < NUMBUFFERS; buffer++) {
    for (i = PATHINPUT; i <= PATHOUTPUT; i++) {
      handle->nativeBuffers_org[buffer][i] = (char *) malloc(handle->nativeSize + 4096);
      handle->nativeBuffers[buffer][i] = (char *)(((uintptr)handle->nativeBuffers_org[buffer][i] + 4095) & ~4095);

      // Initialize timecode buffers.
      handle->timecodes[buffer][i].size = sizeof(sv_direct_timecode);
    }
  }

  return TRUE;
}

void cleanupBuffers(direct_handle * handle)
{
  int buffer;
  int i;

  for (buffer = 0; buffer < NUMBUFFERS; buffer++) {
    for (i = PATHINPUT; i <= PATHOUTPUT; i++) {
      free(handle->nativeBuffers_org[buffer][i]);
    }
  }
}

int main(int argc, char *argv[])
{
  direct_handle data = { 0 };
  direct_handle * handle = &data;
  dvs_thread recordThreadHandle;
  dvs_thread displayThreadHandle;
  int numFrames = -1;
  int flags = 0;
  int res = SV_OK;
  int i, buffer, frame;
  int recordCurrent = 0;
  int displayCurrent = 0;

  signal(SIGTERM, signal_handler);
  signal(SIGINT,  signal_handler);

  for(i = 1; i < argc; i++) {
    if(!strcmp(argv[i], "field")) {
      flags = SV_DIRECT_FLAG_FIELD;
    } else if(!strcmp(argv[i], "verbose")) {
      // increase verbose level
      handle->verbose++;
    }
  }

  res = sv_openex(&handle->sv, "", SV_OPENPROGRAM_DEMOPROGRAM, SV_OPENTYPE_DEFAULT, 0, 0);
  if(res != SV_OK) {
    printf("sv_openex:%d (%s)\n", res, sv_geterrortext(res));
    return 1;
  }

  // raster information
  res = sv_storage_status(handle->sv, 0, NULL, &handle->storageinfo, sizeof(sv_storageinfo), SV_STORAGEINFO_COOKIEISJACK);
  if(res != SV_OK) {
    printf("sv_storage_status():%d (%s)\n", res, sv_geterrortext(res));
    return 1;
  }

  res = sv_option_set(handle->sv, SV_OPTION_ANCCOMPLETE, SV_ANCCOMPLETE_ON);
  if(res != SV_OK) {
    printf("sv_jack_option_set(SV_OPTION_ANCCOMPLETE):%d (%s)\n", res, sv_geterrortext(res));
    return 1;
  }

  // reinit videomode to activate anccomplete option
  res = sv_jack_option_set(handle->sv, 1, SV_OPTION_VIDEOMODE, handle->storageinfo.videomode);
  if(res != SV_OK) {
    printf("sv_jack_option_set(SV_OPTION_VIDEOMODE):%d (%s)\n", res, sv_geterrortext(res));
    return 1;
  }

  initBuffers(handle);

  // output
  res = sv_direct_init(handle->sv, &handle->dh_out, "DVS", 0, NUMBUFFERS, flags);
  if(res != SV_OK) {
    printf("sv_direct_init(out):%d (%s)\n", res, sv_geterrortext(res));
    cleanupBuffers(handle);
    return 1;
  }

  // input
  res = sv_direct_init(handle->sv, &handle->dh_in, "DVS", 1, NUMBUFFERS, flags);
  if(res != SV_OK) {
    printf("sv_direct_init(in):%d (%s)\n", res, sv_geterrortext(res));
    cleanupBuffers(handle);
    return 1;
  }

  for (buffer = 0; buffer < NUMBUFFERS; buffer++) {
    for (i = PATHINPUT; i <= PATHOUTPUT; i++) {
      sv_direct_handle * dh = NULL;
      sv_direct_anc_callback cb = NULL;
      switch(i) {
      case PATHOUTPUT:
        dh = handle->dh_out;
        cb = &anc_display_callback;
        break;
      case PATHINPUT:
        dh = handle->dh_in;
        cb = &anc_record_callback;
        break;
      default:;
      }
      if(dh) {
        res = sv_direct_bind_buffer(handle->sv, dh, buffer, handle->nativeBuffers[buffer][i], handle->nativeSize);
        if(res != SV_OK) {
          printf("sv_direct_bind_buffer:%d (%s)\n", res, sv_geterrortext(res));
          return 1;
        }

        res = sv_direct_bind_timecode(handle->sv, dh, buffer, &handle->timecodes[buffer][i]);
        if(res != SV_OK) {
          printf("sv_direct_bind_timecode:%d (%s)\n", res, sv_geterrortext(res));
          return 1;
        }
      }
      if(cb) {
        res = sv_direct_bind_anc_callback(handle->sv, dh, buffer, cb, NULL, handle);
        if(res != SV_OK) {
          printf("sv_direct_bind_anc_callback:%d (%s)\n", res, sv_geterrortext(res));
          return 1;
        }
      }
    }
  }

  // init events
  for (buffer = 0; buffer < NUMBUFFERS; buffer++) {
    dvs_cond_init(&handle->record_ready[buffer]);
    dvs_mutex_init(&handle->record_ready_mutex[buffer]);
    dvs_cond_init(&handle->display_ready[buffer]);
    dvs_mutex_init(&handle->display_ready_mutex[buffer]);
    dvs_cond_init(&handle->record_go[buffer]);
    dvs_mutex_init(&handle->record_go_mutex[buffer]);
    dvs_cond_init(&handle->display_go[buffer]);
    dvs_mutex_init(&handle->display_go_mutex[buffer]);
  }

  printf("starting threads\n");
  dvs_thread_create(&recordThreadHandle, recordThread, handle, &handle->finish_in);
  dvs_thread_create(&displayThreadHandle, displayThread, handle, &handle->finish_out);


  // first record go
  dvs_cond_broadcast(&handle->record_go[recordCurrent % NUMBUFFERS], &handle->record_go_mutex[recordCurrent % NUMBUFFERS], FALSE);

  for (frame = 0; running && ((numFrames == -1) || (frame < numFrames)); frame++)
  {
    dvs_cond_wait(&handle->record_ready[recordCurrent % NUMBUFFERS], &handle->record_ready_mutex[recordCurrent % NUMBUFFERS], FALSE);
    if(frame > 0) { 
      dvs_cond_wait(&handle->display_ready[displayCurrent % NUMBUFFERS], &handle->display_ready_mutex[displayCurrent % NUMBUFFERS], FALSE);
    }

    // The actual buffer processing could happen here (in this example just copy the data).
    memcpy(handle->nativeBuffers[displayCurrent % NUMBUFFERS][PATHOUTPUT], handle->nativeBuffers[recordCurrent % NUMBUFFERS][PATHINPUT], handle->nativeSize);

    if(handle->verbose > 1) {
      printf("%d> timecodes: dvitc[0]:%08x/%08x dvitc[1]:%08x/%08x ltc:%08x/%08x vtr:%08x/%08x\n", recordCurrent % NUMBUFFERS,
        handle->timecodes[recordCurrent % NUMBUFFERS][PATHINPUT].dvitc_tc[0],
        handle->timecodes[recordCurrent % NUMBUFFERS][PATHINPUT].dvitc_ub[0],
        handle->timecodes[recordCurrent % NUMBUFFERS][PATHINPUT].dvitc_tc[1],
        handle->timecodes[recordCurrent % NUMBUFFERS][PATHINPUT].dvitc_ub[1],
        handle->timecodes[recordCurrent % NUMBUFFERS][PATHINPUT].ltc_tc,
        handle->timecodes[recordCurrent % NUMBUFFERS][PATHINPUT].ltc_ub,
        handle->timecodes[recordCurrent % NUMBUFFERS][PATHINPUT].vtr_tc,
        handle->timecodes[recordCurrent % NUMBUFFERS][PATHINPUT].vtr_ub
      );
    }

    // Simply copy timecodes from input to output.
    memcpy(&handle->timecodes[displayCurrent % NUMBUFFERS][PATHOUTPUT], &handle->timecodes[recordCurrent % NUMBUFFERS][PATHINPUT], sizeof(sv_direct_timecode));
    
    // copy anc packets from input list to output list
    {
      anc_packet_list_t * reclist = &handle->ancpackets[recordCurrent % NUMBUFFERS][PATHINPUT];
      anc_packet_list_t * dislist = &handle->ancpackets[displayCurrent % NUMBUFFERS][PATHOUTPUT];
      while(reclist && reclist->valid) {
        memcpy(&dislist->packet, &reclist->packet, sizeof(sv_direct_ancpacket));
        if(!dislist->next) {
         dislist->next = (anc_packet_list_t *) malloc(sizeof(anc_packet_list_t));
         memset(dislist->next, 0, sizeof(anc_packet_list_t));
        }
        dislist->valid = TRUE;
        dislist = dislist->next;
        
        reclist->valid = FALSE;
        reclist = reclist->next;
      }
    }

    // next input buffer
    recordCurrent++;
    dvs_cond_broadcast(&handle->record_go[recordCurrent % NUMBUFFERS], &handle->record_go_mutex[recordCurrent % NUMBUFFERS], FALSE);
    
    // display current buffer
    dvs_cond_broadcast(&handle->display_go[displayCurrent % NUMBUFFERS], &handle->display_go_mutex[displayCurrent % NUMBUFFERS], FALSE);
    displayCurrent++;
  }

  printf("stopping threads\n");
  dvs_thread_exitcode(&recordThreadHandle, &handle->finish_in);
  dvs_thread_exitcode(&displayThreadHandle, &handle->finish_out);

  for (buffer = 0; buffer < NUMBUFFERS; buffer++) {
    dvs_cond_free(&handle->record_ready[buffer]);
    dvs_mutex_free(&handle->record_ready_mutex[buffer]);
    dvs_cond_free(&handle->display_ready[buffer]);
    dvs_mutex_free(&handle->display_ready_mutex[buffer]);
    dvs_cond_free(&handle->record_go[buffer]);
    dvs_mutex_free(&handle->record_go_mutex[buffer]);
    dvs_cond_free(&handle->display_go[buffer]);
    dvs_mutex_free(&handle->display_go_mutex[buffer]);

    if(handle->ancpackets[buffer][PATHINPUT].next) {
      anc_packet_list_t * list = handle->ancpackets[buffer][PATHINPUT].next;
      while(list) {
        anc_packet_list_t * tmp = list->next;
        free(list);
        list = tmp;
      }
    }
    if(handle->ancpackets[buffer][PATHOUTPUT].next) {
      anc_packet_list_t * list = handle->ancpackets[buffer][PATHOUTPUT].next;
      while(list) {
        anc_packet_list_t * tmp = list->next;
        free(list);
        list = tmp;
      }
    }
  }

  sv_direct_free(handle->sv, handle->dh_in);
  sv_direct_free(handle->sv, handle->dh_out);
  sv_close(handle->sv);

  cleanupBuffers(handle);
  printf("done.\n");

  return 0;
}

int anc_record_callback(void * userdata, int bufferindex, sv_direct_ancpacket * inpacket)
{
  direct_handle * handle = (direct_handle *) userdata;
  anc_packet_list_t * list = &handle->ancpackets[bufferindex][PATHINPUT];
  if(handle->verbose > 1) {
    printf("%d> anc(rec) f:%d l:%d did:%02x:%02x data:%p size:%d %s\n", bufferindex, inpacket->field, inpacket->linenr, inpacket->did, inpacket->sdid, inpacket->data, inpacket->datasize, inpacket->bvanc ? "VANC" : "HANC");
  }

  // find free item in list
  while(list && list->valid) {
    if(!list->next) {
      list->next = (anc_packet_list_t *) malloc(sizeof(anc_packet_list_t));
      memset(list->next, 0, sizeof(anc_packet_list_t));
    }
    list = list->next;
  }
  // copy to cache for looping
  memcpy(&list->packet, inpacket, sizeof(sv_direct_ancpacket));
  list->valid = TRUE;

  return 0;
}

int anc_display_callback(void * userdata, int bufferindex, sv_direct_ancpacket * outpacket)
{
  direct_handle * handle = (direct_handle *) userdata;
  anc_packet_list_t * list = &handle->ancpackets[bufferindex][PATHOUTPUT];

  // find valid item in list
  while(list && !list->valid) {
    list = list->next;
  }
  // copy from cache
  if(list) {
    memcpy(outpacket, &list->packet, sizeof(sv_direct_ancpacket));
    list->valid = FALSE;
  }

  if(list && handle->verbose > 1) {
    printf("%d> anc(dis) f:%d l:%d did:%02x:%02x data:%p size:%d %s\n", bufferindex, outpacket->field, outpacket->linenr, outpacket->did, outpacket->sdid, outpacket->data, outpacket->datasize, outpacket->bvanc ? "VANC" : "HANC");
  }

  if(!list) {
    // no more packets left
    return TRUE;
  }
  return FALSE;
}

void * recordThread(void * arg)
{
  direct_handle * handle = (direct_handle *) arg;
  sv_direct_handle * dh = handle->dh_in;
  sv_direct_bufferinfo bufferinfo = { sizeof(sv_direct_bufferinfo) };
  sv_direct_info info;
  int dropped = 0;
  int res = SV_OK;
  int recordCurrent = 0;

  do {
    if(res == SV_OK) {
      dvs_cond_wait(&handle->record_go[recordCurrent % NUMBUFFERS], &handle->record_go_mutex[recordCurrent % NUMBUFFERS], FALSE);

      res = sv_direct_record(handle->sv, dh, recordCurrent % NUMBUFFERS, SV_DIRECT_FLAG_DISCARD, &bufferinfo);
      if(res != SV_OK) {
        printf("sv_direct_record:%d (%s)\n", res, sv_geterrortext(res));

        switch(res) {
        case SV_ERROR_INPUT_VIDEO_NOSIGNAL:
        case SV_ERROR_INPUT_VIDEO_RASTER:
        case SV_ERROR_INPUT_VIDEO_DETECTING:
          // non-fatal errors
          res = SV_OK;
          break;
        }
      }

      if(handle->verbose) {
        unsigned int vsync = (1000000 / handle->storageinfo.fps);
        uint64 record_go = ((uint64)bufferinfo.clock_high << 32) | bufferinfo.clock_low;
        uint64 dma_ready = ((uint64)bufferinfo.dma.clock_ready_high << 32) | bufferinfo.dma.clock_ready_low;

        printf("%d> record:\twhen:%d vsync->dmadone:%d dmastart->vsync:%d\n", recordCurrent % NUMBUFFERS, bufferinfo.when, (int)(dma_ready - record_go - vsync), bufferinfo.dma.clock_distance);
      }
    }

    if(res == SV_OK) {
      dvs_cond_broadcast(&handle->record_ready[recordCurrent % NUMBUFFERS], &handle->record_ready_mutex[recordCurrent % NUMBUFFERS], FALSE);
      // next input buffer
      recordCurrent++;
    }

    if(res == SV_OK) {
      info.size = sizeof(sv_direct_info);

      res = sv_direct_status(handle->sv, dh, &info);
      if(res != SV_OK) {
        printf("sv_direct_status:%d (%s)\n", res, sv_geterrortext(res));
      } else {
        if(info.dropped > dropped) {
          printf("recordThread:  dropped:%d\n", info.dropped);
          dropped = info.dropped;
        }
      }
    }
  } while(running && (res == SV_OK));

  handle->exitcode_in = 0;
  dvs_thread_exit(&handle->exitcode_in, &handle->finish_in);

  return NULL;
}

void * displayThread(void * arg)
{
  direct_handle * handle = (direct_handle *) arg;
  sv_direct_handle * dh = handle->dh_out;
  sv_direct_bufferinfo bufferinfo = { sizeof(sv_direct_bufferinfo) };
  sv_direct_info info;
  int dropped = 0;
  int res = SV_OK;
  int displayCurrent = 0;

  do {
    if(res == SV_OK) {
      dvs_cond_wait(&handle->display_go[displayCurrent % NUMBUFFERS], &handle->display_go_mutex[displayCurrent % NUMBUFFERS], FALSE);

      res = sv_direct_display(handle->sv, dh, displayCurrent % NUMBUFFERS, SV_DIRECT_FLAG_DISCARD, &bufferinfo);
      if(res != SV_OK) {
        printf("sv_direct_display:%d (%s)\n", res, sv_geterrortext(res));

        switch(res) {
        case SV_ERROR_VSYNCPASSED:
          // might happen in case of delay by GPU
          res = SV_OK;
          break;
        }
      }

      if(handle->verbose) {
        unsigned int vsync = (1000000 / handle->storageinfo.fps);
        printf("%d> display:\twhen:%d vsync->dmastart:%d dmastart->vsync:%d\n", displayCurrent % NUMBUFFERS, bufferinfo.when, (int)(vsync - bufferinfo.dma.clock_distance), bufferinfo.dma.clock_distance);
      }
    }

    if(res == SV_OK) {
      // next display buffer
      displayCurrent++;
      dvs_cond_broadcast(&handle->display_ready[displayCurrent % NUMBUFFERS], &handle->display_ready_mutex[displayCurrent % NUMBUFFERS], FALSE);
    }

    if(res == SV_OK) {
      info.size = sizeof(sv_direct_info);

      res = sv_direct_status(handle->sv, dh, &info);
      if(res != SV_OK) {
        printf("sv_direct_status:%d (%s)\n", res, sv_geterrortext(res));
      } else {
        if(info.dropped > dropped) {
          printf("displayThread: dropped:%d\n", info.dropped);
          dropped = info.dropped;
        }
      }
    }
  } while(running && (res == SV_OK));

  handle->exitcode_out = 0;
  dvs_thread_exit(&handle->exitcode_out, &handle->finish_out);

  return NULL;
}
