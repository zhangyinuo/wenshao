/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/
#include "mysql.h"
#include "mysqld_error.h"
static MYSQL  mysql0;
static MYSQL * mysql = &mysql0;
#define max 0x100

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

static void do_dispatcher()
{
	char sql[512] = {0x0};
	snprintf(sql, sizeof(sql), "select root_domain from t_target" );

	if (mysql_query(mysql, sql))
	{
		LOG(vfs_http_log, LOG_ERROR, "mysql_query error:%s:[%s]", mysql_error(mysql), sql);
		return -1;
	}


	MYSQL_ROW row = NULL;
	MYSQL_RES* result = mysql_store_result(mysql);
	if (result)
	{
		while(NULL != (row = mysql_fetch_row(result)))
		{
			if (row[0])
			{
				add_trust_ip(str2ip(row[0]));
			}
		}
		mysql_free_result(result);
	}
}

static void do_reclaim()
{
}
