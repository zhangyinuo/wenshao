#ifndef __CDC_HTTP_DB_H_
#define __CDC_HTTP_DB_H_
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <dirent.h>
#include <string.h>
enum ROLE {ROLE_FIRST =0 , ROLE_HTTP};

typedef struct {
	char domain[16];
	char ip[16];
	int  port;
}t_first_key;

typedef struct {
	char msg[2048];
}t_first_val;

typedef struct {
	char domain[16];
	char ip[16];
	char url[256];
	int  port;
}t_http_key;

typedef struct {
	char msg[2048];
	int  retcode;
}t_http_val;

#ifdef __cplusplus
extern "C"
{
#endif

int init_db();

void close_db();

int get_sel_count(char *sql);

int mydb_begin();

int mydb_commit();

int mydb_get_first(t_first_key *k, t_first_val *v);

int mydb_get_http(t_http_key *k, t_http_val *v);

void merge_db(time_t last);

void process_db_stat(char rec[][256]);

#ifdef __cplusplus
}
#endif
#endif
