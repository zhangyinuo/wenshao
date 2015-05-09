/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

#include "vfs_init.h"
#include "vfs_so.h"
#include "vfs_file_filter.h"
#include "log.h"
#include "common.h"
#include "thread.h"
#include "vfs_maintain.h"
#include "acl.h"
#include <stddef.h>

volatile int maintain = 0;		//1-维护配置 0 -可以使用
static pthread_rwlock_t init_rwmutex;
static pthread_rwlock_t offline_rwmutex;
extern t_ip_info self_ipinfo;
extern uint8_t self_stat ;
static uint8_t fcslist[MAXFCS] = {0x0};
int init_buff_size = 20480;
static t_cs_dir_info  csinfo[DIR1][DIR2];

char hostname[128];

const char *ispname[MAXISP] = {"tel", "cnc", "edu", "tt", "yd", "hs", "mp4"};

const char *path_dirs[] = {"indir", "outdir", "workdir", "bkdir", "tmpdir", "syncdir"};

list_head_t hothome;
list_head_t offlinehome;
static list_head_t iphome;
/* cfg list */
list_head_t cfg_iplist[256];
/*isp_list*/
list_head_t isp_iplist[256];
t_g_config g_config;

static uint32_t localip[64];

static int init_local_ip()
{
	memset(localip, 0, sizeof(localip));
	struct ifconf ifc;
	struct ifreq *ifr = NULL;
	int i;
	int nifr = 0;

	i = socket(AF_INET, SOCK_STREAM, 0);
	if(i < 0) 
		return -1;
	ifc.ifc_len = 0;
	ifc.ifc_req = NULL;
	if(ioctl(i, SIOCGIFCONF, &ifc) == 0) {
		ifr = alloca(ifc.ifc_len > 128 ? ifc.ifc_len : 128);
		ifc.ifc_req = ifr;
		if(ioctl(i, SIOCGIFCONF, &ifc) == 0)
			nifr = ifc.ifc_len / sizeof(struct ifreq);
	}
	close(i);

	int index = 0;
	for (i = 0; i < nifr; i++)
	{
		if (!strncmp(ifr[i].ifr_name, "lo", 2))
			continue;
		uint32_t ip = ((struct sockaddr_in *)&ifr[i].ifr_addr)->sin_addr.s_addr;
		localip[index%64] = ip;
		index++;
	}
	return 0;
}

void report_err_2_nm (char *file, const char *func, int line, int ret)
{
	char val[256] = {0x0};
	snprintf(val, sizeof(val), "%s:%s:%d  ret=%d err %m", file, func, line, ret);
	LOG(glogfd, LOG_ERROR, "%s", val);
}

int check_self_ip(uint32_t ip)
{
	int i = 0;
	for (i = 0; i < 64; i++)
	{
		if (localip[i] == 0)
			return -1;
		if (localip[i] == ip)
			return 0;
	}
	return -1;
}

void reload_config()
{
	g_config.cs_preday = myconfig_get_intval("cs_preday", 5);
	g_config.fcs_max_connects = myconfig_get_intval("fcs_max_connects", 10);
	g_config.cs_max_connects = myconfig_get_intval("cs_max_connects", 20);
	g_config.cs_max_task_run_once = myconfig_get_intval("cs_max_task_run_once", 32);
	g_config.fcs_max_task = myconfig_get_intval("fcs_max_task", 32);
	g_config.reload_time = myconfig_get_intval("reload_time", 3600);
	g_config.voss_interval = myconfig_get_intval("voss_interval", 300);
	g_config.continue_flag = myconfig_get_intval("continue_flag", 0);
	if (g_config.continue_flag)
		LOG(glogfd, LOG_NORMAL, "vfs run in continue_flag mode !\n");
	else
		LOG(glogfd, LOG_NORMAL, "vfs run in no continue_flag mode !\n");

	g_config.vfs_test = myconfig_get_intval("vfs_test", 0);
	if (g_config.vfs_test)
		LOG(glogfd, LOG_NORMAL, "vfs run in test mode !\n");
	else
		LOG(glogfd, LOG_NORMAL, "vfs run in normal mode !\n");
	g_config.real_rm_time = myconfig_get_intval("real_rm_time", 7200);
	g_config.task_timeout = myconfig_get_intval("task_timeout", 86400);
	g_config.cs_sync_dir = myconfig_get_intval("cs_sync_dir", 0);
	g_config.data_calcu_md5 = myconfig_get_intval("data_calcu_md5", 0);
	g_config.sync_dir_count = myconfig_get_intval("sync_dir_count", 10);
	LOG(glogfd, LOG_NORMAL, "task_timeout = %ld\n", g_config.task_timeout);
}

static int check_storage_dir()
{
	return 0;
	char *p = myconfig_get_value("vfs_src_datadir");
	if (!p)
	{
		LOG(glogfd, LOG_ERROR, "ERR %s:%d no vfs_src_datadir\n", FUNC, LN);
		return -1;
	}
	g_config.src_root_len = strlen(p);

	p = myconfig_get_value("vfs_dst_datadir");
	if (!p)
	{
		LOG(glogfd, LOG_ERROR, "ERR %s:%d no vfs_dst_datadir\n", FUNC, LN);
		return -1;
	}
	return 0;
}

int init_global()
{
	g_config.sig_port = myconfig_get_intval("sig_port", 39090);
	g_config.data_port = myconfig_get_intval("data_port", 49090);
	g_config.timeout = myconfig_get_intval("timeout", 300);
	g_config.cktimeout = myconfig_get_intval("cktimeout", 5);
	g_config.lock_timeout = myconfig_get_intval("lock_timeout", 10);
	g_config.task_splic_count = myconfig_get_intval("task_splic_count", 100);
	g_config.splic_min_size = myconfig_get_intval("splic_min_size", 10240000);
	init_buff_size = myconfig_get_intval("socket_buff", 65536);
	if (init_buff_size < 20480)
		init_buff_size = 20480;

	if (check_storage_dir())
		return -1;

	char *v = myconfig_get_value("default_srcip");
	if (v == NULL)
		v = "127.0.0.1";
	snprintf(g_config.default_srcip, sizeof(g_config.default_srcip), "%s", v);

	g_config.default_srcport = myconfig_get_intval("default_srcport", 80);

	v = myconfig_get_value("callback_ip");
	if (v == NULL)
		v = "127.0.0.1";
	snprintf(g_config.callback_ip, sizeof(g_config.callback_ip), "%s", v);

	g_config.callback_port = myconfig_get_intval("callback_port", 80);

	g_config.retry = myconfig_get_intval("vfs_retry", 0) + 1;

	char *cmdnobody = "cat /etc/passwd |awk -F\\: '{if ($1 == \"nobody\") print $3\" \"$4}'";

	FILE *fp = popen(cmdnobody, "r");
	if (fp == NULL)
	{
		fprintf(stderr, "execute  %s err %m\n", cmdnobody);
		return -1;
	}
	char buf[32] = {0x0};
	fgets(buf, sizeof(buf), fp);
	pclose(fp);
	char *t = strchr(buf, ' ');
	if (t == NULL)
	{
		fprintf(stderr, "execute  %s err %m [%s]\n", cmdnobody, buf);
		return -1;
	}
	g_config.dir_uid = atoi(buf);
	g_config.dir_gid = atoi(t+1);

	reload_config();
	char *v_domain = myconfig_get_value("domain_prefix");
	if (v_domain == NULL)
		snprintf(g_config.domain_prefix, sizeof(g_config.domain_prefix), "fcs");
	else
		snprintf(g_config.domain_prefix, sizeof(g_config.domain_prefix), "%s", v_domain);
	v_domain = myconfig_get_value("domain_suffix");
	if (v_domain == NULL)
		snprintf(g_config.domain_suffix, sizeof(g_config.domain_suffix), "56.com");
	else
		snprintf(g_config.domain_suffix, sizeof(g_config.domain_suffix), "%s", v_domain);

	int i = 0;
	char *mindisk = myconfig_get_value("disk_minfree");
	if (mindisk == NULL)
		g_config.mindiskfree = 4 << 30;
	else
	{
		uint64_t unit_size = 1 << 30;
		char *t = mindisk + strlen(mindisk) - 1;
		if (*t == 'M' || *t == 'm')
			unit_size = 1 << 20;
		else if (*t == 'K' || *t == 'k')
			unit_size = 1 << 10;

		i = atoi(mindisk);
		if (i <= 0)
			g_config.mindiskfree = 4 << 30;
		else
			g_config.mindiskfree = i * unit_size;
	}

	v = myconfig_get_value("docroot");
	if (v == NULL)
		v = "./";
	snprintf(g_config.docroot, sizeof(g_config.docroot), "%s", v);
	LOG(glogfd, LOG_NORMAL, "docroot is %s\n", g_config.docroot);

	v = myconfig_get_value("vfs_sync_starttime");
	if (v == NULL)
		v = "01:00:00";
	snprintf(g_config.sync_stime, sizeof(g_config.sync_stime), "%s", v);
	v = myconfig_get_value("vfs_sync_endtime");
	if (v == NULL)
		v = "09:00:00";
	snprintf(g_config.sync_etime, sizeof(g_config.sync_etime), "%s", v);
	return 0;
}

static int sub_add_cluste_ip(char *name, uint8_t role)
{
	return 0;
	char item[256] = {0x0};
	snprintf(item, sizeof(item), "iplist_%s", name);
	char *v = myconfig_get_value(item);
	if (v == NULL)
	{
		LOG(glogfd, LOG_ERROR, "ERR %s:%d no %s\n", FUNC, LN, item);
		return -1;
	}
	uint32_t uip = str2ip(v);
	if (uip == INADDR_NONE)
	{
		LOG(glogfd, LOG_ERROR, "ERR %s:%d error %s = %s\n", FUNC, LN, item, v);
		return -1;
	}
	t_ip_info ipinfo;
	memset(&ipinfo, 0, sizeof(t_ip_info));
	snprintf(ipinfo.sip, sizeof(ipinfo.sip), "%s", v);
	ipinfo.ip = uip;
	ipinfo.role = role;
	return add_ip_info(&ipinfo);
}

static int add_cluste_ip()
{
	return sub_add_cluste_ip("node", ROLE_CS);
}

int reload_cfg()
{
	struct timespec to;
	to.tv_sec = g_config.lock_timeout + time(NULL);
	to.tv_nsec = 0;
	int ret = pthread_rwlock_timedwrlock(&init_rwmutex, &to);
	if (ret != 0)
	{
		if (ret != EDEADLK)
		{
			LOG(glogfd, LOG_ERROR, "ERR %s:%d pthread_rwlock_timedwrlock error %d\n", FUNC, LN, ret);
			report_err_2_nm(ID, FUNC, LN, ret);
			return -2;
		}
	}
	memset(csinfo, 0, sizeof(csinfo));
	int index = 0;
	for (index = 0; index < 256; index++)
	{
		list_head_t *hashlist = &(cfg_iplist[index&ALLMASK]);
		t_ip_info_list *server = NULL;
		list_head_t *l;
		list_for_each_entry_safe_l(server, l, hashlist, hlist)
		{
			list_del_init(&(server->hlist));
			list_del_init(&(server->hotlist));
			list_del_init(&(server->isplist));
			list_del_init(&(server->archive_list));
			list_add_head(&(server->hlist), &iphome);
		}
	}

	if (add_cluste_ip())
	{
		LOG(glogfd, LOG_ERROR, "ERR %s:%d add_cluste_ip\n", FUNC, LN);
		ret = -1;
	}

	if (pthread_rwlock_unlock(&init_rwmutex))
	{
		LOG(glogfd, LOG_ERROR, "ERR %s:%d pthread_rwlock_unlock error %m\n", FUNC, LN);
		report_err_2_nm(ID, FUNC, LN, ret);
	}
	return ret;
}

int get_isp_by_name(char *name)
{
	int i = 0;
	int nl = strlen(name);
	for (; i < MAXISP; i++)
	{
		int l = strlen(ispname[i]);
		if (l == 0)
			continue;
		if (l != nl)
			continue;
		if (strncmp(name, ispname[i], nl) == 0)
			return i;
	}
	return UNKNOW_ISP;
}

int vfs_init()
{
	memset(hostname, 0, sizeof(hostname));
	if (strlen(hostname) == 0 && gethostname(hostname, sizeof(hostname)))
	{
		LOG(glogfd, LOG_ERROR, "gethostname error %m\n");
		return -1;
	}
	int i = 0;
	for (i = 0; i < 256; i++)
	{
		INIT_LIST_HEAD(&cfg_iplist[i]);
		INIT_LIST_HEAD(&isp_iplist[i]);
	}

	INIT_LIST_HEAD(&iphome);
	INIT_LIST_HEAD(&hothome);
	INIT_LIST_HEAD(&offlinehome);
	t_ip_info_list *ipall = (t_ip_info_list *) malloc (sizeof(t_ip_info_list) * 2048);
	if (ipall == NULL)
	{
		LOG(glogfd, LOG_ERROR, "malloc t_ip_info_list error %m\n");
		return -1;
	}
	memset(ipall, 0, sizeof(t_ip_info_list) * 2048);

	for (i = 0; i < 2048; i++)
	{
		INIT_LIST_HEAD(&(ipall->hlist));
		INIT_LIST_HEAD(&(ipall->hotlist));
		INIT_LIST_HEAD(&(ipall->isplist));
		INIT_LIST_HEAD(&(ipall->archive_list));
		list_add_head(&(ipall->hlist), &iphome);
		ipall++;
	}

	if (init_local_ip())
	{
		LOG(glogfd, LOG_ERROR, "init_local_ip error %m\n");
		return -1;
	}
	if (pthread_rwlock_init(&init_rwmutex, NULL))
	{
		report_err_2_nm(ID, FUNC, LN, 0);
		LOG(glogfd, LOG_ERROR, "pthread_rwlock_init error %m\n");
		return -1;
	}
	if (pthread_rwlock_init(&offline_rwmutex, NULL))
	{
		report_err_2_nm(ID, FUNC, LN, 0);
		LOG(glogfd, LOG_ERROR, "pthread_rwlock_init error %m\n");
		return -1;
	}
	if (reload_cfg())
	{
		LOG(glogfd, LOG_ERROR, "reload_cfg error %m\n");
		return -1;
	}
	return 0;
}

int add_ip_info(t_ip_info *ipinfo)
{
	t_ip_info *ipinfo0;
	if (get_ip_info(&ipinfo0, ipinfo->sip, 0) == 0)
	{
		LOG(glogfd, LOG_ERROR, "exist ip %s\n", ipinfo->sip);
		return 0;
	}
	list_head_t *hashlist = &(cfg_iplist[ipinfo->ip&ALLMASK]);
	t_ip_info_list *server = NULL;
	list_head_t *l;
	int get = 0;
	list_for_each_entry_safe_l(server, l, &iphome, hlist)
	{
		list_del_init(&(server->hlist));
		list_del_init(&(server->hotlist));
		list_del_init(&(server->isplist));
		list_del_init(&(server->archive_list));
		get = 1;
		break;
	}
	if (get == 0)
	{
		server = (t_ip_info_list *)malloc(sizeof(t_ip_info_list));
		LOG(glogfd, LOG_ERROR, "MALLOC\n");
	}
	if (server == NULL)
	{
		LOG(glogfd, LOG_ERROR, "malloc err %m\n");
		return -1;
	}
	INIT_LIST_HEAD(&(server->hlist));
	INIT_LIST_HEAD(&(server->hotlist));
	INIT_LIST_HEAD(&(server->isplist));
	INIT_LIST_HEAD(&(server->archive_list));
	memcpy(&(server->ipinfo), ipinfo, sizeof(t_ip_info));
	list_add_head(&(server->hlist), hashlist);
	return 0;
}

int get_ip_info_by_uint(t_ip_info **ipinfo, uint32_t ip, int type, char *s_ip, char *sip)
{
	if (type)
	{
		if (get_cfg_lock())
			return -2;
	}
	int ret = -1;
	list_head_t *hashlist = &(cfg_iplist[ip&ALLMASK]);
	t_ip_info_list *server = NULL;
	list_head_t *l;
	list_for_each_entry_safe_l(server, l, hashlist, hlist)
	{
		if (ip == server->ipinfo.ip || strcmp(server->ipinfo.sip, sip) == 0 )
		{
			if (type == 0)
				*ipinfo = &(server->ipinfo);
			else
				memcpy(*ipinfo, &(server->ipinfo), sizeof(t_ip_info));
			ret = 0;
			break;
		}
	}
	if (type)
		release_cfg_lock();
	return ret;
}

int get_ip_info(t_ip_info **ipinfo, char *sip, int type)
{
	char s_ip[16] = {0x0};
	uint32_t ip = get_uint32_ip(sip, s_ip);
	return get_ip_info_by_uint(ipinfo, ip, type, s_ip, sip);
}

int get_self_info(t_ip_info *ipinfo0)
{
	int i = 0;
	t_ip_info *ipinfo;
	for (i = 0; i < 64; i++)
	{
		if (localip[i] == 0)
			return -1;
		char ip[16] = {0x0};
		ip2str(ip, localip[i]);
		LOG(glogfd, LOG_DEBUG, "%s:%d %s\n", FUNC, LN, ip);
		if (get_ip_info(&ipinfo, ip, 0) == 0)
		{
			memcpy(ipinfo0, ipinfo, sizeof(t_ip_info));
			return 0;
		}
	}
	return -1;
}

int init_vfs_thread(t_thread_arg *arg)
{
	int iret = 0;
	if((iret = register_thread(arg->name, vfs_signalling_thread, (void *)arg)) < 0)
		return iret;
	LOG(glogfd, LOG_DEBUG, "%s:%s:%d\n", ID, FUNC, LN);
	return 0;
}

void fini_vfs_thread(char *name)
{
	return;
}

int get_cs_info(int dir1, int dir2, t_cs_dir_info *cs)
{
	struct timespec to;
	to.tv_sec = g_config.lock_timeout + time(NULL);
	to.tv_nsec = 0;
	int ret = pthread_rwlock_timedrdlock(&init_rwmutex, &to);
	if (ret != 0)
	{
		if (ret != EDEADLK)
		{
			LOG(glogfd, LOG_ERROR, "ERR %s:%d pthread_rwlock_timedrdlock error %d\n", FUNC, LN, ret);
			report_err_2_nm(ID, FUNC, LN, ret);
			return -1;
		}
	}

	memcpy(cs, &(csinfo[dir1%DIR1][dir2%DIR2]), sizeof(t_cs_dir_info));
	if (pthread_rwlock_unlock(&init_rwmutex))
		LOG(glogfd, LOG_ERROR, "ERR %s:%d pthread_rwlock_unlock error %m\n", FUNC, LN);
	return 0;
}

int get_cs_info_by_path(char *path, t_cs_dir_info *cs)
{
	int dir1, dir2;
	if (get_dir1_dir2(path, &dir1, &dir2))
		return -1;
	return get_cs_info(dir1, dir2, cs);
}

int get_dir1_dir2(char *pathsrc, int *dir1, int *dir2)
{
	char path[256] = {0x0};
	snprintf(path, sizeof(path), "%s", pathsrc);
	char *s = path;
	char *t = strrchr(path, '/');
	if (t == NULL)
	{
		LOG(glogfd, LOG_ERROR, "error path %s\n", pathsrc);
		return -1;
	}
	*t = 0x0;
	t = strrchr(s, '/');
	if (t == NULL)
	{
		LOG(glogfd, LOG_ERROR, "error path %s\n", pathsrc);
		return -1;
	}
	*t = 0x0;
	t++;
	*dir2 = atoi(t)%DIR2;

	t = strrchr(s, '/');
	if (t == NULL)
	{
		LOG(glogfd, LOG_ERROR, "error path %s\n", pathsrc);
		return -1;
	}
	t++;
	*dir1 = atoi(t)%DIR1;
	if (*dir1 < 0 || *dir2 < 0)
	{
		LOG(glogfd, LOG_ERROR, "path %s get %d %d err\n", path, *dir1, *dir2);
		return -1;
	}
	LOG(glogfd, LOG_TRACE, "path %s get %d %d\n", path, *dir1, *dir2);
	return 0;
}

int get_next_fcs(int fcs, uint8_t isp)
{
	int i; 
	for(i = fcs+1; i < MAXFCS; i++)
	{
		if (fcslist[i] == isp)
			return i;
	}
	return -1;
}

int get_fcs_isp(int fcs)
{
	return fcslist[fcs];
}

int get_cfg_lock()
{
	struct timespec to;
	to.tv_sec = g_config.lock_timeout + time(NULL);
	to.tv_nsec = 0;
	int ret = pthread_rwlock_timedrdlock(&init_rwmutex, &to);
	if (ret != 0)
	{
		if (ret != EDEADLK)
		{
			LOG(glogfd, LOG_ERROR, "ERR %s:%d pthread_rwlock_timedrdlock error %d\n", FUNC, LN, ret);
			report_err_2_nm(ID, FUNC, LN, ret);
			return -1;
		}
	}
	return 0;
}

int release_cfg_lock()
{
	if (pthread_rwlock_unlock(&init_rwmutex))
		LOG(glogfd, LOG_ERROR, "ERR %s:%d pthread_rwlock_unlock error %m\n", FUNC, LN);
	return 0;
}

void check_self_stat()
{
	t_ip_info ipinfo;
	t_ip_info *ipinfo0 = &ipinfo;
	while (1)
	{
		int ret = get_ip_info(&ipinfo0, self_ipinfo.sip, 1);
		if (ret == -2)
		{
			sleep(1);
			continue;
		}
	
		if (ret == 0)
		{
			if (self_stat == OFF_LINE)
				self_stat = UNKOWN_STAT;
			return;
		}
		break;
	}
	if (self_stat != OFF_LINE)
	{
		self_stat = OFF_LINE;
		self_offline_time = time(NULL);
	}
}

