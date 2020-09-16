#include <stdio.h>
#include <gst/gst.h>
#include "NX_GstTypes.h"
#include "NX_GstLog.h"
#define LOG_TAG "[NX_StreamParser]"

/* Structure to contain all our information, so we can pass it around */
typedef struct _CustomData
{
	GstElement *playbin;          /* Our one and only element */

	gint n_video;                 /* Number of embedded video streams */
	gint n_audio;                 /* Number of embedded audio streams */
	gint n_text;                  /* Number of embedded subtitle streams */

	gint current_video;           /* Currently playing video stream */
	gint current_audio;           /* Currently playing audio stream */
	gint current_text;            /* Currently playing subtitle stream */

	GMainLoop *main_loop;         /* GLib's Main Loop */
	struct GST_MEDIA_INFO *media_info;
} CustomData;

/* playbin flags */
typedef enum
{
	GST_PLAY_FLAG_VIDEO = (1 << 0),       /* We want video output */
	GST_PLAY_FLAG_AUDIO = (1 << 1),       /* We want audio output */
	GST_PLAY_FLAG_TEXT = (1 << 2)           /* We want subtitle output */
} GstPlayFlags;

/* Forward definition for the message and keyboard processing functions */
static gboolean handle_message (GstBus * bus, GstMessage * msg,
			CustomData * data);

int get_stream_info(const char* filePath, struct GST_MEDIA_INFO *media_info)
{
	GMainContext *worker_context;
	CustomData data;
	GstBus *bus;
	GstStateChangeReturn ret;
	gint flags;
	gchar *uri;

	NXGLOGI("START");

	/* Initialize GStreamer */
	gst_init (NULL, NULL);

	data.media_info = media_info;

	/* Create the elements */
	data.playbin = gst_element_factory_make ("playbin3", "playbin");

	if (!data.playbin) {
	NXGLOGE("Not all elements could be created.");
		return -1;
	}
	if (gst_uri_is_valid (filePath))
		uri = g_strdup (filePath);
	else
		uri = gst_filename_to_uri (filePath, NULL);

	/* Set the URI to play */
	g_object_set (data.playbin, "uri", uri, NULL);
	g_free(uri);

	/* Set flags to show Audio and Video but ignore Subtitles */
	g_object_get (data.playbin, "flags", &flags, NULL);
	flags |= GST_PLAY_FLAG_VIDEO | GST_PLAY_FLAG_AUDIO;
	flags &= ~GST_PLAY_FLAG_TEXT;
	g_object_set (data.playbin, "flags", flags, NULL);

	/* Add a bus watch, so we get notified when a message arrives */
	bus = gst_element_get_bus (data.playbin);
	gst_bus_add_watch (bus, (GstBusFunc) handle_message, &data);
	gst_object_unref (GST_OBJECT (bus));

	/* Start playing */
	ret = gst_element_set_state (data.playbin, GST_STATE_PLAYING);
	if (ret == GST_STATE_CHANGE_FAILURE) {
		NXGLOGE("Unable to set the pipeline to the playing state.");
		gst_object_unref (data.playbin);
	return -1;
	}

	/* Create a GLib Main Loop and set it to run */
	worker_context = g_main_context_new();
	g_main_context_push_thread_default(worker_context);
	data.main_loop = g_main_loop_new (worker_context, FALSE);
	g_main_loop_run (data.main_loop);

	/* Free resources */
	//g_io_channel_unref (io_stdin);
	gst_element_set_state (data.playbin, GST_STATE_NULL);
	gst_object_unref (data.playbin);

	g_main_loop_unref(data.main_loop);
	g_main_context_pop_thread_default(worker_context);
	g_main_context_unref(worker_context);

	return 0;
}

/* Extract some metadata from the streams and print it on the screen */
static void
analyze_streams (CustomData * data)
{
	gint i;
	GstTagList *tags;
	gchar *str;
	guint rate;

	/* Read some properties */
	g_object_get (data->playbin, "n-video", &data->n_video, NULL);
	g_object_get (data->playbin, "n-audio", &data->n_audio, NULL);
	g_object_get (data->playbin, "n-text", &data->n_text, NULL);

	NXGLOGI ("%d video stream(s), %d audio stream(s), %d text stream(s)\n",
			data->n_video, data->n_audio, data->n_text);

	NXGLOGI ("\n");
	for (i = 0; i < data->n_video; i++) {
		tags = NULL;
		/* Retrieve the stream's video tags */
		g_signal_emit_by_name (data->playbin, "get-video-tags", i, &tags);
		if (tags) {
			NXGLOGI ("video stream %d", i);
			gst_tag_list_get_string (tags, GST_TAG_VIDEO_CODEC, &str);
			NXGLOGI ("  codec: %s", str ? str : "unknown");
			g_free (str);
			gst_tag_list_unref (tags);
		}
	}

	NXGLOGI ("\n");
	for (i = 0; i < data->n_audio; i++) {
		tags = NULL;
		/* Retrieve the stream's audio tags */
		g_signal_emit_by_name (data->playbin, "get-audio-tags", i, &tags);
		if (tags) {
			NXGLOGI ("audio stream %d:", i);
			if (gst_tag_list_get_string (tags, GST_TAG_AUDIO_CODEC, &str)) {
				NXGLOGI ("  codec: %s", str);
				g_free (str);
			}
			if (gst_tag_list_get_string (tags, GST_TAG_LANGUAGE_CODE, &str)) {
				NXGLOGI ("  language: %s", str);
				g_free (str);
			}
			if (gst_tag_list_get_uint (tags, GST_TAG_BITRATE, &rate)) {
				NXGLOGI ("  bitrate: %d", rate);
			}
			gst_tag_list_unref (tags);
		}
	}

	NXGLOGI ("\n");
	for (i = 0; i < data->n_text; i++) {
		tags = NULL;
		/* Retrieve the stream's subtitle tags */
		g_signal_emit_by_name (data->playbin, "get-text-tags", i, &tags);
		if (tags) {
			NXGLOGI ("subtitle stream %d", i);
			if (gst_tag_list_get_string (tags, GST_TAG_LANGUAGE_CODE, &str)) {
				NXGLOGI ("  language: %s", str);
				g_free (str);
			}
			gst_tag_list_unref (tags);
		}
	}

	g_object_get (data->playbin, "current-video", &data->current_video, NULL);
	g_object_get (data->playbin, "current-audio", &data->current_audio, NULL);
	g_object_get (data->playbin, "current-text", &data->current_text, NULL);

	NXGLOGI ("\n");
	NXGLOGI("Currently playing video stream %d, audio stream %d and text stream %d",
			data->current_video, data->current_audio, data->current_text);
}

static void
print_tag_foreach (const GstTagList * tags, const gchar * tag,
	gpointer user_data)
{
	GValue val = { 0, };
	gchar *str;
	gint depth = GPOINTER_TO_INT (user_data);

	if (!gst_tag_list_copy_value (&val, tags, tag))
		return;

	if (G_VALUE_HOLDS_STRING (&val))
		str = g_value_dup_string (&val);
	else
		str = gst_value_serialize (&val);

	NXGLOGI ("%*s%s: %s\n", 2 * depth, " ", gst_tag_get_nick (tag), str);
	g_free (str);

	g_value_unset (&val);
}

static void
dump_collection (GstStreamCollection * collection, CustomData * data)
{
	guint i;
	GstTagList *tags;
	GstCaps *caps;
	GstStreamType stype;
	GstStructure *structure;
	guint cur_pro_idx = 0;
	NXGLOGI("");

	guint size = gst_stream_collection_get_size (collection);
	if (size < 1) {
		NXGLOGI("There is no video/audio/subtitle stream");
		return;
	}
  	for (i = 0; i < size; i++)
	{
		GstStream *stream = gst_stream_collection_get_stream (collection, i);
		GstStreamType stype = gst_stream_get_stream_type (stream);
		NXGLOGI (" Stream %u type %s flags 0x%x", i,
				gst_stream_type_get_name (stype),
				gst_stream_get_stream_flags (stream));
		const char* stream_id = gst_stream_get_stream_id (stream);
		NXGLOGI ("  ID: %s", stream_id);

		caps = gst_stream_get_caps (stream);
		if (caps) {
			gchar *caps_str = gst_caps_to_string (caps);
			NXGLOGI ("  caps: %s", caps_str);
			g_free (caps_str);
			gst_caps_unref (caps);
		}

		const GstStructure *structure = gst_caps_get_structure(caps, 0);
		const gchar *mime_type = gst_structure_get_name(structure);

		tags = gst_stream_get_tags (stream);
		if (tags) {
			NXGLOGI ("  tags:");
			gst_tag_list_foreach (tags, print_tag_foreach, GUINT_TO_POINTER (4));
		}

		NXGLOGI("MIME-type (%s)", mime_type);

		if (stype & GST_STREAM_TYPE_AUDIO)
		{
			gint audio_mpegversion, channels, samplerate;
			AUDIO_TYPE audio_type = get_audio_codec_type(mime_type);
			gchar* lang = gst_structure_get_string (structure, GST_TAG_LANGUAGE_CODE);
			int32_t a_idx = data->media_info->ProgramInfo[cur_pro_idx].n_audio;

			data->media_info->ProgramInfo[cur_pro_idx].AudioInfo[a_idx].type = audio_type;
			if (gst_structure_get_int (structure, "mpegversion", &audio_mpegversion))
			{
				if (audio_mpegversion == 1) {
					data->media_info->ProgramInfo[cur_pro_idx].AudioInfo[a_idx].type = AUDIO_TYPE_MPEG_V1;
				} else if (audio_mpegversion == 2) {
					data->media_info->ProgramInfo[cur_pro_idx].AudioInfo[a_idx].type = AUDIO_TYPE_MPEG_V2;
				}
			}
			if (gst_structure_get_int(structure, "channels", &channels)) {
				data->media_info->ProgramInfo[cur_pro_idx].AudioInfo[a_idx].n_channels = channels;
			}
			if (gst_structure_get_int(structure, "rate", &samplerate)) {
				data->media_info->ProgramInfo[cur_pro_idx].AudioInfo[a_idx].samplerate = samplerate;
			}
			data->media_info->ProgramInfo[cur_pro_idx].AudioInfo[a_idx].language_code = g_strdup(lang);
			data->media_info->ProgramInfo[cur_pro_idx].AudioInfo[a_idx].stream_id = g_strdup(stream_id);
			data->media_info->ProgramInfo[cur_pro_idx].n_audio++;
			NXGLOGI("n_audio(%d), audio type(%d), languague_code(%s), stream_id(%s)",
					data->media_info->ProgramInfo[cur_pro_idx].n_audio,
					data->media_info->ProgramInfo[cur_pro_idx].AudioInfo[a_idx].type,
					(lang ? lang:""), (stream_id ? stream_id:""));
		}
		else if (stype & GST_STREAM_TYPE_VIDEO)
		{
			gint video_mpegversion, num, den, width, height;
			VIDEO_TYPE video_type = get_video_codec_type(mime_type);
			int32_t v_idx = data->media_info->ProgramInfo[cur_pro_idx].n_video;

			data->media_info->ProgramInfo[cur_pro_idx].VideoInfo[v_idx].type = video_type;
			if ((structure != NULL) && (video_type == VIDEO_TYPE_MPEG_V4)) {
				gst_structure_get_int (structure, "mpegversion", &video_mpegversion);
				if (video_mpegversion == 1) {
					data->media_info->ProgramInfo[cur_pro_idx].VideoInfo[v_idx].type = VIDEO_TYPE_MPEG_V1;
				} else if (video_mpegversion == 2) {
					data->media_info->ProgramInfo[cur_pro_idx].VideoInfo[v_idx].type = VIDEO_TYPE_MPEG_V2;
				}
			}

			if (gst_structure_get_int(structure, "width", &width) &&
				gst_structure_get_int(structure, "height", &height)) {
				data->media_info->ProgramInfo[cur_pro_idx].VideoInfo[v_idx].width = width;
				data->media_info->ProgramInfo[cur_pro_idx].VideoInfo[v_idx].height = height;
			}

			if (gst_structure_get_fraction(structure, "framerate", &num, &den)) {
				data->media_info->ProgramInfo[cur_pro_idx].VideoInfo[v_idx].framerate_num = num;
				data->media_info->ProgramInfo[cur_pro_idx].VideoInfo[v_idx].framerate_denom = den;
			}

			data->media_info->ProgramInfo[cur_pro_idx].VideoInfo[v_idx].stream_id = g_strdup(stream_id);
			data->media_info->ProgramInfo[cur_pro_idx].n_video++;

			NXGLOGI("n_video(%d), video_width(%d), video_height(%d), framerate(%d/%d), video type(%d) stream_id(%s)",
					data->media_info->ProgramInfo[cur_pro_idx].n_video,
					width, height, num, den,
					data->media_info->ProgramInfo[cur_pro_idx].VideoInfo[v_idx].type,
					(stream_id ? stream_id:""));
		}
		else if (stype & GST_STREAM_TYPE_TEXT)
		{
			SUBTITLE_TYPE sub_type = get_subtitle_codec_type(mime_type);
			gchar* lang = gst_structure_get_string (structure, GST_TAG_LANGUAGE_CODE);
			int32_t sub_idx = data->media_info->ProgramInfo[cur_pro_idx].n_subtitle;

			data->media_info->ProgramInfo[cur_pro_idx].SubtitleInfo[sub_idx].type = sub_type;
			data->media_info->ProgramInfo[cur_pro_idx].SubtitleInfo[sub_idx].language_code = g_strdup(lang);
			data->media_info->ProgramInfo[cur_pro_idx].SubtitleInfo[sub_idx].stream_id = g_strdup(stream_id);
			data->media_info->ProgramInfo[cur_pro_idx].n_subtitle++;

			NXGLOGI("n_subtitle(%d), subtitle_type(%d), language_code(%s), stream_id(%s)",
                data->media_info->ProgramInfo[cur_pro_idx].n_subtitle,
                data->media_info->ProgramInfo[cur_pro_idx].SubtitleInfo[sub_idx].type,
                data->media_info->ProgramInfo[cur_pro_idx].SubtitleInfo[sub_idx].language_code,
				(stream_id ? stream_id:""));
		}

		if (tags) {
			gst_tag_list_unref (tags);
		}
	}
}

/* Process messages from GStreamer */
static gboolean
handle_message (GstBus * bus, GstMessage * msg, CustomData * data)
{
	GError *err;
	gchar *debug_info;

	switch (GST_MESSAGE_TYPE (msg)) {
		case GST_MESSAGE_ERROR:
			gst_message_parse_error (msg, &err, &debug_info);
			g_printerr ("Error received from element %s: %s\n",
			GST_OBJECT_NAME (msg->src), err->message);
			g_printerr ("Debugging information: %s\n",
			debug_info ? debug_info : "none");
			g_clear_error (&err);
			g_free (debug_info);
			g_main_loop_quit (data->main_loop);
			break;
		case GST_MESSAGE_EOS:
			NXGLOGI ("End-Of-Stream reached.\n");
			g_main_loop_quit (data->main_loop);
			break;
		case GST_MESSAGE_STATE_CHANGED:
		{
			GstState old_state, new_state, pending_state;
			gst_message_parse_state_changed (msg, &old_state, &new_state,
						&pending_state);
			if (GST_MESSAGE_SRC (msg) == GST_OBJECT (data->playbin)) {
				if (new_state == GST_STATE_PLAYING) {
					/* Once we are in the playing state, analyze the streams */
					//analyze_streams (data);
				}
			}
		}
		break;
		case GST_MESSAGE_STREAM_COLLECTION:
		{
			GstStreamCollection *collection = NULL;
			GstObject *src = GST_MESSAGE_SRC (msg);

			gst_message_parse_stream_collection (msg, &collection);
			if (collection)
			{
				NXGLOGI("Got a collection from %s:\n",
						src ? GST_OBJECT_NAME (src) : "Unknown");
				dump_collection (collection, data);
				gst_object_unref (collection);
				g_main_loop_quit (data->main_loop);
			}
			break;
		}
		default:
		{
			GstMessageType type = GST_MESSAGE_TYPE(msg);
			// TODO: latency, stream-status, reset-time, async-done, new-clock, etc
			NXGLOGV("Received GST_MESSAGE_TYPE [%s]",
				   gst_message_type_get_name(type));
			break;
		}
	}

	/* We want to keep receiving messages */
	return TRUE;
}