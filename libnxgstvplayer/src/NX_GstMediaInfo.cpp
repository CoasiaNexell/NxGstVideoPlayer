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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gst/gst.h>
#include <string.h>

#include <gst/gst.h>
#include "NX_GstMediaInfo.h"
#include "NX_GstDiscover.h"
#include "NX_TypeFind.h"
#include "NX_GstLog.h"
#define LOG_TAG "[NxGstMediaInfo]"

int32_t isSupportedContents(struct GST_MEDIA_INFO *media_handle,
			int32_t program_num, int32_t video_idx);

NX_GST_RET	OpenMediaInfo(GST_MEDIA_INFO **media_handle)
{
	FUNC_IN();

	GST_MEDIA_INFO *handle = NULL;
	handle = (GST_MEDIA_INFO *)g_malloc0(sizeof(GST_MEDIA_INFO));
	if (handle == NULL)
	{
		return NX_GST_RET_ERROR;
	}

	memset(handle, 0, sizeof(GST_MEDIA_INFO));

	handle->container_type = CONTAINER_TYPE_UNKNOWN;
	handle->demux_type = DEMUX_TYPE_UNKNOWN;

	if (!gst_is_initialized())
    {
		gst_init(NULL, NULL);
	}

	*media_handle = handle;

	FUNC_OUT();

	return NX_GST_RET_OK;
}

NX_GST_ERROR  ParseMediaInfo(GST_MEDIA_INFO *media_handle, const char *filePath)
{
	NXGLOGI("START");

	enum NX_GST_ERROR err = NX_GST_ERROR_NONE;

	// Get demux type
	typefind_demux(media_handle, filePath);
	if (-1 == media_handle->demux_type) {
		err = NX_GST_ERROR_NOT_SUPPORTED_CONTENTS;
		return err;
	}

	if (media_handle->demux_type == DEMUX_TYPE_MPEGTSDEMUX)
	{
		// Get total number of programs, program number list from pat
		get_program_info(filePath, media_handle);
		for (int i=0; i< media_handle->n_program; i++)
		{
			int cur_program_no = media_handle->program_number[i];
			if (cur_program_no != 0) {
				// Get total number of streams in each program from dump_collection
				get_stream_simple_info(filePath, cur_program_no, media_handle);
				for (int vIdx = 0; vIdx < media_handle->ProgramInfo[i].n_video; vIdx++)
				{
					get_video_stream_details_info(filePath,
							cur_program_no, vIdx, media_handle);
				}
				for (int aIdx = 0; aIdx < media_handle->ProgramInfo[i].n_audio; aIdx++)
				{
					get_audio_stream_detail_info(filePath,
							cur_program_no, aIdx, media_handle);
				}
			}
		}
	}
	else
	{
		if (0 == media_handle->n_program) {
			if (media_handle->ProgramInfo[0].n_video > 0) {
				media_handle->n_program = 1;
			}
		}
		// TODO: no language code information from playbin3
		//get_stream_info(filePath, media_handle);
		err = StartDiscover(filePath, media_handle);
	}

	NXGLOGI("END");

	return err;
}

int32_t isSupportedContents(struct GST_MEDIA_INFO *media_handle,
			int32_t program_num, int32_t video_idx)
{
    NXGLOGI();
    int program_idx = 0;
	for (int i=0; i<media_handle->n_program; i++)
	{
		if (media_handle->program_number[i] == program_num) {
			program_idx = i;
			break;
		}
	}
    CONTAINER_TYPE container_type = media_handle->container_type;
    VIDEO_TYPE video_type = media_handle->ProgramInfo[program_idx].VideoInfo[video_idx].type;
    NXGLOGI("container_type(%d), video_type(%d)", container_type, video_type);

    /* Quicktime, 3GP, Matroska, AVI, MPEG (vob) */
    if ((container_type == CONTAINER_TYPE_MPEGTS) ||
        (container_type == CONTAINER_TYPE_QUICKTIME) ||
        (container_type == CONTAINER_TYPE_MSVIDEO) ||
        (container_type == CONTAINER_TYPE_MATROSKA) ||
        (container_type == CONTAINER_TYPE_3GP) ||
        (container_type == CONTAINER_TYPE_MPEG)
#ifdef SW_V_DECODER
        || (container_type == CONTAINER_TYPE_FLV)
#endif
        )
    {
#ifdef SW_V_DECODER
        if (video_type > VIDEO_TYPE_FLV)
#else
        if (video_type >= VIDEO_TYPE_FLV)
#endif
        {
            NXGLOGE("Not supported video type(%d)", video_type);
            return -1;
        }

        return 0;
    }
    else
    {
        NXGLOGE("Not supported container type(%d)", container_type);
        return -1;
    }
}

NX_GST_RET  CopyMediaInfo(GST_MEDIA_INFO *dest, GST_MEDIA_INFO *src)
{
	memcpy(dest, src, sizeof(struct GST_MEDIA_INFO));
	for (int pIdx = 0; pIdx < src->n_program; pIdx++)
	{
		for (int vIdx = 0; vIdx < src->ProgramInfo[pIdx].n_video; vIdx++)
		{
			gchar *stream_id = src->ProgramInfo[pIdx].VideoInfo[vIdx].stream_id;
			dest->ProgramInfo[pIdx].VideoInfo[vIdx].stream_id = g_strdup(stream_id);
		}
		for (int aIdx = 0; aIdx < src->ProgramInfo[pIdx].n_audio; aIdx++)
		{
			gchar *stream_id = src->ProgramInfo[pIdx].AudioInfo[aIdx].stream_id;
			gchar *language_code = src->ProgramInfo[pIdx].AudioInfo[aIdx].language_code;
			dest->ProgramInfo[pIdx].AudioInfo[aIdx].stream_id = g_strdup(stream_id);
			dest->ProgramInfo[pIdx].AudioInfo[aIdx].language_code = g_strdup(language_code);
		}
		for (int sIdx = 0; sIdx < src->ProgramInfo[pIdx].n_subtitle; sIdx++)
		{
			gchar *stream_id = src->ProgramInfo[pIdx].SubtitleInfo[sIdx].stream_id;
			gchar *language_code = src->ProgramInfo[pIdx].SubtitleInfo[sIdx].language_code;
			dest->ProgramInfo[pIdx].SubtitleInfo[sIdx].stream_id = g_strdup(stream_id);
			dest->ProgramInfo[pIdx].SubtitleInfo[sIdx].language_code = g_strdup(language_code);
		}
	}
}

void  CloseMediaInfo(GST_MEDIA_INFO *media_handle)
{
	NXGLOGI("START");

	for (int pIdx = 0; pIdx < media_handle->n_program; pIdx++)
	{
		for (int vIdx = 0; vIdx < media_handle->ProgramInfo[pIdx].n_video; vIdx++)
		{
			g_free(media_handle->ProgramInfo[pIdx].VideoInfo[vIdx].stream_id);
		}
		for (int aIdx = 0; aIdx < media_handle->ProgramInfo[pIdx].n_audio; aIdx++)
		{
			g_free(media_handle->ProgramInfo[pIdx].AudioInfo[aIdx].stream_id);
			g_free(media_handle->ProgramInfo[pIdx].AudioInfo[aIdx].language_code);
		}
		for (int sIdx = 0; sIdx < media_handle->ProgramInfo[pIdx].n_subtitle; sIdx++)
		{
			g_free(media_handle->ProgramInfo[pIdx].SubtitleInfo[sIdx].stream_id);
			g_free(media_handle->ProgramInfo[pIdx].SubtitleInfo[sIdx].language_code);
		}
	}

	g_free(media_handle);

	media_handle = NULL;

	NXGLOGI("END");
}

void PrintMediaInfo(GST_MEDIA_INFO *media_info, const char*filePath)
{
	if (NULL == media_info)
	{
		NXGLOGE("media_info is NULL");
		return;
	}

	NXGLOGI("<=========== [GST_MEDIA_INFO] =========== ");
	NXGLOGI("filePath(%s)", filePath);
	NXGLOGI("container_type(%d), demux_type(%d), n_program(%d), current_program_idx(%d)",
			media_info->container_type, media_info->demux_type,
			media_info->n_program, media_info->current_program_idx);

	if (media_info->demux_type != DEMUX_TYPE_MPEGTSDEMUX)
	{
		media_info->n_program = 1;
	}

	for (int i=0; i<media_info->n_program; i++)
	{
		NXGLOGI("ProgramInfo[%d] - program_number[%d]:%d, "
				"n_video(%d), n_audio(%d), n_subtitlte(%d), seekable(%d)",
				i, i, media_info->program_number[i],
				media_info->ProgramInfo[i].n_video,
				media_info->ProgramInfo[i].n_audio,
				media_info->ProgramInfo[i].n_subtitle,
				media_info->ProgramInfo[i].seekable);

		for (int v_idx=0; v_idx<media_info->ProgramInfo[i].n_video; v_idx++)
		{
			NXGLOGI("%*s [VideoInfo[%d]] "
					"type(%d), width(%d), height(%d), framerate(%d/%d), "
					"stream_id(%s)",
					5, " ", v_idx,
					media_info->ProgramInfo[i].VideoInfo[v_idx].type,
					media_info->ProgramInfo[i].VideoInfo[v_idx].width,
					media_info->ProgramInfo[i].VideoInfo[v_idx].height,
					media_info->ProgramInfo[i].VideoInfo[v_idx].framerate_num,
					media_info->ProgramInfo[i].VideoInfo[v_idx].framerate_denom,
					media_info->ProgramInfo[i].VideoInfo[v_idx].stream_id);
		}
		for (int a_idx=0; a_idx<media_info->ProgramInfo[i].n_audio; a_idx++)
		{
			NXGLOGI("%*s [AudioInfo[%d]] "
					"type(%d), n_channels(%d), samplerate(%d), bitrate(%d), "
					"language_code(%s), stream_id(%s)",
					5, " ", a_idx,
					media_info->ProgramInfo[i].AudioInfo[a_idx].type,
					media_info->ProgramInfo[i].AudioInfo[a_idx].n_channels,
					media_info->ProgramInfo[i].AudioInfo[a_idx].samplerate,
					media_info->ProgramInfo[i].AudioInfo[a_idx].bitrate,
					(media_info->ProgramInfo[i].AudioInfo[a_idx].language_code) ?
						media_info->ProgramInfo[i].AudioInfo[a_idx].language_code:"",
					(media_info->ProgramInfo[i].AudioInfo[a_idx].stream_id) ? 
						media_info->ProgramInfo[i].AudioInfo[a_idx].stream_id:"");
		}
		for (int s_idx=0; s_idx<media_info->ProgramInfo[i].n_subtitle; s_idx++)
		{
			NXGLOGI("%*s [SubtitleInfo[%d]] "
					"type(%d), language_code(%s), stream_id(%s)",
					5, " ", s_idx,
					media_info->ProgramInfo[i].SubtitleInfo[s_idx].type,
					(media_info->ProgramInfo[i].SubtitleInfo[s_idx].language_code)?
						media_info->ProgramInfo[i].SubtitleInfo[s_idx].language_code:"",
					(media_info->ProgramInfo[i].SubtitleInfo[s_idx].stream_id)?
						media_info->ProgramInfo[i].SubtitleInfo[s_idx].stream_id:"");
		}
	}

	NXGLOGI("=========== [GST_MEDIA_INFO] ===========> ");
}

