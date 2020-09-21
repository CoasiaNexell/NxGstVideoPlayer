//------------------------------------------------------------------------------
//
//	Copyright (C) 2015 Nexell Co. All Rights Reserved
//	Nexell Co. Proprietary & Confidential
//
//	NEXELL INFORMS THAT THIS CODE AND INFORMATION IS PROVIDED "AS IS" BASE
//  AND	WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING
//  BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS
//  FOR A PARTICULAR PURPOSE.
//
//	Module		: libnxgstvplayer.so
//	File		:
//	Description	:
//	Author		:
//	Export		:
//	History		:
//
//------------------------------------------------------------------------------

#ifndef __NX_GSTTYPES_H
#define __NX_GSTTYPES_H

#include <glib.h>

#define MAX_TRACK_NUM		10

typedef struct MOVIE_TYPE	*MP_HANDLE;

/*! \enum NX_GST_EVENT
 * \brief Describes the event types */
enum NX_GST_EVENT {
    /*! \brief EOS event */
    MP_EVENT_EOS,
    /*! \brief Demux error */
    MP_EVENT_DEMUX_LINK_FAILED,
    /*! \brief Not supported contents */
    MP_EVENT_NOT_SUPPORTED,
    /*! \brief General error from GStreamer */
    MP_EVENT_GST_ERROR,
    /*! \brief Audio device open error */
    MP_EVENT_ERR_OPEN_AUDIO_DEVICE,
    /*! \brief State is changed */
    MP_EVENT_STATE_CHANGED,
    /*! \brief Subtitle is updated */
    MP_EVENT_SUBTITLE_UPDATED,
    /*! \brief Unknown error   */
    MP_EVENT_UNKNOWN
};

/*! \enum NX_GST_RET
 * \brief Describes the return result */
typedef enum {
    /*! \brief On failure */
    NX_GST_RET_ERROR,
    /*! \brief On Success */
    NX_GST_RET_OK
} NX_GST_RET;

enum NX_GST_ERROR
{
    NX_GST_ERROR_NONE,
    NX_GST_ERROR_DISCOVER_FAILED,
    NX_GST_ERROR_NOT_SUPPORTED_CONTENTS,
    NX_GST_ERROR_DEMUX_LINK_FAILED,
    NX_GST_ERROR_NUM_ERRORS
};

/*! \enum NX_URI_TYPE
 * \brief Describes the URI types */
typedef enum
{
    /*! \brief File type */
    URI_TYPE_FILE,
    /*! \brief URL type */
    URI_TYPE_URL
} NX_URI_TYPE;

/*! \enum NX_MEDIA_STATE
 * \brief Describes the media states */
enum NX_MEDIA_STATE
{
    /*! \brief No pending state */
    MP_STATE_VOID_PENDING	= 0,
    /*! \brief Stopped state or initial state */
    MP_STATE_STOPPED		= 1,
    /*! \brief Ready to go to PAUSED state */
    MP_STATE_READY			= 2,
    /*! \brief Paused state */
    MP_STATE_PAUSED 		= 3,
    /*! \brief Playing state */
    MP_STATE_PLAYING		= 4,
};

/*! \struct DSP_RECT
 * \brief Describes the information of display rectangle */
struct DSP_RECT {
    /*! \brief The X-coordinate of the left side of the rectangle */
    int32_t     left;
    /*! \brief The Y-coordinate of the top of the rectangle */
    int32_t     top;
    /*! \brief The X-coordinate of the right side of the rectangle */
    int32_t     right;
    /*! \brief The Y-coordinate of the bottom of the rectangle */
    int32_t     bottom;
};

/*! \enum SUBTITLE_INFO
 * \brief Describes the subtitle information */
struct SUBTITLE_INFO {
    /*! \brief PTS time */
    gint64 startTime;
    /*! \brief startTime + duration */
    gint64 endTime;
    /*! \brief duration time */
    gint64 duration;
    /*! \brief Subtitle texts */
    gchar*	subtitleText;
};

/*! \enum DISPLAY_MODE
 * \brief Describes the display mode */
enum DISPLAY_MODE {
    /*! \brief If only primary LCD is supported */
    DISPLAY_MODE_LCD_ONLY   = 0,
    /*! \brief If only secondary HDMI display is supported */
    DISPLAY_MODE_HDMI_ONLY  = 1,
    /*! \brief If both the primary LCD and the secondary HDMI display are supported */
    DISPLAY_MODE_LCD_HDMI   = 2,
    /*! \brief Unknown */
    DISPLAY_MODE_NONE       = 3
};

/*! \enum DISPLAY_TYPE
 * \brief Describes the display type */
enum DISPLAY_TYPE {
    /*! \brief If the display type is primary */
    DISPLAY_TYPE_PRIMARY,
    /*! \brief If the display type is secondary */
    DISPLAY_TYPE_SECONDARY
};

/*! \enum DEMUX_TYPE
 * \brief Describes demux type */
typedef enum {
    /*! \brief unknown */
    DEMUX_TYPE_UNKNOWN = -1,
    /*! \brief mpegtsdemux */
    DEMUX_TYPE_MPEGTSDEMUX,
    /*! \brief qtdemux */
    DEMUX_TYPE_QTDEMUX,
    /*! \brief oggdemux */
    DEMUX_TYPE_OGGDEMUX,
    /*! \brief rmdemux */
    DEMUX_TYPE_RMDEMUX,
    /*! \brief avidemux */
    DEMUX_TYPE_AVIDEMUX,
    /*! \brief asfdemux */
    DEMUX_TYPE_ASFDEMUX,
    /*! \brief matroskademux */
    DEMUX_TYPE_MATROSKADEMUX,
    /*! \brief flvdemux */
    DEMUX_TYPE_FLVDEMUX,
    /*! \brief mpegdemux */
    DEMUX_TYPE_MPEGDEMUX,
    /*! \brief dvdemux */
    DEMUX_TYPE_DVDEMUX,
    /*! \brief wavparse */
    DEMUX_TYPE_WAVPARSE
} DEMUX_TYPE;

typedef enum {
    CONTAINER_TYPE_UNKNOWN = -1,
    CONTAINER_TYPE_MPEGTS,
    CONTAINER_TYPE_QUICKTIME,
    CONTAINER_TYPE_MSVIDEO,
    CONTAINER_TYPE_MATROSKA,
    CONTAINER_TYPE_MPEG,
    CONTAINER_TYPE_3GP,
    CONTAINER_TYPE_FLV,
    CONTAINER_TYPE_ASF,
    CONTAINER_TYPE_OGG,
    CONTAINER_TYPE_REALMEDIA,
    CONTAINER_TYPE_DV,
    CONTAINER_TYPE_ANNODEX,
    CONTAINER_TYPE_WAV,
} CONTAINER_TYPE;

/*! \enum VIDEO_TYPE
 * \brief Describes a video codec type */
typedef enum {
    /*! \brief Unknown */
    VIDEO_TYPE_UNKNOWN = -1,
    /*! \brief H.263 */
    VIDEO_TYPE_H263,
    /*! \brief H.264 */
    VIDEO_TYPE_H264,
    /*! \brief MPEG v4 */
    VIDEO_TYPE_MPEG_V4,
    /*! \brief MPEG v2 */
    VIDEO_TYPE_MPEG_V2,
    /*! \brief MPEG v1 */
    VIDEO_TYPE_MPEG_V1,
    /*! \brief Divx */
    VIDEO_TYPE_DIVX,
    /*! \brief XVID */
    VIDEO_TYPE_XVID,
    /*! \brief FLV */
    VIDEO_TYPE_FLV,
    /*! \brief H.265 */
    VIDEO_TYPE_H265,
    /*! \brief Real Video */
    VIDEO_TYPE_RV,
    /*! \brief ASF */
    VIDEO_TYPE_ASF,
    /*! \brief WMV */
    VIDEO_TYPE_WMV,
    /*! \brief Theora */
    VIDEO_TYPE_THEORA,
} VIDEO_TYPE;

typedef enum {
    AUDIO_TYPE_UNKNOWN = -1,
    AUDIO_TYPE_RAW,
	AUDIO_TYPE_MPEG,
    AUDIO_TYPE_MPEG_V1,
    AUDIO_TYPE_MPEG_V2,
	AUDIO_TYPE_MP3,
	AUDIO_TYPE_AAC,
	AUDIO_TYPE_WMA,
	AUDIO_TYPE_OGG,
	AUDIO_TYPE_AC3,
	AUDIO_TYPE_AC3_PRI,
	AUDIO_TYPE_FLAC,
	AUDIO_TYPE_RA,
	AUDIO_TYPE_DTS,
	AUDIO_TYPE_DTS_PRI,
	AUDIO_TYPE_WAV,
} AUDIO_TYPE;

typedef enum {
    SUBTITLE_TYPE_UNKNOWN = -1,
    SUBTITLE_TYPE_RAW,
    SUBTITLE_TYPE_SSA,
    SUBTITLE_TYPE_ASS,
    SUBTITLE_TYPE_USF,
    SUBTITLE_TYPE_DVD,
    SUBTITLE_TYPE_DVB,
} SUBTITLE_TYPE;

#define MAX_LANG_NAME_NUM   3
typedef struct {
    SUBTITLE_TYPE   type;
    char*           stream_id;
    char*           language_code;
} GST_SUBTITLE_INFO;

typedef struct {
    AUDIO_TYPE      type;
    char*           stream_id;
    char*           audio_pad_name;
    char*           language_code;
    int32_t         n_channels;
    int32_t         samplerate;
    int32_t         bitrate;
} GST_AUDIO_INFO;

typedef struct {
    VIDEO_TYPE      type;
    char*           stream_id;
    char*           video_pad_name;
    int32_t         width;
    int32_t         height;
    int32_t         framerate_num;
    int32_t         framerate_denom;
} GST_VIDEO_INFO;

typedef enum {
    STREAM_TYPE_PROGRAM,
	STREAM_TYPE_VIDEO,
	STREAM_TYPE_AUDIO,
    STREAM_TYPE_SUBTITLE,
} STREAM_TYPE;

/*! \def MAX_STREAM_INFO
 * \brief Maximum number of stream information */
#define	MAX_VIDEO_STREAM_NUM		2
#define	MAX_AUDIO_STREAM_NUM		10
#define	MAX_SUBTITLE_STREAM_NUM		15

#define PROGRAM_MAX			16
#define MAX_STREAM_NUM      20

typedef struct STREAM_INFO
{
    STREAM_TYPE  stream_type;
    int32_t     stream_index;
    int32_t     duration;

    int32_t     seekable;

    /* Video */
    int32_t     width;
    int32_t     height;
    int32_t     framerate_num;
    int32_t     framerate_denom;

    /* Audio */
    int32_t     n_channels;
    int32_t     samplerate;
    int32_t     bitrate;

    /* Subtitle */
    char       language_code[MAX_LANG_NAME_NUM];
} STREAM_INFO;

typedef struct PROGRAM_INFO
{
    /*! \brief Total number of videos */
    int32_t             n_video;
    /*! \brief Total number of audio */
    int32_t             n_audio;
    /*! \brief Total number of subtitles */
    int32_t             n_subtitle;

    /*! \brief Total duration */
    int64_t             duration;
    /*! \brief If the content is seekable */
    int32_t             seekable;

    /*! \brief Currently playing video */
    int32_t             current_video;
    /*! \brief Currently playing audio */
    int32_t             current_audio;
    /*! \brief Currently playing subtitles */
    int32_t             current_subtitle;

    /*! \brief Video stream information */
    GST_VIDEO_INFO      VideoInfo[MAX_VIDEO_STREAM_NUM];
    /*! \brief Audio stream information */
    GST_AUDIO_INFO      AudioInfo[MAX_AUDIO_STREAM_NUM];
    /*! \brief Subtitle stream information */
    GST_SUBTITLE_INFO   SubtitleInfo[MAX_SUBTITLE_STREAM_NUM];
    STREAM_INFO     StreamInfo[MAX_STREAM_NUM];
} PROGRAM_INFO;

/*! \struct GST_MEDIA_INFO
 * \brief Describes the media information */
struct GST_MEDIA_INFO {
    /*! \brief Container format */
    CONTAINER_TYPE      container_type;
    /*! \brief Demux Type */
    DEMUX_TYPE          demux_type;

    /*! \brief Total number of programs */
    int32_t             n_program;
    /*! \brief The program number */
    unsigned int        program_number[PROGRAM_MAX];
    /*! \brief Currently playing program */
    int32_t             current_program_idx;

    /*! \brief The stream information of the currently playing program */
    PROGRAM_INFO        ProgramInfo[PROGRAM_MAX];

    struct DSP_RECT        dsp_rect;

    /*! \brief URI type */
    NX_URI_TYPE		uriType;
};

#ifdef __cplusplus
extern "C" {
#endif	//	__cplusplus

#ifdef __cplusplus
}
#endif

#endif // __NX_GSTTYPES_H