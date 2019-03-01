#include <stdio.h>
#include <string.h>
#include "defines.h"
#include "shell_cmd.h"

int shell_cmd_get(char* cmd,char* out)
{
    FILE *fp = NULL;
    char buf[SHELL_MAX_BUF_LEN] = {0};

    fp = popen(cmd, "r");
    if (!fp)
    {
        debug_info("run_shell: popen failed\n");
        return -1;
    }

    if (NULL == fgets(buf, sizeof(buf), fp))
    {
        debug_info("run_shell: fgets failed\n");
        pclose(fp);
        return -1;
    }

    pclose(fp);

	int len = strlen(buf);
	while(len>0 && (buf[len - 1] == '\r' || buf[len - 1] == '\n'))
	{
		buf[len - 1] = '\0';
		len--;
	}

    memcpy(out,buf,len);
    return 0;
}
