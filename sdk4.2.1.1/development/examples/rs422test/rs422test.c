/*
//    Part of the DVS (http://www.dvs.de) *DVS*VERSION* SDK 
//
*/

#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include "../../header/dvs_setup.h"
#include "../../header/dvs_clib.h"

#define READ 0
#define WRITE 1
#define FALSE 0
#define TRUE  1

typedef struct {
  int       cmd;
  char *    name;
} rs422test_cmd;


static rs422test_cmd rs422test_cmds[] = {  
  { 0x0000, "unknown",              },

  // Group 00 commands
	
  { 0x000C, "local_disable",        },
  { 0x0011, "device_type",          },
  { 0x001D, "local_enable",         },
  { 0x00F0, "dvs_command",          },
  { 0x00F1, "dvs_sense",            },	

  // Group 01 replies

  { 0x0101, "ACK",                  },
  { 0x0111, "DEVICETYPE",           },
  { 0x0112, "NAK",                  }, 

  // Group 02 commands

  { 0x0200, "stop",                 },
  { 0x0201, "play",                 },
  { 0x0202, "rec",                  },
  { 0x0204, "standby_off",          },
  { 0x0205, "standby_on",           },
  { 0x020D, "dmc_start",            },
  { 0x020F, "eject",                },
  { 0x0210, "fast_fwd",             },
  { 0x0211, "jog_fwd",              },
  { 0x0212, "var_fwd",              },
  { 0x0213, "shuttle_fwd",          },
  { 0x0214, "step_fwd",             },
  { 0x0215, "var_fwd_sync_play",    },
  { 0x021F, "record_next",          },
  { 0x0220, "rewind",               },
  { 0x0221, "jog_rev",              },
  { 0x0222, "var_rev",              },
  { 0x0223, "shuttle_rev",          },
  { 0x0224, "step_rev",             },
  { 0x0225, "var_rev_sync_play",    },
  { 0x0230, "preroll",              },
  { 0x0231, "cue_up",               },
  { 0x0232, "sync_preroll",         },
  { 0x0234, "sync_play",            },
  { 0x0238, "prog_speed_pos",       },
  { 0x0239, "prog_speed_neg",       },
  { 0x023A, "fwd_sync_preroll",     },
  { 0x023B, "rev_sync__preroll",    },
  { 0x023C, "dmc_preroll",          },
  { 0x0240, "preview",              },
  { 0x0241, "review",               },
  { 0x0242, "auto_edit",            },
  { 0x0243, "outpoint_preview",     },
  { 0x024B, "dmc_run",              },
  { 0x024C, "dmc_preview",          },
  { 0x0252, "tension_release",      },
  { 0x0254, "anticlogtimer_disable",},
  { 0x0255, "anticlogtimer_enable", },
  { 0x025C, "dmc_set_fwd",          },
  { 0x025D, "dmc_set_rev",          },
  { 0x0260, "full_ee_off",          },
  { 0x0261, "full_ee_on",           },
  { 0x0263, "select_ee_on",         },
  { 0x0264, "edit_off",             },
  { 0x0265, "edit_on",              },
  { 0x026A, "freeze_off",           },
  { 0x026B, "freeze_on",            },
	
  // Group 04 commands

  { 0x0400, "timer1_preset",        },
  { 0x0404, "timecode_preset",      },
  { 0x0405, "userbit_preset",       },
  { 0x0408, "timer1_reset",         },
  { 0x040E, "tgc_hold",             },
  { 0x040F, "tgc_run",              },
  { 0x0410, "in_entry",             },
  { 0x0411, "out_entry",            },
  { 0x0412, "ain_entry",            },
  { 0x0413, "aout_entry",           },
  { 0x0414, "in_data_preset",       },
  { 0x0415, "out_data_preset",      },
  { 0x0416, "ain_data_preset",      },
  { 0x0417, "aout_data_preset",     },
  { 0x0418, "in_pos_shift",         },
  { 0x0419, "in_neg_shift",         },
  { 0x041A, "out_pos_shift",        },
  { 0x041B, "out_neg_shift",        },
  { 0x041C, "ain_pos_shift",        },
  { 0x041D, "ain_neg_shift",        },
  { 0x041E, "aout_pos_shift",       },
  { 0x041F, "aout_neg_shift",       },
  { 0x0420, "in_flag_reset",        },
  { 0x0421, "out_flag_reset",       },
  { 0x0422, "ain_flag_reset",       },
  { 0x0423, "aout_flag_reset",      },
  { 0x0424, "in_flag_recall",       },
  { 0x0425, "out_flag_recall",      },
  { 0x0426, "ain_flag_recall",      },
  { 0x0427, "aout_flag_recall",     },
  { 0x042D, "lost_lock_reset",      },
  { 0x0430, "edit_preset",          },
  { 0x0431, "prerolltime_preset",   },
  { 0x0432, "tape_auto_select",     },
  { 0x0433, "servo_ref_select",     },
  { 0x0434, "head_select",          },
  { 0x0435, "colorframe_select",    },
  { 0x0436, "timermode_select",     },
  { 0x0437, "input_check",          },
  { 0x043A, "edit_field_sel",       },
  { 0x043B, "freeze_mode_sel",      },
  { 0x043D, "preread_mode_sel",     },
  { 0x043E, "rec_inhibit",          },
  { 0x0440, "auto_mode_off",        },
  { 0x0441, "auto_mode_on",         },
  { 0x0442, "spot_erase_off",       },
  { 0x0443, "spot_erase_on",        },
  { 0x0444, "audio_split_off",      },
  { 0x0445, "audio_split_on",       },
  { 0x0446, "var_mem_off",          },
  { 0x0447, "var_mem_on",           },
  { 0x0448, "vid_ref_disable_off",  },
  { 0x0449, "vid_ref_disable_on",   },
  { 0x0450, "da_input_select",      },
  { 0x0451, "da_emphasis_preset",   },
  { 0x0498, "output_h_phase",       },
  { 0x049B, "output_video_phase",   },
  { 0x04A0, "audio_input_level",    },
  { 0x04A1, "audio_output_level",   },
  { 0x04A2, "audio_advance_level",  },
  { 0x04A8, "audio_output_phase",   },
  { 0x04A9, "audio_advance_phase",  },
  { 0x04B8, "local_key_map_control",},
  { 0x04C0, "timeline_stop",        },
  { 0x04C1, "timeline_run",         },
  { 0x04C2, "timeline_source",      },
  { 0x04C3, "timeline_preset",      },
  { 0x04C4, "timeline_define_event",},
  { 0x04C5, "timeline_clear_event", },
  { 0x04F8, "still_off_time",       },
  { 0x04FA, "standby_off_time",     },

	
  // Group 06 commands: sense commands

  { 0x060A, "gentime_sense",        },
  { 0x060C, "current_time_sense",   },
  { 0x0610, "in_data_sense",        },
  { 0x0611, "out_data_sense",       },
  { 0x0612, "ain_data_sense",       },
  { 0x0613, "aout_data_sense",      },
  { 0x0620, "status_sense",         },
  { 0x0621, "ext_status_sense",     },
  { 0x0623, "signal_control_data_sense",},
  { 0x0624, "supported_signal_sense",},
  { 0x0625, "video_control_data_sense",},
  { 0x0626, "audio_control_data_sense",},
  { 0x0628, "local_key_map_sense",  },
  { 0x062A, "hm_data_sense",        },
  { 0x062B, "remain_time_sense",    },
  { 0x062E, "cmd_speed_sense",      },
  { 0x0630, "edit_preset_sense",    },
  { 0x0631, "preroll_time_sense",   },
  { 0x0632, "tapeauto_sense",       },
  { 0x0636, "timer_mode_sense",     },
  { 0x063E, "rec_inhibit_sense",    },
  { 0x06C2, "timeline_source_sense",},
  { 0x06C3, "timeline_time_sense",  },
  { 0x06C4, "timeline_event_sensev",},
  { 0x06C6, "timeline_status_sense",},
  { 0x06C7, "timeline_event_queue_sense",},
  { 0x06C8, "timeline_unsuccessful_sense",},

  // Group 07 replies

  { 0x0700, "TIMER1",               },
  { 0x0701, "TIMER2",               },
  { 0x0704, "LTC_TIMECODE",         },
  { 0x0705, "LTC_USERBYTES",        },
  { 0x0706, "VITC_TIMECODE",        },
  { 0x0707, "VITC_USERBYTES",       },
  { 0x0708, "GENERATOR_TIMECODE",   },
  { 0x0709, "GENERATOR_USERBYTES",  },
  { 0x0710, "INPOINT",              },
  { 0x0711, "OUTPOINT",             },
  { 0x0714, "CORRECTED_LTC_TC",     },
  { 0x0715, "HOLD_LTC_UB",          },
  { 0x0716, "CORRECTED_VITC_TC",    },
  { 0x0720, "STATUS",               },
  { 0x0721, "EXTSTATUS",            },
  { 0x072b, "REMAINTIME",           },
  { 0x072e, "CMDSPEED",             },
  { 0x0730, "EDITPRESET",           },
  { 0x0731, "PREROLLTIME",          },
  { 0x0736, "TIMERMODE",            },
  { 0x073e, "RECINHIBIT",           },

  // Group 08 commands
  { 0x0802, "ext_disable_nl_mode"   },
  { 0x0804, "ext_disable_loop_mode" },

  // Group 0A commands
  { 0x0A01, "ext_absolute_goto",    },
  { 0x0A02, "ext_absolute_record",  },
  { 0x0A05, "ext_record_frame_field",},
  { 0x0A07, "ext_determinate_loopplay",},
  { 0x0A09, "ext_determinate_play", },
  { 0x0A0A, "ext_determinate_rec",  },

  // Group 0E commands
  { 0x0E01, "ext_report_position", },
  { 0x0E20, "ext_extended_status_sense", },

  { 0xffff, "unknown",              },
};



int global_running = 1;

void signal_handler(int signum)
{
  printf("User terminated program.\n");

  global_running = 0;
}

/*-----------------------------------------------------------------------------------------------  
rs422test_cmd_find: Search the command name into the commands struct.
----------------------------------------------------------------------------------------------*/
char * rs422test_cmd_find(int command) 
{
  int up, down, half;
  down = 0;
  up = sizeof(rs422test_cmds)/sizeof(rs422test_cmds[0]);
  half = (down + up) / 2;

  while((down <= up) && (rs422test_cmds[half].cmd != command)) {
    if (command < rs422test_cmds[half].cmd) {
      up = half - 1;
    } else {
      down = half + 1;
    }
    half = (down + up) / 2;
  }

  if (command == rs422test_cmds[half].cmd) {
    return rs422test_cmds[half].name;
  } else {
    return "*** Unknown ";
  }
}


/*-----------------------------------------------------------------------------------------------  
rs422test_cmd_dump: print the command name.
----------------------------------------------------------------------------------------------*/
void rs422test_cmd_dump(sv_handle * sv, FILE * fstream, int tick, int tickus, int cmdus, int port, uint8 * buffer, int count) 
{
  char * string;
  int i;
  int crc;

  if(!fstream) {
    fstream = stdout;
  }

  for(crc = i = 0; i < count-1; i++) {
    crc += buffer[i];
  }
  if((crc & 0xff) != buffer[count-1]) {
    fprintf(fstream, "\nCRCERROR\n");
    if(fstream != stdout) {
      printf("%06d:CRCERROR\n", tick);
    }
  }

  string = rs422test_cmd_find(((buffer[0] & 0xf0) << 4) | (buffer[1]));

  if(!port) {
    if(cmdus < 9000) {
      fprintf(fstream, "   %06d ", cmdus);
    } else {
      fprintf(fstream, "\nTIMEOUT\n***%06d ", cmdus);
      if(fstream != stdout) {
        printf("%06d:TIMEOUT\n", tick);
      }
    }
  } else {
    fprintf(fstream, "%02d:%06d ", tick % 100, tickus);
  }

  fprintf(fstream, "%-20s", string);


  for(i = 0; i < count; i++) {
    fprintf(fstream, "%02x ", buffer[i]);
  }

  fprintf(fstream, "\n");

  if(fstream != stdout) {
    fflush(fstream);
  }
}



//----------------------------------------------main----------------------------------------------
int main(int argc, char ** argv)
{
  sv_handle * sv;
  FILE * fstream = NULL;      
  int port; 
  int res;
  int count;
  int us, tickus;
  int tick,oldtick = 0;
  int dump;
  int cmdus;
  unsigned char buffer[2][128];
  int bytecount[2];
  LARGE_INTEGER timeout[2];
  LARGE_INTEGER current;
  LARGE_INTEGER ticktime = {0};
  LARGE_INTEGER frequency;
  int port_base = 2;

  signal(SIGTERM, signal_handler);
  signal(SIGINT, signal_handler);

  if(argc > 1)  {
    if (strcmp(argv[1], "base") == 0) {
      port_base = 0;
    } else if ((fstream = fopen( "rs422.txt", "w" )) == NULL ) {
      printf( "The log file could not be created.\n" );
      exit(-1);
    } 
  } 

  res = sv_openex(&sv, "", SV_OPENPROGRAM_DEMOPROGRAM, SV_OPENTYPE_DEFAULT, 0, 0);
  if(res != SV_OK) {
    printf("rs422test: Error '%s' opening video device", sv_geterrortext(res));
    return -1;
  } 

  res = sv_rs422_open(sv, port_base + 0, 38400, 0);
  if (res != SV_OK ) {
    printf("port 2: sv_rs422_open(sv) failed %d '%s'", res, sv_geterrortext(res));
    sv_close(sv);
    return -1;  
  }
  
  res = sv_rs422_open(sv, port_base + 1, 38400, 0);
  if (res != SV_OK) {
    printf("port 3: sv_rs422_open(sv) failed %d '%s'", res, sv_geterrortext(res));
    sv_rs422_close(sv, port_base + 0);
    sv_close(sv);
    return -1;  
  }

  QueryPerformanceFrequency(&frequency);
  for(port = 0; port < 2; port++) {
    timeout[port].QuadPart = 0;
    bytecount[port] = 0;
  }
  memset(buffer, 0, sizeof(buffer));    

  do {
    res = sv_query(sv, SV_QUERY_TICK, 0, &tick); 
    if(res != SV_OK){ 
      printf("sv_query(SV_QUERY_TICK) failed %d '%s'", res, sv_geterrortext(res));
    }
    if(tick != oldtick) {
      QueryPerformanceCounter(&ticktime);
      oldtick = tick;
    }

    for(port = 0; (res == SV_OK) && (port < 2); port++) {
      res = sv_rs422_rw(sv, port_base+port, READ, (char*)&buffer[port][bytecount[port]], sizeof(buffer[0]) - bytecount[port], &count, 0); 
      if (res != SV_OK) {
        printf("sv_rs422_rw(READ,%d) failed %d '%s'\n", port_base + port, res, sv_geterrortext(res));
      }                                            

      if((res == SV_OK) && (count > 0)) {
        res = sv_rs422_rw(sv, port_base + !port, WRITE, (char*)&buffer[port][bytecount[port]], count, NULL, 0); 
        if (res != SV_OK) {
          printf("sv_rs422_rw(WRITE,%d) failed %d '%s'\n", port_base + !port, res, sv_geterrortext(res));
        }                                

        if(!bytecount[port]) {
          QueryPerformanceCounter(&timeout[port]);
        }

        bytecount[port] += count;
      }

      if((res == SV_OK) && bytecount[port]) {
        if(timeout[port].QuadPart) {
          QueryPerformanceCounter(&current);
          us = (1000000 * (current.QuadPart - timeout[port].QuadPart)) / frequency.QuadPart;

          dump = FALSE;
          if(us > 8000) {
            dump = TRUE;
          } else if(bytecount[port] >= 2) {
            if(((buffer[port][0] & 0xf) + 3) <= bytecount[port]) {
              dump = TRUE;
            }
          }

          if(dump) {
            tickus = (1000000 * (current.QuadPart - ticktime.QuadPart)) / frequency.QuadPart;
            cmdus  = (1000000 * (current.QuadPart - timeout[!port].QuadPart)) / frequency.QuadPart;

            rs422test_cmd_dump(sv, fstream, tick, tickus, cmdus, port, buffer[port], bytecount[port]);
            bytecount[port]        = 0;
          }
        }
      }
    }
  } while(global_running && res == SV_OK);

  sv_rs422_close(sv, port_base + 0);
  sv_rs422_close(sv, port_base + 1);
  sv_close(sv);

  return 0;	
}

