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
#include "GeneralHashFunctions.h"
#include "vfs_localfile.h"
#include "myepoll.h"
#include "protocol.h"
#include "util.h"
#include "acl.h"
#include "list.h"
#include "global.h"
#include "vfs_init.h"
#include "vfs_task.h"
#include "common.h"

int vfs_http_log = -1;
static list_head_t activelist;  //”√¿¥ºÏ≤‚≥¨ ±

extern int topper_queue;
extern int botter_queue;
static __thread int idx_queue = 1;

int svc_init(int fd) 
{
	char *logname = myconfig_get_value("log_server_logname");
	if (!logname)
		logname = "../log/http_log.log";

	char *cloglevel = myconfig_get_value("log_server_loglevel");
	int loglevel = LOG_DEBUG;
	if (cloglevel)
		loglevel = getloglevel(cloglevel);
	int logsize = myconfig_get_intval("log_server_logsize", 100);
	int logintval = myconfig_get_intval("log_server_logtime", 3600);
	int lognum = myconfig_get_intval("log_server_lognum", 10);
	vfs_http_log = registerlog(logname, loglevel, logsize, logintval, lognum);
	if (vfs_http_log < 0)
		return -1;
	INIT_LIST_HEAD(&activelist);
	LOG(vfs_http_log, LOG_DEBUG, "svc_init init log ok!\n");
	return 0;
}

int svc_initconn(int fd) 
{
	LOG(vfs_http_log, LOG_DEBUG, "%s:%s:%d\n", ID, FUNC, LN);
	struct conn *curcon = &acon[fd];
	if (curcon->user == NULL)
		curcon->user = malloc(sizeof(http_peer));
	if (curcon->user == NULL)
	{
		LOG(vfs_http_log, LOG_ERROR, "malloc err %m\n");
		return RET_CLOSE_MALLOC;
	}
	memset(curcon->user, 0, sizeof(http_peer));
	http_peer * peer = (http_peer *)curcon->user;
	peer->hbtime = time(NULL);
	peer->fd = fd;
	INIT_LIST_HEAD(&(peer->alist));
	list_move_tail(&(peer->alist), &activelist);
	LOG(vfs_http_log, LOG_DEBUG, "a new fd[%d] init ok!\n", fd);
	return 0;
}

static char * parse_item(char *src, char *item, char **end)
{
	char *p = strstr(src, item);
	if (p == NULL)
	{
		LOG(vfs_http_log, LOG_ERROR, "data[%s] no [%s]!\n", src, item);
		return NULL;
	}

	p += strlen(item);
	char *e = strstr(p, "\r\n");
	if (e == NULL)
	{
		LOG(vfs_http_log, LOG_ERROR, "data[%s] no [%s] end!\n", src, item);
		return NULL;
	}
	*e = 0x0;

	*end = e + 2;

	return p;
}

static int parse_http(http_peer *peer, char *data, int len)
{
	char *end = NULL;
	char *pret = parse_item(data, "URL: ", &end);
	if (pret == NULL)
		return -1;
	snprintf(peer->base.url, sizeof(peer->base.url), "%s", pret);
	data = end;

	pret = parse_item(data, "dstip: ", &end);
	if (pret == NULL)
		return -1;
	snprintf(peer->base.dstip, sizeof(peer->base.dstip), "%s", pret);
	data = end;

	uint32_t h1, h2, h3;
	get_3_hash(peer->base.url, &h1, &h2, &h3);
	uint32_t idx = h1 & 0x3F;
	snprintf(peer->base.filename, sizeof(peer->base.filename), "%s/%u/%u/%u/%u", g_config.docroot, idx, h1, h2, h3); 
	snprintf(peer->base.tmpfile, sizeof(peer->base.tmpfile), "%s/%u/%u/%u/%u.tmp", g_config.docroot, idx, h1, h2, h3); 
	return 0;
}

static int check_request(int fd, char* data, int len) 
{
	if(len < 14)
		return 1;

	if (len > 1024)
		return -1;

	struct conn *c = &acon[fd];
	http_peer *peer = (http_peer *) c->user;
	if(!strncmp(data, "GET /ED_SPIDER", 14)) {
		char* p;
		if((p = strstr(data, "\r\n\r\n")) != NULL) {
			LOG(vfs_http_log, LOG_DEBUG, "fd[%d] data[%s]!\n", fd, data);
			return parse_http(peer, data, len); 
		}
		else
			return 1;
	}
	else
		return -1;
}

static int push_new_task(http_peer *peer)
{
	if (try_touch_tmp_file(&(peer->base)) == LOCALFILE_OK)
	{
		LOG(vfs_http_log, LOG_TRACE, "fname[%s:%s] do_newtask dup!\n", peer->base.url, peer->base.dstip);
		return -1;
	}
	t_vfs_tasklist *task0 = NULL;
	if (vfs_get_task(&task0, TASK_HOME))
	{
		LOG(vfs_http_log, LOG_ERROR, "fname[%s:%s] do_newtask error!\n", peer->base.url, peer->base.dstip);
		return -1;
	}
	t_vfs_taskinfo *task = &(task0->task);
	memset(&(task->base), 0, sizeof(task->base));
	memcpy(&(task->base), &(peer->base), sizeof(task->base));
	idx_queue++;
	if (idx_queue > topper_queue)
		idx_queue = botter_queue;
	vfs_set_task(task0, idx_queue);
	LOG(vfs_http_log, LOG_NORMAL, "fname[%s:%s:%d] do_newtask ok!\n", peer->base.url, peer->base.dstip, idx_queue);
	return 0;
}

static int handle_request(int cfd) 
{
	char httpheader[256] = {0};
	int fd;
	struct stat st;
	
	struct conn *c = &acon[cfd];
	http_peer *peer = (http_peer *) c->user;
	char *filename = peer->base.filename;
	LOG(vfs_http_log, LOG_DEBUG, "file = %s\n", filename);
	
	fd = open(filename, O_RDONLY);
	if(fd > 0) {
		fstat(fd, &st);
		sprintf(httpheader, "HTTP/1.1 200 OK\r\nContent-Type: video/x-flv\r\nContent-Length: %u\r\n\r\n", (unsigned)st.st_size);
	}
	if(fd > 0)
	{
		set_client_data(cfd, httpheader, strlen(httpheader));
		set_client_fd(cfd, fd, 0, (uint32_t)st.st_size);
		return 0;
	}

	push_new_task(peer);
	return -1;
}

static int check_req(int fd)
{
	char *data;
	size_t datalen;
	if (get_client_data(fd, &data, &datalen))
	{
		LOG(vfs_http_log, LOG_DEBUG, "fd[%d] no data!\n", fd);
		return RECV_ADD_EPOLLIN;  /*no suffic data, need to get data more */
	}
	int clen = check_request(fd, data, datalen);
	if (clen < 0)
	{
		LOG(vfs_http_log, LOG_DEBUG, "fd[%d] data error ,not http!\n", fd);
		return RECV_CLOSE;
	}
	if (clen > 1)
	{
		LOG(vfs_http_log, LOG_DEBUG, "fd[%d] data not suffic!\n", fd);
		return RECV_ADD_EPOLLIN;
	}
	int ret = handle_request(fd);
	consume_client_data(fd, clen);
	if (ret == 0)
		return RECV_SEND;
	else
	{
		struct conn *curcon = &acon[fd];
		http_peer *peer = (http_peer *) curcon->user;
		peer->nostandby = 1;
		peer->hbtime = time(NULL);
		return SEND_SUSPEND;
	}
}

int svc_recv(int fd) 
{
	return check_req(fd);
}

int svc_send(int fd)
{
	struct conn *curcon = &acon[fd];
	http_peer *peer = (http_peer *) curcon->user;
	peer->hbtime = time(NULL);
	return SEND_ADD_EPOLLIN;
}

void svc_timeout()
{
	time_t now = time(NULL);
	http_peer *peer = NULL;
	list_head_t *l;
	list_for_each_entry_safe_l(peer, l, &activelist, alist)
	{
		if (peer == NULL)
			continue;   /*bugs */
		if (peer->nostandby)
		{
			if (handle_request(peer->fd) == 0)
			{
				peer->nostandby = 0;
				modify_fd_event(peer->fd, EPOLLOUT);
			}
			else
			{
				if (now - peer->hbtime > g_config.timeout)
					do_close(peer->fd);
			}
		}
	}
}

void svc_finiconn(int fd)
{
	LOG(vfs_http_log, LOG_DEBUG, "close %d\n", fd);
	struct conn *curcon = &acon[fd];
	if (curcon->user == NULL)
		return;
	http_peer *peer = (http_peer *) curcon->user;
	list_del_init(&(peer->alist));
}
