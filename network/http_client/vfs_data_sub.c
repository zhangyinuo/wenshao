/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

#include "uri_decode.h"
#include "c_api.h"
#include "myconv.h"

FILE *fp_sou = NULL;
FILE *fp_url = NULL;
FILE *fp_head = NULL;
FILE *fp_body = NULL;

#define MAX_CONVERT 204800

enum {SOU = 0, URL, HEAD, BODY};

int active_send(int fd, char *data)
{
	LOG(vfs_sig_log, LOG_DEBUG, "send %d cmdid %s\n", fd, data);
	set_client_data(fd, data, strlen(data));
	modify_fd_event(fd, EPOLLOUT);
	return 0;
}

static int init_stock()
{
	fp_sou = fopen("../path/sou", "w");
	if (fp_sou == NULL)
	{
		LOG(vfs_sig_log, LOG_ERROR, "fopen fp_sou err %m\n");
		return -1;
	}

	fp_url = fopen("../path/url", "w");
	if (fp_url == NULL)
	{
		LOG(vfs_sig_log, LOG_ERROR, "fopen fp_url err %m\n");
		return -1;
	}

	fp_head = fopen("../path/head", "w");
	if (fp_head == NULL)
	{
		LOG(vfs_sig_log, LOG_ERROR, "fopen fp_head err %m\n");
		return -1;
	}

	fp_body = fopen("../path/body", "w");
	if (fp_body == NULL)
	{
		LOG(vfs_sig_log, LOG_ERROR, "fopen fp_body err %m\n");
		return -1;
	}
	return 0;
}

static void do_print_process(int type, char *data)
{
	if (type == SOU)
		fprintf(fp_sou, "%s\n", data);
	else if (type == URL)
		fprintf(fp_url, "%s\n", data);
	else if (type == HEAD)
		fprintf(fp_head, "%s\n", data);
	else if (type == BODY)
		fprintf(fp_body, "%s\n", data);
	return;
}

static int get_title(char *src, int srclen, char *dst)
{
	char *s = strcasestr(src, "<title>");
	if (s == NULL)
		return -1;

	char *e = strcasestr(s, "</title>");
	if (e == NULL)
		return -1;

	LOG(vfs_sig_log, LOG_DEBUG, "prepare head!\n");
	if (e - s - 7 > 1024)
	{
		LOG(vfs_sig_log, LOG_DEBUG, "prepare head error!\n");
		return -1;
	}

	strncpy(dst, s + 7, e - s - 7);
	LOG(vfs_sig_log, LOG_DEBUG, "get head [%s]!\n", dst);
	return 0;

}

void do_process_sub(char *data, int len)
{
	if (isprint(*data) == 0)
		return;

	char title[1024] = {0x0};

	if (get_title(data, len, title) == 0)
		do_print_process(HEAD, title);
	do_print_process(BODY, data);
}

