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
#include "vfs_data_task.h"
#include "vfs_localfile.h"

static __thread int vfs_sig_log = -1;
extern uint8_t self_stat ;
extern t_ip_info self_ipinfo;
/* online list */
static __thread list_head_t activelist;  //用来检测超时
static __thread list_head_t online_list[256]; //用来快速定位查找

int g_proxyed = 0;
static __thread int g_queue = 1;
t_vfs_up_proxy g_proxy;
int svc_initconn(int fd); 
int active_send(int fd, char *data);
const char *sock_stat_cmd[] = {"LOGOUT", "CONNECTED", "LOGIN", "IDLE", "PREPARE_RECVFILE", "RECVFILEING", "SENDFILEING", "LAST_STAT"};

#include "vfs_data_sub.c"
#include "vfs_data_base.c"
#include "vfs_data_task.c"

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
	if (g_proxyed)
		LOG(vfs_sig_log, LOG_NORMAL, "proxy mode!\n");
	else
		LOG(vfs_sig_log, LOG_NORMAL, "not proxy mode!\n");
	
	g_queue = queue;
	LOG(vfs_sig_log, LOG_NORMAL, "queue = %d !\n", g_queue);

	return init_stock();
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

/*校验是否有一个完整请求*/
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
	char *end = strstr(data, "\r\n\r\n");
	if (end == NULL)
		return -1;
	end += 4;

	char *pret = strstr(data, "HTTP/");
	if (pret == NULL)
	{
		LOG(vfs_sig_log, LOG_ERROR, "%s:%d fd[%d] ERROR no HTTP/!\n", FUNC, LN, fd);
		return RECV_CLOSE;
	}

	pret = strchr(pret, ' ');
	if (pret == NULL)
	{
		LOG(vfs_sig_log, LOG_ERROR, "%s:%d fd[%d] ERROR no http blank!\n", FUNC, LN, fd);
		return RECV_CLOSE;
	}

	int retcode = atoi(pret + 1);
	if (retcode != 200 && retcode != 206)
	{
		LOG(vfs_sig_log, LOG_ERROR, "%s:%d fd[%d] ERROR retcode = %d!\n", FUNC, LN, fd, retcode);
		return RECV_CLOSE;
	}

	off_t fsize = 1024000;
	int clen = end - data;
	char *pleng = strstr(data, "Content-Length: ");
	if (pleng == NULL)
		LOG(vfs_sig_log, LOG_ERROR, "%s:%d fd[%d] ERROR Content-Length: !\n", FUNC, LN, fd);
	else
	{
		fsize = atol(pleng + 16);
		LOG(vfs_sig_log, LOG_DEBUG, "%s:%d fd[%d] Content-Length: %ld!\n", FUNC, LN, fd, fsize);
		if (fsize > 1024000)
		{
			LOG(vfs_sig_log, LOG_ERROR, "%s:%d fd[%d] Content-Length: %ld too long!\n", FUNC, LN, fd, fsize);
			return RECV_CLOSE;
		}
	}
	struct conn *curcon = &acon[fd];
	vfs_cs_peer *peer = (vfs_cs_peer *) curcon->user;
	peer->sock_stat = RECV_HEAD_END;
	/*
	 * write all rsp to file include http header and http body
	 */
	return do_prepare_recvfile(fd, fsize + clen);
}

int svc_recv(int fd) 
{
	struct conn *curcon = &acon[fd];
	vfs_cs_peer *peer = (vfs_cs_peer *) curcon->user;
recvfileing:
	peer->hbtime = time(NULL);
	list_move_tail(&(peer->alist), &activelist);
	LOG(vfs_sig_log, LOG_TRACE, "fd[%d] sock stat %d!\n", fd, peer->sock_stat);
	if (peer->sock_stat == RECV_BODY_ING)
	{
		char *data;
		size_t datalen;
		if (get_client_data(fd, &data, &datalen))
			return RECV_ADD_EPOLLIN;
		t_task_base *base = &(peer->base);
		int remainlen = base->fsize - base->lastlen;
		datalen = datalen <= remainlen ? datalen : remainlen ; 
		int n = write(peer->local_in_fd, data, datalen);
		if (n < 0)
		{
			LOG(vfs_sig_log, LOG_ERROR, "fd[%d] write error %m close it!\n", fd);
			return RECV_CLOSE;  /* ERROR , close it */
		}
		consume_client_data(fd, n);
		base->lastlen += n;
		if (base->lastlen >= base->fsize)
		{
			close_tmp_check_mv(base, peer->local_in_fd);
			LOG(vfs_sig_log, LOG_NORMAL, "fd[%d] recv ok %s\n", fd, base->filename);
			peer->local_in_fd = -1;
			return RECV_CLOSE;
		}
		return RECV_ADD_EPOLLIN;
	}
	
	int ret = RECV_ADD_EPOLLIN;;
	int subret = 0;
	while (1)
	{
		subret = check_req(fd);
		if (subret == -1)
			break;
		if (subret == RECV_CLOSE)
			return RECV_CLOSE;
		if (peer->sock_stat == RECV_BODY_ING)
			goto recvfileing;
	}
	return ret;
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
	if (peer->local_in_fd <= 0)
		return;

	t_task_base *base = &(peer->base);
	close_tmp_check_mv(base, peer->local_in_fd);
	LOG(vfs_sig_log, LOG_NORMAL, "fd[%d] recv error %s\n", fd, base->filename);
	peer->local_in_fd = -1;
}
