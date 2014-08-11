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

#ifndef AST_MODULE
#define AST_MODULE "test_xoip"
#endif 

ASTERISK_FILE_VERSION(__FILE__, "")

#include "asterisk/test.h"
#include "asterisk/module.h"
#include "asterisk/logger.h"
#include "asterisk/config.h" /* PARSE_PORT_* */

/* xoip internal includes */
#include "xoip_types.h"
#include "xoip_utils.h"
#include "xoip_messages.h"

/* helper variables for check if the test passed (by test function type  */
static int g_answer_test_ok = 0;
static int g_commut_test_ok = 0;
static int g_hangup_test_ok = 0;
static int g_send_data_test_ok = 0;
static int g_volume_adjust_test_ok = 0;
static int g_send_freq_test_ok = 0;

/* */
int g_track = 1;
int g_callref = 9999;

/* */
static int g_volume_adjust_volume = 5;

/* */
static int g_send_data_mode_transfer = 1;
static char *g_send_data_data = "12345";
static char *g_send_data_mode_operator = "M";
static char *g_send_data_break_operator = "H";

/* */
static int g_send_freq_feq = 2100;
static int g_send_freq_duration = 5;
static int g_send_freq_level = 50;
static char *g_send_freq_mode = "M";
static char *g_send_freq_break_record = "H";
/* 
 * START of the callback messages parssing 
 *
 */
int answer_fn(int track, int callref, char proto, int level){
    int res = 0;
    if(track != g_track){
        ast_log(AST_LOG_ERROR, "Test answer_fn failed cause the test track [%d] but get [%d].\n", track, g_track);
        return -1;
    }

    if(track != g_track){
        ast_log(AST_LOG_ERROR, "Test answer_fn failed cause the test callref [%d] but get [%d].\n", callref, g_callref);
        return -1;
    }

    g_answer_test_ok++;
    ast_log(AST_LOG_NOTICE, "Test answer_fn called : [%d].\n", g_answer_test_ok);
    return res;
}


int commut_fn(int track, int callref, char* callee, char* trans, char* volum){
    int res = 0;
    g_commut_test_ok++;
    ast_log(AST_LOG_NOTICE, "Test commut_fn called : [%d].\n", g_commut_test_ok);
    return res;
}

int send_data_fn(int track, int callref, int mode_transfer, char *data, char* mode_operator, char* break_record){

    if(mode_transfer != g_send_data_mode_transfer){
        ast_log(AST_LOG_ERROR, "Failed test send_data.\n");
        ast_log(AST_LOG_ERROR, "Mode transfer origin : %d reveived : %d.\n", g_send_data_mode_transfer, mode_transfer);
        return -1;
    }

    if(strcmp(data, g_send_data_data) != 0){
        ast_log(AST_LOG_ERROR, "Failed test send_data.\n");
        ast_log(AST_LOG_ERROR, "Data origin : %s reveived : %s.\n", g_send_data_data, data);
        return -1;

    }

    
    if(strcmp(mode_operator, g_send_data_mode_operator) != 0){
        ast_log(AST_LOG_ERROR, "Failed test send_data.\n");
        ast_log(AST_LOG_ERROR, "Mode operator origin : %s reveived : %s.\n", g_send_data_mode_operator, mode_operator);
        return -1;

    }

    if(strcmp(break_record, g_send_data_break_operator) != 0){
        ast_log(AST_LOG_ERROR, "Failed test send_data.\n");
        ast_log(AST_LOG_ERROR, "Break operator origin : %s reveived : %s.\n", g_send_data_break_operator, break_record);
        return -1;

    }



    g_send_data_test_ok++;
    ast_log(AST_LOG_NOTICE, "Test send_data : [%d].\n", g_send_data_test_ok);
    return 0;

}

int volume_adjust_fn(int track, int callref, int volume){
    if(volume != g_volume_adjust_volume){
        ast_log(AST_LOG_ERROR, "Failed test volume_adjust.\n");
        ast_log(AST_LOG_ERROR, "Volume  original : %d reveived : %d.\n", g_volume_adjust_volume, volume);
        return -1;

    }

    g_volume_adjust_test_ok++;
    ast_log(AST_LOG_NOTICE, "Test volume_adjust_fn : [%d].\n", g_volume_adjust_test_ok);
    return 0;
}


int send_freq_fn(int track, int callref, int freq, int duration, int level, char* mode, char *break_record)
{
        if(freq != g_send_freq_feq || duration != g_send_freq_duration || level != g_send_freq_level){
        ast_log(AST_LOG_ERROR, "Failed test send_freq_fn.\n");
        ast_log(AST_LOG_ERROR, "Frequance original : %d reveived : %d.\n", g_send_freq_feq, freq);
        ast_log(AST_LOG_ERROR, "Duration original : %d reveived : %d.\n", g_send_freq_duration, duration);
        ast_log(AST_LOG_ERROR, "Level original : %d reveived : %d.\n", g_send_freq_level, level);
        return -1;
    }
    g_send_freq_test_ok++;
    ast_log(AST_LOG_NOTICE, "Test send_freq_fn called : [%d].\n", g_send_freq_test_ok);
    return 0;
}


int hangup_fn(int track, int callref){
    int res = 0;
    g_hangup_test_ok++;
    ast_log(AST_LOG_NOTICE, "Test hangup_fn called : [%d].\n", g_hangup_test_ok);
    return res;

}

/* 
 * END of callbacks messages parcing
 *  
 */

AST_TEST_DEFINE(test_generate_callref)
{
	int res = AST_TEST_PASS;

        switch (cmd) {
	    case TEST_INIT:
		info->name = "test_generate_callref";
		info->category = "/apps/app_xoip/";
		info->summary = "xoip unit test for generate callref number";
		info->description =
			"This tests for xoip basics functions.";
		return AST_TEST_NOT_RUN;
	    case TEST_EXECUTE:
               	break;
	}

        int n = MAX_CALLREF + 1;
        int callref = 0;
        int i = 0;
        for ( i = 0; i < n; i += 1 ) {
            callref = get_current_callref(); 
        }

        if(callref != 1){
            ast_test_status_update(test, "Failed to get correct callref. Callref value %d.\n", callref);
	    res = AST_TEST_FAIL;
        }
        ast_test_status_update(test, "Test value %d.\n", callref);
        ast_log(AST_LOG_NOTICE, "test generate callref.\n");


	return res;
}

AST_TEST_DEFINE(test_generate_track)
{
	int res = AST_TEST_PASS;

        switch (cmd) {
	    case TEST_INIT:
		info->name = "test_generate_track";
		info->category = "/apps/app_xoip/";
		info->summary = "xoip unit test for generate track number";
		info->description =
			"This tests for xoip basics functions.";
		return AST_TEST_NOT_RUN;
	    case TEST_EXECUTE:
		break;
	}

        int n = MAX_TRACK + 1;
        int track = 0;
        int i = 0;
        for ( i = 0; i < n; i += 1 ) {
            track = get_current_track(); 
        }

        if(track != 1){
            ast_test_status_update(test, "Failed to get correct track. Track value %d.\n", track);
	    res = AST_TEST_FAIL;
        }
        ast_test_status_update(test, "Test value %d.\n", track);
        ast_log(AST_LOG_NOTICE, "test generate track.\n");


	return res;
}

AST_TEST_DEFINE(test_f1_messages_handler)
{
	int res = AST_TEST_PASS;

        switch (cmd) {
	    case TEST_INIT:
		info->name = "test_f1_messages_handler";
		info->category = "/apps/app_xoip/";
		info->summary = "xoip unit test for f1 messages handler";
		info->description =
			"This tests for xoip basics messages handler(parsing).";
		return AST_TEST_NOT_RUN;
	    case TEST_EXECUTE:
		break;
	}
        
        /* */
        struct f1_messages_handlers handlers= {
            .answer = answer_fn,
            .commut = commut_fn,
            .send_data = send_data_fn,
            .volume_adjust = volume_adjust_fn,
            .send_freq = send_freq_fn,
            .hangup = hangup_fn
        };
        //
        char buf[BUFLEN] = {'\0'};
        snprintf(buf, sizeof(buf) - 1, "F G,%02d,%04X,P,100\r", g_track, g_callref);
        ast_test_status_update(test, "Test one message packet.\n");

        res = process_message(buf, strlen(buf), &handlers);
        if(res <= 0){
            return AST_TEST_FAIL;
        }

        if(strlen(buf) != res){
            ast_test_status_update(test, "Test buffer length %d but get the response %d.\n", strlen(buf),res);
            return AST_TEST_FAIL;
        }

        ast_test_status_update(test, "Test mutiple messages packet.\n");

        memset(buf,0, sizeof(buf));

        snprintf(buf, sizeof(buf) - 1, "F G,%02d,%04X,P,100\rF R,%02d,%04X\r", 
                g_track, g_callref, g_track, g_callref);

        res = process_message(buf, strlen(buf), &handlers);

        if(strlen(buf) != res){
            ast_test_status_update(test, "Test buffer length %d but get the response %d.\n", strlen(buf),res);
            return AST_TEST_FAIL;
        }


        memset(buf,0, sizeof(buf));

        snprintf(buf, sizeof(buf) - 1, "F H,%02d,%04X,9002,6006\rF R,%02d,%04X\r", 
                g_track, g_callref);

        res = process_message(buf, strlen(buf), &handlers);

        if(strlen(buf) != res){
            ast_test_status_update(test, "Test buffer length %d but get the response %d.\n", strlen(buf),res);
            return AST_TEST_FAIL;
        }
#if 1
        /* */
        memset(buf,0,sizeof(buf));
        sprintf(buf,"F L,%02d,%04X,%01d,%s,%s,%s\r", 
                g_track, g_callref, g_send_data_mode_transfer,
                g_send_data_data, g_send_data_mode_operator,g_send_data_break_operator);
        res = process_message(buf, strlen(buf), &handlers);
#endif 

#if 1
        /* */
        memset(buf,0,sizeof(buf));
        snprintf(buf, sizeof(buf),"F V,%02d,%04X,%01d\r", 
                g_track, g_callref, g_volume_adjust_volume);

        res = process_message(buf, strlen(buf), &handlers);
#endif
        /* */

#if 1
        memset(buf,0,sizeof(buf));
        snprintf(buf, sizeof(buf),"F F,%02d,%04X,%04d,%04d,%03d,%s,%s\r", 
                g_track, g_callref, g_send_freq_feq, g_send_freq_duration, g_send_freq_level,g_send_freq_mode,g_send_freq_break_record);

        res = process_message(buf, strlen(buf), &handlers);
#endif
        /* check different values of testing */
        
        if(g_answer_test_ok <= 0){
             ast_test_status_update(test, "Test answer message did not passed.\n");
            return AST_TEST_FAIL;
        }

        if(g_commut_test_ok <= 0){
             ast_test_status_update(test, "Test commut message did not pasesd.\n");
            return AST_TEST_FAIL;
        }
        

        if(g_hangup_test_ok <= 0){
            ast_test_status_update(test, "Test hangup message did not passed.\n");
            return AST_TEST_FAIL;
        }

        if(g_send_data_test_ok <= 0){
            ast_test_status_update(test, "Test send data message did not passed.\n");
            return AST_TEST_FAIL;

        }

        if(g_volume_adjust_test_ok <= 0){
            ast_test_status_update(test, "Test volume adjust message did not passed.\n");
            return AST_TEST_FAIL;
        }

        if(g_send_freq_test_ok <= 0){
            ast_test_status_update(test, "Test send freq message did not passed.\n");
            return AST_TEST_FAIL;
        }


        return  AST_TEST_PASS;
}


static int unload_module(void)
{
	AST_TEST_UNREGISTER(test_generate_callref);
	AST_TEST_UNREGISTER(test_generate_track);
        AST_TEST_UNREGISTER(test_f1_messages_handler);
	return 0;
}

static int load_module(void)
{
	AST_TEST_REGISTER(test_generate_callref);
	AST_TEST_REGISTER(test_generate_track);
        AST_TEST_REGISTER(test_f1_messages_handler);
        ast_log(AST_LOG_NOTICE, "module loaded enregistred 1");
	return AST_MODULE_LOAD_SUCCESS;
}

AST_MODULE_INFO_STANDARD(ASTERISK_GPL_KEY, "XoIP utils test module");

