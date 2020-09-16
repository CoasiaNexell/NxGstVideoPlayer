
#ifndef __NX_GSTLOG_H
#define __NX_GSTLOG_H

#include <stdarg.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif	//	__cplusplus

#ifndef LOG_TAG
#define LOG_TAG "[NxGstVPLAYER]"
#endif

#define	VBS_MSG			0
#define	DBG_FUNCTION		0

void nx_gst_info(const char *format, ...);
void nx_gst_warn(const char *format, ...);
void nx_gst_error(const char *format, ...);
void nx_gst_debug(const char *format, ...);

#define NXGLOGI(fmt, arg...) do { \
        nx_gst_info(LOG_TAG"/I %s() " fmt, __FUNCTION__, ## arg);   \
} while (0)

#define NXGLOGW(fmt, arg...) do { \
        nx_gst_warn(LOG_TAG"/W %s() " fmt, __FUNCTION__, ## arg);   \
} while (0)

#define NXGLOGE(fmt, arg...) do { \
        nx_gst_error(LOG_TAG"/E %s() " fmt, __FUNCTION__, ## arg);   \
} while (0)

#ifdef DEBUG

#if	DBG_FUNCTION
#define	FUNC_IN()			g_print("%s %s() In\n", LOG_TAG, __FUNCTION__)
#define	FUNC_OUT()			g_print("%s %s() Out\n", LOG_TAG, __FUNCTION__)
#else
#define	FUNC_IN()			do{}while(0)
#define	FUNC_OUT()			do{}while(0)
#endif	//	DBG_FUNCTION

#define NXGLOGD(fmt, arg...) do { \
        nx_gst_debug(LOG_TAG"/D %s() " fmt, __FUNCTION__, ## arg);   \
} while (0)

#if VBS_MSG
#define NXGLOGV(fmt, arg...) do { \
        nx_gst_debug(LOG_TAG"/V %s() " fmt, __FUNCTION__, ## arg);   \
} while (0)
#else
#define NXGLOGV(fmt, arg...)    do{}while(0)
#endif

#else   // DEBUG

#define	FUNC_IN()			do{}while(0)
#define	FUNC_OUT()			do{}while(0)

#define NXGLOGD(fmt, arg...)            do{}while(0)
#define NXGLOGV(fmt, arg...)            do{}while(0)

#endif   // DEBUG

#ifdef __cplusplus
}
#endif

#undef LOG_TAG

#endif // __NX_GSTLOG_H





