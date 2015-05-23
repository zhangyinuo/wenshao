
static void process_db(char *rec[8], int role)
{
	LOG(cdc_db_log, LOG_DEBUG, "%s\n", rec[0]);
}

static int process_line(char *buf, int role)
{
	char *rec[8];
	char *s = buf;
	int i = 0;
	char *t = NULL;
	for (; i < 3; i++)
	{
		t = strchr(s, ' ');
		if (t == NULL)
			break;
		*t = 0x0;
		rec[i] = s;
		s = t + 1;
	}
	if (i != 3)
	{
		LOG(cdc_db_log, LOG_ERROR, " i = %d\n", i);
		return -1;
	}

	if (role == ROLE_HTTP)
	{
		t = strchr(s, ' ');
		if (t == NULL)
		{
			LOG(cdc_db_log, LOG_ERROR, " i = %d\n", i);
			return -1;
		}
		*t = 0x0;
		rec[i] = s;
	}
	process_db(rec, role);
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
		int role;
		if (dirp->d_name[0] == 'f')
			role = ROLE_FIRST;
		else if (dirp->d_name[0] == 'h')
			role = ROLE_HTTP;
		else
			continue;

		fpin = fopen(fullfile, "r");
		if (fpin == NULL)
		{
			LOG(cdc_db_log, LOG_ERROR, "process file:[%s] %m\n", fullfile);
			break;
		}
		while (fgets(buff, sizeof(buff), fpin))
		{
			LOG(cdc_db_log, LOG_TRACE, "process line:[%s]\n", buff);
			process_line(buff, role);
			memset(buff, 0, sizeof(buff));
		}
		fclose(fpin);
	}
	closedir(dp);
	return 0;
}
