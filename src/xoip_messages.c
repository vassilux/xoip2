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

#include "asterisk.h"
#include "asterisk/logger.h"
//#include "asterisk/astmm.h"

#include "xoip_messages.h"

int do_process_message (const char *buf, int size,
			struct f1_messages_handlers *handlers);


int
do_process_message (const char *buf, int size,
		    struct f1_messages_handlers *handlers)
{
  int res = 0;
  char *buffer = strdup (buf);
  const char *sep = ",";
  int i = 0;

  char messages[10][255] = { '\0' };
  int count = 0;

  //ast_log(AST_LOG_VERBOSE, " %s \n", buffer);

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
  int track = atoi (messages[1]);
  int callref = atoi (messages[2]);
  /* so far sor good */
  switch (type)
    {
    case 'G':
      //ast_log(AST_LOG_VERBOSE, " Processing G.\n");
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
    case 'r':
    case 'R':
      //ast_log(AST_LOG_VERBOSE, " Processing R.\n");
      handlers->hangup (track, callref);
      break;
    default:
      ast_log (AST_LOG_VERBOSE, " Processing but type not found : [%c]..\n",
	       type);
      break;
    }

  res = size;
  free (buffer);
  return res;

}




int
xoip_build_XC_msg (int voie, int callref, const char *callee,
		   const char *caller, const char *real_caller, char *dest,
		   int size)
{
  snprintf (dest, size, "X C,%02d,%04X,%s,%s,T%s\r",
	    voie, callref, callee, caller, real_caller);
  return 0;

}



int
xoip_build_Xc_msg (int voie, int callref, const char *callee,
		   const char *caller, const char *real_caller, char *dest,
		   int size)
{
  snprintf (dest, size, "X c,%02d,%04X,%s,%s,T%s\r",
	    voie, callref, callee, caller, real_caller);
  return 0;

}

/*!
 * \brief Build 
 */
int
xoip_build_Xr_msg (int voie, int callref, char *dest, int size)
{
  snprintf (dest, size, "X r,%02d,%04d\r", voie, callref);
  return 0;

}


int
xoip_build_Xh_msg (int voie, int callref, int state, const char *callee,
		   char *dest, int size)
{
  snprintf (dest, size, "X h,%02d,%04d,%01d,%s\r",
	    voie, callref, state, callee);
  return 0;
}



int xoip_build_XA_msg (int voie, int callref, const char *data, char *dest, int size)
{
    snprintf (dest, size, "X A,%02d,%04d,%s\r",
	    voie, callref, data);
    return 0;
}



/*!
 * \brief Parrsing message from the given buffer.
 * Buffer can content few messges 
 * \param buf messages buffer
 * \param size the length of the buffer
 * \params handlers the structur of callback for each type of message
 *
 * \retval -1 on error
 * \retval > 0 the lenth of processing messages
 */
int
process_message (const char *buf, int size,
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



