/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

#include "vfs_localfile.h"
#include "vfs_time_stamp.h"
#include "vfs_file_filter.h"
#include "vfs_timer.h"
#include "common.h"
#include "vfs_del_file.h"
#include <sys/vfs.h>
#include <utime.h>
#include <libgen.h>
extern int glogfd;
extern t_g_config g_config;
extern t_ip_info self_ipinfo;

/*
 *本文件有优化余地
 */


static int createdir(char *file)
{
	char *pos = file;
	int len = 1;
	while (1)
	{
		pos = strchr(file + len, '/');
		if (!pos)
			break;
		*pos = 0;
		if (access(file, F_OK) != 0)
		{
			if (mkdir(file, 0755) < 0)
			{
				LOG(glogfd, LOG_ERROR, "mkdir [%s] [%m][%d]!\n", file, errno);
				return -1;
			}
			if (chown(file, g_config.dir_uid, g_config.dir_gid))
				LOG(glogfd, LOG_ERROR, "chown [%s] %d %d [%m][%d]!\n", file, g_config.dir_uid, g_config.dir_gid, errno);
		}

		*pos = '/';
		len = pos - file + 1;
	}
	if (chown(file, g_config.dir_uid, g_config.dir_gid))
		LOG(glogfd, LOG_ERROR, "chown [%s] %d %d [%m][%d]!\n", file, g_config.dir_uid, g_config.dir_gid, errno);
	return 0;
}

static int get_localdir(t_task_base *task, char *outdir)
{
	sprintf(outdir, "%s", task->filename);
	return 0;
}

void real_rm_file(char *file)
{
	int ret = unlink(file);
	if (ret && errno != ENOENT)
	{
		LOG(glogfd, LOG_ERROR, "file [%s] unlink err %m\n", file);
		return ;
	}
	LOG(glogfd, LOG_NORMAL, "file [%s] be unlink\n", file);
}

int delete_localfile_now(t_task_base *task)
{
	char outdir[256] = {0x0};
	if (get_localdir(task, outdir))
		return LOCALFILE_DIR_E;
	if (unlink(outdir))
	{
		LOG(glogfd, LOG_ERROR, "file [%s] unlink err %m\n", outdir);
		return LOCALFILE_UNLINK_E;
	}
	return LOCALFILE_OK;
}

int delete_localfile(t_task_base *task)
{
	char outdir[256] = {0x0};
	if (get_localdir(task, outdir))
		return LOCALFILE_DIR_E;
	char rmfile[256] = {0x0};
	snprintf(rmfile, sizeof(rmfile), "%s.tmp4rm", outdir);
	if (rename(outdir, rmfile))
	{
		LOG(glogfd, LOG_ERROR, "file [%s] rename [%s]err %m\n", outdir, rmfile);
		return LOCALFILE_RENAME_E;
	}
	t_vfs_timer vfs_timer;
	memset(&vfs_timer, 0, sizeof(vfs_timer));
	snprintf(vfs_timer.args, sizeof(vfs_timer.args), "%s", rmfile);
	vfs_timer.span_time = g_config.real_rm_time;
	vfs_timer.cb = real_rm_file;
	if (add_to_delay_task(&vfs_timer))
		LOG(glogfd, LOG_ERROR, "file [%s] rename [%s]add delay task err %m\n", outdir, rmfile);
	return LOCALFILE_OK;
}

int check_localfile_md5(t_task_base *task, int type)
{
	return LOCALFILE_OK;
}

int open_localfile_4_read(t_task_base *task, int *fd)
{
	char outdir[256] = {0x0};
	if (get_localdir(task, outdir))
		return LOCALFILE_DIR_E;
	char *t = strrchr(task->filename, '/');
	if (t == NULL)
		return LOCALFILE_DIR_E;
	t++;
	strcat(outdir, t);
	*fd = open(outdir, O_RDONLY);
	if (*fd < 0)
	{
		LOG(glogfd, LOG_ERROR, "open %s err %m\n", outdir);
		return LOCALFILE_OPEN_E;
	}
	return LOCALFILE_OK;
}

int open_tmp_localfile_4_write(t_task_base *task, int *fd)
{
	char *outdir = task->tmpfile;
	if (access(outdir, F_OK) != 0)
	{
		LOG(glogfd, LOG_DEBUG, "dir %s not exist, try create !\n", outdir);
		if (createdir(outdir))
		{
			LOG(glogfd, LOG_ERROR, "dir %s create %m!\n", outdir);
			return LOCALFILE_DIR_E;
		}
	}
	*fd = open(outdir, O_CREAT | O_RDWR | O_LARGEFILE, 0755);
	if (*fd < 0)
	{
		LOG(glogfd, LOG_ERROR, "open %s err %m\n", outdir);
		return LOCALFILE_OPEN_E;
	}
	return LOCALFILE_OK;
}

int close_tmp_check_mv(t_task_base *task, int fd)
{
	if (fd < 0)
	{
		LOG(glogfd, LOG_ERROR, "fd < 0 %d %s\n", fd, task->filename);
		return LOCALFILE_OPEN_E;
	}
	close(fd);
	if (rename(task->tmpfile, task->filename))
	{
		LOG(glogfd, LOG_ERROR, "rename %s to %s err %m\n", task->tmpfile, task->filename);
		return LOCALFILE_RENAME_E;
	}
	return LOCALFILE_OK;
}

int try_touch_tmp_file(t_task_base *base)
{
	if (access(base->tmpfile, F_OK) != 0)
	{
		LOG(glogfd, LOG_DEBUG, "dir %s not exist, try create !\n", base->tmpfile);
		if (createdir(base->tmpfile))
		{
			LOG(glogfd, LOG_ERROR, "dir %s create %m!\n", base->tmpfile);
			return LOCALFILE_DIR_E;
		}
		int fd = open(base->tmpfile, O_CREAT | O_RDWR | O_LARGEFILE, 0755);
		if (fd < 0)
		{
			LOG(glogfd, LOG_ERROR, "open %s error %m!\n", base->tmpfile);
			return LOCALFILE_OK;
		}
		close(fd);
		return LOCALFILE_DIR_E;
	}
	return LOCALFILE_OK;
}

int check_disk_space(t_task_base *base)
{
	char path[256] = {0x0};
	if (get_localdir(base, path))
	{
		LOG(glogfd, LOG_ERROR, "get local dir err %s\n", base->filename);
		return LOCALFILE_DIR_E;
	}
	if (access(path, F_OK) != 0)
	{
		LOG(glogfd, LOG_DEBUG, "dir %s not exist, try create !\n", path);
		if (createdir(path))
		{
			LOG(glogfd, LOG_ERROR, "dir %s create %m!\n", path);
			return LOCALFILE_DIR_E;
		}
	}
	struct statfs sfs;
	if (statfs(path, &sfs))
	{
		LOG(glogfd, LOG_ERROR, "statfs %s err %m\n", path);
		return DISK_ERR;
	}

	uint64_t diskfree = sfs.f_bfree * sfs.f_bsize;
	if (g_config.mindiskfree > diskfree)
	{
		LOG(glogfd, LOG_ERROR, "statfs %s space not enough %llu:%llu\n", path, diskfree, g_config.mindiskfree);
		return DISK_SPACE_TOO_SMALL;
	}
	return DISK_OK;
}

void localfile_link_task(t_task_base *task)
{
}

int get_localfile_stat(t_task_base *task)
{
	return LOCALFILE_OK;
}

static time_t get_dir_lasttime(char *path)
{
	time_t now = time(NULL) - g_config.reload_time;
	DIR *dp;
	struct dirent *dirp;
	if ((dp = opendir(path)) == NULL) 
	{
		LOG(glogfd, LOG_ERROR, "opendir %s err  %m\n", path);
		return now;
	}
	LOG(glogfd, LOG_TRACE, "opendir %s ok \n", path);
	time_t maxtime = 0;
	while((dirp = readdir(dp)) != NULL) 
	{
		if (dirp->d_name[0] == '.')
			continue;
		char file[256] = {0x0};
		snprintf(file, sizeof(file), "%s/%s", path, dirp->d_name);
		if (check_file_filter(dirp->d_name))
			continue;

		struct stat filestat;
		if(stat(file, &filestat) < 0) 
		{
			LOG(glogfd, LOG_ERROR, "stat error,filename:%s\n", file);
			continue;
		}
		if (filestat.st_ctime >= maxtime)
			maxtime = filestat.st_ctime;
	}
	closedir(dp);
	if (maxtime)
		return maxtime;
	return now;
}

time_t get_fcs_dir_lasttime(int d1, int d2)
{
	char outdir[256] = {0x0};
	char *datadir = myconfig_get_value("vfs_fcs_datadir");
	sprintf(outdir, "%s/%d/%d/", datadir, d1, d2);
	return get_dir_lasttime(outdir);
}

time_t get_cs_dir_lasttime(int d1, int d2, int domain)
{
	char outdir[256] = {0x0};
	char *datadir = myconfig_get_value("vfs_cs_datadir");
	sprintf(outdir, "%s/%d/%d/fcs%d.56.com/flvdownload/", datadir, d1, d2, domain);
	return get_dir_lasttime(outdir);
}

