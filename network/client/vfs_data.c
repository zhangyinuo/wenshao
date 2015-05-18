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
#include "vfs_so.h"
#include "myepoll.h"
#include "protocol.h"
#include "vfs_data.h"
#include "util.h"
#include "acl.h"
#include "vfs_task.h"
#include "mysql.h"
#include "mysqld_error.h"
#define MAX_PORT 0x100
#define MAX_URL 0x1000
#define MAX_LEN_URL 0x100

static __thread MYSQL * mysql;

static __thread int g_port[MAX_PORT];
static __thread int g_port_count;

static __thread char g_url[MAX_URL][MAX_LEN_URL];
static __thread int g_url_count;

static __thread int vfs_sig_log = -1;
/* online list */
static __thread list_head_t activelist;  //用来检测超时
static __thread list_head_t online_list[256]; //用来快速定位查找

static __thread int g_queue = 1;
int svc_initconn(int fd); 
int active_send(int fd, char *data);

#include "vfs_data_base.c"

static int init_proxy_info()
{
	return 0;
}

int svc_init(int queue) 
{
	char *logname = myconfig_get_value("log_data_logname");
	if (!logname)
		logname = "./data_log.log";

	char logname2[256] = {0x0};
	snprintf(logname2, sizeof(logname2), "%s_%d", logname, queue);

	char *cloglevel = myconfig_get_value("log_data_loglevel");
	int loglevel = LOG_NORMAL;
	if (cloglevel)
		loglevel = getloglevel(cloglevel);
	int logsize = myconfig_get_intval("log_data_logsize", 100);
	int logintval = myconfig_get_intval("log_data_logtime", 3600);
	int lognum = myconfig_get_intval("log_data_lognum", 10);
	vfs_sig_log = registerlog(logname2, loglevel, logsize, logintval, lognum);
	if (vfs_sig_log < 0)
		return -1;
	LOG(vfs_sig_log, LOG_NORMAL, "svc_init init log ok!\n");
	INIT_LIST_HEAD(&activelist);
	int i = 0;
	for (i = 0; i < 256; i++)
	{
		INIT_LIST_HEAD(&online_list[i]);
	}
	if (init_proxy_info())
	{
		LOG(vfs_sig_log, LOG_ERROR, "init_proxy_info err!\n");
		return -1;
	}
	
	g_queue = queue;
	LOG(vfs_sig_log, LOG_NORMAL, "queue = %d !\n", g_queue);

	return init_db() || reload_port_url();
}

int svc_initconn(int fd) 
{
	LOG(vfs_sig_log, LOG_TRACE, "%s:%s:%d\n", ID, FUNC, LN);
	struct conn *curcon = &acon[fd];
	if (curcon->user == NULL)
		curcon->user = malloc(sizeof(vfs_cs_peer));
	if (curcon->user == NULL)
	{
		LOG(vfs_sig_log, LOG_ERROR, "malloc err %m\n");
		char val[256] = {0x0};
		snprintf(val, sizeof(val), "malloc err %m");
		SetStr(VFS_MALLOC, val);
		return RET_CLOSE_MALLOC;
	}
	vfs_cs_peer *peer;
	memset(curcon->user, 0, sizeof(vfs_cs_peer));
	peer = (vfs_cs_peer *)curcon->user;
	peer->hbtime = time(NULL);
	peer->sock_stat = CONNECTED;
	peer->fd = fd;
	INIT_LIST_HEAD(&(peer->alist));
	list_move_tail(&(peer->alist), &activelist);
	LOG(vfs_sig_log, LOG_TRACE, "a new fd[%d] init ok!\n", fd);
	return 0;
}

static int check_req(int fd)
{
	LOG(vfs_sig_log, LOG_DEBUG, "%s:%s:%d\n", ID, FUNC, LN);
	char *data;
	size_t datalen;
	if (get_client_data(fd, &data, &datalen))
	{
		LOG(vfs_sig_log, LOG_TRACE, "%s:%d fd[%d] no data!\n", FUNC, LN, fd);
		return -1;  /*no suffic data, need to get data more */
	}
	if (datalen > 1024000)
		return 0;

	dump_return_msg(fd, data, datalen);
	return 0;
}

int svc_recv(int fd) 
{
	struct conn *curcon = &acon[fd];
	vfs_cs_peer *peer = (vfs_cs_peer *) curcon->user;
	peer->hbtime = time(NULL);
	list_move_tail(&(peer->alist), &activelist);
	LOG(vfs_sig_log, LOG_TRACE, "fd[%d] sock stat %d!\n", fd, peer->sock_stat);
	if (check_req(fd))
		return RECV_ADD_EPOLLIN;
	return RECV_CLOSE;
}

int svc_send_once(int fd)
{
	struct conn *curcon = &acon[fd];
	vfs_cs_peer *peer = (vfs_cs_peer *) curcon->user;
	peer->hbtime = time(NULL);
	return SEND_ADD_EPOLLIN;
}

int svc_send(int fd)
{
	struct conn *curcon = &acon[fd];
	vfs_cs_peer *peer = (vfs_cs_peer *) curcon->user;
	peer->hbtime = time(NULL);
	list_move_tail(&(peer->alist), &activelist);
	return SEND_ADD_EPOLLIN;
}

void svc_timeout()
{
	time_t now = time(NULL);
	int to = g_config.timeout * 10;
	vfs_cs_peer *peer = NULL;
	list_head_t *l;
	list_for_each_entry_safe_l(peer, l, &activelist, alist)
	{
		if (peer == NULL)
			continue;   /*bugs */
		if (now - peer->hbtime < to)
			break;
		do_close(peer->fd);
	}
	check_task();
}

void svc_finiconn(int fd)
{
	LOG(vfs_sig_log, LOG_DEBUG, "close %d\n", fd);
	struct conn *curcon = &acon[fd];
	if (curcon->user == NULL)
		return;
	vfs_cs_peer *peer = (vfs_cs_peer *) curcon->user;
	list_del_init(&(peer->alist));
}
