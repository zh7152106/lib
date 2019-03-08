#ifndef _HTTP_CMD_H_
#define _HTTP_CMD_H_

#define TARGET_PORT 80
#define HTTP_MAX_HOST_LEN 256
#define HTTP_MAX_PATH_LEN 1024

typedef struct t_http_sc
{
    int    port;
    int    sockid;
    char   host[HTTP_MAX_HOST_LEN];
    char   path[HTTP_MAX_PATH_LEN];
}http_sc;

int http_get(http_sc* sc);
int http_post(http_sc* sc, char* data);

char* http_recv_msg(http_sc* sc);

http_sc* http_init(char* url);

int http_close(http_sc* sc);
int http_recv_body2file(http_sc* sc, char* file_name, int file_len);
#endif