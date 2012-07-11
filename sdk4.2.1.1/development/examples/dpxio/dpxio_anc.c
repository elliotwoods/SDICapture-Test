/*
//    Part of the DVS (http://www.dvs.de) *DVS*VERSION* SDK 
//
//    dpxio - Shows the use of the fifoapi to do display and record 
//            of images directly to a file.
//
*/

#include "dpxio.h"

#ifdef WIN32
# pragma pack(1)
#endif

#if defined(__GNUC__) && !defined(__APPLE_CC__)
# define PACKED __attribute__((packed))
#else
# define PACKED
#endif

typedef struct {
  uint8 groupid;
  uint8 grouplength;
  uint8 videotc[4];
  uint8 videoub[4];
  uint8 audiotc[4];
  uint8 audioub[4];
  uint8 audiophase;
  uint8 kkmfg;
  uint8 kkemulsion;
  uint8 kk[8];
  uint8 inkprefix[4];
  uint8 ink[3];
  uint8 pulldown;
  uint8 sequence;
  uint8 absframes[4];
  uint8 videoformat;
  uint8 audiomodulus;
  uint8 filmformat;
  uint8 filmrate;
} rp215_temporal_10 PACKED;

typedef struct {
  uint8 groupid;
  uint8 grouplength;
  uint8 tags[4];
  uint8 flags[3];
  uint8 equipid[4];
  uint8 proddate[4];
  uint8 dailyroll[8];
  uint8 camroll[8];
  uint8 soundroll[8];
  uint8 labroll[8];
  uint8 scene[8];
  uint8 take[4];
  uint8 slate[8];
} rp215_material_20 PACKED;

typedef struct {
  uint8 groupid;
  uint8 grouplength;
  uint8 vtroll[8];
  uint8 title[20];
  uint8 episode[8];
  uint8 facility[20];
  uint8 misc[40];
} rp215_label_30 PACKED;

typedef struct {
  rp215_temporal_10 temporal;
  rp215_material_20 material;
  rp215_label_30    label;
  uint8 crc[2];
} rp215data PACKED;

typedef struct {
  uint8 groupid;
  uint8 grouplength;
  uint8 videotc[4];
  uint8 videoub[4];
  uint8 audiotc[4];
  uint8 audioub[4];
  uint8 audiophase;
  uint8 audiomodulus;
} rp215_temporal_11 PACKED; 

typedef struct {
  uint8 groupid;
  uint8 grouplength;
  uint8 tags[4];
  uint8 syncmark[2];
  uint8 proddate[4];
  uint8 camroll[8];
  uint8 soundroll[8];
  uint8 scene[8];
  uint8 take[4];
  uint8 slate[8];
} rp215_material_21 PACKED;

typedef struct {
  uint8 groupid;
  uint8 grouplength;
  uint8 title[20];
  uint8 episode[8];
  uint8 crewid[4];
  uint8 operatorid[4];
  uint8 directorid[4];
  uint8 producerid[4];
} rp215_label_31 PACKED;

typedef struct {
  uint8 groupid;
  uint8 grouplength;
  uint8 modelcode[2];
  uint8 serialnumber[4];
  uint8 framenumber[4];
  uint8 cameraformat[1];
  uint8 stepshutter[1];
  uint8 mastergain[1];
  uint8 ndfilter[1];
  uint8 ccfilter[1];
  uint8 gamma[1];
  uint8 presetmatrix[1];
  uint8 gammatable[1];
  uint8 gammastandard[1];
  uint8 gammauser[1];
  uint8 lfeframerate[1];
  uint8 switch1[1];
  uint8 switch2[1];
  uint8 whitegain_r[2];
  uint8 whitegain_g[2];
  uint8 whitegain_b[2];
  uint8 gammablack[2];
  uint8 masterkneepoint[2];
  uint8 masterkneeslope[2];
  uint8 mastergamma[2];
  uint8 detaillevel[2];
  uint8 blackmaster[2];
  uint8 blackgain_r[2];
  uint8 blackgain_g[2];
  uint8 blackgain_b[2];
} rp215_camera_40 PACKED;

typedef struct {
  uint8 groupid;
  uint8 grouplength;
  uint8 lensid[4];
  uint8 focallength[2];
  uint8 focalposition[2];
  uint8 iris[2];
  uint8 camheight[4];
  uint8 camtilt[2];
  uint8 camroll[2];
  uint8 campan[2];
} rp215_dolly_50  PACKED;

typedef struct {
  uint8 groupid;
  uint8 grouplength;
  uint8 data[66];
} rp215_user_f0 PACKED;

typedef struct {
  rp215_temporal_11 temporal;
  rp215_material_21 material;
  rp215_label_31    label;
  rp215_camera_40   camera;
  rp215_dolly_50    dolly;
  rp215_user_f0     user;
  uint8 crc[2];
} rp215adata PACKED;


static void getstring(char * buffer, int count, uint8 * string)
{
  int i;
  
  for(i = 0; *string && (i < count); i++) {
    if((*string >= 32) || (*string < 128)) {
      *buffer++ = *string++; 
    } else {
      *buffer++ = '.';
    }
  }
  for(; (i < count); i++) {
    *buffer++ = ' ';
  }
  *buffer++ = 0;
}

static void rpvalue(uint8 * p, int count, int value)
{
  int i;
  for(i = 0; (i < 4) && i < count; i++) {
    *p++ = (value >> (8*(count-i-1))) & 0xff; 
  }
}


static void rpstring(uint8 * p, int count, char * string)
{
  int i;
  for(i = 0; *string && (i < count); i++) {
    *p++ = *string++; 
  }
}


int rp215_crc(unsigned char * ancdata, int count)
{
  int crc;
  int k, j;

  for(crc = 0, k = 0; k < count-2; k++) {
    for(j = 7; j >= 0; j--) {
      if(ancdata[k] & (1<<j)) {
       if(crc & 0x8000) {
          crc = ((crc << 1) | 1) ^ 0x1021;
        } else {
          crc =  (crc << 1) | 1;
        }
      } else {
        if(crc & 0x8000) {
          crc = (crc << 1) ^ 0x1021;
        } else {
          crc = (crc << 1);
        }
      }
    }
  }
  
  return crc & 0xffff;
}

void rp215_dump(unsigned char * ancdata, int datacount)
{
  int offset = 0;
  char buffer[128];
  int  tmp;
  int  crc;

  while(offset < datacount - 2) {
    int label = ancdata[offset];
    int count = ancdata[offset+1];

    printf("%02x-", label);
    switch(label) {
    case 0x10: 
      printf("TEMPORAL\n");
      {
        rp215_temporal_10 * p = (rp215_temporal_10 *)&ancdata[offset];
        printf(" videotc      %02x:%02x:%02x:%02x\n", p->videotc[0], p->videotc[1], p->videotc[2], p->videotc[3]);
        tmp = 0x01000000*p->videoub[0] + 0x010000 * p->videoub[1] + 0x0100 * p->videoub[2] + p->videoub[3];
        printf(" videoub      %02x%02x%02x%02x (%d)\n", p->videoub[0], p->videoub[1], p->videoub[2], p->videoub[3], tmp);
        printf(" audiotc      %02x:%02x:%02x:%02x\n", p->audiotc[0], p->audiotc[1], p->audiotc[2], p->audiotc[3]);
        tmp = 0x01000000*p->audioub[0] + 0x010000 * p->audioub[1] + 0x0100 * p->audioub[2] + p->audioub[3];
        printf(" audioub      %02x%02x%02x%02x (%d)\n", p->audioub[0], p->audioub[1], p->audioub[2], p->audioub[3], tmp);
        printf(" audiophase   %02x\n", p->audiophase);
        printf(" kkmfg        %02x\n", p->kkmfg);
        printf(" kkemulsion   %02x\n", p->kkemulsion);
        printf(" kk           %02x%02x%02x%02x%02x%02x%02x%02x\n", p->kk[0], p->kk[1], p->kk[2], p->kk[3], p->kk[4], p->kk[5], p->kk[6], p->kk[7]);
        printf(" inkprefix    %02x%02x%02x%02x\n", p->inkprefix[0], p->inkprefix[1], p->inkprefix[2], p->inkprefix[3]);
        printf(" ink          %02x%02x%02x\n", p->ink[0], p->ink[1], p->ink[2]);
        printf(" pulldown     %02x\n", p->pulldown);
        printf(" sequence     %02x\n", p->sequence);
        tmp = 0x01000000*p->absframes[0] + 0x010000 * p->absframes[1] + 0x0100 * p->absframes[2] + p->absframes[3];
        printf(" absframes    %02x%02x%02x%02x (%d)\n", p->absframes[0], p->absframes[1], p->absframes[2], p->absframes[3], tmp);
        printf(" videoformat  %02x\n", p->videoformat);
        printf(" audiomod     %02x\n", p->audiomodulus);
        printf(" filmformat   %02x\n", p->filmformat);
        printf(" filmrate     %02x\n", p->filmrate);
      }
      break;
    case 0x20:
      printf("MATERIAL\n");
      {
        rp215_material_20 * p = (rp215_material_20 *)&ancdata[offset];
        printf(" tags         %02x%02x%02x%02x\n", p->tags[0], p->tags[1], p->tags[2], p->tags[3]);
        printf(" flags        %02x%02x%02x\n", p->flags[0], p->flags[1], p->flags[2]);
        getstring(buffer, 4, p->equipid);
        printf(" equipid      '%s'\n", buffer);
        printf(" proddate     %02x%02x%02x%02x\n", p->proddate[0], p->proddate[1], p->proddate[2], p->proddate[3]); 
        getstring(buffer, 8, p->dailyroll);
        printf(" dailyroll    '%s'\n", buffer);
        getstring(buffer, 8, p->camroll);
        printf(" camroll      '%s'\n", buffer);
        getstring(buffer, 8, p->soundroll);
        printf(" soundroll    '%s'\n", buffer);
        getstring(buffer, 8, p->labroll);
        printf(" labroll      '%s'\n", buffer);
        getstring(buffer, 8, p->scene);
        printf(" scene        '%s'\n", buffer);
        getstring(buffer, 4, p->take);
        printf(" take         '%s'\n", buffer);
        getstring(buffer, 8, p->slate);
        printf(" slate        '%s'\n", buffer);
      }
      break;
    case 0x30:
      printf("LABEL\n");
      {
        rp215_label_30 * p = (rp215_label_30 *)&ancdata[offset];
        getstring(buffer, 8, p->vtroll);
        printf(" vtroll       '%s'\n", buffer);
        getstring(buffer, 20, p->title);
        printf(" title        '%s'\n", buffer);
        getstring(buffer, 8, p->episode);
        printf(" episode      '%s'\n", buffer);
        getstring(buffer, 20, p->facility);
        printf(" facility     '%s'\n", buffer);
        getstring(buffer, 40, p->misc);
        printf(" misc         '%s'\n", buffer);
      }
      break;
    case 0x11:
      printf("TEMPORAL\n");
      {
        rp215_temporal_11 * p = (rp215_temporal_11 *)&ancdata[offset];
        printf(" videotc      %02x:%02x:%02x:%02x\n", p->videotc[0], p->videotc[1], p->videotc[2], p->videotc[3]);
        tmp = 0x01000000*p->videoub[0] + 0x010000 * p->videoub[1] + 0x0100 * p->videoub[2] + p->videoub[3];
        printf(" videoub      %02x%02x%02x%02x (%d)\n", p->videoub[0], p->videoub[1], p->videoub[2], p->videoub[3], tmp);
        printf(" audiotc      %02x:%02x:%02x:%02x\n", p->audiotc[0], p->audiotc[1], p->audiotc[2], p->audiotc[3]);
        tmp = 0x01000000*p->audioub[0] + 0x010000 * p->audioub[1] + 0x0100 * p->audioub[2] + p->audioub[3];
        printf(" audioub      %02x%02x%02x%02x (%d)\n", p->audioub[0], p->audioub[1], p->audioub[2], p->audioub[3], tmp);
        printf(" audiophase   %02x\n", p->audiophase);
        printf(" audiomod     %02x\n", p->audiomodulus);
      }
      break;
    case 0x21:
      printf("MATERIAL\n");
      {
        rp215_material_21 * p = (rp215_material_21 *)&ancdata[offset];
        printf(" tags         %02x%02x%02x%02x\n", p->tags[0], p->tags[1], p->tags[2], p->tags[3]);
        printf(" syncmark     %02x%02x\n", p->syncmark[0], p->syncmark[1]);
        printf(" proddate     %02x%02x%02x%02x\n", p->proddate[0], p->proddate[1], p->proddate[2], p->proddate[3]); 
        getstring(buffer, 8, p->camroll);
        printf(" camroll      '%s'\n", buffer);
        getstring(buffer, 8, p->soundroll);
        printf(" soundroll    '%s'\n", buffer);
        getstring(buffer, 8, p->scene);
        printf(" scene        '%s'\n", buffer);
        getstring(buffer, 4, p->take);
        printf(" take         '%s'\n", buffer);
        getstring(buffer, 8, p->slate);
        printf(" slate        '%s'\n", buffer);
      }
      break;
    case 0x31:
      printf("LABEL\n");
      {
        rp215_label_31 * p = (rp215_label_31 *)&ancdata[offset];
        getstring(buffer, 8, p->title);
        printf(" title        '%s'\n", buffer);
        getstring(buffer, 8, p->episode);
        printf(" episode      '%s'\n", buffer);
        getstring(buffer, 4, p->crewid);
        printf(" crewid       '%s'\n", buffer);
        getstring(buffer, 4, p->operatorid);
        printf(" operatorid   '%s'\n", buffer);
        getstring(buffer, 4, p->directorid);
        printf(" directorid   '%s'\n", buffer);
        getstring(buffer, 4, p->producerid);
        printf(" producerid   '%s'\n", buffer);
      }
      break;
    case 0x40:
      printf("CAMERA\n");
      {
        rp215_camera_40 * p = (rp215_camera_40 *)&ancdata[offset];
        printf(" modelcode    %02x%02x\n", p->modelcode[0], p->modelcode[1]);
        printf(" serialnumber %02x%02x%02x%02x\n", p->serialnumber[0], p->serialnumber[1], p->serialnumber[2], p->serialnumber[3]);
        tmp = 0x01000000*p->framenumber[0] + 0x010000 * p->framenumber[1] + 0x0100 * p->framenumber[2] + p->framenumber[3];
        printf(" framenumber  %02x%02x%02x%02x/%d\n", p->framenumber[0], p->framenumber[1], p->framenumber[2], p->framenumber[3], tmp);
        printf(" cameraformat %02x/%d\n", p->cameraformat[0], p->cameraformat[0]);
        printf(" stepshutter  %02x/%d\n", p->stepshutter[0], p->stepshutter[0]);
        printf(" mastergain   %02x/%d\n", p->mastergain[0], p->mastergain[0]);
        printf(" ndfilter     %02x/%d\n", p->ndfilter[0], p->ndfilter[0]);
        printf(" ccfilter     %02x/%d\n", p->ccfilter[0], p->ccfilter[0]);
        printf(" gamma        %02x/%d\n",  p->gamma[0], p->gamma[0]);
        printf(" presetmatrix %02x/%d\n", p->presetmatrix[0], p->presetmatrix[0]);
        printf(" gammatable   %02x/%d\n", p->gammatable[0], p->gammatable[0]);
        printf(" gammastd     %02x/%d\n", p->gammastandard[0], p->gammastandard[0]);
        printf(" gammauser    %02x/%d\n", p->gammauser[0], p->gammauser[0]);
        printf(" lfeframerate %02x/%d\n", p->lfeframerate[0], p->lfeframerate[0]);
        printf(" switch1      %02x/%d\n", p->switch1[0], p->switch1[0]);
        printf(" switch2      %02x/%d\n", p->switch2[0], p->switch2[0]);
        printf(" whitegain_r  %02x%02x\n", p->whitegain_r[0], p->whitegain_r[1]);
        printf(" whitegain_g  %02x%02x\n", p->whitegain_g[0], p->whitegain_g[1]);
        printf(" whitegain_b  %02x%02x\n", p->whitegain_b[0], p->whitegain_b[1]);
        printf(" gammablack   %02x%02x\n", p->gammablack[0], p->gammablack[1]);
        printf(" masterkneept %02x%02x\n", p->masterkneepoint[0], p->masterkneepoint[1]);
        printf(" masterkneesl %02x%02x\n", p->masterkneeslope[0], p->masterkneeslope[1]);
        printf(" mastergamma  %02x%02x\n", p->mastergamma[0], p->mastergamma[1]);
        printf(" detaillevel  %02x%02x\n", p->detaillevel[0], p->detaillevel[1]);
        printf(" blackmaster  %02x%02x\n", p->blackmaster[0], p->blackmaster[1]);
        printf(" blackgain_r  %02x%02x\n", p->blackgain_r[0], p->blackgain_r[1]);
        printf(" blackgain_g  %02x%02x\n", p->blackgain_g[0], p->blackgain_g[1]);
        printf(" blackgain_b  %02x%02x\n", p->blackgain_b[0], p->blackgain_b[1]);
      }
      break;
    case 0x50:
      printf("DOLLY\n");
      {
        rp215_dolly_50 * p = (rp215_dolly_50 *)&ancdata[offset];
        printf(" lensid       %02x%02x%02x%02x\n", p->lensid[0], p->lensid[1], p->lensid[2], p->lensid[3]);
        printf(" focallength  %02x%02x\n", p->focallength[0], p->focallength[1]);
        printf(" focalpos     %02x%02x\n", p->focalposition[0], p->focalposition[1]);
        printf(" iris         %02x%02x\n", p->iris[0], p->iris[1]);
        printf(" camheight    %02x%02x%02x%02x\n", p->camheight[0], p->camheight[1], p->camheight[2], p->camheight[3]);
        printf(" camtilt      %02x%02x\n", p->camtilt[0], p->camtilt[1]);
        printf(" camroll      %02x%02x\n", p->camroll[0], p->camroll[1]);
        printf(" campan       %02x%02x\n", p->campan[0], p->campan[1]);
      }
      break;
    case 0xf0:
      printf("USER\n");
      {
        rp215_user_f0 * p = (rp215_user_f0 *)&ancdata[offset];
        int i,j;
        for(j = 0; j < 66; j+=16) {
          printf(" %03x ", j);
          for(i = 0; (i < 16) && (i+j < 66); i++) {
            printf("%02x ", p->data[i+j]);
          }
          for(; (i < 16); i++) {
            printf("   ");
          }
          printf(" ");
          for(i = 0; (i < 16) && (i+j < 66); i++) {
            if((p->data[i+j] >= 32) && (p->data[i+j] < 127)) {
              printf("%c", p->data[i+j]);
            } else {
              printf(" ");
            }
          }
          printf("\n");
        }
      }
      break;
    default:
      printf("Unknown %02x\n", ancdata[offset]);
    }

    if(count > 0) {
      offset += count + 2;
    } else {
      offset = datacount;
    }
  }

  if(datacount) {
    printf("CRC-0x%02x%02x\n",ancdata[datacount-2], ancdata[datacount-1]);
  
    crc = rp215_crc(ancdata, datacount);
    tmp = (ancdata[datacount-2] << 8) | ancdata[datacount-1];
  
    if(tmp != crc) {
      printf("CRC Wrong Calc %04x != %04x\n", crc, tmp);
    }
  }
}


int rp215_create(unsigned char * ancdata, int frame, char pulldownphase, int field)
{
  rp215data * p = 0;
  int crc       = 0;
  int bit7      = 0;
  int bits0_6   = 0;

  p = (rp215data *)ancdata;

  memset(ancdata, 0, sizeof(rp215data));

  /*
  //  Determination of pulldown member of temporal group
  */
  switch(pulldownphase) {
  case 'A':
    bit7    = 0;
    bits0_6 = 1;
    break;
  case 'B':
    if(field == 2) {
      bit7  = 128;
    } else {
      bit7  = 0;
    }
    bits0_6 = 2;
    break;
  case 'C':
    bit7    = 128;
    bits0_6 = 3;
    break;
  case 'D':
    if(field == 0) {
      bit7  = 128;
    } else {
      bit7  = 0;
    }
    bits0_6 = 4;
    break;
  }

  /*
  //  Add 1000 to frame, and fill in some data into all text fields.
  */
  frame += 1000;

  /*
  //  Temporal group
  */
  p->temporal.groupid     = 0x10;
  p->temporal.grouplength = sizeof(p->temporal)-2;
  rpvalue(p->temporal.videotc,    4, 0);
  rpvalue(p->temporal.videoub,    4, frame);
  rpvalue(p->temporal.audiotc,    4, 0);
  rpvalue(p->temporal.audioub,    4, frame);
  rpvalue(p->temporal.absframes,  4, frame);
  p->temporal.kkmfg      = 'K';
  p->temporal.kkemulsion = 'L';
  p->temporal.kk[0] = 0;
  p->temporal.kk[1] = 0;
  p->temporal.kk[2] = 0x12;
  p->temporal.kk[3] = 0x34;
  p->temporal.kk[4] = 0x56;
  p->temporal.kk[5] = 0x10 * ((frame / 100000) % 10) + ((frame / 10000) % 10);
  p->temporal.kk[6] = 0x10 * ((frame /   1000) % 10) + ((frame /   100) % 10);
  p->temporal.kk[7] = 0x10 * ((frame /     10) % 10) + ((frame        ) % 10);
  p->temporal.pulldown = (bit7 | bits0_6);
  p->temporal.sequence = (field + 1);

  /*
  //  Material group
  */
  p->material.groupid     = 0x20;
  p->material.grouplength = sizeof(p->material)-2;
  rpvalue(p->material.tags,       4, 0x87654321);
  rpvalue(p->material.flags,      3, 0x123456);
  rpstring(p->material.equipid,   4, "equipid");
  rpvalue(p->material.proddate,   4, 0x20080101);
  rpstring(p->material.dailyroll, 8, "dailyroll");
  rpstring(p->material.camroll,   8, "camroll");
  rpstring(p->material.soundroll, 8, "soundroll");
  rpstring(p->material.labroll,   8, "labroll");
  rpstring(p->material.scene,     8, "scene");
  rpstring(p->material.take,      4, "take");
  rpstring(p->material.slate,     8, "slate");

  /*
  //  Label group
  */
  p->label.groupid     = 0x30;
  p->label.grouplength = sizeof(p->label)-2;
  rpstring(p->label.vtroll,       8, "vtroll");
  rpstring(p->label.facility,    20, "facility");
  rpstring(p->label.episode,      8, "episode");
  rpstring(p->label.title,       20, "title");
  rpstring(p->label.misc,        40, "rp215 test with DVS SDK dpxio");

  crc = rp215_crc(ancdata, sizeof(rp215data));
  p->crc[0] = (crc & 0xff00) >> 8;
  p->crc[1] = (crc & 0x00ff);

  return sizeof(rp215data);
}


int rp215a_create(unsigned char * ancdata, int frame, int length)
{
  rp215adata * p = (rp215adata *) ancdata;
  int crc;

  memset(ancdata, 0, sizeof(rp215adata));

#ifdef _DEBUG
  if(sizeof(p->temporal)-2 != 18) {
    printf("rp215a_create: sizeof(p->temporal) wrong\n");
  }
  if(sizeof(p->material)-2 != 46) {
    printf("rp215a_create: sizeof(p->material) wrong\n");
  }
  if(sizeof(p->label)-2 != 44) {
    printf("rp215a_create: sizeof(p->label) wrong\n");
  }
  if(sizeof(p->camera)-2 != 47) {
    printf("rp215a_create: sizeof(p->camera) wrong\n");
  }
  if(sizeof(p->dolly)-2 != 20) {
    printf("rp215a_create: sizeof(p->dolly) wrong\n");
  }
  if(sizeof(p->user)-2 != 66) {
    printf("rp215a_create: sizeof(p->user) wrong\n");
  }
  if(sizeof(*p) != 255) {
    printf("rp215a_create: sizeof(*p) wrong\n");
  }
#endif

  /*
  //  Add 1000 to frame, and fill in some data into all text fields.
  */
  frame += 1000;

  /*
  //  Temporal group
  */
  p->temporal.groupid     = 0x11;
  p->temporal.grouplength = sizeof(p->temporal)-2;
  rpvalue(p->temporal.videotc,    4, 0);
  rpvalue(p->temporal.videoub,    4, frame);
  rpvalue(p->temporal.audiotc,    4, 0);
  rpvalue(p->temporal.audioub,    4, frame);
  p->temporal.audiophase     = 1;
  p->temporal.audiomodulus   = 2;

  /*
  //  Material group
  */
  p->material.groupid     = 0x21;
  p->material.grouplength = sizeof(p->material)-2;
  rpvalue(p->material.tags,       4, 0x87654321);
  rpvalue(p->material.proddate,   4, 0x20060713);
  rpstring(p->material.camroll,   8, "camroll");
  rpstring(p->material.soundroll, 8, "soundroll");
  rpstring(p->material.scene,     8, "scene");
  rpstring(p->material.take,      4, "take");
  rpstring(p->material.slate,     8, "slate");

  /*
  //  Label group
  */
  p->label.groupid     = 0x31;
  p->label.grouplength = sizeof(p->label)-2;
  rpstring(p->label.title,       20, "title");
  rpstring(p->label.episode,      8, "episode");
  rpstring(p->label.crewid,       4, "crew");
  rpstring(p->label.operatorid,   4, "oper");
  rpstring(p->label.directorid,   4, "dire");
  rpstring(p->label.producerid,   4, "prod");

  /*
  //  Camera group
  */
  p->camera.groupid     = 0x40;
  p->camera.grouplength = sizeof(p->camera)-2;
  rpvalue(p->camera.modelcode,        2, 0x9876);
  rpvalue(p->camera.serialnumber,     4, 0x12345678);
  rpvalue(p->camera.framenumber,      4, 1000);
  rpvalue(p->camera.cameraformat,     1, 0x10);
  rpvalue(p->camera.stepshutter,      1, 0x20);
  rpvalue(p->camera.mastergain,       1, 0x30);
  rpvalue(p->camera.ndfilter,         1, 0x40);
  rpvalue(p->camera.ccfilter,         1, 0x50);
  rpvalue(p->camera.gamma,            1, 0x60);
  rpvalue(p->camera.presetmatrix,     1, 0x70);
  rpvalue(p->camera.gammatable,       1, 0x80);
  rpvalue(p->camera.gammastandard,    1, 0x90);
  rpvalue(p->camera.gammauser,        1, 0xa0);
  rpvalue(p->camera.lfeframerate,     1, 0xb0);
  rpvalue(p->camera.switch1,          1, 0xff);
  rpvalue(p->camera.switch2,          1, 0xff);
  rpvalue(p->camera.whitegain_r,      2, 0x1234);
  rpvalue(p->camera.whitegain_g,      2, 0x5678);
  rpvalue(p->camera.whitegain_b,      2, 0x9012);
  rpvalue(p->camera.gammablack,       2, 0x3456);
  rpvalue(p->camera.masterkneepoint,  2, 0x7890);
  rpvalue(p->camera.masterkneeslope,  2, 0x1234);
  rpvalue(p->camera.mastergamma,      2, 0x5678);
  rpvalue(p->camera.detaillevel,      2, 0x9012);
  rpvalue(p->camera.blackmaster,      2, 0x3456);
  rpvalue(p->camera.blackgain_r,      2, 0x7890);
  rpvalue(p->camera.blackgain_g,      2, 0x1234);
  rpvalue(p->camera.blackgain_b,      2, 0x5678);

  /*
  //  Dolly group
  */
  p->dolly.groupid     = 0x50;
  p->dolly.grouplength = sizeof(p->dolly)-2;
  rpvalue(p->dolly.lensid,        4, 0x12345678);
  rpvalue(p->dolly.focallength,   2, 0x9012);
  rpvalue(p->dolly.focalposition, 2, 0x3456);
  rpvalue(p->dolly.iris,          2, 0x7890);
  rpvalue(p->dolly.camheight,     4, 0x12345678);
  rpvalue(p->dolly.camtilt,       2, 0x9012);
  rpvalue(p->dolly.camroll,       2, 0x3456);
  rpvalue(p->dolly.campan,        2, 0x7890);


  /*
  //  User group
  */
  if(length > 189) { /* 185 length + id + length + data == 190 min length */
    if(length >= 255) {
      length = 255;
    }
    p->user.groupid     = 0xf0;
    p->user.grouplength = length - 189;
    rpstring(p->user.data, 66, "rp215a test with DVS SDK dpxio");
  } else {
    length = 187;
  }

  crc = rp215_crc(ancdata, length);
  ancdata[length-2] = (crc & 0xff00) >> 8;
  ancdata[length-1] = (crc & 0x00ff);

  return length;
}


int dvstimecode_decode(unsigned char * data, unsigned int * ptimecode, int * pframe, int * pchannel)
{
  int ok = TRUE;
  int i;
  int crc;

  if((data[0] != 0xff) || (data[1] != 0xaa)) {
    ok = FALSE;
  }

  for(crc = i = 0; i < 14; i++) {
    crc += data[i];
  }
  if(crc != data[14] * 0x100 + data[15]) {
    ok = FALSE;
  }

  if(ptimecode) {
    if(ok) {
      *ptimecode = (data[2] << 24) | (data[3] << 16) | (data[4] << 8) | (data[5]);
    } else {
      *ptimecode = 0;
    }
  } 
  
  if(pframe) {
    if(ok) {
      *pframe = (data[6] << 24) | (data[7] << 16) | (data[8] << 8) | (data[9]);
    } else {
      *pframe = 0;
    }
  } 

  if(pchannel) {
    if(ok) {
      *pchannel = data[10];
    } else {
      *pchannel = 0;
    }
  }

  return ok;
}


int dvstimecode_image(unsigned char * buffer, int xsize, int storagemode, unsigned int * ptimecode, int * pframenr)
{
  int ok  = TRUE;
  unsigned char data[16];
  unsigned char * ptr;
  int inc = 0;
  int i,j;
  int red,grn,blu;
 
  switch(storagemode & (SV_MODE_COLOR_MASK | SV_MODE_NBIT_MASK)) {
  case SV_MODE_COLOR_RGB_BGR:
  case SV_MODE_COLOR_RGB_RGB:
    inc = 3 * 4;
    break;
  case SV_MODE_COLOR_RGB_RGB | SV_MODE_NBIT_10BDPX:
  case SV_MODE_COLOR_RGB_BGR | SV_MODE_NBIT_10BDPX:
    inc = 4 * 4;
    break;
  case SV_MODE_COLOR_YUV422 | SV_MODE_NBIT_8B:
    inc = 4 * 2;
    break;
  default:
    ok = FALSE;
  }
  /*
  //  Jump to the middle without, eg 8 lines down.
  */
  if(ok) {
    ptr = buffer + 2 * inc * xsize + inc / 2;
    for(i = 0; i < 16; i++) {
      data[i] = 0;
      for(j = 7; j >= 0; j--) {
        switch(storagemode & (SV_MODE_COLOR_MASK | SV_MODE_NBIT_MASK)) {
        case SV_MODE_COLOR_RGB_BGR:
        case SV_MODE_COLOR_RGB_RGB:
          red  = ptr[0];
          grn  = ptr[1];
          blu  = ptr[2];
          break;
        case SV_MODE_COLOR_RGB_RGB | SV_MODE_NBIT_10BDPX:
        case SV_MODE_COLOR_RGB_BGR | SV_MODE_NBIT_10BDPX:
          red  = ((ptr[0])                 ) & 0xff;
          grn  = ((ptr[1]<<2) | (ptr[2]>>6)) & 0xff;
          blu  = ((ptr[2]<<4) | (ptr[3]>>4)) & 0xff;
          break;
        case SV_MODE_COLOR_YUV422 | SV_MODE_NBIT_8B:
          red = blu = ptr[0]; grn = 0;
          break;
        default:
          red = grn = blu = 0;
        }
        ptr += inc;

        if((red > 0xa0) && (blu > 0xb0) && (grn < 0x40)) {
          data[i] |=  (1<<j);
        }
      }
    }
  }

  if(ok) {
    ok = dvstimecode_decode(&data[0], ptimecode, pframenr, NULL);
  } else {
    if(ptimecode) {
      *ptimecode = 0;
    }
    if(pframenr) {
      *pframenr = 0;
    }
  }

  return ok;
}


void dvstimecode_audio(char * buffer, int buffersize, unsigned int * ptimecode, int *psampleoffset, int channels)
{
  uint8 data[16][16];
  int tmp[16];
  int s,c,b,foundthis;
  int ok;
  int timecodesync[16] = { 0 };
  int timecodestart[16] = { 0 };
  int littleendian = TRUE;
  int bits         = 32;
  int samples      = buffersize / channels / 4;

  for(c = 0; c < 16; c++) {
    ptimecode[c] = 0;
    psampleoffset[c] = 0;
  }

  for(s = 0; (s < samples); s++) {
    foundthis = FALSE;
    for(c = 0; c < channels; c++) {
      if(littleendian) {
        for(b = 0; b < bits; b+=8) {
          tmp[(bits-b-1)/8] = *buffer++;
        }
      } else {
        for(b = 0; b < bits; b+=8) {
          tmp[b/8] = *buffer++;
        }
      }

      if(timecodesync[c] < 8) {
        memset(&data[c], 0, sizeof(data[0]));
        if(((tmp[0] & 0xff) == 0xff) && ((tmp[1] & 0xf0) == 0xf0)) {
          timecodesync[c]++;
        } else {
          timecodestart[c] = s - 63;
          timecodesync[c]  = 0;
        }
      } else if(timecodesync[c] < 128) {
        if((tmp[1] & 0xff ) < 0xf4) {
          data[c][timecodesync[c] / 8] |= (1 << (7 - (timecodesync[c] & 7)));
        }
        timecodesync[c]++;
      } else {
        int timecode;
        int framenr;
        int channel;

        data[c][0] = 0xff;
        ok = dvstimecode_decode(&data[c][0], (unsigned int *)&timecode, &framenr, &channel);

        if(!ptimecode[c]) {
          if(ok) {
            ptimecode[c] = timecode;
          } else {
            ptimecode[c] = (unsigned int)~0;
          }
        }
        if(!psampleoffset[c]) {
          if(ok) {
            psampleoffset[c] = timecodestart[c];
          } else {
            psampleoffset[c] = 0;
          }
        }
        timecodesync[c] = 0;
        foundthis    = TRUE;
      }
    }
  }
}


void dpxio_audio_timecode(unsigned char * buffer, int samples, int channels, unsigned int *ptimecode, int * poffset) 
{
  dvstimecode_audio((char *)buffer, samples * channels * 4, ptimecode, poffset, channels);
}


int dpxio_dpx_timecode(unsigned char * buffer, int dpxtype, int xsize) 
{
  unsigned int timecode;
  int framenr;
  int storagemode;

  switch (dpxtype) {
  case 100:
    storagemode = SV_MODE_COLOR_YUV422 | SV_MODE_NBIT_8B;
    break;

  case 50:
  default:
    storagemode = SV_MODE_COLOR_RGB_RGB |SV_MODE_NBIT_10BDPX;
    break;
  }

  dvstimecode_image(buffer, xsize, storagemode, &timecode, &framenr);
  return timecode;
}
