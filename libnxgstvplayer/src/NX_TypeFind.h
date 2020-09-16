#ifndef __NX_TYPEFIND_H
#define __NX_TYPEFIND_H

#include "NX_GstTypes.h"

#ifdef __cplusplus
extern "C" {
#endif	//	__cplusplus

gint typefind_demux (struct GST_MEDIA_INFO *media_handle, const char* filePath);
int get_stream_num_type(struct GST_MEDIA_INFO *media_handle, const char *filePath);
int typefind_codec_info(struct GST_MEDIA_INFO *media_handle, const char *uri,
        gint stream_type, gint program_idx, gint track_num);
gint get_program_info(const char* filePath, struct GST_MEDIA_INFO *media_info);
gint get_stream_simple_info(const char* filePath,
        gint program_number, struct GST_MEDIA_INFO *media_info);
gint get_video_stream_details_info(const char* filePath,
	gint program_number, gint video_index, struct GST_MEDIA_INFO *media_info);
gint get_audio_stream_detail_info(const char* filePath,
        gint program_number, gint audio_index, struct GST_MEDIA_INFO *media_info);
int get_stream_info(const char* filePath, struct GST_MEDIA_INFO *media_info);
#ifdef __cplusplus
}
#endif

#undef LOG_TAG

#endif // __NX_TYPEFIND_H

