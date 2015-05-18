/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <unistd.h>
#include <sys/syscall.h>
#include "common.h"
#include "global.h"
#include "GeneralHashFunctions.h"
#include "vfs_so.h"
#include "vfs_init.h"
#include "myepoll.h"
#include "protocol.h"
#include "util.h"
#include "acl.h"
#include "cdc_http.h"
int vfs_http_log = -1;

uint32_t g_index = 0;

t_path_info cdc_path;

typedef struct {
	char *f;
	char *d;
	char *fc;
} t_sync_file;

typedef struct {
	char ip[16];
	uint8_t isp;
} t_ip_isp;

typedef struct {
	char ip[16];
} t_ips;

#include "cdc_http_sub.c"

int svc_init() 
{
	char *logname = myconfig_get_value("log_data_logname");
	if (!logname)
		logname = "./http_log.log";

	char *cloglevel = myconfig_get_value("log_data_loglevel");
	int loglevel = LOG_NORMAL;
	if (cloglevel)
		loglevel = getloglevel(cloglevel);
	int logsize = myconfig_get_intval("log_data_logsize", 100);
	int logintval = myconfig_get_intval("log_data_logtime", 3600);
	int lognum = myconfig_get_intval("log_data_lognum", 10);
	vfs_http_log = registerlog(logname, loglevel, logsize, logintval, lognum);
	if (vfs_http_log < 0)
		return -1;
	LOG(vfs_http_log, LOG_DEBUG, "svc_init init log ok!\n");

	char *v = myconfig_get_value("path_indir");
	if (v == NULL)
	{
		LOG(vfs_http_log, LOG_ERROR, "config have not path_indir!\n");
		return -1;
	}
	snprintf(cdc_path.indir, sizeof(cdc_path.indir), "%s", v);

	v = myconfig_get_value("path_outdir");
	if (v == NULL)
	{
		LOG(vfs_http_log, LOG_ERROR, "config have not path_outdir!\n");
		return -1;
	}
	snprintf(cdc_path.outdir, sizeof(cdc_path.outdir), "%s", v);

	return init_db(); 
}

int svc_send(int fd)
{
	return 0;
}

int svc_recv(int fd)
{
	return 0;
}

void svc_timeout()
{
	do_dispatcher();

	do_reclaim();
}
