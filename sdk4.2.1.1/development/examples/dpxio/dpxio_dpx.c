/*
//    Part of the DVS (http://www.dvs.de) *DVS*VERSION* SDK 
//
//    dpxio - Shows the use of the fifoapi to do display and record 
//            of images directly to a file.
//
*/

#include "dpxio.h"

#include "../common/dpxformat.h"

int dpxio_dpx_getstoragemode(dpxio_handle * dpxio, int dpxtype, int nbits)
{
  return dpxformat_getstoragemode(dpxtype, nbits);
}

int dpxio_dpx_framesize(dpxio_handle * dpxio, int xsize, int ysize, int dpxmode, int nbits, int * poffset, int * ppadding)
{
  return dpxformat_framesize(xsize, ysize, dpxmode, nbits, poffset, ppadding);
}

int dpxio_dpx_opensequence(dpxio_handle * dpxio, dpxio_fileio * pfileio, char * filename, int firstframe, int lastframe)
{
  return TRUE;
}

int dpxio_dpx_closesequence(dpxio_handle * dpxio, dpxio_fileio * pfileio)
{
  return TRUE;
}

int dpxio_dpx_readframe(dpxio_handle * dpxio, dpxio_fileio * pfileio, int framenr, sv_fifo_buffer * pbuffer, char * buffer, int buffersize, int * offset, int * xsize, int * ysize, int * dpxtype, int * nbits, int * lineoffset)
{
  char filename[MAX_PATHNAME];

  sprintf(filename, pfileio->filename, pfileio->framenr + framenr);

  if(dpxio->io.dryrun) {
    if(dpxio->verbose) {
      printf("read video frame '%s'\n", filename);
    }
    return *offset; /* Simulate dummy dma transfer */
  }

  return dpxformat_readframe(filename, buffer, buffersize, offset, xsize, ysize, dpxtype, nbits, lineoffset);
}


int dpxio_dpx_writeframe(dpxio_handle * dpxio, dpxio_fileio * pfileio, int framenr, sv_fifo_buffer * pbuffer, char * buffer, int buffersize, int xsize, int ysize, int dpxtype, int nbits, int offset, int padding, int timecode)
{
  char filename[MAX_PATHNAME];

  sprintf(filename, pfileio->filename, pfileio->framenr + framenr);

  if(dpxio->io.dryrun) {
    if(dpxio->verbose) {
      printf("write video frame '%s'\n", filename);
    }
    return 0x1000; /* Simulate dummy dma transfer */
  }

  return dpxformat_writeframe(filename, buffer, buffersize, offset, xsize, ysize, dpxtype, nbits, padding, timecode);
}

