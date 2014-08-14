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


#define XOIP_CONFIG "xoip.conf"

#define XOIP_STATE_KEY "XOIP_STATE"
#define XOIP_STATE_VALUE_ANSWER "ANSWER"
#define XOIP_STATE_VALUE_HANGUP "HANGUP"
#define XOIP_STATE_VALUE_COMMUT "COMMUT"

#define XOIP_PROTOCOL_KEY "XOIP_PROTOCOL"


#define XOIP_COMMUT_CONTEXT "XOIP_COMMUT_CONTEXT"
#define XOIP_COMMUT_EXTEN   "XOIP_COMMUT_EXTEN"



/*!
 * \brief Contexts used by xoip application
 */
static char *XOIP_CONTEXT_COMMUT ="xoip-commut";
static char *XOIP_CONTEXT_WAIT = "xoip-comm-waiting";
static char *XOIP_CONTEXT_HANGUP = "xoip-comm-hangup";


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

/*!
 * Generate tones.
 *
 * \param chan The channel to generate tones
 * \param data The buffer of data to generate to the channel
 * \param size The size of generated data
 * \paral tone_type The type of generated tone. Please see enum of used tones
 *
 * \retval -1 on error
 * \retval 0 on success
 */
int xoip_generate_tones(struct ast_channel *chan, const char *data, int size, int tone_types, int duration);

/*!
 * Generate frequency.
 *
 * \param xcomm The xoip communication
 * \param freq The given frequency to generate
 * \duration The duration msec of generated frequency
 * \loudness The frequancy loudness
 *
 * \rertval -1 on error
 * \retval 0 on success
 */
int xoip_generate_freq(struct xoip_comm *xcomm, int freq, int duration, int loudness);

/*!
 * \brief Read data from the given channel.
 * Considered only DTMF
 *
 * \param xcomm The xoip communication
 * \param data The buffer for received data from the channel
 * \maxsize The maximal size of the data's buffer
 * \timeout The timeout for ast_waitfor
 *
 * \retval -1 on error
 * \retval 0 on success
 */
int xoip_read_data(struct xoip_comm *xcomm, char *data, int maxsize, double timeout);

/*!
 * \brief Not yet ready for production using.!!!!!
 *
 * \param xcomm The xoip communication
 * \param volume The volume to set into channel
 */
int xoip_set_talk_volume(struct xoip_comm *xcomm, int volume);

/*!
 * \brief Not yet ready for production using.!!!!!
 *
 * \param xcomm The xoip communication
 * \param volume The volume to set into channel
 *
 * \retval -1 on error
 * \retval 0 on success
 */
int xoip_set_listen_volume(struct xoip_comm *xcomm, int volume);

/*!
 * Allow to set a call in a wait state.
 * This is something like park.
 * Please see the dialplan context xoip-comm-waiting
 *
 * \param xcomm The xoip communication
 * \param mode The indication that hangup the PBX side(not used)
 *
 * \retval -1 on error
 * \retval 0 on error
 */
int xoip_queuing_call(struct xoip_comm *xcomm, char *mode);


/*!
 * \brief Helper for print the given frame informations
 */
void print_frame(struct ast_frame *frame);



#if defined(__cplusplus) || defined(c_plusplus)
}
#endif

#endif   /* ----- #ifndef XOIP_UTILS_INC  ----- */
