#include "http_cmd.h"
#include "defines.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <netdb.h>

typedef struct t_http_header
{
    int    status;
    int    length;
    char   type[256];
    char   location[1024];
}http_header;

static int get_ip_from_dns(char * host_url, char * ip_addr, int buf_len)
{
    int i;
    int ret = -1;

    int ip_a, ip_b, ip_c, ip_d;

    if (4 == sscanf(host_url, "%d.%d.%d.%d", &ip_a, &ip_b, &ip_c, &ip_d))
    {
        snprintf(ip_addr, buf_len, "%d.%d.%d.%d", ip_a, ip_b, ip_c, ip_d);
        return 0;
    }
    // 注意 类似于 www.baidu.com:8080这样的地址 这里不支持
    struct hostent *host = gethostbyname(host_url);
    if (NULL == host)
    {
        ip_addr = NULL;
        return -1;
    }

    /* 返回第一个地址 */
    for (i = 0; host->h_addr_list[i]; i++)
    {
        strncpy(ip_addr, inet_ntoa(*(struct in_addr*) host->h_addr_list[i]), buf_len);
        ret = 0;
        break;
    }

    return ret;
}

int http_close(http_sc* sc)
{
    if(sc != NULL)
    {
        close(sc->sockid);
        free(sc);
    }

    return 0;
}

http_sc* http_init(char* url)
{
    http_sc* sc = malloc(sizeof(http_sc));
    if(sc == NULL)
    {
        debug_info("malloc fail for http connect");
        return NULL;
    }

    memset(sc,0,sizeof(http_sc));

    char host[HTTP_MAX_PATH_LEN] = {0};
    if(2 != sscanf(url,"http://%[^/]%s",host,sc->path))
    {
        debug_info("get host info error : %s",url);
        free(sc);
        return NULL;
    }

    if(2 != sscanf(host,"%[^:]:%d",sc->host,&sc->port))
    {
        debug_info("not config port ");
        sc->port = TARGET_PORT;
        snprintf(sc->host,sizeof(sc->host),"%s",host);
    }

    char ip_addr[64] = {0};
    if (0 != get_ip_from_dns(sc->host, ip_addr, sizeof(ip_addr)))
    {
        debug_info("get_ip_from_dns failed \n");
        free(sc);
        return NULL;
    }

    sc->sockid = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sc->sockid < 0)
    {
        debug_info("invalid socket : %d\n", sc->sockid);
        free(sc);
        return NULL;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = inet_addr(ip_addr);
    addr.sin_port        = htons(sc->port);

    /* 设置接收超时时间，避免影响control其他模块使用 */
    struct timeval tv;
    tv.tv_sec = 10;
    tv.tv_usec = 0;
    if(0 != setsockopt(sc->sockid, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(struct timeval)))
    {
        debug_info("setsockopt set recvtime  fail\n");
        http_close(sc);
        return NULL;
    }

    int res = 0;
    res = connect(sc->sockid, (struct sockaddr *) &addr, sizeof(addr));
    if (-1 == res)
    {
        debug_info("connect failed:%d ip:%s, port: %d\n", res, ip_addr, sc->port);
        http_close(sc);
        return NULL;
    }
    return sc;
}

int http_get(http_sc* sc)
{
    char send_buf[1024] = {0};
    snprintf(send_buf,sizeof(send_buf),\
        "GET %s HTTP/1.1\r\n"\
        "User-Agent: Mozilla/4.0 (compatible; MSIE 5.00; Windows 98)\r\n"\
        "Accept: */*\r\n"\
        "Host: %s:%d\r\n"\
        "\r\n",sc->path,sc->host,sc->port);

    debug_info("msg info : %s",send_buf);
    int res = send(sc->sockid, send_buf, strlen(send_buf), 0);
    if (res != (int)strlen(send_buf))
    {
        debug_info("send failed : %d\n", res);
        http_close(sc);
        return -1;
    }

    return 0;
}

/* */
int http_post(http_sc* sc, char* data)
{
    char send_buf[1024] = {0};
    snprintf(send_buf,sizeof(send_buf),\
        "POST %s HTTP/1.1\r\n"\
        "User-Agent: Mozilla/4.0 (compatible; MSIE 5.00; Windows 98)\r\n"\
        "Accept: */*\r\n"\
        "Content-Type: application/json\r\n"\
        "Content-length:%d\r\n"\
        "Host:%s:%d\r\n"\
        "\r\n"\
        "%s",sc->path,(int)strlen(data),sc->host,sc->port,data);

    int res = send(sc->sockid, send_buf, strlen(send_buf), 0);
    if (res != (int)strlen(send_buf))
    {
        debug_info("send failed : %d\n", res);
        http_close(sc);
        return -1;
    }

    return 0;
}

static void get_resp_header(const char *response, int *status_code, char*content_type, long* content_length,char* location)
{
    char *pos = (char*)strstr(response, "HTTP/");
    if (NULL != pos)
    {
        sscanf(pos, "%*s %d", status_code);
    }

    pos = (char*)strstr(response, "Content-Type:");
    if (NULL != pos)
    {
        sscanf(pos, "%*s %s", content_type);
    }

    pos = (char*)strstr(response, "Content-Length:");
    if (NULL != pos)
    {
        sscanf(pos, "%*s %ld",content_length);
    }

    pos = (char*)strstr(response, "Location:");
    if (NULL != pos)
    {
        sscanf(pos, "%*s %s",location);
    }
}

/* 每个字节的读取并判断 */
int http_recv_header(http_sc* sc, http_header* header)
{
    int index = 0;
    int is_ok = 0;
    char response;
    char recvbuf[1024] = {0};

    /* recv http response header */
    while (0 != recv(sc->sockid, &response, sizeof(response), 0))
    {
       recvbuf[index++] = response;
       if (response == '\r')
       {
           if (0 != recv(sc->sockid, &response, sizeof(response), 0))
           {
               recvbuf[index++] = response;
               if (response == '\n')
               {
                   if (0 != recv(sc->sockid, &response, sizeof(response), 0))
                   {
                       recvbuf[index++] = response;
                       if (response == '\r')
                       {
                           if (0 != recv(sc->sockid, &response, sizeof(response), 0))
                           {
                               recvbuf[index++] = response;
                               if (response == '\n')
                               {
                                   is_ok = 1;
                                   debug_info("success get head \n");
                                   break;
                               }
                           }
                       }
                   }
               }
           }
       }
    }

    if(is_ok == 1)
    {
       get_resp_header(recvbuf, &header->status, header->type, (long*)&header->length,header->location);
       debug_info("buf len : %d,http_ status:%d, type:%s, length:%d \n", strlen(recvbuf),header->status, header->type, header->length);
       return 0;
    }

    return -1;
}

/* 接收http应答并将内容存入申请的内存中 */
char* http_recv_msg(http_sc* sc)
{
    http_header header;
    memset(&header,0,sizeof(header));

    http_recv_header( sc, &header);
    char* info = NULL;

    if(header.status == 200)
    {
       info = calloc(header.length,1);
       if(info != NULL)
       {
         recv(sc->sockid, info, header.length, 0);
       }
    }

    return info;
}

/* 保存内容到文件中 */
int http_recv_body2file(http_sc* sc, char* file_name, int file_len)
{
    int ret = -1;
    debug_info("start write file to local disk \n");
    FILE * fp = fopen(file_name, "wb");
    if (NULL == fp)
    {
        debug_info("fopen failed \n");
        return -1;
    }

    int len = 0;
    int write_len = 0;
    int pr_count = 0;
    unsigned char buf[1024] = {0};
    while (0 != (len = recv(sc->sockid, buf, sizeof(buf), 0)))
    {
        if (len != (int)fwrite(buf, 1, len, fp))
        {
            debug_info("fwrite failed \n");
            break;
        }
        write_len += len;
        if (write_len == file_len)
        {
            debug_info("write end total len %d \n", write_len);
            ret = 0;
            break;
        }
        if (1000 == pr_count++)
        {
            pr_count = 0;
            debug_info("http download %d bytes \n", write_len);
        }
    }
    fclose(fp);

    debug_info("revc body2file\n");
    return ret;
}

int http_down_version(char * url, char * file_name)
{
    int ret = -1;
    http_sc *sc = http_init(url);
    if(sc == NULL)
    {
        return ret;
    }
    if(http_get(sc) < 0)
    {
        http_close(sc);
        return ret;
    }

    http_header header;
    memset(&header,0,sizeof(header));
    http_recv_header(sc, &header);

    debug_info("recv header status code : %d,location:%s\n",header.status,header.location);
    if(header.status == 200)
    {
        http_recv_body2file(sc,file_name, header.length);
    }
    else if(header.status == 302)
    {
        /* redirect to other url */
        http_close(sc);
        return http_down_version(header.location, file_name);
    }

    http_close(sc);
    return ret;
}