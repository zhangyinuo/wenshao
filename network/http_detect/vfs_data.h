/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

#ifndef __VFS_SIG_SO_H
#define __VFS_SIG_SO_H
#include "list.h"
#include "global.h"
#include "vfs_init.h"
#include "vfs_task.h"
#include "common.h"
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <dirent.h>

#define SELF_ROLE ROLE_CS

enum SOCK_STAT {LOGOUT = 0, CONNECTED, RECV_HEAD_ING, RECV_HEAD_END, RECV_BODY_ING};

extern const char *sock_stat_cmd[] ;

typedef struct {
	char url[256];
	char dstip[16];
	char domain[128];
	int  port;
	int fd;
	uint32_t hbtime;
	uint32_t sock_stat;
	list_head_t alist;
} t_peer_info;

typedef t_peer_info vfs_cs_peer; 

#endif
