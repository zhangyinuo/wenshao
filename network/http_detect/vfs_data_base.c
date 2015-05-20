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
#define outdir "../path/outdir"
#define tmpdir "../path/tmpdir"

static int reload_port_url()
{
	char sql[512] = {0x0};
	memset(sql, 0, sizeof(sql));
	snprintf(sql, sizeof(sql), "select url from t_url" );

	if (mysql_query(mysql, sql))
	{
		LOG(vfs_sig_log, LOG_ERROR, "mysql_query error:%s:[%s]", mysql_error(mysql), sql);
		return -1;
	}

	memset(g_url, 0, sizeof(g_url));
	g_url_count = 0;

	MYSQL_ROW row = NULL;
	MYSQL_RES* result = mysql_store_result(mysql);
	if (result)
	{
		while(NULL != (row = mysql_fetch_row(result)))
		{
			if (row[0])
			{
				strcpy(g_url[g_url_count++], row[0]);
			}
		}
		mysql_free_result(result);
	}

	memset(sql, 0, sizeof(sql));
	snprintf(sql, sizeof(sql), "select port from t_port" );

	if (mysql_query(mysql, sql))
	{
		LOG(vfs_sig_log, LOG_ERROR, "mysql_query error:%s:[%s]", mysql_error(mysql), sql);
		return -1;
	}

	memset(g_port, 0, sizeof(g_port));
	g_port_count = 0;

	row = NULL;
	result = mysql_store_result(mysql);
	if (result)
	{
		while(NULL != (row = mysql_fetch_row(result)))
		{
			if (row[0])
			{
				g_port[g_port_count++] = atoi(row[0]);
				LOG(vfs_sig_log, LOG_NORMAL, "port %d %d\n", g_port[g_port_count], g_port_count);
			}
		}
		mysql_free_result(result);
	}
	return 0;
}

static int connect_db(t_db_info *db)
{
    mysql = mysql_init(0);
	if (mysql == NULL)
	{
		LOG(vfs_sig_log, LOG_ERROR, "mysql_init error %m\n");
		return -1;
	}
	mysql_options(mysql, MYSQL_SET_CHARSET_NAME, "gbk");
    if (NULL == mysql_real_connect(mysql, db->host, db->username, db->passwd, db->db, db->port, NULL, 0))
	{
		LOG(vfs_sig_log, LOG_ERROR, "mysql_real_connect err %m\n");
		return -1;
	}
	return reload_port_url();
}

static int init_db()
{
	t_db_info db;
	memset(&db, 0, sizeof(db));
	char *v = myconfig_get_value("db_host");
	if (v == NULL)
	{
		LOG(vfs_sig_log, LOG_ERROR, "no db_host\n");
		return -1;
	}
	snprintf(db.host, sizeof(db.host), "%s", v);

	v = myconfig_get_value("db_username");
	if (v == NULL)
	{
		LOG(vfs_sig_log, LOG_ERROR, "no db_username\n");
		return -1;
	}
	snprintf(db.username, sizeof(db.username), "%s", v);

	v = myconfig_get_value("db_password");
	if (v == NULL)
	{
		LOG(vfs_sig_log, LOG_ERROR, "no db_password\n");
		return -1;
	}
	snprintf(db.passwd, sizeof(db.passwd), "%s", v);

	v = myconfig_get_value("db_db");
	if (v == NULL)
	{
		LOG(vfs_sig_log, LOG_ERROR, "no db_db\n");
		return -1;
	}
	snprintf(db.db, sizeof(db.db), "%s", v);

	db.port = myconfig_get_intval("db_port", 3306);
	return connect_db(&db); 
}

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

static void do_scan(int fd, char *domain, int port, char *ip, char *url)
{
	struct conn *curcon = &acon[fd];
	vfs_cs_peer *peer = (vfs_cs_peer *) curcon->user;

	snprintf(peer->domain, sizeof(peer->domain), "%s", domain);
	snprintf(peer->dstip, sizeof(peer->dstip), "%s", ip);
	peer->port = port;

	char sendbuff[4096] = {0x0};

	int l = create_header(domain, url, sendbuff);
	LOG(vfs_sig_log, LOG_DEBUG, "send %d cmdid %s\n", fd, sendbuff);
	set_client_data(fd, sendbuff, l);
	modify_fd_event(fd, EPOLLOUT);
}

static void gen_scan_task(char *ip, char *domain, int port)
{
	int i = 0;
	for (; i < g_url_count; i++)
	{
		int fd = active_connect(ip, port);
		if (fd > 0)
			do_scan(fd, domain, port, ip, g_url[i]);
	}
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
		gen_scan_task(base->dstip, base->domain, base->port);
		vfs_set_task(task, TASK_HOME);
	}
}

static void remove_space(char *src, char *dst, char *space, int len)
{
	while(*src)
	{
		if (*src == '\n' || *src == '\r')
		{
			memcpy(dst, space, len);
			dst += len;
		}
		else
		{
			*dst = *src;
			dst++;
		}
		src++;
	}
}

static void dump_return_msg(int fd, char *data, size_t len)
{
	static FILE *fp = NULL;
	static time_t last = 0;
	if (last == 0)
		last = time(NULL);

	static char tmpfile[256] = {0x0};

	time_t now = time(NULL);
	if (now - last > 60)
	{
		last = now;
		if (fp)
		{
			fclose(fp);
			fp = NULL;

			char outfile[256] = {0x0};
			snprintf(outfile, sizeof(outfile), "%s/%s", outdir, basename(tmpfile));
			if (rename(tmpfile, outfile))
				LOG(vfs_sig_log, LOG_ERROR, "rename %s to %s error %m\n", tmpfile, outfile);
			else
				LOG(vfs_sig_log, LOG_NORMAL, "rename %s to %s OK\n", tmpfile, outfile);
		}
	}
	if (fd < 0)
		return;
	if (fp == NULL)
	{
		char buft[16] = {0x0};
		memset(tmpfile, 0, sizeof(tmpfile));
		get_strtime(buft);
		snprintf(tmpfile, sizeof(tmpfile), "%s/http_%s_%ld", tmpdir, buft, syscall(SYS_gettid));
		fp = fopen(tmpfile, "w");
		if (fp == NULL)
		{
			LOG(vfs_sig_log, LOG_ERROR, "open %s error %m\n", tmpfile);
			return;
		}
	}

	struct conn *curcon = &acon[fd];
	vfs_cs_peer *peer = (vfs_cs_peer *) curcon->user;

	char *t = strstr(data, "\r\n\r\n");
	if (t)
		*t = 0x0;
	else
		return;

	char dst[409600] = {0x0};
	remove_space(data, dst, "&&", 2);

	fprintf(fp, "%s %s %d [%s]\n", peer->domain, peer->dstip, peer->port, dst);
}
