#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "moni.h"
#include "myconfig.h"
#include "global.h"
#include "util.h"
#include "log.h"

#define INTVAL_SEC 5

static char *tc_face_name = NULL;
static int recv_limit = 0;
static int send_limit = 0;

volatile int recv_pass_ratio;
volatile int send_pass_ratio; 
int max_pend_value = 0xFF;
static int pass_step = 3;
static int pass_up_value = 240;
static int max_pass_up_value = 300;
static int max_pass_up_value_2 = 400;
static int pass_down_value = 160;
static int min_pass_size = 1024;

static int get_tc_recv_send(uint64_t *recv_bytes, uint64_t *send_bytes)
{
	char cmdstr[256] = {0x0};
	snprintf(cmdstr, sizeof(cmdstr), "cat /proc/net/dev |grep %s |awk -F\\: '{print $2}' |awk '{print $1\" \"$9}'", tc_face_name);

	FILE *fp = popen(cmdstr, "r");
	if (fp == NULL)
	{
		LOG(glogfd, LOG_ERROR, "get_tc_recv_send popen %s error\n", cmdstr);
		send_pass_ratio = max_pend_value;
		recv_pass_ratio = max_pend_value;
		return -1;
	}
	char buf[256] = {0x0};
	fread(buf, 1, sizeof(buf), fp);
	pclose(fp);

	char *p = strchr(buf, ' ');
	if (p == NULL)
	{
		LOG(glogfd, LOG_ERROR, "buf %s error\n", buf);
		send_pass_ratio = max_pend_value;
		recv_pass_ratio = max_pend_value;
		return -1;
	}

	*p = 0x0;
	*recv_bytes = atol(buf);
	*send_bytes = atol(p+1);
	return 0;
}

static void cal_limit(volatile int *result, uint64_t two, uint64_t one, int int_limit)
{
	if (two - one < min_pass_size)
	{
		*result = *result << 1;
		LOG(glogfd, LOG_DEBUG, "%s %d\n", ID, LN);
		if (*result > max_pend_value)
			*result = max_pend_value;
		if (*result < 5)
			*result = 5;
		return;
	}
	uint64_t cur_speed = 8 * (two - one) / INTVAL_SEC;
	uint64_t cur_pass_ratio = max_pend_value * cur_speed / int_limit;
	if (cur_pass_ratio <= pass_down_value)
	{
		LOG(glogfd, LOG_DEBUG, "%s %d %lu %lu %d %lu %d\n", ID, LN, two, one, int_limit, cur_pass_ratio, pass_down_value);
		*result += pass_step;
		if (*result > max_pend_value)
			*result = max_pend_value;
		return ;
	}

	if (cur_pass_ratio <= pass_up_value)
		return ;

	LOG(glogfd, LOG_DEBUG, "%s %d\n", ID, LN);
	if (cur_pass_ratio <= max_pass_up_value)
	{
		LOG(glogfd, LOG_DEBUG, "%s %d\n", ID, LN);
		*result -= pass_step; 
	}
	else if (cur_pass_ratio <= max_pass_up_value_2)
	{
		LOG(glogfd, LOG_DEBUG, "%s %d\n", ID, LN);
		*result -= pass_step * 2;
	}
	else
	{
		LOG(glogfd, LOG_DEBUG, "%s %d\n", ID, LN);
		*result = *result >> 1;
	}
	if (*result < 5)
		*result = 5;
}

void check_tc() 
{
	if (tc_face_name == NULL)
		return;
	uint64_t recv_bytes = 0;
	uint64_t send_bytes = 0;

	if (get_tc_recv_send(&recv_bytes, &send_bytes))
		return;
	sleep (INTVAL_SEC);

	uint64_t recv_bytes_2 = 0;
	uint64_t send_bytes_2 = 0;

	if (get_tc_recv_send(&recv_bytes_2, &send_bytes_2))
		return;

	if (send_limit > 0)
		cal_limit(&send_pass_ratio, send_bytes_2, send_bytes, send_limit);
	LOG(glogfd, LOG_DEBUG, "send = %lu send = %lu limit = %d pass = %d\n", send_bytes_2, send_bytes, send_limit, send_pass_ratio);
	if (recv_limit > 0)
		cal_limit(&recv_pass_ratio, recv_bytes_2, recv_bytes, recv_limit);
	LOG(glogfd, LOG_DEBUG, "recv = %lu recv = %lu limit = %d pass = %d\n", recv_bytes_2, recv_bytes, recv_limit, recv_pass_ratio);
}

int init_tc() 
{
	send_pass_ratio = max_pend_value;
	recv_pass_ratio = max_pend_value;
	tc_face_name = myconfig_get_value("tc_face_name");
	if (tc_face_name == NULL)
	{
		LOG(glogfd, LOG_DEBUG, "no tc_face_name.\n");
		return 0;
	}

	recv_limit = myconfig_get_intval("tc_recv_limit", -1);
	send_limit = myconfig_get_intval("tc_send_limit", -1);
	LOG(glogfd, LOG_DEBUG, "tc_face_name = %s send = %d recv = %d\n", tc_face_name, send_limit, recv_limit);
	return 0;
}
