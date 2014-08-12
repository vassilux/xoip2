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

#ifndef  XOIP_MESSAGES_INC
#define  XOIP_MESSAGES_INC

#include <stdio.h>

struct f1_messages_handlers
{
    
    /*!
     * \brief Process (F G), message from f1 endpoint.
     */
    int (*answer) (int track, int callref, char proto, int loudness);

    /*!
     * \brief Process (F H) message from f1 endpoint.
     */
    int (*commut) (int track, int callref, char *callee, char *trans,
		 char *volum);
    /*!
     * \brief Process (F I) message from f1 endpoint.
     */
    int (*switch_protocol)(int track, int callref, char proto, int loudness);

    /*!
     * \brief Process (F K) message from f1 endpoint
     */
    int (*ack_alarm)(int track, int callref);
    
    /*!
     * \brief Process (F L) message from f1 endpoint.
     */
    int (*send_data)(int track, int callref, int mode_transfer, char *data, char* mode_operator, char* break_record);

    /*!
     * \brief Process (F M) messge from f1 endpoint.
     */
    int (*record_request)(int track, int callref, int record_track);

    int (*polling)(char *f1_version);
    /*!
     * \brief Process (F V) message from f1 endpoint.
     */
    int (*volume_adjust)(int track, int callref, int volume);

    /*!
     * \brief Process (F F) message from f1 endpoint.
     */
    int (*send_freq)(int track, int callref, int freq, int duration, int level, char* mode_operator, char *break_record);

    /*!
     * \brief Process (F R) message from f1 endpoint.
     */
    int (*hangup) (int track, int callref);

    int (*queuing_call)(int track, int callref, char *mode);
};


int xoip_build_XA_msg (int voie, int callref, const char *data, char *dest, int size);


int xoip_build_XC_msg (int voie, int callref, const char *callee,
		       const char *caller, const char *real_caller,
		       char *dest, int size);

int xoip_build_Xc_msg (int voie, int callref, const char *callee,
		       const char *caller, const char *real_caller,
		       char *dest, int size);

int xoip_build_Xg_msg (int voie, int callref, int state, int res, char *dest, int size);


int xoip_build_Xh_msg (int voie, int callref, int state, const char *callee,
		       char *dest, int size);

int xoip_build_Xi_msg(int voie, int callref, int state, int res, char *dest, int size);

int xoip_build_Xk_msg(int voie, int callref, int state, int res, char *dest, int size);

int xoip_build_Xf_msg(int voie, int callref, int state, int res, char *dest, int size);

int xoip_build_XL_msg(int voie, int callref, int res, char *dest, int size);

int xoip_build_Xm_msg(int voie, int callref, int state, int res, char *dest, int size);

int xoip_build_XE_msg(int status, char *dest, int size);

int xoip_build_Xr_msg (int voie, int callref, char *dest, int size);

int xoip_build_Xw_msg (int voie, int callref, int res, char *dest, int size);


int process_message (const char *buf, int size,
		     struct f1_messages_handlers *handlers);




#endif /* ----- #ifndef XOIP_MESSAGES_INC  ----- */
