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
  int (*answer) (int track, int callref, char proto, int level);
  /*!
   * \brief Process (F H) message from f1 endpoint.
   */
  int (*commut) (int track, int callref, char *callee, char *trans,
		 char *volum);

  /*!
   * \brief Process (F R) message from f1 endpoint.
   */
  int (*hangup) (int track, int callref);
};


int xoip_build_XC_msg (int voie, int callref, const char *callee,
		       const char *caller, const char *real_caller,
		       char *dest, int size);

int xoip_build_Xc_msg (int voie, int callref, const char *callee,
		       const char *caller, const char *real_caller,
		       char *dest, int size);

int xoip_build_Xr_msg (int voie, int callref, char *dest, int size);

int xoip_build_Xh_msg (int voie, int callref, int state, const char *callee,
		       char *dest, int size);

int xoip_build_XA_msg (int voie, int callref, const char *data, char *dest, int size);

int process_message (const char *buf, int size,
		     struct f1_messages_handlers *handlers);




#endif /* ----- #ifndef XOIP_MESSAGES_INC  ----- */
