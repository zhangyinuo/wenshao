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

static int g_get_info_queue_index = 9;
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
	close (fd);
	return 0;
}

static void do_scan(char *domain, int port, char *ip)
{
	t_vfs_tasklist *task = NULL;
	if (vfs_get_task(&task, TASK_HOME) != GET_TASK_OK)
		return ;

	t_task_base *base = &(task->task.base);

	snprintf(base->dstip, sizeof(base->dstip), "%s", ip);
	snprintf(base->domain, sizeof(base->domain), "%s", domain);
	base->port = port;

	vfs_set_task(task, g_get_info_queue_index);
	g_get_info_queue_index++;

	if (g_get_info_queue_index > 16)
		g_get_info_queue_index = 9;
}

static void gen_scan_task(char *ip, char *domain)
{
	int i = 1;

	for (; i < 255; i++)
	{
		char dstip[16] = {0x0};
		snprintf(dstip, sizeof(dstip), "%s.%d", ip, i);

		int j = 0;
		for ( ; j < g_port_count; j++)
		{
			int fd = active_connect(dstip, g_port[j]);
			if (fd == 0)
				do_scan(domain, g_port[j], dstip);
		}
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

	LOG(vfs_sig_log, LOG_NORMAL, "%s %s %d\n", __FILE__, __func__, __LINE__);
		t_task_base *base = (t_task_base *) (&(task->task.base));
		gen_scan_task(base->dstip, base->domain);
		vfs_set_task(task, TASK_HOME);
	}
}
