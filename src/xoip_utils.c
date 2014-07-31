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

#include <stdlib.h>

#include <math.h>
#include <sys/wait.h>
#include <sys/time.h>

#include "asterisk.h"
#include "asterisk/lock.h"
#include "asterisk/file.h"
#include "asterisk/channel.h"
#include "asterisk/pbx.h"
#include "asterisk/module.h"
#include "asterisk/translate.h"
#include "asterisk/ulaw.h"
#include "asterisk/app.h"
#include "asterisk/dsp.h"
#include "asterisk/indications.h"

#include "xoip_types.h"
#include "xoip_utils.h"


static int g_current_callref = 0x0;
static int g_current_track = 0x0;



/*!
 * \brief Generate a callref. 
 * This is naive way to do it.
 *
 */
int
get_current_callref (void)
{
  if (g_current_callref >= MAX_CALLREF)
    {
      g_current_callref = 0;
    }
  g_current_callref++;
  return g_current_callref;
}

/*!
 * \brief Generate a track number
 * This is naive way to do it.
 */
int
get_current_track (void)
{
  if (g_current_track >= MAX_TRACK)
    {
      g_current_track = 0;
    }
  g_current_track++;
  return g_current_track;
}


bool
valid_protocol (int proto)
{
  switch (proto)
    {
    case PROT_NULL:
    case PROT_PABX:
    case PROT_DATA_ENTRANT:
    case PROT_V21:
    case PROT_APTEL:
    case PROT_SURTEC:
    case PROT_CESA:
    case PROT_SERIEE:
    case PROT_CESAKONE:
    case PROT_KONEXION:
    case PROT_AETA:
    case PROT_ANEP:
    case PROT_AMPHITECH:
    case PROT_STRATEL:
    case PROT_SCANCOM:
    case PROT_BIOTEL102:
    case PROT_SIA:
    case PROT_CONTACT_ID:
    case PROT_BOSCH:
    case PROT_HORSIN:
    case PROT_WIT:
    case PROT_ANT:
    case PROT_DAITEM:
    case PROT_PHONIQUE:
    case PROT_TOURRET:
    case PROT_PLATON:
    case PROT_SIA_Long:
    case PROT_PHO_DEC:
    case PROT_SIA_1S:
    case PROT_STDB:
    case PROT_STMF:
    case PROT_DATACALL:
    case PROT_ATENDO:
    case PROT_CORA:
    case PROT_SCANCOM2:
    case PROT_SECOM3:
    case PROT_CESA_court:
    case PROT_CESA_synto:
    case PROT_SILENT_KNIGHT:
    case PROT_FAST_SILENT_KNIGHT:
    case PROT_ROBOFON:
    case PROT_L400:
    case PROT_FAST_PULSE:
    case PROT_SCANCOM3:
    case PROT_TELIM:
    case PROT_FAST_PULSE_1400:
    case PROT_RB2000E:
    case PROT_SAFELINE:
    case PROT_SCANCOMFA:
      return true;
    default:
      return false;
    }
}

xoip_protocol_t
get_protocol (int proto)
{

  switch (proto)
    {
    case PROT_NULL:
      return PROT_NULL;
    case PROT_PABX:
      return PROT_PABX;
    case PROT_DATA_ENTRANT:
      return PROT_DATA_ENTRANT;
    case PROT_V21:
      return PROT_V21;
    case PROT_APTEL:
      return PROT_APTEL;
    case PROT_SURTEC:
      return PROT_SURTEC;
    case PROT_CESA:
      return PROT_CESA;
    case PROT_SERIEE:
      return PROT_SERIEE;
    case PROT_CESAKONE:
      return PROT_CESAKONE;
    case PROT_KONEXION:
      return PROT_KONEXION;
    case PROT_AETA:
      return PROT_AETA;
    case PROT_ANEP:
      return PROT_ANEP;
    case PROT_AMPHITECH:
      return PROT_AMPHITECH;
    case PROT_STRATEL:
      return PROT_STRATEL;
    case PROT_SCANCOM:
      return PROT_SCANCOM;
    case PROT_BIOTEL102:
      return PROT_BIOTEL102;
    case PROT_SIA:
      return PROT_SIA;
    case PROT_CONTACT_ID:
      return PROT_CONTACT_ID;
    case PROT_BOSCH:
      return PROT_BOSCH;
    case PROT_HORSIN:
      return PROT_HORSIN;
    case PROT_WIT:
      return PROT_WIT;
    case PROT_ANT:
      return PROT_ANT;
    case PROT_DAITEM:
      return PROT_DAITEM;
    case PROT_PHONIQUE:
      return PROT_PHONIQUE;
    case PROT_TOURRET:
      return PROT_TOURRET;
    case PROT_PLATON:
      return PROT_PLATON;
    case PROT_SIA_Long:
      return PROT_SIA_Long;
    case PROT_PHO_DEC:
      return PROT_PHO_DEC;
    case PROT_SIA_1S:
      return PROT_SIA_1S;
    case PROT_STDB:
      return PROT_STDB;
    case PROT_STMF:
      return PROT_STMF;
    case PROT_DATACALL:
      return PROT_DATACALL;
    case PROT_ATENDO:
      return PROT_ATENDO;
    case PROT_CORA:
      return PROT_CORA;
    case PROT_SCANCOM2:
      return PROT_SCANCOM2;
    case PROT_SECOM3:
      return PROT_SECOM3;
    case PROT_CESA_court:
      return PROT_CESA_court;
    case PROT_CESA_synto:
      return PROT_CESA_synto;
    case PROT_SILENT_KNIGHT:
      return PROT_SILENT_KNIGHT;
    case PROT_FAST_SILENT_KNIGHT:
      return PROT_FAST_SILENT_KNIGHT;
    case PROT_ROBOFON:
      return PROT_ROBOFON;
    case PROT_L400:
      return PROT_L400;
    case PROT_FAST_PULSE:
      return PROT_FAST_PULSE;
    case PROT_SCANCOM3:
      return PROT_SCANCOM3;
    case PROT_TELIM:
      return PROT_TELIM;
    case PROT_FAST_PULSE_1400:
      return PROT_FAST_PULSE_1400;
    case PROT_RB2000E:
      return PROT_RB2000E;
    case PROT_SAFELINE:
      return PROT_SAFELINE;
    case PROT_SCANCOMFA:
      return PROT_SCANCOMFA;
    default:
      return -1;
    }

  return -1;
}


/* this part wiil be extracted to others h.c files */

/*
* Build a MuLaw data block for a single frequency tone
*/
static void
make_tone_burst (unsigned char *data, float freq, float loudness, int len,
		 int *x)
{
  int i;
  float val;

  for (i = 0; i < len; i++)
    {
      val = loudness * sin ((freq * 2.0 * M_PI * (*x)++) / 8000.0);
      data[i] = AST_LIN2MU ((int) val);
    }

  /* wrap back around from 8000 */

  if (*x >= 8000)
    *x = 0;
  return;
}

#if 1
/*
* Send a single tone burst for a specifed duration and frequency.
* Returns 0 if successful
*/
static int send_tone_burst(struct ast_channel *chan, float freq, int duration, int tldn)
{
	int res = 0;
	int i = 0;
	int x = 0;
	struct ast_frame *f, wf;
	
	struct {
		unsigned char offset[AST_FRIENDLY_OFFSET];
		unsigned char buf[640];
	} tone_block;

	for (;;) {

		if (ast_waitfor(chan, -1) < 0) {
			res = -1;
			break;
		}

		f = ast_read(chan);
		if (!f) {
			res = -1;
			break;
		}

		if (f->frametype == AST_FRAME_VOICE) {
			wf.frametype = AST_FRAME_VOICE;
			ast_format_set(&wf.subclass.format, AST_FORMAT_ULAW, 0);
			wf.offset = AST_FRIENDLY_OFFSET;
			wf.mallocd = 0;
			wf.data.ptr = tone_block.buf;
			wf.datalen = f->datalen;
			wf.samples = wf.datalen;
			
			make_tone_burst(tone_block.buf, freq, (float) tldn, wf.datalen, &x);

			i += wf.datalen / 8;
			if (i > duration) {
				ast_frfree(f);
				break;
			}
			if (ast_write(chan, &wf)) {
				ast_verb(4, "AlarmReceiver: Failed to write frame on %s\n", ast_channel_name(chan));
				ast_log(LOG_WARNING, "AlarmReceiver Failed to write frame on %s\n",ast_channel_name(chan));
				res = -1;
				ast_frfree(f);
				break;
			}
		}

		ast_frfree(f);
	}
	return res;
}

#endif


static int xoip_senddigit_begin(struct ast_channel *chan, char digit)
{
	/* Device does not support DTMF tones, lets fake
	 * it by doing our own generation. */
	static const char * const dtmf_tones[] = {
		"941+1336", /* 0 */
		"697+1209", /* 1 */
		"697+1336", /* 2 */
		"697+1477", /* 3 */
		"770+1209", /* 4 */
		"770+1336", /* 5 */
		"770+1477", /* 6 */
		"852+1209", /* 7 */
		"852+1336", /* 8 */
		"852+1477", /* 9 */
		"697+1633", /* A */
		"770+1633", /* B */
		"852+1633", /* C */
		"941+1633", /* D */
		"941+1209", /* * */
		"941+1477"  /* # */
	};

	if (!ast_channel_tech(chan)->send_digit_begin){
            ast_log(AST_LOG_DEBUG, "XoIP : Unable to generate DTMF tone '%c' for '%s'send_digit_begin is false\n", digit, ast_channel_name(chan));
	    return 0;
        }

	ast_channel_lock(chan);
	ast_channel_sending_dtmf_digit_set(chan, digit);
	ast_channel_sending_dtmf_tv_set(chan, ast_tvnow());
	ast_channel_unlock(chan);

	/*if (!ast_channel_tech(chan)->send_digit_begin(chan, digit))
		return 0;
        */
	if (digit >= '0' && digit <='9')
		ast_playtones_start(chan, 0, dtmf_tones[digit-'0'], 0);
	else if (digit >= 'A' && digit <= 'D')
		ast_playtones_start(chan, 0, dtmf_tones[digit-'A'+10], 0);
	else if (digit == '*')
		ast_playtones_start(chan, 0, dtmf_tones[14], 0);
	else if (digit == '#')
		ast_playtones_start(chan, 0, dtmf_tones[15], 0);
	else {
		/* not handled */
		ast_log(AST_LOG_DEBUG, "Unable to generate DTMF tone '%c' for '%s'\n", digit, ast_channel_name(chan));
	}

	return 0;
}


static int xoip_senddigit_end(struct ast_channel *chan, char digit, unsigned int duration)
{
	int res = -1;

	if (ast_channel_tech(chan)->send_digit_end)
		res = ast_channel_tech(chan)->send_digit_end(chan, digit, duration);

	ast_channel_lock(chan);
	if (ast_channel_sending_dtmf_digit(chan) == digit) {
		ast_channel_sending_dtmf_digit_set(chan, 0);
	}
	ast_channel_unlock(chan);

	if (res && ast_channel_generator(chan))
		ast_playtones_stop(chan);

	return 0;
}

static int xoip_senddigit(struct ast_channel *chan, char digit, unsigned int duration)
{
	if (ast_channel_tech(chan)->send_digit_begin) {
		xoip_senddigit_begin(chan, digit);
		ast_safe_sleep(chan, (duration >= XOIP_DEFAULT_EMULATE_DTMF_DURATION ? duration : XOIP_DEFAULT_EMULATE_DTMF_DURATION));
	}

	return xoip_senddigit_end(chan, digit, (duration >= XOIP_DEFAULT_EMULATE_DTMF_DURATION ? duration : XOIP_DEFAULT_EMULATE_DTMF_DURATION));
}


static int xoip_generate_dtmf_tones(struct ast_channel *chan, const char *data, int size, int duration, int between)
{
    int res = 0;
    int i = 0;
    ast_log (AST_LOG_NOTICE, "XoIP : Send dtmf [%s].\n", data);
    int tldn = 4096;
    struct ast_silence_generator *silgen = NULL;

    if(!between){
        between = 100;
    }

    if(ast_opt_transmit_silence){
        silgen = ast_channel_start_silence_generator(chan);
    }

    res = ast_safe_sleep(chan, 100);
    if(res){
        goto dtmf_tones_cleanup;
    }

    ast_safe_sleep(chan,100);
    for (i = 0; i < size; i += 1)
    {
	char digit = data[i];
	res = xoip_senddigit (chan, digit, duration);
        res = ast_safe_sleep(chan, between);
	if (res < 0)
	{
	    ast_log (AST_LOG_ERROR, "XoIp : Failed to send digit [%c].\n");
	    return -1;
	}
        
    }

dtmf_tones_cleanup:
    if(silgen){
        ast_channel_stop_silence_generator(chan, silgen);
    }
    return res;


}

int
xoip_generate_tones (struct ast_channel *chan, const char *data, int size,
		     int tone_types, int duration)
{
  int res = 0;
  int i = 0;
  ast_log (AST_LOG_DEBUG, "XoIP : Send tones [%s] for the type %d.\n", data,
	   tone_types);
  int tldn = 4096;

  if ((tone_types & TONES_DTMF) > 0){
      //res = ast_dtmf_stream(chan, NULL, data, 0, 0);
      res = xoip_generate_dtmf_tones(chan ,data, size, duration, 0);
  }

#if 0
      res = send_tone_burst(chan, 1400.0, 100, tldn);

			if (!res)
				res = ast_safe_sleep(chan, 100);
			if (!res) {
				ast_verb(4, "AlarmReceiver: Sending 2300Hz 100ms burst (ACK)\n");
				res = send_tone_burst(chan, 2300.0, 100, tldn);
			}
#endif 
    
  return res;
}


int xoip_read_data(struct xoip_comm *xcomm, char *data, int maxsize, double timeout)
{
    int res = 0;
#if 0
    ast_stopstream(chan);
    res = ast_waitfordigit(chan, timeout);
    ast_playtones_stop(chan);
    res = ast_app_getdata(chan, NULL, data, maxsize, 1);
    ast_log (AST_LOG_DEBUG, "XoIP : read data [%s]  and length [%d] with the result [%d].\n", data, strlen(data), res);
    //
    if(res == 1){
        res = strlen(data);
    }
#else
    int i = 0;
    int r;
    struct ast_frame *f;
    struct timeval lastdigittime;
    int fdto = 1000;
    int sdto = 500;
    lastdigittime = ast_tvnow();
    struct ast_channel *chan = xcomm->chan;
    
    ast_playtones_stop(chan);

    for (;;) {
    /* if outa time, leave */
    if (ast_tvdiff_ms(ast_tvnow(), lastdigittime) > ((i > 0) ? sdto : fdto)) {
        ast_log(AST_LOG_DEBUG, "XoIP : DTMF Digit Timeout on %s\n", ast_channel_name(chan));
	res = 1;
	break;
    }

    if ((r = ast_waitfor(chan, -1)) < 0) {
        ast_log(AST_LOG_DEBUG, "XoIP : Waitfor returned %d\n", r);
	continue;
    }
    
    /*if (ast_channel_state(chan) != AST_STATE_UP){
        continue;

    }*/

    f = ast_read(chan);
    if (f == NULL) {
        res = -1;
	break;
    }

    /* If they hung up, leave */
    if ((f->frametype == AST_FRAME_CONTROL) && (f->subclass.integer == AST_CONTROL_HANGUP)) {
        if (f->data.uint32) {
	    ast_channel_hangupcause_set(chan, f->data.uint32);
	}
	ast_frfree(f);
	res = -1;
	break;
    }

    /* if not DTMF, just do it again */
    if (f->frametype != AST_FRAME_DTMF) {
        ast_frfree(f);
	continue;
    }

    data[i++] = f->subclass.integer;  /* save digit */

    ast_frfree(f);

    /* If we have all the digits we expect, leave */
    if(i >= maxsize)
	break;
	lastdigittime = ast_tvnow();
    }

    data[i] = '\0'; /* Nul terminate the end of the digit string */
  
#endif
    return res;
}



