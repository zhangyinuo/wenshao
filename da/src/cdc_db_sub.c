
static void process_db(char rec[][256], char *sip, int role)
{
}

static int process_line(char *buf, int role)
{
	process_db(rec, role);
	return 0;
}

static int process_stat(char *buf)
{
	char rec[16][256];
	memset(rec, 0, sizeof(rec));
	int n = sscanf(buf, "%[^|]|%[^|]|%[^|]|%[^|]|", rec[0], rec[1], rec[2], rec[3]);
	if (n < 3)
	{
		LOG(cdc_db_log, LOG_ERROR, "err buf:[%s]\n", buf);
		return -1;
	}
	if (rec[2][0] < 'A' || rec[2][0] > 'Z')
		return 0;
	process_db_stat(rec);
	return 0;
}

static int do_bk_file(char *file, char *day, int type)
{
	char bkdir[256] = {0x0};
	snprintf(bkdir, sizeof(bkdir), "%s/%s", cdc_path.bkdir, day);
	if (mkdir(bkdir, 0755) && errno != EEXIST)
	{
		LOG(cdc_db_log, LOG_ERROR, "mkdir %s err %m\n", bkdir);
		return -1;
	}

	char bkfile[256] = {0x0};
	/*
	if (type == ROLE_HOT_CS)
	{
		snprintf(bkfile, sizeof(bkfile), "%s/%s", redisdir, basename(file));
		if(link(file, bkfile))
			LOG(cdc_db_log, LOG_ERROR, "link %s to %s err %m\n", file, bkfile);
		memset(bkfile, 0, sizeof(bkfile));
	}
	*/
	snprintf(bkfile, sizeof(bkfile), "%s/%s", bkdir, basename(file));

	if(rename(file, bkfile))
	{
		LOG(cdc_db_log, LOG_ERROR, "rename %s to %s err %m\n", file, bkfile);
		return -1;
	}
	return 0;
}

static int get_ip_day_from_file (char *file, char *sip, char *day)
{
	char *t = strstr(file, "voss_stat_");
	if (t)
	{
		t += 10;
		snprintf(day, 16, "%.8s", t);
		return 0;
	}
	t = strstr(file, "voss_");
	if (t == NULL)
		return -1;
	t += 5;
	char *e = strchr(t, '_');
	if (e == NULL)
		return -1;
	*e = 0x0;
	strcpy(sip, t); 
	*e = '_';
	e++;
	t = strchr(e, '.');
	if (t == NULL)
		return -1;
	*t = 0x0;
	snprintf(day, 16, "%.8s", e);
	*t = '.';
	return 0;
}

int do_refresh_run_task()
{
	DIR *dp;
	struct dirent *dirp;
	char buff[2048];
	char fullfile[256];

	if ((dp = opendir(cdc_path.indir)) == NULL) 
	{
		LOG(cdc_db_log, LOG_ERROR, "opendir %s error %m!\n", cdc_path.indir);
		return -1;
	}

	FILE *fpin = NULL;
	while((dirp = readdir(dp)) != NULL) {
		if (dirp->d_name[0] == '.')
			continue;
		memset(fullfile, 0, sizeof(fullfile));
		snprintf(fullfile, sizeof(fullfile), "%s/%s", cdc_path.indir, dirp->d_name);
		int role = UNKOWN;
		if (dirp->d_name[0] == 'f')
			role = ROLE_FIRST;
		else if (dirp->d_name[0] == 'h')
			role = ROLE_HTTP;
		else
			continue;

		while (fgets(buff, sizeof(buff), fpin))
		{
			LOG(cdc_db_log, LOG_TRACE, "process line:[%s]\n", buff);
			process_line(buff, role);
			memset(buff, 0, sizeof(buff));
		}
	}
	closedir(dp);
	return 0;
}
