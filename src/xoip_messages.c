/*
 * XoIP -- telephony toolkit.
 *
 * Copyright (C) <2014>, <vassilux>
 *
 * <Vassili Gontcharov> <v.gontcharov@gmail.com>
 *
 */

/*! \file
 *
 * \brief Xoipeton application
 *
 * \author\verbatim <Vassili Gontcharov> <vassili.gontcharov@esifrance.net> \endverbatim
 * 
 * This is a Xoip for development of an Asterisk application 
 * \ingroup applications
 */

/*** MODULEINFO
	<defaultenabled>no</defaultenabled>
	<support_level>core</support_level>
 ***/

#include <stdio.h>
#include <stdlib.h>
#include "asterisk.h"
#include "asterisk/logger.h"
//#include "asterisk/astmm.h"

#include "xoip_messages.h"

int do_process_message (const char *buf, int size,
			struct f1_messages_handlers *handlers);

static void do_process_message_fa(int track, int callref, char *code, struct f1_messages_handlers *handlers)
{
    if(code == NULL){
        return;
    }

    if(code[0] == 'M'){
        handlers->mute_micro_operator(track, callref, true);
    }else if(code[0] == 'N'){
        handlers->mute_micro_operator(track, callref, false);
    }
}

/*!
 * \brief Intiaialize the emmessions configuration for the given communication.
 *
 * \param track The track number.
 * \param callref The call reference.
 * \param messages The string array of parameters.
 * \param handlers The pointer to the wrapper of the app_xoip functions.
 */
static void do_process_message_fc(int track, int callref, char messages[10][255], struct f1_messages_handlers *handlers)
{
    char *emmission_type = messages[3];
    int dtmf_duration = 0;
    int silence_duration = 0;
    int loudness = 0;

    if(emmission_type != NULL &&(strcmp(emmission_type, "DTMF") == 0)){
        if(messages[4] != NULL){
            /* 
             * pay an attention cause the F1 send packege without , between DTMF
             * and duration
             */
            dtmf_duration = atoi(messages[4] + 4); // move 4 to skip DTMF string
        }

        if(messages[5] != NULL){
            silence_duration = atoi(messages[5]);
        }

        if(messages[6] != NULL){
            loudness = atoi(messages[6]);
        }
    }

    
}

/*!
 * \brief Send the frequencies to th] channel identified by the given track and callref
 */
static void do_process_message_ff(int track, int callref, char messages[10][255], struct f1_messages_handlers *handlers)
{
    int freq;
    int duration;
    int level;
    char *mode = NULL;
    char *break_record;

    if(messages[3] != NULL){
        freq = atoi(messages[3]);
    }

    if(messages[4] != NULL){
        duration = atoi(messages[4]);
    }

    if(messages[5] != NULL){
        level = atoi(messages[5]);
    }

    mode = messages[6];
    break_record = messages[6];

    handlers->send_freq(track, callref, freq, duration, level, mode, break_record);
}

/*!
 * \brief Process FL message. Send data to the channel identificated by the given track and callref
 */
static void do_process_message_fl(int track, int callref, char messages[10][255], struct f1_messages_handlers *handlers)
{
    int mode_transfer;
    char *data;
    char *mode_operator = NULL;
    char *break_record;

    if(messages[3] != NULL){
        mode_transfer = atoi(messages[3]);
    }

    data = messages[4];

    mode_operator = messages[5];
    break_record = messages[6];

    handlers->send_data(track, callref, mode_transfer, data, mode_operator, break_record);
}

/*!
 *
 */
int do_process_message (const char *buf, int size,
		    struct f1_messages_handlers *handlers)
{
  int res = 0;
  char *buffer = strdup (buf);
  const char *sep = ",";

  char messages[10][255] = { '\0' };
  int count = 0;

  ast_verb(5, "Get packet from f1 endpoint :  [%s\n]", buffer);

  char *token = NULL;

  for (token = strtok (buffer, sep); token; token = strtok (NULL, sep))
    {
      if (token != NULL)
	{
	  strcpy (messages[count], token);
	  count++;
	}
    }

  char type = messages[0][2];
  /* this is special case for ithe polling and mode request messages */
  if(messages[0][0] == 'R' && type == 'S'){
      handlers->request_reboot();
      goto goout;
  }else if(type == 'N'){
      /* skip this one */
      handlers->request_mode_normal();
      goto goout;
  }else if(messages[0][0]== 'V' && type == 'R'){
      goto goout;
  }else if(type == 'P'){
      ast_verb(5, "Get polling [%s].\n", messages[3]);
      handlers->polling(messages[3]);
      goto goout;
  }
  int track = atoi (messages[1]);
  long callref = 0;
  sscanf(messages[2], "%04X", &callref);
  /* so far sor good */
  switch (type)
    {
    case 'A':
        do_process_message_fa(track, callref, messages[3], handlers);
        break;
    case 'C':
        do_process_message_fc(track, callref, messages, handlers);
        break;
    case 'F':
        do_process_message_ff(track, callref, messages, handlers);
        break;
    case 'G':
      {
	char proto = messages[3][0];
	int level = 0;
	if (messages[4] != NULL)
	  {
	    level = atoi (messages[4]);
	  }
	handlers->answer (track, callref, proto, level);
      }
      break;
    case 'H':
      {
	char *callee = messages[3];
	char *caller = messages[4];
	char *volum = messages[5];
	handlers->commut (track, callref, callee, caller, volum);
      }
      break;
    case 'I':
      {
        char proto = messages[3][0];
        int loudness = 0;
        if(messages[4] != NULL){
            loudness = atoi(messages[4]);
        }
        handlers->switch_protocol(track, callref, proto, loudness);
      }
      break;
    case 'K':
      handlers->ack_alarm(track, callref);
      break;
    case 'L':
       do_process_message_fl(track, callref, messages, handlers);
      break;
    case 'M':
      {
          int record_track = 0;
          if(messages[3] != NULL){
              record_track = atoi(messages[4]);
          }
          handlers->record_request(track, callref, record_track);
      }
      break;
    case 'r':
    case 'R':
      //ast_log(AST_LOG_VERBOSE, " Processing R.\n");
      handlers->hangup (track, callref);
      break;
    case 'V':
      {
        int volume;
        if(messages[3] != NULL){
          volume = atoi(messages[3]);
        }
        handlers->volume_adjust(track, callref, volume);
    }
      break;
    case 'W':
      handlers->queuing_call(track, callref, messages[3]);
      break;
    case 'U':
      break;
    default:
      ast_log (AST_LOG_VERBOSE, " Processing but type not found : [%c]..\n",
	       buf);
      break;
    }
  goto goout;

goout:
  res = size;
  free (buffer);
  return res;

}

/*!
 *
 */
int xoip_build_XC_msg (int voie, int callref, const char *did,
		   const char *caller, const char *transfered, char *dest,
		   int size)
{
  snprintf (dest, size - 1, "X C,%02d,%04X,%s,%s,T%s\r",
	    voie, callref, did, caller, transfered);
  if(strlen(dest) == 0){
      return -1;
  }
  return 0;

}



/*!
 *
 */
int xoip_build_Xg_msg (int voie, int callref, int state, int res, char *dest, int size)
{
     if(res != 0){
         snprintf (dest, size - 1, "X g,%02d,%04X,1,%01d\r",
	    voie, callref, res);

     }else{
         snprintf (dest, size - 1, "X g,%02d,%04X,%d\r",
	    voie, callref,state);
    }
    
    if(strlen(dest) == 0){
      return -1;
    }

    return 0;
}

/*!
 * 
 */
int xoip_build_XL_msg (int voie, int callref, int res, char *dest, int size)
{
    if(res != 0){
         snprintf (dest, size - 1, "X L,%02d,%04X,1,%01d\r",
	    voie, callref, res);
    }else{
         snprintf (dest, size - 1, "X L,%02d,%04d\r",
	    voie, callref);
    }
    
    if(strlen(dest) == 0){
      return -1;
    }

    return 0;
}

/*!
 *
 */
int xoip_build_Xm_msg(int voie, int callref, int state, int res, char *dest, int size)
{
    if(res != 0){
         snprintf (dest, size - 1, "X m,%02d,%04X,1,%01d\r",
	    voie, callref, res);
    }else{
         snprintf (dest, size - 1, "X m,%02d,%04d\r",
	    voie, callref);
    }
    
    if(strlen(dest) == 0){
      return -1;
    }

    return 0;
}

/*!
 *
 */
int xoip_build_XE_msg(int status, char *dest, int size)
{
    snprintf (dest, size - 1, "X E,%01d,%s\r",
	    status, "XoIP Alarms-V0.2.0.80,Dem V0000,Mod V0000");
    if(strlen(dest) == 0){
      return -1;
    }

    return 0;

}

/*!
 * 
 */
int xoip_build_Xr_msg (int voie, int callref, char *dest, int size)
{
  snprintf (dest, size, "X r,%02d,%04d\r", voie, callref);
  return 0;
}

/*!
 *
 */
int xoip_build_Xh_msg (int voie, int callref, int state, const char *callee,
		   char *dest, int size)
{
  snprintf (dest, size, "X h,%02d,%04X,%01d,%s\r",
	    voie, callref, state, callee);
  return 0;
}

/*!
 *
 */
int xoip_build_Xi_msg(int voie, int callref, int state, int res, char *dest, int size)
{
     snprintf (dest, size - 1, "X i,%02d,%04X,%01d,%01d\r",
	    voie, callref, state, res);
    
    if(strlen(dest) == 0){
      return -1;
    }

    return 0;
}

/*!
 *
 */
int xoip_build_Xk_msg(int voie, int callref, int state, int res, char *dest, int size)
{
     snprintf (dest, size - 1, "X k,%02d,%04X,%01d,%01d\r",
	    voie, callref, state, res);
    
    if(strlen(dest) == 0){
      return -1;
    }

    return 0;
}


/*!
 *
 */
int xoip_build_Xf_msg (int voie, int callref, int state, int res, char *dest, int size)
{
   snprintf (dest, size - 1, "X f,%02d,%04X,%01d,%01d\r",
	    voie, callref, state, res);
    
    if(strlen(dest) == 0){
      return -1;
    }

    return 0;

}


/*!
 *
 */
int xoip_build_XA_msg (int voie, int callref, const char *data, char *dest, int size)
{
    snprintf (dest, size, "X A,%02d,%04X,%s\r",
	    voie, callref, data); 
    return 0;
}

/*!
 * \brief Build X w message.
 * The response for f1 F W request : qeueuing a call
 */
int xoip_build_Xw_msg(int voie, int callref, int res, char *dest, int size)
{
    snprintf (dest, size - 1, "X w,%02d,%04X,%01d\r",
	    voie, callref, res);
    
    if(strlen(dest) == 0){
      return -1;
    }

    return 0;
}



/*!
 * \brief Parsing message from the given buffer.
 * Buffer can content few messges 
 *
 * \param buf messages buffer
 * \param size the length of the buffer
 * \params handlers the structur of callback for each type of message
 *
 * \retval -1 on error
 * \retval > 0 the lenth of processing messages
 */
int process_message (const char *buf, int size,
		 struct f1_messages_handlers *handlers)
{
  int res = 0;
  int i = 0;
  int count = 0;
  char *buffer = strdup (buf);
  const char *sep = "\r";
  char *token = NULL;
  /* must be changed */
  char messages[10][255] = { '\0' };

  for (token = strtok (buffer, sep); token; token = strtok (NULL, sep))
    {
      strcpy (messages[count], token);
      count++;
    }
  res = 0;
  for (i = 0; i < count; i++)
    {
      /* increment by the token length */
      res += do_process_message (messages[i], strlen (messages[i]), handlers);
    }
  /* add to the result the number of tokens. May be addt length(sep) can be good idea */
  res += count;
  free (buffer);
  return res;
}



