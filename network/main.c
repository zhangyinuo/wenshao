/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/epoll.h>
#include <sys/poll.h>
#include "watchdog.h"
#include "myconfig.h"
#include "daemon.h"
#include "thread.h"
#include "common.h"
#include "fdinfo.h"
#include "version.h"
#include "log.h"
#include "vfs_so.h"
#include "vfs_init.h"
#include "vfs_task.h"
#include "vfs_agent.h"
#include "vfs_time_stamp.h"
#include "vfs_file_filter.h"
#include "vfs_tmp_status.h"
#include "vfs_timer.h"

int topper_queue = 1;
int botter_queue = 1;
extern t_g_config g_config;
t_ip_info self_ipinfo;
time_t vfs_start_time;  /*vfs Æô¶¯Ê±¼ä*/
int glogfd = -1;
char *iprole[] = {"unkown", "uc", "sm", "tracker", "voss_master", "self_ip"}; 

static int init_glog()
{
	char *logname = myconfig_get_value("log_main_logname");
	if (!logname)
		logname = "./main_log.log";

	char *cloglevel = myconfig_get_value("log_main_loglevel");
	int loglevel = LOG_NORMAL;
	if (cloglevel)
		loglevel = getloglevel(cloglevel);
	int logsize = myconfig_get_intval("log_main_logsize", 100);
	int logintval = myconfig_get_intval("log_main_logtime", 3600);
	int lognum = myconfig_get_intval("log_main_lognum", 10);
	glogfd = registerlog(logname, loglevel, logsize, logintval, lognum);
	return glogfd;
}

static void gen_pidfile() {
	mode_t oldmask = umask(0);
	int fd = open("./watch-"_NS_".pid", O_CREAT | O_RDWR | O_TRUNC, 0644);
	if(fd > 0) {
		if(watchdog_pid != 0)	
			dprintf(fd, "%d\n", watchdog_pid);
		else 
			dprintf(fd, "%d\n", mainthread.pid);
		close(fd);
	}
	else {  
		printf("genpidfile fail, %m\n");
	}
	umask(oldmask);
}

static int parse_args(int argc, char* argv[]) {

	if(argc > 1) {
		if(!strncasecmp(argv[1], "-v", 2)) 
		{
			printf("Version:   %s\n", version_string);
			printf("Date   :   %s\n", compiling_date);	
			return -1;
		}
	}
	return 0;	
}
/* main thread loop */
static void main_loop(struct threadstat *thst) 
{
	time_t last = time(NULL);
	while(!stop) {
		sleep(5);
		scan_delay_task();
		thread_reached(thst);

		time_t cur = time(NULL);
		if (cur - last > 3600)
		{
			last = cur;
			do_timeout_task();
		}
	}
}

/* server main */
#define ICALL(x)	if((err=x()) < 0) goto error
int main(int argc, char **argv) {
	if(parse_args(argc, argv))
		return 0;
	int err = 0;
	printf("Starting Server %s (%s)...%ld\n", version_string, compiling_date, nowtime());
	if(myconfig_init(argc, argv) < 0) {
		printf("myconfig_init fail %m\n");
		goto error;
	}
	daemon_start(argc, argv);
	umask(0);
	ICALL(init_log);
	ICALL(init_glog);
	ICALL(init_fdinfo);
	ICALL(delay_task_init);
	ICALL(init_thread);
	ICALL(init_global);
	ICALL(vfs_init);
	ICALL(init_task_info);
	ICALL(init_file_filter);

	t_thread_arg args[MAX_TASK_QUEUE];
	memset(args, 0, sizeof(args));
	int i = 1;
	t_thread_arg *arg = NULL;
	topper_queue = 8;
	for( ; i <= 8; i++)
	{
		arg = &(args[i]);
		arg->queue = i;
		snprintf(arg->name, sizeof(arg->name), "./http_client.so");
		LOG(glogfd, LOG_NORMAL, "prepare start %s\n", arg->name);
		arg->maxevent = myconfig_get_intval("vfs_data_maxevent", 4096);
		if (init_vfs_thread(arg))
			goto error;
	}

	arg = &(args[i]);
	snprintf(arg->name, sizeof(arg->name), "./http_server.so");
	LOG(glogfd, LOG_NORMAL, "prepare start %s\n", arg->name);
	arg->port = g_config.sig_port;
	arg->maxevent = myconfig_get_intval("vfs_data_maxevent", 4096);
	if (init_vfs_thread(arg))
		goto error;

	thread_jumbo_title();
	struct threadstat *thst = get_threadstat();
	if(start_threads() < 0)
		goto out;
	thread_reached(thst);
	gen_pidfile();	
	printf("Server Started\n");
	vfs_start_time = time(NULL);
	main_loop(thst);
out:
	printf("Stopping Server %s (%s)...\n", version_string, compiling_date);
	thread_reached(thst);
	stop_threads();
	myconfig_cleanup();
	fini_fdinfo();
	printf("Server Stopped.\n");
	return restart;
error:
	if(err == -ENOMEM) 
		printf("\n\033[31m\033[1mNO ENOUGH MEMORY\033[0m\n");
	
	printf("\033[31m\033[1mStart Fail.\033[0m\n");
	return -1;
}
