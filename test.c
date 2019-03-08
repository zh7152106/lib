#include <stdio.h>
#include "shell_cmd.h"
#include "http_cmd.h"
#include <stdlib.h>


int main(void)
{
	char buf[1024] = {0};
	shell_cmd_get("df -h",buf);
	
	printf("shell cmd get result: %s\n",buf);
	
	http_sc *sc = http_init("http://www.baidu.com/");
#if 1	
	http_get(sc);
	
	char* info = http_recv_msg(sc);
	if(info != NULL)
	{
		printf("recv msg: %s\n",info);
		free(info);
	}
	http_close(sc);
#endif	
	return 0;
}