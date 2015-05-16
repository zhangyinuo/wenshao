/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

/*
 *base文件，查询基础配置信息，设置相关基础状态及其它```
 *Tracker 数目较少，放在一个静态数组
 *CS和FCS数目较多，放在hash链表
 *CS FCS ip信息采用uint32_t 存储，便于存储和查找
 */
volatile extern int maintain ;		//1-维护配置 0 -可以使用
extern t_vfs_up_proxy g_proxy;

static int active_connect(char *ip, int port)
{
	int fd = createsocket(ip, port);
	if (fd < 0)
	{
		LOG(vfs_sig_log, LOG_ERROR, "connect %s:%d err %m\n", ip, port);
		return -1;
	}
	if (svc_initconn(fd))
	{
		LOG(vfs_sig_log, LOG_ERROR, "svc_initconn err %m\n");
		close(fd);
		return -1;
	}
	add_fd_2_efd(fd);
	LOG(vfs_sig_log, LOG_NORMAL, "fd [%d] connect %s:%d\n", fd, ip, port);
	return fd;
}

static int create_header(char *domain, char *url, char *httpheader)
{
	int l = sprintf(httpheader, "GET /%s HTTP/1.1\r\n", url);
	l += sprintf(httpheader + l, "Host: %s\r\nUser-Agent: HTTPCLIENT\r\nConnection: Close\r\n\r\n", domain);
	return l;
}

static void check_task()
{
	t_vfs_tasklist *task = NULL;
	int ret = 0;
	int once = 0;
	while (1)
	{
		if (once >= g_config.cs_max_task_run_once)
		{
			LOG(vfs_sig_log, LOG_DEBUG, "too many task in cs %d %d\n", once, g_config.cs_max_task_run_once);
			break;
		}
		ret = vfs_get_task(&task, g_queue);
		if (ret != GET_TASK_OK)
		{
			LOG(vfs_sig_log, LOG_TRACE, "vfs_get_task get notihng %d\n", ret);
			break;
		}
		once++;

		t_task_base *base = (t_task_base *) (&(task->task.base));
		char *t = strchr(base->url, '/');
		if (t == NULL)
		{
			LOG(vfs_sig_log, LOG_ERROR, "error url format %s\n", base->url);
			vfs_set_task(task, TASK_HOME);
			continue;
		}

		*t = 0x0;
		int fd = active_connect(base->dstip, 80);
		if (fd < 0)
		{
			LOG(vfs_sig_log, LOG_ERROR, "active_connect %s:80 error %m\n", base->dstip);
			vfs_set_task(task, TASK_HOME);
			continue;
		}

		vfs_set_task(task, TASK_HOME);
		char httpheader[1024] = {0x0};
		create_header(base->url, t + 1, httpheader);
		active_send(fd, httpheader);
		*t = '/';

		struct conn *curcon = &acon[fd];
		vfs_cs_peer *peer = (vfs_cs_peer *) curcon->user;
		memcpy(&(peer->base), base, sizeof(peer->base));
	}
}

