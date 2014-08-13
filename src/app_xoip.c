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

#include <sys/time.h>
#include <signal.h>

#include <fcntl.h>

#ifndef AST_MODULE
#define AST_MODULE "app_xoip"
#endif 

ASTERISK_FILE_VERSION(__FILE__, "$Revision: $")

#include "asterisk/file.h"
#include "asterisk/channel.h"
#include "asterisk/pbx.h"
#include "asterisk/module.h"
#include "asterisk/lock.h"
#include "asterisk/app.h"
#include "asterisk/utils.h"
#include "asterisk/logger.h"
#include "asterisk/linkedlists.h"
#include "asterisk/pbx.h"
#include "asterisk/dial.h"
#include "asterisk/features.h"
#include "asterisk/audiohook.h"
#include "asterisk/dsp.h"
#include "asterisk/features.h"

/* xoip internal includes */
#include "xoip_types.h"
#include "xoip_utils.h"
#include "xoip_messages.h"

/*** DOCUMENTATION
	<application name="Xoip" language="en_US">
		<synopsis>
			Xoip alarm receiver.
		</synopsis>
	</application>
***/

#define XOIP_CONFIG "xoip.conf"

#define XOIP_STATE_KEY "XOIP_STATE"
#define XOIP_STATE_VALUE_ANSWER "ANSWER"
#define XOIP_STATE_VALUE_HANGUP "HANGUP"
#define XOIP_STATE_VALUE_COMMUT "COMMUT"

#define XOIP_PROTOCOL_KEY "XOIP_PROTOCOL"


#define XOIP_COMMUT_CONTEXT "XOIP_COMMUT_CONTEXT"
#define XOIP_COMMUT_EXTEN   "XOIP_COMMUT_EXTEN"


static char *app_xoip_alarms = "XoIPAlarms";
static char *app_xoip_hangup = "XoIPHangup";
static char *app_xoip_park = "XoIPPark";


/** configuration **/
static char config_f1_adr[128] = {'\0'};
static int config_f1_port = 8118;


/*!
 * THIS IS JUST TEMPORAIRE TEST VALUE 
 * 
 */
//static struct ast_channel *current_chan = NULL;


/*! 
 * \breif the xoip dispatcher thread
 */
static struct {
	pthread_t id;
	unsigned int stop:1;
} dispatch_thread = {
	.id = AST_PTHREADT_NULL,
};

static struct {
    int sfd;
    struct sockaddr_in si_other;
    struct sockaddr_in si_me;
    char buf[BUFLEN];

} udp_f1_comm = {
    .sfd= -1,
    .buf={'\0'},
};


static AST_LIST_HEAD_STATIC(xoip_comms, xoip_comm);




/*!
 * \brief send a message packet to the f1 server
 * \param data the message to send
 * \param the size of the given message
 *
 * retval -1 error
 * retvzl > 0 success
 */
static int send_f1_packet(char *data, int size)
{
    int slen = sizeof(udp_f1_comm.si_other);
    int res = sendto(udp_f1_comm.sfd, data, size, 0, 
                &udp_f1_comm.si_other, slen);
    return res;

}


/*!
 * \brief Data callback 
 * This callback used for sending the received data(for example DTMF)
 * \param track , the given track
 * \param callref, the given callref
 * \param data, the received data
 *
 * \retval -1 on error
 * \retval 0 on success
 */
int data_callback_fn(int track, int callref, const char* data)
{
    char buf[BUFLEN];
    memset(buf, 0, sizeof(buf));

    xoip_build_XA_msg(track, callref, data, 
                        buf, sizeof(buf) -1);
    if(!send_f1_packet(buf, strlen(buf))){
        return -1;
    }
    return 0;
}


/*!
 * \brief Delete a xoip communicaton from the communication list
 * \param track
 * \param callref
 *
 * \retval -1 on error
 * \retval 0 on success
 *
 */
static int xoip_channel_del(int track, int callref)
{
    int res = -1;
    struct xoip_comm *xcomm;

    AST_LIST_LOCK(&xoip_comms);
    AST_LIST_TRAVERSE_SAFE_BEGIN(&xoip_comms, xcomm, list){
        if((xcomm->track == track) && (xcomm->callref == callref)){
            AST_LIST_REMOVE_CURRENT(list);
            //ast_audiohook_destroy(&xcomm->audiohook);
            if(xcomm->dsp){
                ast_dsp_free(xcomm->dsp);
            }
            free(xcomm);
            res = 0;
        }

    }
    AST_LIST_TRAVERSE_SAFE_END;
    AST_LIST_UNLOCK(&xoip_comms);

    return res;
}

/*!
 * \brief Create a new XoIP communicaiton and initialize 
 * \param track, the given track
 * \param callref, the given callref
 * \param chan , the asterisk channel.
 *
 * \retval NULL on error
 * \retval the valid pointer to xoip_comm struct
 */
static struct xoip_comm* xoip_comm_new(int track, int callref, struct ast_channel *chan)
{
    struct xoip_comm *xcomm;
    if(!(xcomm = ast_calloc(1, sizeof(*xcomm)))){
        return NULL;
    }
    //memset(xcomm->state, 0, sizeof(xcomm->state));
    xcomm->track = track;
    xcomm->callref = callref;
    xcomm->stop_tones = 0;
    xcomm->extout = 0;
    xcomm->func_data_callback = data_callback_fn;
    xcomm->chan = chan;
    xcomm->dsp = ast_dsp_new();
    ast_dsp_set_threshold(xcomm->dsp, 256);
    int features = 0;
    features |= DSP_FEATURE_DIGIT_DETECT;
    ast_dsp_set_features(xcomm->dsp, features | DSP_DIGITMODE_RELAXDTMF);

    return xcomm;

}

/*!
 * \brief Add a new xoip communication into the communications list
 */
static int xoip_channel_add(int track, int callref, struct ast_channel *chan)
{
    struct xoip_comm *xcomm = xoip_comm_new(track, callref, chan);
    if(!xcomm){
        return -1;
    }
    //
    AST_LIST_LOCK(&xoip_comms);
    AST_LIST_INSERT_HEAD(&xoip_comms,xcomm, list);
    AST_LIST_UNLOCK(&xoip_comms);

    return 0;
}

/*!
 *
 */
static struct xoip_comm* xoip_channel_get(int track, int callref)
{
    struct xoip_comm *xcomm = NULL;

    AST_LIST_LOCK(&xoip_comms);
    AST_LIST_TRAVERSE_SAFE_BEGIN(&xoip_comms, xcomm, list){
        if((xcomm->track == track) && (xcomm->callref == callref)){
            break;
        }

    }
    AST_LIST_TRAVERSE_SAFE_END;
    AST_LIST_UNLOCK(&xoip_comms);

    return xcomm;

}

/*!
 * \brief Find a xoip communication by given asterisk channel
 * Incoming channel used for searching process.
 * \param chan, the asterisk channel
 *
 * \retval NULL on error
 * \retval the valid xoip_comm pointer on success.
 *
 */
static struct xoip_comm* xoip_channel_find(struct ast_channel *chan)
{
    struct xoip_comm *xcomm = NULL;

    AST_LIST_LOCK(&xoip_comms);
    AST_LIST_TRAVERSE_SAFE_BEGIN(&xoip_comms, xcomm, list){
        if(xcomm->chan == chan){
            break;
        }

    }
    AST_LIST_TRAVERSE_SAFE_END;
    AST_LIST_UNLOCK(&xoip_comms);

    return xcomm;

}

/*!
 * \brief Send the call notification state to f1.
 * 
 */
static void send_f1_fh_packet(struct xoip_comm *xcomm, int state)
{
    char buf[BUFLEN] = {'\0'};

    if(xcomm == NULL){
        return;
    }

    const char* xoip_exten_var = pbx_builtin_getvar_helper(xcomm->chan, XOIP_COMMUT_EXTEN);
    
    if(xoip_exten_var == NULL){
        return;
    }

    xoip_build_Xh_msg(xcomm->track, xcomm->callref, state, xoip_exten_var,
                buf,
                sizeof(buf));
    send_f1_packet(buf, strlen(buf));
    ast_log(LOG_NOTICE, "Send the call state to the f1 for extention [%s] and state [%d].\n", xoip_exten_var, state);


}

/*!
 *
 */
static void do_translate_send_fh(struct xoip_comm *xcomm, enum ast_dial_result state){

    const char* xoip_exten_var = pbx_builtin_getvar_helper(xcomm->chan, XOIP_COMMUT_EXTEN);

    switch ( state ) {
        case AST_DIAL_RESULT_INVALID:
            send_f1_fh_packet(xcomm, 1);
            break;

        case AST_DIAL_RESULT_FAILED:
            send_f1_fh_packet(xcomm, 1);
            break;

        case AST_DIAL_RESULT_TRYING:
            send_f1_fh_packet(xcomm, 9);
            break;


        case AST_DIAL_RESULT_RINGING:	
            send_f1_fh_packet(xcomm, 5);
            break;

        case AST_DIAL_RESULT_PROCEEDING:	
            send_f1_fh_packet(xcomm, 9);
            break;


        case AST_DIAL_RESULT_ANSWERED:	
            send_f1_fh_packet(xcomm, 0);
            break;
        /* group codes cause for f1 it is same thing */
        case AST_DIAL_RESULT_TIMEOUT:	
        case AST_DIAL_RESULT_HANGUP:
        case AST_DIAL_RESULT_UNANSWERED:
            send_f1_fh_packet(xcomm, 4);
            break;


        default:
            ast_log(LOG_NOTICE, "Translate skip the call state [%d] for extention [%s].\n", state, xoip_exten_var);
            break;
    }				/* -----  end switch  ----- */

}

/*!
 * \brief Callback for a call
 * Used to notify the call state to f1 server
 * 
 * \param dial , the active dial with xoip_comm data like into user data
 */
static void xoip_dial_state_callback(struct ast_dial *dial)
{
    struct xoip_comm *xcomm = ast_dial_get_user_data(dial);
    if(xcomm == NULL){
        ast_log(LOG_WARNING, "Can not find XoIP communication.\n");
        return;
    }
    enum ast_dial_result state = ast_dial_state(dial);
    do_translate_send_fh(xcomm, state);
}

/*!
 * \brief Bridge an activel channel to a destination
 */
static  int do_xoip_commut(struct xoip_comm* xcomm){
    const char *context = "xoip-commut";
    struct ast_channel *chan = xcomm->chan;
    const char* xoip_exten_var = pbx_builtin_getvar_helper(xcomm->chan, XOIP_COMMUT_EXTEN);
#if 0
    /* keep as an example CAN BE DELETED */
    if(xoip_exten_var != NULL && strlen(xoip_exten_var) > 0){
        struct ast_app *dialapp = pbx_findapp("Dial");
        char dial_options[1024];
        snprintf(dial_options, sizeof(dial_options), "Local/%s@%s,,%s",xoip_exten_var,context, "Tm");
        ast_log(LOG_NOTICE, "Dial application options : [%s].\n",dial_options);
        pbx_exec(chan, dialapp, dial_options);
        ast_log(LOG_NOTICE, "Leave do_xoip_commut.\n");

    }
#else
    struct ast_bridge_config config;    
    struct ast_dial *dial;
    int res = 0;
    /* the deal to use a Lcoal channel to send the call to the xoip context */
    char *tech="Local";
    char resource[1024];
    snprintf(resource, sizeof(resource), "%s@%s",
            xoip_exten_var,context);

    
    memset(&config,0,sizeof(config));
    ast_set_flag(&(config.features_caller), AST_FEATURE_NO_H_EXTEN);

    if (!(dial = ast_dial_create())){
            ast_log(LOG_WARNING, "XoIP[%02d,%04X] Can not create the dial struct.\n",
                    xcomm->track, xcomm->callref);
	    return -1;
    }
    ast_dial_set_state_callback(dial, xoip_dial_state_callback);
	
    if (ast_dial_append(dial, tech, resource) == -1) {
		ast_dial_destroy(dial);
                ast_log(LOG_ERROR, "XoIP[%02d,%04X] Can not append to dial the tech [%s] and the resource [%s].\n", xcomm->track, xcomm->callref, tech, resource);
		return -1;
    }
    ast_dial_option_global_enable(dial, AST_DIAL_OPTION_ANSWER_EXEC, "");
    ast_dial_set_user_data(dial, xcomm);

    res = ast_dial_run(dial, chan, 0);

    if (res != AST_DIAL_RESULT_ANSWERED) {
		ast_log(LOG_ERROR, "XoIP[%02d,%04X] Failed call notification.\n", 
                        xcomm->track, xcomm->callref);
                //return -1;
    } else {
            struct ast_channel *peer;
            struct ast_channel *caller = xcomm->chan;
            peer = ast_dial_answered_steal(dial);
            if(peer == NULL){
                return -1;
            }
            ast_channel_context_set(peer, context);
            
            /* Be sure no generators are left on it */
	    ast_deactivate_generator(caller);
	    /* Make sure channels are compatible */
	    res = ast_channel_make_compatible(caller, peer);
	    if (res < 0) {
			ast_log(LOG_WARNING, "XoIP[%02d,%04X] Had to drop call because I couldn't make %s compatible with %s\n", 
                                xcomm->track, xcomm->callref,
                                ast_channel_name(caller),     ast_channel_name(peer));
			ast_autoservice_chan_hangup_peer(caller, peer);
                        return -1;
	    }

	    res = ast_bridge_call(caller, peer, &config);
            //if(xcomm->extout == 0){
                //the call is not parking
                enum ast_dial_result state = ast_dial_state(dial);
                //ast_hangup(peer);
                ast_autoservice_chan_hangup_peer(caller, peer);
                if(state ==  AST_DIAL_RESULT_ANSWERED){
                    do_translate_send_fh(xcomm, AST_DIAL_RESULT_HANGUP);
                }
                /* */
                if(!(ast_channel_softhangup_internal_flag(caller) & 
                            AST_SOFTHANGUP_ASYNCGOTO)){
                    ast_softhangup(caller, AST_SOFTHANGUP_ALL);
                    ast_log(LOG_NOTICE, "XoIP[%02d,%04X]: XoIP send X r for the channel [%s]. Bye\n", 
                            xcomm->track, xcomm->callref, ast_channel_name(caller));
                }     
                


            //}
    }
#endif
    return 0;
}

/*!
 * \brief Set a channel into answer state by change the channel variable XOIP_STATE_KEY
 * Please see the value of XOIP_STATE_KEY
 * \param track, the track number
 * \param callref , the callref 
 * \param proto the alarm protocole identifier
 * \param level the sound level
 *
 * \retval -1 on error
 * \retbal 0 on success
 */
int answer_fn(int track, int callref, char proto, int level){
    int res = 0;
     char buf[BUFLEN];
     memset(buf, 0, sizeof(buf));


    struct xoip_comm *xcomm  = xoip_channel_get(track, callref);
    if(xcomm == NULL){
        ast_log(LOG_WARNING, "Can not find the xcomm for track [%d] and callref [%d].\n", track, callref);
        xoip_build_Xg_msg(track, callref, 1,-1, buf, sizeof(buf));
        send_f1_packet(buf, strlen(buf));

        return -1;
    }
    
    char proto_str[8];
    snprintf(proto_str, sizeof(proto_str), "%c",proto);

    ast_log(LOG_NOTICE, "Protocol [%s] for track [%d] and callref [%d].\n", proto_str, track, callref);

    pbx_builtin_setvar_helper(xcomm->chan, XOIP_PROTOCOL_KEY, proto_str);
    pbx_builtin_setvar_helper(xcomm->chan, XOIP_STATE_KEY, XOIP_STATE_VALUE_ANSWER);
    ast_indicate(xcomm->chan, AST_CONTROL_RINGING);

    res = ast_answer(xcomm->chan);

    if(res){
        ast_log(AST_LOG_ERROR, "Channel is not answered for the track [%d] and callref  [%d].\n",track, callref);
        xoip_build_Xg_msg(track, callref, 1,res, buf, sizeof(buf));
        send_f1_packet(buf, strlen(buf));
        return -1;
    }

    /* send notificaiton that it is ringing */
    xoip_build_Xg_msg(track, callref, 0, res, buf, sizeof(buf));
    send_f1_packet(buf, strlen(buf));

    return 0;
}

/*!
 * \brief Commut/bridge a channel to the given number(callee)
 *
 * \param track, the track number
 * \param callref , the callref 
 * \param callee, the destination to bridge
 * \param trans, the transmittor identificator. This parameter is not used 
 * \param volume, the volum for bridging
 *
 * \retval -1, on error xoip communication can be found
 * \retval 0, on success
 *
 */
int commut_fn(int track, int callref, char* callee, char* trans, char* volume){
  
    struct xoip_comm *xcomm  = xoip_channel_get(track, callref);
    if(xcomm == NULL){
        ast_log(LOG_WARNING, "Can not find the xcomm for track [%d] and callref [%d].\n", track, callref);
        return -1;
    }

    const char *context = ast_channel_context(xcomm->chan);

    ast_log(AST_LOG_NOTICE, "XoIP[%02d,%04X] : Current context [%s].\n",
                xcomm->track, xcomm->callref, context);
    if(strcmp(context, "xoip-comm-waiting-666") == 0){
        do_xoip_commut(xcomm);
        //
        char *context = "xoip-commut";
        char *exten = "8181";
        int pi = 1;
        if (ast_channel_pbx(xcomm->chan)) {
			ast_channel_lock(xcomm->chan);
			/* don't let the after-bridge code run the h-exten */
			ast_set_flag(ast_channel_flags(xcomm->chan), AST_FLAG_BRIDGE_HANGUP_DONT);
			ast_channel_unlock(xcomm->chan);
		}
	ast_async_goto(xcomm->chan, context, exten, pi);


        return 0;
    }

#if 0
    struct ast_channel *bc  = ast_bridged_channel(xcomm->chan);
    if(!bc){
        ast_log(AST_LOG_ERROR, "XoIP[%02d,%04X] : Failed find bridged channel.\n",
                xcomm->track, xcomm->callref);
        return -1;
    }
#endif 

    pbx_builtin_setvar_helper(xcomm->chan, XOIP_COMMUT_EXTEN, callee);
    pbx_builtin_setvar_helper(xcomm->chan, XOIP_STATE_KEY, XOIP_STATE_VALUE_COMMUT);
    return 0;
}

/*!
 * \brief Switch the protocol afer the incoming call answered.
 * \param track, the given call track
 * \param callref, the given callref
 * \param proto, the protocol identificaiton char
 * \param loudness, the loudness 
 *
 * \retval -1, on error
 * \retval 0, on success 
 */
int switch_protocol_fn(int track, int callref, char proto, int loudness)
{
    char buf[BUFLEN];
    memset(buf, 0, sizeof(buf));
    int res = 0;

    res = xoip_build_Xi_msg(track, callref, 1, -1, buf, sizeof(buf));

    if(!res){
        res = send_f1_packet(buf, strlen(buf));
    }else{
        ast_log(AST_LOG_ERROR, "XoIP[%02d,%04X] : Failed build X i packet.\n", track, callref);
    }
    return -1;
}

/*!
 * \brief Process ack alarm 
 * \param track, the given track
 * \param callref, the given callref
 *
 * \retval -1, on error
 * \retval 0, on success
 */
int ack_alarm_fn(int track, int callref){
    char buf[BUFLEN];
    memset(buf, 0, sizeof(buf));
    int res = 0;

    res = xoip_build_Xk_msg(track, callref, 1, -1, buf, sizeof(buf));

    if(!res){
        res = send_f1_packet(buf, strlen(buf));
    }else{
        ast_log(AST_LOG_ERROR, "XoIP[%02d,%04X] : Failed build X k packet.\n", track, callref);
    }
    return -1;

}

/*!
 * \brief Start call record
 * \param track , the given track
 * \param callref, the callref of the call
 * \param record_track, the record track nulber
 *
 * \retval -1, on error
 * \retval 0, on success
 *
 * Note: record_track can be depricated cause track/callref can be used.
 */
int record_request_fn(int track, int callref, int record_track)
{
    char buf[BUFLEN];
    memset(buf, 0, sizeof(buf));
    int res = 0;

    res = xoip_build_Xm_msg(track, callref, 1, -1, buf, sizeof(buf));

    if(!res){
        res = send_f1_packet(buf, strlen(buf));
    }else{
        ast_log(AST_LOG_ERROR, "XoIP[%02d,%04X] : Failed build X m packet.\n", track, callref);
    }
    return -1;
}

/*!
 * \brief Process the polling request
 * \param request, the request string 
 */
int polling_fn(char *request){
    char buf[BUFLEN];
    memset(buf, 0, sizeof(buf));
    int res = 0;
    int status = 0;

    res = xoip_build_XE_msg(status, buf, sizeof(buf));

    if(!res){
        res = send_f1_packet(buf, strlen(buf));
    }else{
        ast_log(AST_LOG_ERROR, "XoIP : Failed build X E packet.\n");
    }
    return 0;
}


/*!
 * \brief Adjust volume of the communication.
 * \param track , the track number
 * \param callref, the callref
 * \param volume, the given volume
 *
 * \retval -1, on error the xoip communication can be find
 * \retval 0, on success
 *
 */
int volume_adjust_fn(int track, int callref, int volume)
{
    struct xoip_comm *xcomm  = NULL;

    if(volume <= 0 || volume > 10){
        return -1;
    }

    xcomm = xoip_channel_get(track, callref);
    if(xcomm == NULL){
        ast_log(LOG_WARNING, "Can not find the xcomm for track [%d] and callref [%d].\n", track, callref);
        return 0;
    }

       //
    xoip_set_talk_volume(xcomm, volume);
    xoip_set_listen_volume(xcomm, volume);    
    return 0;
}

/*!
 * \brief Send data to the caller channel
 * \param track, the track number
 * \param callref, the callref 
 * \param mode_transfer, the transfer mode can be 
 *      0 or 1 or 2 or 3 or 4
 *      Please see X-ISDN protocole specificaiton :-) I'm lazy
 *  \data, the data to transfer 
 *  \mode_operator, the micro state : M to mute N to non mute
 *  \break_record, the set off recording while sending data to the caller
 *
 *  \retval -1, on error : xoip communication can be found
 *  \retval 0, on success 
 *
 */
int send_data_fn(int track, int callref, int mode_transfer, char *data, char *mode_operator, char *break_record)
{
     char buf[BUFLEN];
     memset(buf, 0, sizeof(buf));

    struct xoip_comm *xcomm  = xoip_channel_get(track, callref);
    if(xcomm == NULL){
        ast_log(LOG_WARNING, "XoIP : Failed find a valid communication for the track [%d] and the callref [%d].\n", track, callref);
        return -1;
    }


    struct ast_channel *chan = xcomm->chan;
    int tone_types = 0;
    tone_types |= TONES_DTMF;

    int res = xoip_generate_tones (chan, data, strlen(data),
		     tone_types, 100);

    /* build notificaiton message */
    res = xoip_build_XL_msg(track, callref, res, buf, sizeof(buf));

    if(!res){
        res = send_f1_packet(buf, strlen(buf));
    }else{
        ast_log(AST_LOG_WARNING, "XoIP : Failed build X L packet for the track [%d] and the callref [%d].\n", track, callref);
    }

    return 0;
}

/*!
 * \brief Send a frequence to a call,identificated by given track/callref
 * \param track, the given track
 * \param callref, the given callref
 * \param freq, the frequency to send
 * \param duration, the frequency duration
 * \param loudness, the loudness of the sending frequency
 */
int send_freq_fn(int track, int callref, int freq, int duration, int loudness, char* mode, char *break_record)
{
    int res = 0;
    char buf[BUFLEN];
    memset(buf, 0, sizeof(buf));

    loudness = 1024;
    struct xoip_comm *xcomm  = xoip_channel_get(track, callref);
    if(xcomm == NULL){
        ast_log(LOG_WARNING, "XoIP : Failed find a valid communication for the track [%d] and the callref [%d].\n", track, callref);
        return -1;
    }
    /* adjust to the given duration to ms */
    res = xoip_generate_freq(xcomm, freq, duration * 100, loudness);

    res = xoip_build_Xf_msg(track, callref, 1, res, buf, sizeof(buf));

    if(!res){
        res = send_f1_packet(buf, strlen(buf));
    }else{
        ast_log(AST_LOG_WARNING, "XoIP : Failed build X f packet for the track [%d] and the callref [%d].\n", track, callref);
    }
    return 0;
}



/*!
 *
 */
int hangup_fn(int track, int callref){
    struct xoip_comm *xcomm  = xoip_channel_get(track, callref);
    if(xcomm == NULL){
        ast_log(LOG_WARNING, "Can not find the xcomm for track [%d] and callref [%d].\n", track, callref);
        return 0;
    }
        
    int causecode = 16;
    ast_channel_softhangup_withcause_locked(xcomm->chan, causecode);

    pbx_builtin_setvar_helper(xcomm->chan, XOIP_STATE_KEY, XOIP_STATE_VALUE_HANGUP);

#if 0
    if (ast_channel_state(xcomm->chan) != AST_STATE_DOWN){
	ast_softhangup(xcomm->chan, AST_SOFTHANGUP_ALL);
        ast_log(LOG_DEBUG, "Channel hangup for the track [%d] and callref  [%d].\n",track, callref);

    }else{
        ast_log(LOG_DEBUG, "Channel is not hangup for the track [%d] and callref  [%d].\n",track, callref);
    }
#endif 
    return 0;
}

int queuing_call_fn(int track, int callref, char *mode)
{

    int res = 0;
    char buf[BUFLEN];
    memset(buf, 0, sizeof(buf));

    struct xoip_comm *xcomm  = xoip_channel_get(track, callref);
    if(xcomm == NULL){
        ast_log(LOG_WARNING, "XoIP : Failed find a valid communication for the track [%d] and the callref [%d].\n", track, callref);
        return -1;
    }
    /* process queuing tha call */
    res = xoip_queuing_call(xcomm, mode);

    res = xoip_build_Xw_msg(track, callref, res, buf, sizeof(buf));

    if(!res){
        res = send_f1_packet(buf, strlen(buf));
    }else{
        ast_log(AST_LOG_WARNING, "XoIP : Failed build X w packet for the track [%d] and the callref [%d].\n", track, callref);
    }
    return 0;
}

/*!
 * \brief struct of handlers for f1 messages 
 */
struct f1_messages_handlers g_f1_handlers= {
            .answer = answer_fn,
            .commut = commut_fn,
            .switch_protocol = switch_protocol_fn,
            .ack_alarm = ack_alarm_fn,
            .record_request = record_request_fn,
            .polling = polling_fn,
            .send_data = send_data_fn,
            .volume_adjust = volume_adjust_fn,
            .send_freq = send_freq_fn,
            .hangup = hangup_fn,
            .queuing_call = queuing_call_fn,
        };


/*!
 * \brief Receive the data from the f1 socket
 * \param buf the message buffer
 * \param maxsize the maximale size of the receiver message
 *
 * \retval -1 on error
 * \retval > 0 on success
 */
static int recv_f1_packet(char* buf, int maxsize)
{
    int res = -1;

    socklen_t n;
    struct sockaddr sender_addr;

    n = sizeof(sender_addr);
    res = recvfrom(udp_f1_comm.sfd, buf, maxsize,0,&sender_addr, &n);
    return res;
}



/*!
 * \brief async handler for the f1 socket
 * \param sig the signal
 */
static void f1comm_recv_handler(int sig)
{
    UNUSED(sig);
    char buf[BUFLEN];
    memset(buf, 0, sizeof(buf)); 
    int res = recv_f1_packet(buf, BUFLEN);
    if(res > 0 ){
        ast_log(LOG_NOTICE, "Get f1 packet %s of %d\n.", buf, strlen(buf));
        res = process_message(buf, strlen(buf), &g_f1_handlers);
        //
    }
}

/*!
 * \brief Eanble async of the given socket
 * \param sfd the given socket id
 *
 * \retval -1 on error
 * \retval 0 on success
 */
static int enable_async(int sfd)
{
    int flags;
    struct sigaction sa;
    flags = fcntl(sfd, F_GETFL);
    //flags |= O_NONBLOCK;
    flags |= O_ASYNC;
    fcntl(udp_f1_comm.sfd, F_SETFL, flags);
    sa.sa_flags = 0;
    sa.sa_handler = f1comm_recv_handler;
    sigemptyset(&sa.sa_mask);

    if(sigaction(SIGIO,&sa,NULL)){
        ast_log(LOG_NOTICE, "Failed sigaction into enable_async\\n.");
  	return -1;
    }

    if(fcntl(sfd,F_SETOWN, getpid()) < 0){
        ast_log(LOG_NOTICE, "Failed set F_SETOWN into enable_async\n.");
        return -1;
    }

    if(fcntl(sfd,F_SETSIG, SIGIO) < 0){
        ast_log(LOG_NOTICE, "Failed set F_SETSIG into enable_async\n.");
        return -1;
    }
    
    return 0;

}


/*!
 * \brief Initialize the server socket for f1 communication
 *
 * \retval -1 on error
 * \retval 0 on success
 */
int init_f1_communication()
{
    struct hostent *host_other;
    struct ast_hostent ahp;
       /* */
    memset((char*) &udp_f1_comm.si_other, 0, sizeof(udp_f1_comm.si_other));
    memset((char*) &udp_f1_comm.si_me, 0, sizeof(udp_f1_comm.si_me));
    if ((udp_f1_comm.si_other.sin_addr.s_addr = inet_addr(config_f1_adr)) == -1) {
		/* its a name rather than an ipnum */
		host_other = ast_gethostbyname(config_f1_adr, &ahp);

		if (host_other == NULL) {
			ast_log(LOG_WARNING, "Failed gethostbyname \n");
			return -1;
		}
		memmove(&udp_f1_comm.si_other.sin_addr, host_other->h_addr, host_other->h_length);
	}
	/** **/
	udp_f1_comm.si_other.sin_family = AF_INET;
	udp_f1_comm.si_other.sin_port = htons(config_f1_port);
	/* */
	udp_f1_comm.si_me.sin_family = AF_INET;
  	udp_f1_comm.si_me.sin_addr.s_addr = htonl(INADDR_ANY);
  	udp_f1_comm.si_me.sin_port = htons(config_f1_port);
  	/* create_new_socket(AF_INET) */
  	if ((udp_f1_comm.sfd = socket(AF_INET,SOCK_DGRAM,0)) < 0) {
		ast_log(LOG_NOTICE, "Failed to create a new f1comm socket\n.");
		return -1;
	}

         
  	int res = bind(udp_f1_comm.sfd, (struct sockaddr *) &udp_f1_comm.si_me, 
                sizeof(udp_f1_comm.si_me));
  	if( res <0) {
  		ast_log(LOG_NOTICE, "Failed bind port for a f1comm socket\n.");
                close(udp_f1_comm.sfd);
  		return -1;
  	}

        res = enable_async(udp_f1_comm.sfd);
        if(res < 0){
            ast_log(LOG_NOTICE, "Failed sigaction for f1comm socket\n.");
            close(udp_f1_comm.sfd);
  	    return -1;

        }

    return 0;
}

/*!
 * \brief Dispatcher thread for f1 incomming messages
 */
static void *dispatch_thread_handler(void *data)
{
    UNUSED(data);
    ast_log(LOG_DEBUG, "Enter into the dispatcher thread\n.");
    char buf[BUFLEN];
    int i = 0;

    while (!dispatch_thread.stop) {
	sprintf(buf, "{test : %d}", i);
	//ast_log(LOG_NOTICE, "Send test %s of %d\n.", buf, strlen(buf));
	/*res = sendto(udp_f1_sender.fd, buf, strlen(buf), 0, 
                &udp_f1_sender.dest_addr, slen);*/
        /*res = send_f1_packet(buf, strlen(buf));
        memset(buf, 0, sizeof(buf));*/

        /*res = recv_f1_packet(buf, BUFLEN);
        if(res > 0 ){
           ast_log(LOG_NOTICE, "Get f1 packet %s of %d\n.", buf, strlen(buf));
        }
        memset(buf, 0, sizeof(buf)); */       
        i++;
        sleep(1);
    }
    ast_log(LOG_DEBUG, "Leave the dispatcher thread\n.");
    return NULL;
}


/*!
 * \brief cleanup module
 * Try to stop the dispatcher thread
 * Close the f1 socket
 */
static void cleanup_module(void)
{
    dispatch_thread.stop = 1;
    if(dispatch_thread.id != AST_PTHREADT_NULL){
        pthread_join(dispatch_thread.id, NULL);

    }

    if(udp_f1_comm.sfd != -1){
        close(udp_f1_comm.sfd);
        udp_f1_comm.sfd = -1;
    }
}



/*!
 * \brief Load XOIP_CONFIG config file
 * \return 1 on load, 0 file does not exist
*/
static int load_config(void){
    memset((char*) config_f1_adr, 0, sizeof(config_f1_adr));
    struct ast_config *cfg;
    struct ast_flags config_flags = { 0 };
    const char *p;

    /* loading application configuration */
    cfg = ast_config_load(XOIP_CONFIG, config_flags);
    if (cfg == CONFIG_STATUS_FILEMISSING || cfg == CONFIG_STATUS_FILEINVALID) {
        ast_log(LOG_WARNING, "Failed load the configuration file.\n");
        return -1;
    }

    p = ast_variable_retrieve(cfg, "f1", "address");
    if (!p) {
        ast_log(LOG_WARNING, "Cannot load parameter address from f1 configuration section\n.");
        return -1;
    }

    ast_copy_string(config_f1_adr, p, sizeof(config_f1_adr));
    //config_f1_adr[sizeof(config_f1_adr) - 1] = '\0';
    p = ast_variable_retrieve(cfg, "f1", "port");
    if (!p) {
        ast_log(LOG_WARNING, "Cannot load parameter port from f1 configuration section\n.");
        return -1;
    }

    config_f1_port = atoi(p);
    ast_log(LOG_NOTICE, "Configuration loaded f1 address : %s and port %d\n", config_f1_adr, config_f1_port);
    return 0;
}


#if 0
/*!
 * Depricated and keep for an example
 */
static void *xoip_read_data_thread(void* data)
{
    int res = 0;
    char data_buf[BUFLEN];
    struct xoip_comm *xcomm = data;
    struct ast_channel *chan = xcomm->chan;

    //ast_playtones_stop(chan);
    ast_channel_lock(chan);
    ast_set_flag(ast_channel_flags(chan), AST_FLAG_END_DTMF_ONLY);
    ast_channel_unlock(chan);
    
    while(!xcomm->stop_tones){
    //for (;;) { 
    //while(ast_waitfor(chan, -1) > -1){
        if(ast_check_hangup(chan)) {
            ast_log(AST_LOG_NOTICE, "XoIP : the read thread leave. Channel [%s]. HANGUP\n", 
                        ast_channel_name(chan));
	    break;
	}

        memset(data_buf, 0, sizeof(data_buf));

        res = xoip_read_data(xcomm, data_buf, sizeof(data_buf) - 1, 1);
        if(res == -1){
             ast_log(AST_LOG_NOTICE, "XoIP : the read thread leave. Channel [%s]. Get -1\n", 
                        ast_channel_name(chan));
            break;
        }

        if(strlen(data_buf) > 0){
            xcomm->func_data_callback(xcomm->track, xcomm->callref, data_buf);
        }

        
    }
    
    ast_log(AST_LOG_NOTICE, "XoIP : the read thread leave.\n");
    //xcomm->tones_thread_id = AST_PTHREADT_NULL;

    pthread_exit(0);

}
#endif 

static void hook_destroy_cb(void *framedata)
{
    ast_free(framedata);
    ast_log(AST_LOG_NOTICE, "XoIP : hook_destroy_cb called.\n");
}

/*!
 * \brief Callback for get DTMF ot other
 */

static struct ast_frame *hook_frame_event_cb(struct ast_channel *chan, struct ast_frame *frame, enum ast_framehook_event event, void *data)
{
    if (!frame) {
        return frame;
    }

    struct xoip_comm *xcomm = xoip_channel_find(chan);
    if(xcomm == NULL){
        ast_log(AST_LOG_WARNING, 
                "XoIP : Failed find the xoip comm for the channel [%s].\n",
                ast_channel_name(chan));
            return frame;
        }

    /* skip if the hangup signaled */
    if (!frame || (frame->frametype == AST_FRAME_CONTROL &&
                (frame->subclass.integer == AST_CONTROL_HANGUP || 
                 frame->subclass.integer == AST_CONTROL_BUSY ||
                 frame->subclass.integer == AST_CONTROL_CONGESTION))) {
        ast_log(AST_LOG_WARNING, 
                "XoIP[%02d,%04X] : Hangup signaled , delete channel[%s].\n",
                xcomm->track, xcomm->callref, 
                ast_channel_name(chan));
	return frame;
    }

    if (event != AST_FRAMEHOOK_EVENT_READ) {
        return frame;
    }
    
    struct ast_frame *frame2 = ast_dsp_process(chan, xcomm->dsp, frame);
    if(!frame2){
        ast_log(AST_LOG_WARNING, 
                "XoIP[%02d,%04X] : Bad DSP received for the channel [%s].\n",
                xcomm->track, xcomm->callref,ast_channel_name(chan));
        frame2 = frame;
    }

    if(frame2->frametype == AST_FRAME_DTMF_END){
        char buf[2] = {'\0'};
        snprintf (buf, sizeof(buf), "%c", frame2->subclass.integer);
        //print_frame(frame);
         xcomm->func_data_callback(xcomm->track, xcomm->callref, buf);
    }

    return frame2;
}


/*!
 *
 */
static int process_incomming_alarm(int track, int callref, struct ast_channel *chan, const char *data){
    int res = 0;
    char buf[BUFLEN];
    memset(buf, 0, sizeof(buf));
    struct xoip_comm *xcomm =  xoip_channel_get(track, callref);
    /* the channel must be answered */
    /* F1 endpoint notified by X g messages */

    if (ast_set_write_format_by_id(xcomm->chan,AST_FORMAT_ALAW)) {
        ast_log(LOG_WARNING, 
                "XoIP[%02d,%04X] : Unable to set write format to a-law on %s\n",
                xcomm->track, xcomm->callref, ast_channel_name(xcomm->chan));
        return -1;
    }

    res = ast_safe_sleep(chan, 500);
    /* keep for test must be changed */       
    const char *invitation_data = "A**A8181";
    int tone_types = 0;
    tone_types |= TONES_DTMF;
    res = xoip_generate_tones(xcomm->chan, 
            invitation_data, strlen(invitation_data), 
            tone_types, 100);
    if(res != 0){
        ast_log(AST_LOG_ERROR, "XoIP[%02d,%04X] : Failed to send dtmf.\n",
                xcomm->track, xcomm-callref);
        return -1;
    }

    int waiting = 1000;
    while (ast_waitfor(chan, waiting) > -1){
        /* If we fail to read in a frame, that means they hung up */
        if(ast_check_hangup(chan)) {
            break;
	}

        if(xcomm != NULL){
            const char* xoip_state_var = pbx_builtin_getvar_helper(xcomm->chan,
                    XOIP_STATE_KEY);
            if(xoip_state_var != NULL && 
                strcmp(xoip_state_var, XOIP_STATE_VALUE_COMMUT) == 0){
                break;
            }

            if(xoip_state_var != NULL  && 
                strcmp(xoip_state_var, XOIP_STATE_VALUE_HANGUP) == 0){
                    break;
            }
        }
        /* */
        memset(buf, 0, sizeof(buf));
        res = xoip_read_data(xcomm, buf, sizeof(buf) - 1, 2000);
        if(res == -1){
            ast_log(AST_LOG_NOTICE, 
                    "XoIP[%02d, %04d] : Failed read data on channel [%s].\n", 
                    xcomm->track, xcomm->callref, ast_channel_name(chan));
        }
        
        if(strlen(buf) > 0){
            xcomm->func_data_callback(xcomm->track, xcomm->callref, buf);
        }
    } /* end of while  */
    
    return 0;
}

/*!
 * \brief Applicaton itself 
 * \param chan the asterisk active channel
 * \param data data for the given channel
 *
 * \retval -1 on error
 * \retval 0 on success
 */
static int xoip_alarms(struct ast_channel *chan, const char *data)
{
    int res = 0;
    ast_log(LOG_NOTICE, "XoIP : starting with data : [%s].", data);
    
    if(ast_check_hangup(chan)) {
        ast_log(LOG_NOTICE, "XoIP : Early channel [%s] HANGUP.\n",
                ast_channel_name(chan));
        return 0;
    }

    struct ast_framehook_interface interface = {
        .version = AST_FRAMEHOOK_INTERFACE_VERSION,
	.event_cb = hook_frame_event_cb,
	.destroy_cb = hook_destroy_cb,
    };

    char buf[BUFLEN];
    int track = get_current_track();
    int callref = get_current_callref();
    xoip_channel_add(track, callref, chan);

    struct xoip_comm *xcomm = xoip_channel_get(track, callref);
    res = xoip_build_XC_msg(track, callref,ast_channel_exten(chan),
            ast_channel_cdr(chan)->src,
            ast_channel_cdr(chan)->clid,
            buf,
            sizeof(buf));
    if(res  < 0){
        ast_log(LOG_WARNING,
                "XoIP[%02d,%04X]: Failed build X C message for channel [%s].\n",
                track, callref, ast_channel_name(chan));
        return -1;
    }

    res = send_f1_packet(buf, strlen(buf));

    if(xcomm == NULL){
        ast_log(LOG_WARNING,
                "XoIP[%02d,%04X]: Failed add the communication to the global [%s].\n",
                track, callref, ast_channel_name(chan));
        return -1;
    }
    /* */
    char *hangup_cb = "xoip-comm-hangup,s,1";
    ast_pbx_hangup_handler_push(chan, hangup_cb);
    /* ringing to the caller */
    ast_indicate(chan, AST_CONTROL_RINGING);
    
    int i = 0;
    struct frame_trace_data *framedata;
    char *value = "DTMF_END";
    if (!(framedata = ast_calloc(1, sizeof(*framedata)))) {
        return 0;
    }

    interface.data = framedata;

    ast_channel_lock(chan);
    i = ast_framehook_attach(chan, &interface);
    ast_verb(5, "XoIP[%02d,%04X]: Framehook attached with result [%d].\n",
            xcomm->track, xcomm->callref, i);
    ast_channel_unlock(chan);

    while (ast_waitfor(chan, -1) > -1){
        /* If we fail to read in a frame, that means they hung up */
        if(ast_check_hangup(chan)) {
            ast_verb(5, "XoIP[%02d,%04X]: XoIP detect hangup into xoip_exec for the channel [%s]. Bye\n",
                    xcomm->track, xcomm->callref, ast_channel_name(chan));
            break;
        }

        if(xcomm != NULL){
            const char* xoip_state_var = pbx_builtin_getvar_helper(xcomm->chan, XOIP_STATE_KEY);

            if(xoip_state_var != NULL &&
                    strcmp(xoip_state_var, XOIP_STATE_VALUE_ANSWER) == 0){
                    pbx_builtin_setvar_helper(xcomm->chan, XOIP_STATE_KEY, "");
                    res = process_incomming_alarm(track, callref,chan, data);
            }

            if(xoip_state_var != NULL && 
                    strcmp(xoip_state_var, XOIP_STATE_VALUE_COMMUT) == 0){
                    /* stop the alarm processing */
                    pbx_builtin_setvar_helper(xcomm->chan, XOIP_STATE_KEY, "");
                    do_xoip_commut(xcomm);
            }
        }
    }
    /* detach event callback */      
    return 0;
}

/*!
 * \brief Send a hangup notification to the f1 endpoint
 * Delete the XoIP communication from the global list
 * \param chan The given channel
 * \param data The given data of the channel
 *
 * \retval -1 on error
 * \retval 0 on success
 */
static int xoip_hangup(struct ast_channel *chan, const char *data)
{
    int res = 0;
    char buf[BUFLEN];
    struct xoip_comm *xcomm = xoip_channel_find(chan);
    if(!chan){
        ast_log(AST_LOG_WARNING, "XoIP : Failed to find the xoip valid communication for the channel[%s].\n",
                ast_channel_name(chan));
        return -1;
    }
    
    int track = xcomm->track;
    int callref = xcomm->callref;

    res = xoip_channel_del(track, callref);
    if(res < 0){
        ast_log(LOG_DEBUG, 
                "XoIP[%02d,%04X] : Failed to remove xoip communication.\n", track, callref);
    }
    memset(buf, 0, sizeof(buf));
    res = xoip_build_Xr_msg(track, callref,
                buf,
                sizeof(buf));

    res = send_f1_packet(buf, strlen(buf)); 
    return 0;
}

/*!
 * \brief Set a channel into wait state.
 * \param chan The given channel
 * \param data The data of the given channel
 *
 * \retval -1 on error
 * \retval 0 on success
 */
static int xoip_park(struct ast_channel *chan, const char *data)
{
    /* try identify the xoip active communication */
    struct xoip_comm *xcomm = xoip_channel_find(chan);
    if(!chan){
        ast_log(AST_LOG_WARNING, 
                "XoIP : Failed to find the xoip comm for the channel[%s].\n",
                ast_channel_name(chan));
        return -1;
    }

    while (ast_waitfor(chan, -1) > -1){
        /* If we fail to read in a frame, that means they hung up */
        if(ast_check_hangup(chan)) {
            ast_log(LOG_NOTICE, 
                    "XoIP[%02d,%04X]: XoIP detect hangup the channel [%s]. Bye\n",
                    xcomm->track, xcomm->callref, ast_channel_name(chan));
            break;
        }

        if(xcomm != NULL){
            const char* xoip_state_var = pbx_builtin_getvar_helper(xcomm->chan, XOIP_STATE_KEY);
            if(xoip_state_var != NULL && strcmp(xoip_state_var,
                        XOIP_STATE_VALUE_COMMUT) == 0){
                /* clean up the state variable value */
                pbx_builtin_setvar_helper(xcomm->chan, XOIP_STATE_KEY, "");
                do_xoip_commut(xcomm);
                /* Finish the park after commut because the call
                 * can be set on wait state.
                * */
                break;                     
            }
        }
    }
    return 0;
}

/*!
 * \brief the standard asterisk module function
 *
 * \retval -1 on success
 * \reval 0 on success
 */
static int unload_module(void)
{
	ast_unregister_application(app_xoip_alarms);
        ast_unregister_application(app_xoip_park);
        ast_unregister_application(app_xoip_hangup);
        cleanup_module();

        return 0;
}

/*!
 * \brief module load function
 * Initialize communicaiton with f1(prepare the server socket)
 * Register xoip_exec function
 *
 * \retval -1 on error
 * \retval 0 on success
 */
static int load_module(void)
{
        int res = 0;
        res = load_config();
	if(res < 0){
            res = AST_MODULE_LOAD_DECLINE;
            ast_log(LOG_ERROR, "Failed load configuration.\n");
            goto failed;
        }

        res = init_f1_communication();
        if(res != 0){
            ast_log(LOG_ERROR, "Failed initialize f1 communication.\n");
            goto failed;
        }

        if (ast_pthread_create_background(&dispatch_thread.id, NULL,
			dispatch_thread_handler, NULL)) {
		ast_log(LOG_ERROR, "Failed start the xoip dispatch thread.\n");
		goto failed;
	}

	res = ast_register_application_xml(app_xoip_alarms, xoip_alarms);
        if(res){
            ast_log(LOG_ERROR, "Failed to register XoIP application.\n");
            goto failed;
        }

        res = ast_register_application_xml(app_xoip_park, xoip_park);
        if(res){
            ast_log(LOG_ERROR, "Failed to register XoIP application.\n");
            goto failed;
        }



        res = ast_register_application_xml(app_xoip_hangup, xoip_hangup);
        if(res){
            ast_log(LOG_ERROR, "Failed to register XoIP hangup application.\n");
            goto failed;
        }

        
        return AST_MODULE_LOAD_SUCCESS;

        failed:
            ast_log(LOG_ERROR, "xoip loading failed.\n");
	    cleanup_module();     

	return AST_MODULE_LOAD_DECLINE;
}

AST_MODULE_INFO_STANDARD(ASTERISK_GPL_KEY, "Xoip Application");
