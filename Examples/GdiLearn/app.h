#ifndef __APP_H__
#define __APP_H__

#if defined(APP_EXPORT)
#   define __api__  __declspec(dllexport)
#else
#   define __api__  __declspec(dllimport)
#endif

#define HOTDYLIB_API __api__
#include <HotDylib.h>

#include <stdio.h>
#include <stdarg.h>

__api__ void app_error(const char* fmt, ...);
__api__ void app_msgbox(const char* title, const char* message, int mode);

__api__ void app_loginfo(const char* fmt, ...);
__api__ void app_logerror(const char* fmt, ...);

__api__ int  app_isquit(void);
__api__ void app_usleep(long us);
__api__ void app_update(HotDylib* lib);
__api__ void app_create_window(void* hInstance);

#endif
