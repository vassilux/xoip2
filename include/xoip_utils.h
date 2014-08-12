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

#ifndef  XOIP_UTILS_INC
#define  XOIP_UTILS_INC

#include "asterisk.h"
#include "asterisk/channel.h"
#include "asterisk/linkedlists.h"
#include "asterisk/audiohook.h"
#include "asterisk/dsp.h"

#if defined(__cplusplus) || defined(c_plusplus)
extern "C" {
#endif


#define BUFLEN 1024
/*!
 * \brief Maximale callref number.
 * Used to generate / rotate callref for f1.
 */
#define MAX_CALLREF 9999
/*!
 * \brief Maximale track number
 * Can be a good idea to use a paramer for make difference between T0/T2
 */
#define MAX_TRACK   30

#define XOIP_DEFAULT_EMULATE_DTMF_DURATION 100


#define UNUSED(x) (void)(x)
/*!
 * \brief the xoip communication
 *
 * XoIP communication identified by id and the ast_channel
 * All comunications stored into the static list xoip_comms
 */
struct xoip_comm {
    int track;
    int callref;
    int extout;
    unsigned int stop_tones:1;
    xoip_protocol_t proto;
    int (*func_data_callback) (int track, int callref, const char* data);
    struct ast_channel *chan;
    struct ast_dsp *dsp;
    struct ast_audiohook audiohook;
    AST_LIST_ENTRY(xoip_comm) list;
};

/*struct xoip_comm_data_callback {
    struct xoip_comm *xcomm;
    int (*func_data_callback) (int track, int callref, const char* data);
};*/

/*!
 * \brief Generate a callref. 
 * This is naive way to do it.
 *
 */
int get_current_callref(void);

/*!
 * \brief Generate a track number
 * This is naive way to do it.
 */
int get_current_track(void);


bool valid_protocol(int proto);

xoip_protocol_t get_protocol(int proto);

/* this part wiil be extracted to others h.c files */

enum {
    TONES_DTMF = 0x01,
    TONES_FSK = 0x02,
    TONES_I_DONT_KNOW_WHAT = 0x04
};



static struct {
	enum ast_frame_type type;
	const char *str;
} frametype2str[] = {
	{ AST_FRAME_DTMF_BEGIN,   "DTMF_BEGIN" },
	{ AST_FRAME_DTMF_END,   "DTMF_END" },
	{ AST_FRAME_VOICE,   "VOICE" },
	{ AST_FRAME_VIDEO,   "VIDEO" },
	{ AST_FRAME_CONTROL,   "CONTROL" },
	{ AST_FRAME_NULL,   "NULL" },
	{ AST_FRAME_IAX,   "IAX" },
	{ AST_FRAME_TEXT,   "TEXT" },
	{ AST_FRAME_IMAGE,   "IMAGE" },
	{ AST_FRAME_HTML,   "HTML" },
	{ AST_FRAME_CNG,   "CNG" },
	{ AST_FRAME_MODEM,   "MODEM" },
};

struct frame_trace_data {
	int list_type; /* 0 = white, 1 = black */
	int values[ARRAY_LEN(frametype2str)];
};


static const char gain_map[] = {
	-15,
	-13,
	-10,
	-6,
	0,
	0,
	0,
	6,
	10,
	13,
	15,
};





int xoip_generate_tones(struct ast_channel *chan, const char *data, int size, int tone_types, int duration);

int xoip_generate_freq(struct xoip_comm *xcomm, int freq, int duration, int loudness);

int xoip_read_data(struct xoip_comm *xcomm, char *data, int maxsize, double timeout);

int xoip_set_talk_volume(struct xoip_comm *xcomm, int volume);

int xoip_set_listen_volume(struct xoip_comm *xcomm, int volume);

int xoip_queuing_call(struct xoip_comm *xcomm, char *mode);

/** helpers **/
void print_frame(struct ast_frame *frame);



#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif   /* ----- #ifndef XOIP_UTILS_INC  ----- */
