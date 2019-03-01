#ifndef _DEFINES_H_
#define _DEFINES_H_

#define DEBUG_LEVEL 1

int new_print(int way, const char* fmt, ...);

#define debug_info(fmt,args...) \
            new_print(DEBUG_LEVEL, "%d [control %s:%d INFO] "fmt, DEBUG_LEVEL, __func__, __LINE__, ##args)


#endif