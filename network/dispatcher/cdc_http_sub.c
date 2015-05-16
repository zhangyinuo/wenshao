/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/
#include "mysql.h"
#include "mysqld_error.h"
static MYSQL  mysql0;
static MYSQL * mysql = &mysql0;
#define MAX_TARGET 0x100
#define MAX_SUB 0x100
#define MAX_LEN_TARGET 0x40
#define MAX_LEN_SUB 0x10

static char g_target[MAX_TARGET][MAX_LEN_TARGET];
static char g_sub_domain[MAX_SUB][MAX_LEN_SUB];

static int g_target_count;
static int g_sub_count;

/* for http port scan url */

#define MAX_URL 0x1000
#define MAX_LEN_URL 0x100
static char g_url[MAX_URL][MAX_LEN_URL];
static int g_url_count;

static int connect_db(t_db_info *db)
{
    mysql_init(mysql);
	mysql_options(mysql, MYSQL_SET_CHARSET_NAME, "gbk");
    if (NULL == mysql_real_connect(mysql, db->host, db->username, db->passwd, db->db, db->port, NULL, 0))
	{
		LOG(vfs_http_log, LOG_ERROR, "mysql_real_connect err %m\n");
		return -1;
	}
	return 0;
}

static int init_db()
{
	t_db_info db;
	memset(&db, 0, sizeof(db));
	char *v = myconfig_get_value("db_host");
	if (v == NULL)
	{
		LOG(vfs_http_log, LOG_ERROR, "no db_host\n");
		return -1;
	}
	snprintf(db.host, sizeof(db.host), "%s", v);

	v = myconfig_get_value("db_username");
	if (v == NULL)
	{
		LOG(vfs_http_log, LOG_ERROR, "no db_username\n");
		return -1;
	}
	snprintf(db.username, sizeof(db.username), "%s", v);

	v = myconfig_get_value("db_password");
	if (v == NULL)
	{
		LOG(vfs_http_log, LOG_ERROR, "no db_password\n");
		return -1;
	}
	snprintf(db.passwd, sizeof(db.passwd), "%s", v);

	v = myconfig_get_value("db_db");
	if (v == NULL)
	{
		LOG(vfs_http_log, LOG_ERROR, "no db_db\n");
		return -1;
	}
	snprintf(db.db, sizeof(db.db), "%s", v);

	db.port = myconfig_get_intval("db_port", 3306);
	return connect_db(&db); 
}

static void get_ip_range()
{
	int i_target = 0;
	int i_domain = 0;

	for (; i_target < g_target_count; i_target++)
	{
		i_domain = 0;
		for (; i_domain < g_sub_count; i_domain++)
		{
			char realname[256] = {0x0};
			snprintf(realname, sizeof(realname), "%s.%s", g_sub_domain[i_domain], g_target[i_target]);

			char ip[16][16];
			memset(ip, 0, sizeof(ip));
			int i = get_multi_ip_by_domain(ip, realname);

			if (i > 0)
			{
			}
		}
	}
}

static void do_dispatcher()
{
	char sql[512] = {0x0};
	snprintf(sql, sizeof(sql), "select target from t_target" );

	if (mysql_query(mysql, sql))
	{
		LOG(vfs_http_log, LOG_ERROR, "mysql_query error:%s:[%s]", mysql_error(mysql), sql);
		return;
	}

	memset(g_target, 0, sizeof(g_target));
	g_target_count = 0;

	MYSQL_ROW row = NULL;
	MYSQL_RES* result = mysql_store_result(mysql);
	if (result)
	{
		while(NULL != (row = mysql_fetch_row(result)))
		{
			if (row[0])
			{
				strcpy(g_target[g_target_count++], row[0]);
			}
		}
		mysql_free_result(result);
	}

	memset(sql, 0, sizeof(sql));
	snprintf(sql, sizeof(sql), "select sub from t_sub" );

	if (mysql_query(mysql, sql))
	{
		LOG(vfs_http_log, LOG_ERROR, "mysql_query error:%s:[%s]", mysql_error(mysql), sql);
		return;
	}

	memset(g_sub_domain, 0, sizeof(g_sub_domain));
	g_sub_count = 0;

	row = NULL;
	result = mysql_store_result(mysql);
	if (result)
	{
		while(NULL != (row = mysql_fetch_row(result)))
		{
			if (row[0])
			{
				strcpy(g_sub_domain[g_sub_count++], row[0]);
			}
		}
		mysql_free_result(result);
	}

	memset(sql, 0, sizeof(sql));
	snprintf(sql, sizeof(sql), "select url from t_url" );

	if (mysql_query(mysql, sql))
	{
		LOG(vfs_http_log, LOG_ERROR, "mysql_query error:%s:[%s]", mysql_error(mysql), sql);
		return;
	}

	memset(g_url, 0, sizeof(g_url));
	g_url_count = 0;

	row = NULL;
	result = mysql_store_result(mysql);
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

	get_ip_range();
}

static void do_reclaim()
{
}

