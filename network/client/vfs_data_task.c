/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

#include "vfs_data_task.h"
#include "vfs_data.h"
#include "vfs_task.h"
#include "common.h"
#include "global.h"
#include "vfs_so.h"
#include "myepoll.h"
#include "protocol.h"
#include "util.h"
#include "acl.h"


void dump_task_info (char *from, int line, t_task_base *task)
{
}

int do_prepare_recvfile(int fd, off_t fsize)
{
	struct conn *curcon = &acon[fd];
	vfs_cs_peer *peer = (vfs_cs_peer *) curcon->user;
	if (peer->sock_stat != RECV_HEAD_END)
	{
		LOG(vfs_sig_log, LOG_ERROR, "fd[%d] status not recv [%x]\n", fd, peer->sock_stat);
		return RECV_CLOSE;
	}
	t_task_base *base = &(peer->base);
	base->fsize = fsize;
	base->lastlen = 0;

	if (peer->local_in_fd > 0)
		close(peer->local_in_fd);
	if (open_tmp_localfile_4_write(base, &(peer->local_in_fd)) != LOCALFILE_OK) 
	{
		LOG(vfs_sig_log, LOG_ERROR, "fd[%d] open_tmp_localfile_4_write error %s\n", fd, base->filename);
		return RECV_CLOSE;
	}
	else
		peer->sock_stat = RECV_BODY_ING;
	return RECV_ADD_EPOLLIN;
}

