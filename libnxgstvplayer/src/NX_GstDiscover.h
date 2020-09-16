#ifndef __GST_DISCOVER_H
#define __GST_DISCOVER_H

#include "NX_GstTypes.h"

#ifdef __cplusplus
extern "C" {
#endif	//	__cplusplus

static struct {
    const char *mimetype;
    CONTAINER_TYPE  type;
    DEMUX_TYPE demux_type;
    const char *demux_name;
} CONTAINER_DESC[] = {
    {"video/quicktime",         CONTAINER_TYPE_QUICKTIME,   DEMUX_TYPE_QTDEMUX,         "qtdemux"},
    {"application/ogg",         CONTAINER_TYPE_OGG,         DEMUX_TYPE_OGGDEMUX,        "oggdemux"},
    {"application/vnd.rn-realmedia", CONTAINER_TYPE_REALMEDIA,   DEMUX_TYPE_RMDEMUX,    "rmdemux"},
    {"video/x-msvideo",         CONTAINER_TYPE_MSVIDEO,     DEMUX_TYPE_AVIDEMUX,        "avidemux"},
    {"video/x-ms-asf",          CONTAINER_TYPE_ASF,         DEMUX_TYPE_ASFDEMUX,        "asfdemux"},
    {"video/x-matroska",        CONTAINER_TYPE_MATROSKA,    DEMUX_TYPE_MATROSKADEMUX,   "matroskademux"},
    {"video/x-flv",             CONTAINER_TYPE_FLV,         DEMUX_TYPE_FLVDEMUX,        "flvdemux"},
    {"video/mpeg",              CONTAINER_TYPE_MPEG,        DEMUX_TYPE_MPEGDEMUX,       "mpegdemux"},
    {"video/mpegts",            CONTAINER_TYPE_MPEGTS,      DEMUX_TYPE_MPEGTSDEMUX,     "mpegtsdemux"},
    {"video/x-dv",              CONTAINER_TYPE_DV,          DEMUX_TYPE_DVDEMUX,         "dvdemux"},
    {"application/x-3gp",       CONTAINER_TYPE_3GP,         DEMUX_TYPE_QTDEMUX ,        "qtdemux"},
    {"application/x-annodex",   CONTAINER_TYPE_ANNODEX,     DEMUX_TYPE_OGGDEMUX,        "oggdemux"},
    {"audio/x-wav",             CONTAINER_TYPE_WAV,         DEMUX_TYPE_WAVPARSE,        "wavparse"},
    //{"application/x-id3",     CONTAINER_TYPE_,            DEMUX_TYPE_,                ""},
    //{"audio/x-flac",          CONTAINER_TYPE_,            DEMUX_TYPE_,                ""},
    //{"audio/x-m4a",           CONTAINER_TYPE_,            DEMUX_TYPE_,                ""},
    //{"audio/mpeg",            CONTAINER_TYPE_,            DEMUX_TYPE_,                ""},
    //{"audio/x-ac3",           CONTAINER_TYPE_,            DEMUX_TYPE_,                ""},
    //{"audio/x-dts",           CONTAINER_TYPE_,            DEMUX_TYPE_,                ""},
    {NULL,                      CONTAINER_TYPE_UNKNOWN,     DEMUX_TYPE_UNKNOWN,         ""},
};

static struct {
    const char *mimetype;
    VIDEO_TYPE type;
} VIDEO_DESC[] = {
    {"video/x-h264",            VIDEO_TYPE_H264},
	{"video/x-h263",            VIDEO_TYPE_H263},
	{"video/mpeg",              VIDEO_TYPE_MPEG_V4},
    {"video/x-h265",            VIDEO_TYPE_H265},
	{"video/x-flash-video",     VIDEO_TYPE_FLV},
	{"video/x-pn-realvideo",    VIDEO_TYPE_RV},
	{"video/x-divx",            VIDEO_TYPE_DIVX},
	{"video/x-ms-asf",          VIDEO_TYPE_ASF},
	{"video/x-wmv",             VIDEO_TYPE_WMV},
	{"video/x-theora",          VIDEO_TYPE_THEORA},
	{"video/x-xvid",            VIDEO_TYPE_XVID},
	{NULL,                      VIDEO_TYPE_UNKNOWN},
};

static struct {
    const char *mimetype;
    AUDIO_TYPE type;
} AUDIO_DESC[] = {
    {"audio/x-raw",             AUDIO_TYPE_RAW},
    {"audio/mpeg",              AUDIO_TYPE_MPEG},
	{"audio/mp3",               AUDIO_TYPE_MP3},
	{"audio/aac",               AUDIO_TYPE_AAC},
	{"audio/x-wma",             AUDIO_TYPE_WMA},
	{"audio/x-vorbis",          AUDIO_TYPE_OGG},
	{"audio/x-ac3",             AUDIO_TYPE_AC3},
	{"audio/x-private1-ac3",    AUDIO_TYPE_AC3_PRI},
	{"audio/x-flac",            AUDIO_TYPE_FLAC},
	{"audio/x-pn-realaudio",    AUDIO_TYPE_RA},
	{"audio/x-dts",             AUDIO_TYPE_DTS},
	{"audio/x-private1-dts",    AUDIO_TYPE_DTS_PRI},
    {"audio/x-wav",             AUDIO_TYPE_WAV},
	{NULL,                      AUDIO_TYPE_UNKNOWN},
};

static struct {
    const char *mimetype;
    SUBTITLE_TYPE type;
} SUBTITLE_DESC[] = {
    {"text/x-raw",              SUBTITLE_TYPE_RAW},
    {"application/x-ssa",       SUBTITLE_TYPE_SSA},
    {"application/x-ass",       SUBTITLE_TYPE_ASS},
    {"application/x-usf",       SUBTITLE_TYPE_USF},
    {"application/x-dvd",       SUBTITLE_TYPE_DVD},
	{"subpicture/x-dvb",        SUBTITLE_TYPE_DVB},
	{NULL,                      SUBTITLE_TYPE_UNKNOWN},
};

enum NX_GST_ERROR StartDiscover(const char* pUri, struct GST_MEDIA_INFO *pInfo);

#ifdef __cplusplus
}
#endif

#undef LOG_TAG

#endif // __GST_DISCOVER_H

