#include <stdio.h>
#include "shell_cmd.h"

int main(void)
{
	char buf[1024] = {0};
	shell_cmd_get("df -h",buf);
	
	printf("shell cmd get result: %s\n",buf);
	return 0;
}