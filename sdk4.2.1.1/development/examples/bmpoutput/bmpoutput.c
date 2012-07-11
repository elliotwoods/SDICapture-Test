/*
//    Part of the DVS (http://www.dvs.de) *DVS*VERSION* SDK 
//
//    The bmpoutput program copies the Windows video buffer into a 
//    bitmap memory buffer and transfers it to the video card
//    buffer using FIFOAPI DMA. 
//
//    This example only works under Windows
//
*/

/******************** INCLUDES **************************/

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <windows.h>
#include <wingdi.h>
#include <winuser.h>
#include <winbase.h>
#include <signal.h>

#include "..\..\header\dvs_clib.h"
#include "..\..\header\dvs_fifo.h"

/******************** DEFINES ****************************/

#define OUTPUT_CLOCK       0x001  //!<OutputOptionDefine
#define OUTPUT_FULLSCREEN  0x002  //!<OutputOptionDefine
#define OUTPUT_VERBOSE     0x004  //!<OutputOptionDefine

static int global_running = TRUE; //!< GlobalRunning

typedef  struct {                 //!<Windows video information
    int videosize;                //!<Windows video memory buffer size
    int xsize;                    //!<Windows video xsize
    int ysize;                    //!<Windows video ysize
} bmpoutput_sv_setup;                

typedef struct {                    //!< Status variables
    int           nframes;          //!< Number of frames displayed at the moment
    LARGE_INTEGER frequency;        //!< CPU tick per second frecuency 
    LARGE_INTEGER display_time;     //!< Time inverted in sending data to the card buffer 
    LARGE_INTEGER reference_timer;  
    int           fps;              //!< fps screen refresh counter
} bmpoutput_status;

typedef struct {                    //!< Windows screen resolution
    int xscreen;
    int yscreen;
} bmpoutput_screen;

typedef struct {                    //!< Drawing variables
    RECT textrect;
    HFONT clock_font;
    POINT point;
} bmpoutput_draw;

typedef struct _bmpoutput_struct{

  sv_handle * sv;               //!< Handle to the sv structure
  sv_fifo * poutput;            //!< FIFO init structure
  sv_fifo_buffer * buffer;      //!< Buffer to be send to the SDIO
  char * videoptr;              //!< The video memory buffer (Windows video memory)

  bmpoutput_sv_setup  sv_setup; //!< Video raster information
  bmpoutput_screen screen;      //!< Screen resolution informacion
  bmpoutput_status status;      //!< Status variables
  bmpoutput_draw   draw;        //!< draw variables 
  
  HDC hdcScreen;                //!< DC to the desktop screen
  HDC hdcCompatible;            //!< DC to make the copy
  HDC hdcPaint;                 //!< Used only for clock option

  int option;                   //!< Describes the command options
             


  HBITMAP hbmScreen;            //!< Screen BitMap (contains the Windows Desktop Bitmap)
  HBITMAP hbmVideo;             

  BITMAPINFO bmi;               //!< Header of the bitmap type we use

} bmpoutput_handle;


///Stop and Close the current fifo
/**
\param sv Pointer to the videodevice
\param fifo Pointer to the current fifo

\code
  if( sv )
  {
    //Stop fifo output. And display last shown buffer
    if(fifoReturnCode == SV_OK){
      fifoReturnCode = sv_fifo_stop( sv, fifo, 0 );
    }
  
    //Close fifo. All fifo Buffer will destroyed.
    if(fifoReturnCode == SV_OK){
      fifoReturnCode = sv_fifo_free( sv, fifo );
      if(fifoReturnCode == SV_OK) closedVideoSuccessfully = TRUE;
    }
  }
\endcode
*/
int closeFifo( sv_handle *sv, sv_fifo *fifo )
{
  int closedVideoSuccessfully = FALSE;
  int fifoReturnCode          = SV_OK;

  if( sv )
  {
    //Stop fifo output. And display last shown buffer
    if(fifoReturnCode == SV_OK){
      fifoReturnCode = sv_fifo_stop( sv, fifo, 0 );
    }
    
    //Close fifo. All fifo Buffer will destroyed.
    if(fifoReturnCode == SV_OK){
      fifoReturnCode = sv_fifo_free( sv, fifo );
      if(fifoReturnCode == SV_OK) closedVideoSuccessfully = TRUE;
    }
  }

  return closedVideoSuccessfully;
}


///Read option and when it is valid, set option flag
/**
\param output_option pointer to int option
\param option character which includes the new option
*/
//int set_flags(bmpoutput_handle * bmp_output, char * option)
int set_flag( int * output_option, char * new_option)
{
  int foundValidOption = FALSE;

  if(strcmp(new_option, "-v") == 0) {
    *output_option |= OUTPUT_VERBOSE; 
    foundValidOption = TRUE;
  } else if(strcmp(new_option,"-c") == 0) {
    *output_option |= OUTPUT_CLOCK; 
    foundValidOption = TRUE;
  } else if(strcmp(new_option,"-f") == 0) {
    *output_option |= OUTPUT_FULLSCREEN;
    foundValidOption = TRUE;
  }

  return foundValidOption;
}


///Open Videodevice set Raster to RGB and allocate a videobuffer into RAM
/**
\param **sv Pointer to the videodevice
\param sv_setup Includes raster information from the device
\param *videoptr Pointer to the videobuffer

\code
  //Open Videodevice
  res = sv_openex( sv, "", SV_OPENPROGRAM_DEMOPROGRAM, SV_OPENTYPE_DEFAULT, 0, 0);
  if(res != SV_OK) {
    fprintf(stderr, "bmpoutput: Error opening device '%s'\n", sv_geterrortext(res));
    last_result = FALSE;
  } 
\endcode

*/
int init_sv( sv_handle **sv, bmpoutput_sv_setup *sv_setup, char **videoptr )
{ 
  int res            = SV_OK; //svReturnCode
  int last_result    = TRUE;  //errorStatus
  int mode_Available = TRUE;  //RGBModeAvailable?
  int raster         = 0;     //ContainerForRasterBitmask
  sv_storageinfo  storage;    //DeviceInformartion
  
  memset(&storage, 0, sizeof(storage));

  //Open Videodevice
  res = sv_openex( sv, "", SV_OPENPROGRAM_DEMOPROGRAM, SV_OPENTYPE_DEFAULT, 0, 0);
  if(res != SV_OK) {
    fprintf(stderr, "bmpoutput: Error opening device '%s'\n", sv_geterrortext(res));
    last_result = FALSE;
  } 

  //Get current raster
  if( last_result )
  {
    res = sv_query( *sv, SV_QUERY_MODE_CURRENT, 0, &raster);
    if( res != SV_OK ) {
      printf("Checking current raster failed: %d %s", res, sv_geterrortext(res));
      last_result = FALSE;
    }
  }

  if( last_result )
  {
    if((raster & ~(SV_MODE_MASK | SV_MODE_FLAG_RASTERINDEX))!= (SV_MODE_COLOR_RGB_BGR | SV_MODE_STORAGE_FRAME | SV_MODE_STORAGE_BOTTOM2TOP))
    {
      raster &= (SV_MODE_MASK | SV_MODE_FLAG_RASTERINDEX);
      raster |= (SV_MODE_COLOR_RGB_BGR | SV_MODE_STORAGE_FRAME | SV_MODE_STORAGE_BOTTOM2TOP);
  
      res = sv_query(*sv, SV_QUERY_MODE_AVAILABLE, raster, &mode_Available);
      if( (res!=SV_OK) | (mode_Available!=TRUE) ) {
        printf("Card does not support RGB BMP color mode: %d %s", res, sv_geterrortext(res));
        last_result = FALSE;
      }
    
      if( last_result )
      {
        res = sv_videomode( *sv , raster);  // Selects the raster we want to use
        if(res != SV_OK) {
          printf("Video raster initialization failed: %d %s", res, sv_geterrortext(res));
          last_result = FALSE;
        }
      }
    }
  }
  
  if( last_result )
  {
    res = sv_storage_status(*sv, 0, NULL, &storage, sizeof(storage), SV_STORAGEINFO_COOKIEISJACK);
    if(res != SV_OK) {
      printf("Video raster status failed: %d %s", res, sv_geterrortext(res));
      last_result = FALSE;
    }
  }

  // Video raster setup variables
  if( last_result )
  {
    sv_setup->videosize = storage.buffersize;
    sv_setup->xsize     = storage.storagexsize;
    sv_setup->ysize     = storage.storageysize;

    // Allocate memory for the video buffer
    *videoptr = malloc(storage.buffersize);
    if(*videoptr == NULL) {
      printf("Problem allocating video buffermemory(%d).\n", storage.buffersize);
      last_result = FALSE;
    }
  }
  return last_result;
}


///Open Fifo
/**
\param **sv Pointer to the videodevice
\param **outputFifo Pointer to fifo

\code
  // Initializes the FIFO
  if( TRUE == last_result )
  {                   //handle //Fifo   //Input?//Shared//dma//flags//nFrames
    res = sv_fifo_init( *sv, *outputFifo, FALSE, FALSE, TRUE, FALSE, 0);
    if(res != SV_OK){
      printf("sv_fifo_init error : %d %s.\n", res, sv_geterrortext(res));
      last_result = FALSE;
    }
  }

  // Start FIFO
  if( TRUE == last_result ){
    res = sv_fifo_start( *sv, *outputFifo);
    if(res != SV_OK){
      printf("fifo start error : %d %s.\n", res, sv_geterrortext(res));
      last_result = FALSE;
    }
  }
\endcode

*/
int init_fifo( sv_handle **sv, sv_fifo **outputFifo)
{ 
  int res         = SV_OK; //svReturnCode
  int last_result = TRUE;  //errorStatus

  // Initializes the FIFO
  if( TRUE == last_result )
  {                   //handle //Fifo   //Input?//Shared//dma//flags//nFrames
    res = sv_fifo_init( *sv, outputFifo, FALSE, FALSE, TRUE, FALSE, 0);
    if(res != SV_OK){
      printf("sv_fifo_init error : %d %s.\n", res, sv_geterrortext(res));
      last_result = FALSE;
    }
  }

  // Start FIFO
  if( TRUE == last_result ){
    res = sv_fifo_start( *sv, *outputFifo);
    if(res != SV_OK){
      printf("fifo start error : %d %s.\n", res, sv_geterrortext(res));
      last_result = FALSE;
    }
  }

  return last_result;
}


///...
/**
\param bmp_output Handle to output option
\param option character which includes the new option
*/
int init_windows_draw(bmpoutput_handle * bmp_output)
{
  int last_result = TRUE;  //errorStatus

  // Status variables initialization

  bmp_output->status.nframes               = 0; 
  bmp_output->status.fps                   = 1;
  bmp_output->status.display_time.QuadPart = 0;

  if(!QueryPerformanceFrequency(&bmp_output->status.frequency)) {
    printf(" Error initializing frecuency: %d\n", GetLastError());
  }

  if(!QueryPerformanceCounter(&bmp_output->status.reference_timer)) {
    printf("Error performance counter :%d\n", GetLastError());
  }

  // Initializes screen DC's and bitmaps

  bmp_output->hdcScreen = CreateDC("DISPLAY", NULL, NULL, NULL);
  if(bmp_output->hdcScreen == NULL) {
    printf("Error : %d while creating a DC for the screen. \n",GetLastError());
    last_result = FALSE;
  }

  if( last_result )
  {
    bmp_output->hdcCompatible = CreateCompatibleDC(bmp_output->hdcScreen);
    if(bmp_output->hdcCompatible == NULL) {
      printf("Error : %d while creating a CompatibleDC for the screen DC. \n",GetLastError());
      last_result = FALSE;
    }
  }

  if( last_result )
  {
    bmp_output->hdcPaint = CreateCompatibleDC(bmp_output->hdcScreen);
    if(bmp_output->hdcPaint == NULL) {
      printf("Error : %d while creating the Paint DC. \n",GetLastError());
      last_result = FALSE;
    }
  }

  if( last_result )
  {
    // Screen resolution variables

    bmp_output->screen.xscreen = GetDeviceCaps(bmp_output->hdcScreen,HORZRES);
    bmp_output->screen.yscreen = GetDeviceCaps(bmp_output->hdcScreen,VERTRES);
  
    // Clock drawing rectangle
    if(bmp_output->screen.xscreen > bmp_output->sv_setup.xsize) {
      bmp_output->draw.textrect.left   = (int) (bmp_output->sv_setup.xsize * 0.1);
      bmp_output->draw.textrect.right  = (int) (bmp_output->sv_setup.xsize * 0.9);
      bmp_output->draw.textrect.top    = (int) (bmp_output->sv_setup.ysize * 0.25);
      bmp_output->draw.textrect.bottom = (int) (bmp_output->sv_setup.ysize * 0.75);
    } else {
      bmp_output->draw.textrect.left   = (int) (bmp_output->screen.xscreen * 0.1);
      bmp_output->draw.textrect.right  = (int) (bmp_output->screen.xscreen * 0.9);
      bmp_output->draw.textrect.top    = (int) (bmp_output->screen.yscreen * 0.25);
      bmp_output->draw.textrect.bottom = (int) (bmp_output->screen.yscreen * 0.75);
    }

    // bitmap type header
    bmp_output->bmi.bmiHeader.biSize          = sizeof(bmp_output->bmi);
    bmp_output->bmi.bmiHeader.biWidth         = bmp_output->sv_setup.xsize;
    bmp_output->bmi.bmiHeader.biHeight        = bmp_output->sv_setup.ysize;
    bmp_output->bmi.bmiHeader.biPlanes        = 1;
    bmp_output->bmi.bmiHeader.biBitCount      = 24;  
    bmp_output->bmi.bmiHeader.biCompression   = 0; 
    bmp_output->bmi.bmiHeader.biSizeImage     = bmp_output->sv_setup.videosize; 
    bmp_output->bmi.bmiHeader.biXPelsPerMeter = 0; 
    bmp_output->bmi.bmiHeader.biYPelsPerMeter = 0; 
    bmp_output->bmi.bmiHeader.biClrUsed       = 0;
    bmp_output->bmi.bmiHeader.biClrImportant  = 0; 

    bmp_output->hbmVideo = CreateDIBSection( bmp_output->hdcCompatible, 
                                                    &(bmp_output->bmi),     // bitmap data
                                                        DIB_RGB_COLORS,
                                                 &bmp_output->videoptr,     // bit values
                                                                  NULL,
                                                                     0 );

    if( !bmp_output->hbmVideo ){
      printf("Error : %d while creating DIB selection. \n",GetLastError());
      last_result = FALSE;
    }
  }

  if( last_result )
  {
    SelectObject(bmp_output->hdcCompatible, bmp_output->hbmVideo);
  
    bmp_output->draw.clock_font = CreateFont(
                     102,               // height of font
                      56,               // average character width
                       0,               // angle of escapement
                       0,               // base-line orientation angle
               FW_NORMAL,               // font weight
                   FALSE,               // italic attribute option
                   FALSE,               // underline attribute option
                   FALSE,               // strikeout attribute option
            ANSI_CHARSET,               // character set identifier
      OUT_DEFAULT_PRECIS,               // output precision
     CLIP_DEFAULT_PRECIS,               // clipping precision
         DEFAULT_QUALITY,               // output quality
           DEFAULT_PITCH,               // pitch and family
                    NULL );             // typeface name
  
    if(bmp_output->draw.clock_font == NULL){
      last_result = FALSE;
      printf("Error creating font : %d", GetLastError());
    }
  }

  return last_result;
}


/*
//    windows_paint_clock: Draw clock into bitmap
*/
int windows_paint_clock(HDC hDC, RECT * ptextrect, HFONT font)
{
  SYSTEMTIME * s_time = NULL;
  char timestr[8];
  HFONT oldfont;

  GetTimeFormat(LOCALE_SYSTEM_DEFAULT, TIME_FORCE24HOURFORMAT, s_time, "hh':'mm':'ss ", timestr, sizeof(timestr));
  
  SetBkMode(hDC, TRANSPARENT);          
  SetTextColor(hDC, RGB(100, 100, 100));
  oldfont = GetCurrentObject(hDC, OBJ_FONT);
  SelectObject(hDC, font);

  if(!DrawText(hDC, timestr, sizeof(timestr), ptextrect, DT_CENTER)){
    return FALSE;
  }

  if(!OffsetRect( ptextrect, 4, 4)){
    return FALSE;
  }

  SetTextColor(hDC, RGB(255, 255, 255));
  if(!DrawText(hDC, timestr, sizeof(timestr), ptextrect, DT_CENTER)){
    return FALSE;
  }

  if(!OffsetRect( ptextrect, -4, -4)){
    return FALSE;
  }
  
  SetBkMode(hDC, OPAQUE);
  SetTextColor(hDC, RGB(0, 0, 0));
  SelectObject(hDC, oldfont); 

  return TRUE;
}


/* 
//    windows_display_verbose - Prints running statistics
*/
void windows_display_verbose(HDC hDC, bmpoutput_status * pstatus, bmpoutput_screen * pscreen, bmpoutput_sv_setup * psetup)
{
  char buffer[256];

  wsprintf(buffer, "FRAME %d Video Raster = %dx%d Screen resolution = %dx%d", 
    pstatus->nframes, psetup->xsize, psetup->ysize, pscreen->xscreen, pscreen->yscreen); 
  TextOut(hDC, 10, 10, buffer, (int)strlen(buffer));
  wsprintf(buffer, "%d fps screen refresh. %2dms per frame ", 
    pstatus->fps,(int)(1000 / pstatus->fps)); 
  TextOut(hDC, 10, 30, buffer, (int)strlen(buffer));
  wsprintf(buffer,"%3dms every card memory refresh", 
    (int) ((1000 * pstatus->display_time.QuadPart) / pstatus->frequency.QuadPart)); 
  TextOut(hDC, 10, 50, buffer, (int)strlen(buffer));
}



/* 
//    windows_paint_screen - Does the buffer refresh with the windows screen's information 
*/ 
int windows_paint_screen(bmpoutput_handle * bmp_output, int fullscreen)
{
  LARGE_INTEGER timer;
  static int reference_counter = 0;
  
  if(!fullscreen){
    if (!BitBlt(bmp_output->hdcCompatible, 0,0, bmp_output->sv_setup.xsize, bmp_output->sv_setup.ysize, bmp_output->hdcScreen, 0, 0, SRCCOPY)) {
      printf("Error : %d while copying the bitmaps into the compatible DC. \n",GetLastError());
      return FALSE;
    }
  } else {
  
    if (!StretchBlt(bmp_output->hdcCompatible,        // handle to destination DC
                          0,                          // x-coord of destination upper-left corner
                          0,                          // y-coord of destination upper-left corner
                          bmp_output->sv_setup.xsize, // width of destination rectangle
                          bmp_output->sv_setup.ysize, // height of destination rectangle
                          bmp_output->hdcScreen,      // handle to source DC
                          0,                          // x-coord of source upper-left corner
                          0,                          // y-coord of source upper-left corner
                          bmp_output->screen.xscreen, // width of source rectangle
                          bmp_output->screen.yscreen, // height of source rectangle
                          SRCCOPY)) {                 // raster operation code
      printf("Error : %d while scaling the bitmaps into the compatible DC. \n",GetLastError());
      return FALSE;
    }
  }

  bmp_output->status.nframes += 1;

  if(!QueryPerformanceCounter(&timer)) {
     printf("Error performance counter :%d\n",GetLastError());
     return FALSE;
  }

  if(( 1000000*(timer.QuadPart - bmp_output->status.reference_timer.QuadPart) 
                            / bmp_output->status.frequency.QuadPart) > 1000000){

    if( !QueryPerformanceCounter(&bmp_output->status.reference_timer) ){
      printf("Error performance counter :%d\n",GetLastError());
      return FALSE;
      }

    bmp_output->status.fps = bmp_output->status.nframes - reference_counter; 
    reference_counter = bmp_output->status.nframes;
  }

  return TRUE;
}


///Display screen loop
/**
\code
  if( last_result )
  {
    //It is possible that the program fill the buffers faster as the output raster draw push them out.
    //The filled buffers queue get bigger and bigger, to improve that situation you are able to
    //syncronice the fill speed with the raster speed. Check follows.
    do
    {
      res = sv_fifo_status(bmp_output->sv, bmp_output->poutput, &infostart);
      if(res != SV_OK){
        printf("sv_fifo_status error : %d %s.\n", res, sv_geterrortext(res));
        last_result = FALSE;
      }

      //The reason why nbuffers is one to big knows only God
      bufferfilled = (infostart.nbuffers-1) - infostart.availbuffers;
    
      //Wait if delay is reached
      if(bufferfilled >= delay){
        res = sv_fifo_vsyncwait(bmp_output->sv, bmp_output->poutput);
        if(res != SV_OK){
          printf("sv_fifo_vsyncwait error : %d %s.\n", res, sv_geterrortext(res));
          last_result = FALSE;
        }
        needToWait = TRUE;
      }else{
        needToWait = FALSE;
      }
    }while( needToWait && last_result );
    
  }

  
  // Get new empty videobuffer for dmatransfer
  res = sv_fifo_getbuffer(bmp_output->sv, bmp_output->poutput, &bmp_output->buffer, NULL, SV_FIFO_FLAG_VIDEOONLY|SV_FIFO_FLAG_FLUSH);
  if(res != SV_OK){
    printf("sv_fifo_getbuffer error : %d %s.\n", res, sv_geterrortext(res));
    last_result = FALSE;
  }

  if( last_result )
  {
    // Save dmaadress values into videobuffer
    bmp_output->buffer->dma.addr = bmp_output->videoptr;
    bmp_output->buffer->dma.size = bmp_output->sv_setup.videosize;
    
    // Push whole videobuffer with dma to dvsboard
    res = sv_fifo_putbuffer(bmp_output->sv, bmp_output->poutput, bmp_output->buffer, NULL);
    if(res != SV_OK){
      printf("sv_fifo_putbuffer error : %d %s\n", res, sv_geterrortext(res));
      last_result = FALSE;
    }
  }
\endcode

*/
int bmpoutput_display_screen(bmpoutput_handle * bmp_output)
{
  int last_result = TRUE;  //errorStatus
  int res;
  int bufferfilled = 0;
  int delay = 3; //min 1, max (infostart.nbuffers-1) more does not make sense
  int needToWait = FALSE;
  LARGE_INTEGER timer1, timer2;
  sv_fifo_info infostart = {0};
  
  timer1.QuadPart = 0;
  timer2.QuadPart = 0;

  //Check Fullscreen
  if((OUTPUT_FULLSCREEN & bmp_output->option) == OUTPUT_FULLSCREEN){
    last_result = windows_paint_screen(bmp_output, TRUE);
  } else {
    last_result = windows_paint_screen(bmp_output, FALSE);
  }

  //Check Clock
  if( last_result ){
    if((OUTPUT_CLOCK & bmp_output->option) == OUTPUT_CLOCK){
      last_result = windows_paint_clock(bmp_output->hdcCompatible, &bmp_output->draw.textrect, bmp_output->draw.clock_font);
    }
  }
  
  //Check Display Verbose
  if( last_result ){
    if((OUTPUT_VERBOSE & bmp_output->option) == OUTPUT_VERBOSE){
        windows_display_verbose( bmp_output->hdcCompatible, 
                                       &bmp_output->status, 
                                       &bmp_output->screen,
                                       &bmp_output->sv_setup); 
    }
  }

  if( last_result )
  {
    //It is possible that the program fill the buffers faster as the output raster draw push them out.
    //The filled buffers queue get bigger and bigger, to improve that situation you are able to
    //syncronice the fill speed with the raster speed. Check follows.
    do
    {
      res = sv_fifo_status(bmp_output->sv, bmp_output->poutput, &infostart);
      if(res != SV_OK){
        printf("sv_fifo_status error : %d %s.\n", res, sv_geterrortext(res));
        last_result = FALSE;
      }

      //The reason why nbuffers is one to big knows only God
      bufferfilled = (infostart.nbuffers-1) - infostart.availbuffers;
    
      //Wait if delay is reached
      if(bufferfilled >= delay){
        res = sv_fifo_vsyncwait(bmp_output->sv, bmp_output->poutput);
        if(res != SV_OK){
          printf("sv_fifo_vsyncwait error : %d %s.\n", res, sv_geterrortext(res));
          last_result = FALSE;
        }
        needToWait = TRUE;
      }else{
        needToWait = FALSE;
      }
    }while( needToWait && last_result );
    
  }
  
  if( last_result )
  {
    if( !QueryPerformanceCounter(&timer1) ){
      printf("Error performance counter :%d\n",GetLastError());
    }
    // Get new empty videobuffer for dmatransfer
    res = sv_fifo_getbuffer(bmp_output->sv, bmp_output->poutput, &bmp_output->buffer, NULL, SV_FIFO_FLAG_VIDEOONLY);
    if(res != SV_OK){
      printf("sv_fifo_getbuffer error : %d %s.\n", res, sv_geterrortext(res));
      last_result = FALSE;
    }
  }

  if( last_result )
  {
    // Save dmaadress values into videobuffer
    bmp_output->buffer->dma.addr = bmp_output->videoptr;
    bmp_output->buffer->dma.size = bmp_output->sv_setup.videosize;
 
    // Push whole videobuffer with dma to dvsboard
    res = sv_fifo_putbuffer(bmp_output->sv, bmp_output->poutput, bmp_output->buffer, NULL);
    if(res != SV_OK){
      printf("sv_fifo_putbuffer error : %d %s\n", res, sv_geterrortext(res));
      last_result = FALSE;
    }
  }

  if( last_result ){
    if( !QueryPerformanceCounter(&timer2) ){
      printf("Error performance counter :%d\n",GetLastError());
    }

    bmp_output->status.display_time.QuadPart = timer2.QuadPart - timer1.QuadPart;
  }

  return last_result;
}


void bmp_output_signalhandler(int signum)
{
  (void)signum;
  global_running = FALSE;
}


///Start the main program
/**
\param argc Number of options first option is the name of the program 
\param argv characters which includes the new option
*/

int main(int argc, char * argv[])
{      
  bmpoutput_handle bmph;
  bmpoutput_handle *bmp_output = &bmph;
  int running = TRUE;
  int res;
  int option_counter;

  signal(SIGINT,  bmp_output_signalhandler);
  signal(SIGTERM, bmp_output_signalhandler);
  //Get options 
  bmp_output -> option = (bmp_output->option & 0x000); // No clock, no verbose, no full screen by default
  for( option_counter=1; option_counter<argc; option_counter++ )
  {
    running = set_flag( &bmp_output->option ,argv[option_counter]);
    if( running != TRUE ){
      printf("Unknown option\n\nUsage : bmpoutput [-v] [-c] [-f]\nv: Verbose\nc: Clock\nf: full screen\n");
    }
  }

  /*
  // Open video connection with desired video raster
  */
  if(running){
 
    /*
    // Bmpoutput variables initialization
    */
    if(running) {
      running = init_sv(   &bmp_output->sv , &bmp_output->sv_setup, &bmp_output->videoptr);
    }
    if(running) {
      running = init_fifo( &bmp_output->sv , &bmp_output->poutput);
    }
    if(running) {
      running = init_windows_draw(bmp_output);
    } 
    
    /*
    // Main loop : Fills buffer & Displays buffer 
    */

    while(running && global_running) {
      //Desplay loop
      running = bmpoutput_display_screen(bmp_output);
      if( !running )printf("Error displaying image\n");
    } 

    /*
    // Stop and Closing FIFO 
    */ 
    closeFifo(bmp_output->sv, bmp_output->poutput);
    
    /*
    // Closing connection
    */
    if(bmp_output->sv) {
      
      sv_black( bmp_output->sv );

      res = sv_close(bmp_output->sv);
      if(res != SV_OK){
        fprintf(stderr, "bmpoutput: Error '%s' closing video device\n", sv_geterrortext(res));
      }
    }
    
    signal(SIGINT,  NULL);
    signal(SIGTERM, NULL);
  } 

  return 0;
}

