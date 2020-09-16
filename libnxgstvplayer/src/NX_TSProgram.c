/* GStreamer
 *
 * ts-parser.c: sample application to display mpeg-ts info from any pipeline
 * Copyright (C) 2013
 *           Edward Hervey <bilboed@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define DUMP_DESCRIPTORS 0

#include <glib.h>
#include <glib-object.h>
#include <glib/gprintf.h>
#include <gst/gst.h>
#include <gst/mpegts/mpegts.h>
#include "NX_TypeFind.h"
#include "NX_GstLog.h"
#define LOG_TAG "[NX_TSProgram]"

#define MPEGTIME_TO_GSTTIME(t) ((t) * (guint64)100000 / 9)

// gst-launch-1.0 -v filesrc location=.ts ! 
// tsdemux name=demux demux. ! queue !  decodebin ! typefind ! fakesink
typedef struct MpegTsSt {
	GMainLoop*  loop;
	GstBus      *bus;
	GstElement  *pipeline;
	GstElement  *demuxer;
	GstElement  *filesrc;
	GstElement  *video_queue;
	GstElement  *subtitle_queue;
	GstElement  *decodebin;
	GstElement  *typefind;
	GstElement  *fakesink;

	GstElement  *audio_queue;
	GstElement  *audio_decodebin;
	GstElement  *audio_typefind;
	GstElement  *audio_fakesink;

	GstElement  *audio_queue2;
	GstElement  *audio_decodebin2;
	GstElement  *audio_typefind2;
	GstElement  *audio_fakesink2;

	GstElement	*target_sink;
	gint		pad_added_video_num;
	gint		pad_added_audio_num;
	gint		pad_added_subtitle_num;

	gint		current_video_idx;
	gint		current_audio_idx;
	gint		current_subtitle_idx;

	gint		program_index;
	gint		select_video_idx;
	gint		select_audio_idx;
	gint		select_subtitle_idx;

	struct GST_MEDIA_INFO *media_info;
} MpegTsSt;

static gboolean
idle_exit_loop (gpointer data)
{
	g_main_loop_quit ((GMainLoop *) data);

	/* once */
	return FALSE;
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

	NXGLOGV ("%*s%s: %s\n", 2 * depth, " ", gst_tag_get_nick (tag), str);
	g_free (str);

	g_value_unset (&val);
}

static void
gst_info_dump_mem_line (gchar * linebuf, gsize linebuf_size,
		const guint8 * mem, gsize mem_offset, gsize mem_size)
{
	gchar hexstr[50], ascstr[18], digitstr[4];

	if (mem_size > 16)
		mem_size = 16;

	hexstr[0] = '\0';
	ascstr[0] = '\0';

	if (mem != NULL) {
		guint i = 0;

		mem += mem_offset;
		while (i < mem_size) {
			ascstr[i] = (g_ascii_isprint (mem[i])) ? mem[i] : '.';
			g_snprintf (digitstr, sizeof (digitstr), "%02x ", mem[i]);
			g_strlcat (hexstr, digitstr, sizeof (hexstr));
			++i;
		}
		ascstr[i] = '\0';
	}

	g_snprintf (linebuf, linebuf_size, "%08x: %-48.48s %-16.16s",
			(guint) mem_offset, hexstr, ascstr);
}

static void
dump_memory_bytes (guint8 * data, guint len, guint spacing)
{
	gsize off = 0;

	while (off < len) {
		gchar buf[128];

		/* gst_info_dump_mem_line will process 16 bytes at most */
		gst_info_dump_mem_line (buf, sizeof (buf), data, off, len - off);
		NXGLOGV("%*s   %s", spacing, "", buf);
		off += 16;
	}
}

#define dump_memory_content(desc, spacing) dump_memory_bytes((desc)->data + 2, (desc)->length, spacing)

static const gchar *
descriptor_name (gint val)
{
	GEnumValue *en;

	en = g_enum_get_value (G_ENUM_CLASS (g_type_class_peek
					(GST_TYPE_MPEGTS_DESCRIPTOR_TYPE)), val);
	if (en == NULL)
		// Else try with DVB enum types
		en = g_enum_get_value (G_ENUM_CLASS (g_type_class_peek
						(GST_TYPE_MPEGTS_DVB_DESCRIPTOR_TYPE)), val);
	if (en == NULL)
		// Else try with ATSC enum types
		en = g_enum_get_value (G_ENUM_CLASS (g_type_class_peek
						(GST_TYPE_MPEGTS_ATSC_DESCRIPTOR_TYPE)), val);
	if (en == NULL)
		// Else try with ISB enum types
		en = g_enum_get_value (G_ENUM_CLASS (g_type_class_peek
						(GST_TYPE_MPEGTS_ISDB_DESCRIPTOR_TYPE)), val);
	if (en == NULL)
		// Else try with misc enum types
		en = g_enum_get_value (G_ENUM_CLASS (g_type_class_peek
						(GST_TYPE_MPEGTS_MISC_DESCRIPTOR_TYPE)), val);
	if (en == NULL)
		return "UNKNOWN/PRIVATE";
	return en->value_nick;
}

static const gchar *
table_id_name (gint val)
{
	GEnumValue *en;

	en = g_enum_get_value (G_ENUM_CLASS (g_type_class_peek
					(GST_TYPE_MPEGTS_SECTION_TABLE_ID)), val);
	if (en == NULL)
		/* Else try with DVB enum types */
		en = g_enum_get_value (G_ENUM_CLASS (g_type_class_peek
						(GST_TYPE_MPEGTS_SECTION_DVB_TABLE_ID)), val);
	if (en == NULL)
		/* Else try with ATSC enum types */
		en = g_enum_get_value (G_ENUM_CLASS (g_type_class_peek
						(GST_TYPE_MPEGTS_SECTION_ATSC_TABLE_ID)), val);
	if (en == NULL)
		/* Else try with SCTE enum types */
		en = g_enum_get_value (G_ENUM_CLASS (g_type_class_peek
						(GST_TYPE_MPEGTS_SECTION_SCTE_TABLE_ID)), val);
	if (en == NULL)
		return "UNKNOWN/PRIVATE";
	return en->value_nick;
}

static const gchar *
stream_type_name (gint val)
{
	GEnumValue *en;

	en = g_enum_get_value (G_ENUM_CLASS (g_type_class_peek
					(GST_TYPE_MPEGTS_STREAM_TYPE)), val);
	if (en == NULL)
		/* Else try with SCTE enum types */
		en = g_enum_get_value (G_ENUM_CLASS (g_type_class_peek
						(GST_TYPE_MPEGTS_SCTE_STREAM_TYPE)), val);
	if (en == NULL)
		return "UNKNOWN/PRIVATE";
	return en->value_nick;
}

static const gchar *
enum_name (GType instance_type, gint val)
{
	GEnumValue *en;

	en = g_enum_get_value (G_ENUM_CLASS (g_type_class_peek (instance_type)), val);

	if (!en)
		return "UNKNOWN/PRIVATE";
	return en->value_nick;
}

static void
dump_cable_delivery_descriptor (GstMpegtsDescriptor * desc, guint spacing)
{
	GstMpegtsCableDeliverySystemDescriptor res;

	if (gst_mpegts_descriptor_parse_cable_delivery_system (desc, &res)) {
		NXGLOGV("%*s Cable Delivery Descriptor", spacing, "");
		NXGLOGV("%*s   Frequency   : %d Hz", spacing, "", res.frequency);
		NXGLOGV("%*s   Outer FEC   : %d (%s)", spacing, "", res.outer_fec,
				enum_name (GST_TYPE_MPEGTS_CABLE_OUTER_FEC_SCHEME, res.outer_fec));
		NXGLOGV("%*s   modulation  : %d (%s)", spacing, "", res.modulation,
				enum_name (GST_TYPE_MPEGTS_MODULATION_TYPE, res.modulation));
		NXGLOGV("%*s   Symbol rate : %d sym/s", spacing, "", res.symbol_rate);
		NXGLOGV("%*s   Inner FEC   : %d (%s)", spacing, "", res.fec_inner,
				enum_name (GST_TYPE_MPEGTS_DVB_CODE_RATE, res.fec_inner));
	}
}

static void
dump_terrestrial_delivery (GstMpegtsDescriptor * desc, guint spacing)
{
	GstMpegtsTerrestrialDeliverySystemDescriptor res;

	if (gst_mpegts_descriptor_parse_terrestrial_delivery_system (desc, &res)) {
		NXGLOGV("%*s Terrestrial Delivery Descriptor", spacing, "");
		NXGLOGV("%*s   Frequency         : %d Hz", spacing, "", res.frequency);
		NXGLOGV("%*s   Bandwidth         : %d Hz", spacing, "", res.bandwidth);
		NXGLOGV("%*s   Priority          : %s", spacing, "",
				res.priority ? "TRUE" : "FALSE");
		NXGLOGV("%*s   Time slicing      : %s", spacing, "",
				res.time_slicing ? "TRUE" : "FALSE");
		NXGLOGV("%*s   MPE FEC           : %s", spacing, "",
				res.mpe_fec ? "TRUE" : "FALSE");
		NXGLOGV("%*s   Constellation     : %d (%s)", spacing, "",
				res.constellation, enum_name (GST_TYPE_MPEGTS_MODULATION_TYPE,
						res.constellation));
		NXGLOGV("%*s   Hierarchy         : %d (%s)", spacing, "",
				res.hierarchy, enum_name (GST_TYPE_MPEGTS_TERRESTRIAL_HIERARCHY,
						res.hierarchy));
		NXGLOGV("%*s   Code Rate HP      : %d (%s)", spacing, "",
				res.code_rate_hp, enum_name (GST_TYPE_MPEGTS_DVB_CODE_RATE,
						res.code_rate_hp));
		NXGLOGV("%*s   Code Rate LP      : %d (%s)", spacing, "",
				res.code_rate_lp, enum_name (GST_TYPE_MPEGTS_DVB_CODE_RATE,
						res.code_rate_lp));
		NXGLOGV("%*s   Guard Interval    : %d (%s)", spacing, "",
				res.guard_interval,
				enum_name (GST_TYPE_MPEGTS_TERRESTRIAL_GUARD_INTERVAL,
						res.guard_interval));
		NXGLOGV("%*s   Transmission Mode : %d (%s)", spacing, "",
				res.transmission_mode,
				enum_name (GST_TYPE_MPEGTS_TERRESTRIAL_TRANSMISSION_MODE,
						res.transmission_mode));
		NXGLOGV("%*s   Other Frequency   : %s", spacing, "",
				res.other_frequency ? "TRUE" : "FALSE");
	}
}

static void
dump_dvb_service_list (GstMpegtsDescriptor * desc, guint spacing)
{
	GPtrArray *res;

	if (gst_mpegts_descriptor_parse_dvb_service_list (desc, &res)) {
		guint i;
		NXGLOGV("%*s DVB Service List Descriptor", spacing, "");
		for (i = 0; i < res->len; i++) {
			GstMpegtsDVBServiceListItem *item = g_ptr_array_index (res, i);
			NXGLOGV("%*s   Service #%d, id:0x%04x, type:0x%x (%s)",
					spacing, "", i, item->service_id, item->type,
					enum_name (GST_TYPE_MPEGTS_DVB_SERVICE_TYPE, item->type));
		}
		g_ptr_array_unref (res);
	}
}

static void
dump_logical_channel_descriptor (GstMpegtsDescriptor * desc, guint spacing)
{
	GstMpegtsLogicalChannelDescriptor res;
	guint i;

	if (gst_mpegts_descriptor_parse_logical_channel (desc, &res)) {
		NXGLOGV("%*s Logical Channel Descriptor (%d channels)", spacing, "",
				res.nb_channels);
		for (i = 0; i < res.nb_channels; i++) {
			GstMpegtsLogicalChannel *chann = &res.channels[i];
			NXGLOGV("%*s   service_id: 0x%04x, logical channel number:%4d",
					spacing, "", chann->service_id, chann->logical_channel_number);
		}
	}
}

static void
dump_multiligual_network_name (GstMpegtsDescriptor * desc, guint spacing)
{
	GPtrArray *items;
	if (gst_mpegts_descriptor_parse_dvb_multilingual_network_name (desc, &items)) {
		guint i;
		for (i = 0; i < items->len; i++) {
			GstMpegtsDvbMultilingualNetworkNameItem *item =
					g_ptr_array_index (items, i);
			NXGLOGV("%*s item : %u", spacing, "", i);
			NXGLOGV("%*s   language_code : %s", spacing, "", item->language_code);
			NXGLOGV("%*s   network_name  : %s", spacing, "", item->network_name);
		}
		g_ptr_array_unref (items);
	}
}

static void
dump_multiligual_bouquet_name (GstMpegtsDescriptor * desc, guint spacing)
{
	GPtrArray *items;
	if (gst_mpegts_descriptor_parse_dvb_multilingual_bouquet_name (desc, &items)) {
		guint i;
		for (i = 0; i < items->len; i++) {
			GstMpegtsDvbMultilingualBouquetNameItem *item =
					g_ptr_array_index (items, i);
			NXGLOGV("%*s item : %u", spacing, "", i);
			NXGLOGV("%*s   language_code : %s", spacing, "", item->language_code);
			NXGLOGV("%*s   bouguet_name  : %s", spacing, "", item->bouquet_name);
		}
		g_ptr_array_unref (items);
	}
}

static void
dump_multiligual_service_name (GstMpegtsDescriptor * desc, guint spacing)
{
	GPtrArray *items;
	if (gst_mpegts_descriptor_parse_dvb_multilingual_service_name (desc, &items)) {
		guint i;
		for (i = 0; i < items->len; i++) {
			GstMpegtsDvbMultilingualServiceNameItem *item =
					g_ptr_array_index (items, i);
			NXGLOGV("%*s item : %u", spacing, "", i);
			NXGLOGV("%*s   language_code : %s", spacing, "", item->language_code);
			NXGLOGV("%*s   service_name  : %s", spacing, "", item->service_name);
			NXGLOGV("%*s   provider_name : %s", spacing, "", item->provider_name);
		}
		g_ptr_array_unref (items);
	}
}

static void
dump_multiligual_component (GstMpegtsDescriptor * desc, guint spacing)
{
	GPtrArray *items;
	guint8 tag;
	if (gst_mpegts_descriptor_parse_dvb_multilingual_component (desc, &tag,
					&items)) {
		guint8 i;
		NXGLOGV("%*s component_tag : 0x%02x", spacing, "", tag);
		for (i = 0; i < items->len; i++) {
			GstMpegtsDvbMultilingualComponentItem *item =
					g_ptr_array_index (items, i);
			NXGLOGV("%*s   item : %u", spacing, "", i);
			NXGLOGV("%*s     language_code : %s", spacing, "",
					item->language_code);
			NXGLOGV("%*s     description   : %s", spacing, "", item->description);
		}
		g_ptr_array_unref (items);
	}
}

static void
dump_linkage (GstMpegtsDescriptor * desc, guint spacing)
{
	GstMpegtsDVBLinkageDescriptor *res;

	if (gst_mpegts_descriptor_parse_dvb_linkage (desc, &res)) {
		NXGLOGV("%*s Linkage Descriptor : 0x%02x (%s)", spacing, "",
				res->linkage_type, enum_name (GST_TYPE_MPEGTS_DVB_LINKAGE_TYPE,
						res->linkage_type));

		NXGLOGV("%*s   Transport Stream ID : 0x%04x", spacing, "",
				res->transport_stream_id);
		NXGLOGV("%*s   Original Network ID : 0x%04x", spacing, "",
				res->original_network_id);
		NXGLOGV("%*s   Service ID          : 0x%04x", spacing, "",
				res->service_id);

		switch (res->linkage_type) {
			case GST_MPEGTS_DVB_LINKAGE_MOBILE_HAND_OVER:
			{
				const GstMpegtsDVBLinkageMobileHandOver *linkage =
						gst_mpegts_dvb_linkage_descriptor_get_mobile_hand_over (res);
				NXGLOGV("%*s   hand_over_type    : 0x%02x (%s)", spacing,
						"", linkage->hand_over_type,
						enum_name (GST_TYPE_MPEGTS_DVB_LINKAGE_HAND_OVER_TYPE,
								linkage->hand_over_type));
				NXGLOGV("%*s   origin_type       : %s", spacing, "",
						linkage->origin_type ? "SDT" : "NIT");
				NXGLOGV("%*s   network_id        : 0x%04x", spacing, "",
						linkage->network_id);
				NXGLOGV("%*s   initial_service_id: 0x%04x", spacing, "",
						linkage->initial_service_id);
				break;
			}
			case GST_MPEGTS_DVB_LINKAGE_EVENT:
			{
				const GstMpegtsDVBLinkageEvent *linkage =
						gst_mpegts_dvb_linkage_descriptor_get_event (res);
				NXGLOGV("%*s   target_event_id   : 0x%04x", spacing, "",
						linkage->target_event_id);
				NXGLOGV("%*s   target_listed     : %s", spacing, "",
						linkage->target_listed ? "TRUE" : "FALSE");
				NXGLOGV("%*s   event_simulcast   : %s", spacing, "",
						linkage->event_simulcast ? "TRUE" : "FALSE");
				break;
			}
			case GST_MPEGTS_DVB_LINKAGE_EXTENDED_EVENT:
			{
				guint i;
				const GPtrArray *items =
						gst_mpegts_dvb_linkage_descriptor_get_extended_event (res);

				for (i = 0; i < items->len; i++) {
					GstMpegtsDVBLinkageExtendedEvent *linkage =
							g_ptr_array_index (items, i);
					NXGLOGV("%*s   target_event_id   : 0x%04x", spacing, "",
							linkage->target_event_id);
					NXGLOGV("%*s   target_listed     : %s", spacing, "",
							linkage->target_listed ? "TRUE" : "FALSE");
					NXGLOGV("%*s   event_simulcast   : %s", spacing, "",
							linkage->event_simulcast ? "TRUE" : "FALSE");
					NXGLOGV("%*s   link_type         : 0x%01x", spacing, "",
							linkage->link_type);
					NXGLOGV("%*s   target_id_type    : 0x%01x", spacing, "",
							linkage->target_id_type);
					NXGLOGV("%*s   original_network_id_flag : %s", spacing, "",
							linkage->original_network_id_flag ? "TRUE" : "FALSE");
					NXGLOGV("%*s   service_id_flag   : %s", spacing, "",
							linkage->service_id_flag ? "TRUE" : "FALSE");
					if (linkage->target_id_type == 3) {
						NXGLOGV("%*s   user_defined_id   : 0x%02x", spacing, "",
								linkage->user_defined_id);
					} else {
						if (linkage->target_id_type == 1)
							NXGLOGV("%*s   target_transport_stream_id : 0x%04x",
									spacing, "", linkage->target_transport_stream_id);
						if (linkage->original_network_id_flag)
							NXGLOGV("%*s   target_original_network_id : 0x%04x",
									spacing, "", linkage->target_original_network_id);
						if (linkage->service_id_flag)
							NXGLOGV("%*s   target_service_id          : 0x%04x",
									spacing, "", linkage->target_service_id);
					}
				}
				break;
			}
			default:
				break;
		}
		if (res->private_data_length > 0) {
			dump_memory_bytes (res->private_data_bytes, res->private_data_length,
					spacing + 2);
		}
		gst_mpegts_dvb_linkage_descriptor_free (res);
	}
}

static void
dump_component (GstMpegtsDescriptor * desc, guint spacing)
{
	GstMpegtsComponentDescriptor *res;

	if (gst_mpegts_descriptor_parse_dvb_component (desc, &res)) {
		NXGLOGV("%*s stream_content : 0x%02x (%s)", spacing, "",
				res->stream_content,
				enum_name (GST_TYPE_MPEGTS_COMPONENT_STREAM_CONTENT,
						res->stream_content));
		NXGLOGV("%*s component_type : 0x%02x", spacing, "",
				res->component_type);
		NXGLOGV("%*s component_tag  : 0x%02x", spacing, "", res->component_tag);
		NXGLOGV("%*s language_code  : %s", spacing, "", res->language_code);
		NXGLOGV("%*s text           : %s", spacing, "",
				res->text ? res->text : "NULL");
		gst_mpegts_dvb_component_descriptor_free (res);
	}
}

static void
dump_content (GstMpegtsDescriptor * desc, guint spacing)
{
	GPtrArray *contents;
	guint i;

	if (gst_mpegts_descriptor_parse_dvb_content (desc, &contents)) {
		for (i = 0; i < contents->len; i++) {
			GstMpegtsContent *item = g_ptr_array_index (contents, i);
			NXGLOGV("%*s content nibble 1 : 0x%01x (%s)", spacing, "",
					item->content_nibble_1,
					enum_name (GST_TYPE_MPEGTS_CONTENT_NIBBLE_HI,
							item->content_nibble_1));
			NXGLOGV("%*s content nibble 2 : 0x%01x", spacing, "",
					item->content_nibble_2);
			NXGLOGV("%*s user_byte        : 0x%02x", spacing, "",
					item->user_byte);
		}
		g_ptr_array_unref (contents);
	}
}

static void
dump_iso_639_language (GstMpegtsDescriptor * desc, guint spacing)
{
	guint i;
	GstMpegtsISO639LanguageDescriptor *res;

	if (gst_mpegts_descriptor_parse_iso_639_language (desc, &res)) {
		for (i = 0; i < res->nb_language; i++) {
			NXGLOGV
					("%*s ISO 639 Language Descriptor %s , audio_type:0x%x (%s)",
					spacing, "", res->language[i], res->audio_type[i],
					enum_name (GST_TYPE_MPEGTS_ISO639_AUDIO_TYPE, res->audio_type[i]));
		}
		gst_mpegts_iso_639_language_descriptor_free (res);
	}
}

static void
dump_dvb_extended_event (GstMpegtsDescriptor * desc, guint spacing)
{
	GstMpegtsExtendedEventDescriptor *res;

	if (gst_mpegts_descriptor_parse_dvb_extended_event (desc, &res)) {
		guint i;
		NXGLOGV("%*s DVB Extended Event", spacing, "");
		NXGLOGV("%*s   descriptor_number:%d, last_descriptor_number:%d",
				spacing, "", res->descriptor_number, res->last_descriptor_number);
		NXGLOGV("%*s   language_code:%s", spacing, "", res->language_code);
		NXGLOGV("%*s   text : %s", spacing, "", res->text);
		for (i = 0; i < res->items->len; i++) {
			GstMpegtsExtendedEventItem *item = g_ptr_array_index (res->items, i);
			NXGLOGV("%*s     #%d [description:item]  %s : %s",
					spacing, "", i, item->item_description, item->item);
		}
		gst_mpegts_extended_event_descriptor_free (res);
	}
}

static void
dump_descriptors (GPtrArray * descriptors, guint spacing)
{
	guint i;

	for (i = 0; i < descriptors->len; i++) {
		GstMpegtsDescriptor *desc = g_ptr_array_index (descriptors, i);
		NXGLOGV("%*s [descriptor 0x%02x (%s) length:%d]", spacing, "",
				desc->tag, descriptor_name (desc->tag), desc->length);
		if (DUMP_DESCRIPTORS)
			dump_memory_content (desc, spacing + 2);
		switch (desc->tag) {
			case GST_MTS_DESC_REGISTRATION:
			{
				const guint8 *data = desc->data + 2;
#define SAFE_CHAR(a) (g_ascii_isprint(a) ? a : '.')
				NXGLOGV("%*s   Registration : %c%c%c%c [%02x%02x%02x%02x]", spacing,
						"", SAFE_CHAR (data[0]), SAFE_CHAR (data[1]), SAFE_CHAR (data[2]),
						SAFE_CHAR (data[3]), data[0], data[1], data[2], data[3]);

				break;
			}
			case GST_MTS_DESC_CA:
			{
				guint16 ca_pid, ca_system_id;
				const guint8 *private_data;
				gsize private_data_size;
				if (gst_mpegts_descriptor_parse_ca (desc, &ca_system_id, &ca_pid,
								&private_data, &private_data_size)) {
					NXGLOGV("%*s   CA system id : 0x%04x", spacing, "", ca_system_id);
					NXGLOGV("%*s   CA PID       : 0x%04x", spacing, "", ca_pid);
					if (private_data_size) {
						NXGLOGV("%*s   Private Data :", spacing, "");
						dump_memory_bytes ((guint8 *) private_data, private_data_size,
								spacing + 2);
					}
				}
				break;
			}
			case GST_MTS_DESC_DVB_NETWORK_NAME:
			{
				gchar *network_name;
				if (gst_mpegts_descriptor_parse_dvb_network_name (desc, &network_name)) {
					NXGLOGV("%*s   Network Name : %s", spacing, "", network_name);
					g_free (network_name);
				}
				break;
			}
			case GST_MTS_DESC_DVB_SERVICE_LIST:
			{
				dump_dvb_service_list (desc, spacing + 2);
				break;
			}
			case GST_MTS_DESC_DVB_CABLE_DELIVERY_SYSTEM:
				dump_cable_delivery_descriptor (desc, spacing + 2);
				break;
			case GST_MTS_DESC_DVB_TERRESTRIAL_DELIVERY_SYSTEM:
				dump_terrestrial_delivery (desc, spacing + 2);
				break;
			case GST_MTS_DESC_DVB_BOUQUET_NAME:
			{
				gchar *bouquet_name;
				if (gst_mpegts_descriptor_parse_dvb_bouquet_name (desc, &bouquet_name)) {
					NXGLOGV("%*s   Bouquet Name Descriptor, bouquet_name:%s", spacing,
							"", bouquet_name);
					g_free (bouquet_name);
				}
				break;
			}
			case GST_MTS_DESC_DTG_LOGICAL_CHANNEL:
				dump_logical_channel_descriptor (desc, spacing + 2);
				break;
			case GST_MTS_DESC_DVB_SERVICE:
			{
				gchar *service_name, *provider_name;
				GstMpegtsDVBServiceType service_type;
				if (gst_mpegts_descriptor_parse_dvb_service (desc, &service_type,
								&service_name, &provider_name)) {
					NXGLOGV("%*s   Service Descriptor, type:0x%02x (%s)", spacing, "",
							service_type, enum_name (GST_TYPE_MPEGTS_DVB_SERVICE_TYPE,
									service_type));
					NXGLOGV("%*s      service_name  : %s", spacing, "", service_name);
					NXGLOGV("%*s      provider_name : %s", spacing, "",
							provider_name);
					g_free (service_name);
					g_free (provider_name);

				}
				break;
			}
			case GST_MTS_DESC_DVB_MULTILINGUAL_BOUQUET_NAME:
			{
				dump_multiligual_bouquet_name (desc, spacing + 2);
				break;
			}
			case GST_MTS_DESC_DVB_MULTILINGUAL_NETWORK_NAME:
			{
				dump_multiligual_network_name (desc, spacing + 2);
				break;
			}
			case GST_MTS_DESC_DVB_MULTILINGUAL_SERVICE_NAME:
			{
				dump_multiligual_service_name (desc, spacing + 2);
				break;
			}
			case GST_MTS_DESC_DVB_MULTILINGUAL_COMPONENT:
			{
				dump_multiligual_component (desc, spacing + 2);
				break;
			}
			case GST_MTS_DESC_DVB_PRIVATE_DATA_SPECIFIER:
			{
				guint32 specifier;
				guint8 len = 0, *data = NULL;

				if (gst_mpegts_descriptor_parse_dvb_private_data_specifier (desc,
								&specifier, &data, &len)) {
					NXGLOGV("%*s   private_data_specifier : 0x%08x", spacing, "",
							specifier);
					if (len > 0) {
						dump_memory_bytes (data, len, spacing + 2);
						g_free (data);
					}
				}
				break;
			}
			case GST_MTS_DESC_DVB_FREQUENCY_LIST:
			{
				gboolean offset;
				GArray *list;
				if (gst_mpegts_descriptor_parse_dvb_frequency_list (desc, &offset,
								&list)) {
					guint j;
					for (j = 0; j < list->len; j++) {
						guint32 freq = g_array_index (list, guint32, j);
						NXGLOGV("%*s   Frequency : %u %s", spacing, "", freq,
								offset ? "kHz" : "Hz");
					}
					g_array_unref (list);
				}
				break;
			}
			case GST_MTS_DESC_DVB_LINKAGE:
				dump_linkage (desc, spacing + 2);
				break;
			case GST_MTS_DESC_DVB_COMPONENT:
				dump_component (desc, spacing + 2);
				break;
			case GST_MTS_DESC_DVB_STREAM_IDENTIFIER:
			{
				guint8 tag;
				if (gst_mpegts_descriptor_parse_dvb_stream_identifier (desc, &tag)) {
					NXGLOGV("%*s   Component Tag : 0x%02x", spacing, "", tag);
				}
				break;
			}
			case GST_MTS_DESC_DVB_CA_IDENTIFIER:
			{
				GArray *list;
				guint j;
				guint16 ca_id;
				if (gst_mpegts_descriptor_parse_dvb_ca_identifier (desc, &list)) {
					for (j = 0; j < list->len; j++) {
						ca_id = g_array_index (list, guint16, j);
						NXGLOGV("%*s   CA Identifier : 0x%04x", spacing, "", ca_id);
					}
					g_array_unref (list);
				}
				break;
			}
			case GST_MTS_DESC_DVB_CONTENT:
				dump_content (desc, spacing + 2);
				break;
			case GST_MTS_DESC_DVB_PARENTAL_RATING:
			{
				GPtrArray *ratings;
				guint j;

				if (gst_mpegts_descriptor_parse_dvb_parental_rating (desc, &ratings)) {
					for (j = 0; j < ratings->len; j++) {
						GstMpegtsDVBParentalRatingItem *item =
								g_ptr_array_index (ratings, j);
						NXGLOGV("%*s   country_code : %s", spacing, "",
								item->country_code);
						NXGLOGV("%*s   rating age   : %d", spacing, "", item->rating);
					}
					g_ptr_array_unref (ratings);
				}
				break;
			}
			case GST_MTS_DESC_DVB_DATA_BROADCAST:
			{
				GstMpegtsDataBroadcastDescriptor *res;

				if (gst_mpegts_descriptor_parse_dvb_data_broadcast (desc, &res)) {
					NXGLOGV("%*s   data_broadcast_id : 0x%04x", spacing, "",
							res->data_broadcast_id);
					NXGLOGV("%*s   component_tag     : 0x%02x", spacing, "",
							res->component_tag);
					if (res->length > 0) {
						NXGLOGV("%*s   selector_bytes:", spacing, "");
						dump_memory_bytes (res->selector_bytes, res->length, spacing + 2);
					}
					NXGLOGV("%*s   text              : %s", spacing, "",
							res->text ? res->text : "NULL");
					gst_mpegts_dvb_data_broadcast_descriptor_free (res);
				}
				break;
			}
			case GST_MTS_DESC_ISO_639_LANGUAGE:
				dump_iso_639_language (desc, spacing + 2);
				break;
			case GST_MTS_DESC_DVB_SHORT_EVENT:
			{
				gchar *language_code, *event_name, *text;
				if (gst_mpegts_descriptor_parse_dvb_short_event (desc, &language_code,
								&event_name, &text)) {
					NXGLOGV("%*s   Short Event, language_code:%s", spacing, "",
							language_code);
					NXGLOGV("%*s     event_name : %s", spacing, "", event_name);
					NXGLOGV("%*s     text       : %s", spacing, "", text);
					g_free (language_code);
					g_free (event_name);
					g_free (text);
				}
			}
				break;
			case GST_MTS_DESC_DVB_EXTENDED_EVENT:
			{
				dump_dvb_extended_event (desc, spacing + 2);
				break;
			}
			case GST_MTS_DESC_DVB_SUBTITLING:
			{
				gchar *lang;
				guint8 type;
				guint16 composition;
				guint16 ancillary;
				guint j;

				for (j = 0;
						gst_mpegts_descriptor_parse_dvb_subtitling_idx (desc, j, &lang,
								&type, &composition, &ancillary); j++) {
					NXGLOGV("%*s   Subtitling, language_code:%s", spacing, "", lang);
					NXGLOGV("%*s      type                : %u", spacing, "", type);
					NXGLOGV("%*s      composition page id : %u", spacing, "",
							composition);
					NXGLOGV("%*s      ancillary page id   : %u", spacing, "",
							ancillary);
					g_free (lang);
				}
			}
				break;
			case GST_MTS_DESC_DVB_TELETEXT:
			{
				GstMpegtsDVBTeletextType type;
				gchar *lang;
				guint8 magazine, page_number;
				guint j;

				for (j = 0;
						gst_mpegts_descriptor_parse_dvb_teletext_idx (desc, j, &lang, &type,
								&magazine, &page_number); j++) {
					NXGLOGV("%*s   Teletext, type:0x%02x (%s)", spacing, "", type,
							enum_name (GST_TYPE_MPEGTS_DVB_TELETEXT_TYPE, type));
					NXGLOGV("%*s      language    : %s", spacing, "", lang);
					NXGLOGV("%*s      magazine    : %u", spacing, "", magazine);
					NXGLOGV("%*s      page number : %u", spacing, "", page_number);
					g_free (lang);
				}
			}
				break;
			default:
				break;
		}
	}
}

static void
dump_pat (GstMpegtsSection * section, MpegTsSt *handle)
{
	GPtrArray *pat = gst_mpegts_section_get_pat (section);
	guint i = 0, pIdx = 0, len = 0;

	// n_program
	len = pat->len;
	handle->media_info->n_program = pat->len;
	NXGLOGI("   %d program(s):", len);

	for (i = 0; i < len; i++) {
		GstMpegtsPatProgram *patp = g_ptr_array_index (pat, i);
		NXGLOGI("     program_number:%6d (0x%04x), network_or_program_map_PID:0x%04x",
				patp->program_number, patp->program_number,
				patp->network_or_program_map_PID);

		// Do not count n_program if it has reserved program info
		if (0 == patp->program_number) {
			if (handle->media_info->n_program >= 1) {
				handle->media_info->n_program--;
			}
			continue;
		}
		// program_number
		handle->media_info->program_number[pIdx] = patp->program_number;
		NXGLOGI(" n_program(%d), handle->media_info->program_number[%d] = %d",
				handle->media_info->n_program, pIdx, handle->media_info->program_number[pIdx]);
		pIdx++;
	}

	g_ptr_array_unref (pat);
}

static void
dump_pmt (GstMpegtsSection * section, MpegTsSt *handle)
{
	const GstMpegtsPMT *pmt = gst_mpegts_section_get_pmt (section);
	guint i, len;

	NXGLOGI("     program_number : 0x%04x", section->subtable_extension);
	NXGLOGI("     pcr_pid        : 0x%04x", pmt->pcr_pid);
	dump_descriptors (pmt->descriptors, 7);
	len = pmt->streams->len;
	NXGLOGI("     %d Streams:", len);

	for (i = 0; i < len; i++) {
		GstMpegtsPMTStream *stream = g_ptr_array_index (pmt->streams, i);
		const gchar* stream_type = stream_type_name (stream->stream_type);
		NXGLOGI("       pid:0x%04x , stream_type:0x%02x (%s)", stream->pid,
				stream->stream_type, stream_type);
		dump_descriptors (stream->descriptors, 9);
	}
}

static void
dump_eit (GstMpegtsSection * section)
{
	const GstMpegtsEIT *eit = gst_mpegts_section_get_eit (section);
	guint i, len;

	g_assert (eit);

	NXGLOGV("     service_id          : 0x%04x", section->subtable_extension);
	NXGLOGV("     transport_stream_id : 0x%04x", eit->transport_stream_id);
	NXGLOGV("     original_network_id : 0x%04x", eit->original_network_id);
	NXGLOGV("     segment_last_section_number:0x%02x, last_table_id:0x%02x",
			eit->segment_last_section_number, eit->last_table_id);
	NXGLOGV("     actual_stream : %s, present_following : %s",
			eit->actual_stream ? "TRUE" : "FALSE",
			eit->present_following ? "TRUE" : "FALSE");

	len = eit->events->len;
	NXGLOGV("     %d Event(s):", len);
	for (i = 0; i < len; i++) {
		gchar *tmp = (gchar *) "<NO TIME>";
		GstMpegtsEITEvent *event = g_ptr_array_index (eit->events, i);

		if (event->start_time)
			tmp = gst_date_time_to_iso8601_string (event->start_time);
		NXGLOGV("       event_id:0x%04x, start_time:%s, duration:%"
				GST_TIME_FORMAT "", event->event_id, tmp,
				GST_TIME_ARGS (event->duration * GST_SECOND));
		NXGLOGV("       running_status:0x%02x (%s), free_CA_mode:%d (%s)",
				event->running_status, enum_name (GST_TYPE_MPEGTS_RUNNING_STATUS,
						event->running_status), event->free_CA_mode,
				event->free_CA_mode ? "MAYBE SCRAMBLED" : "NOT SCRAMBLED");
		if (event->start_time)
			g_free (tmp);
		dump_descriptors (event->descriptors, 9);
	}
}

static void
dump_atsc_mult_string (GPtrArray * mstrings, guint spacing)
{
	guint i;

	for (i = 0; i < mstrings->len; i++) {
		GstMpegtsAtscMultString *mstring = g_ptr_array_index (mstrings, i);
		gint j, n;

		n = mstring->segments->len;

		NXGLOGV("%*s [multstring entry (%d) iso_639 langcode: %s]", spacing, "",
				i, mstring->iso_639_langcode);
		NXGLOGV("%*s   segments:%d", spacing, "", n);
		for (j = 0; j < n; j++) {
			GstMpegtsAtscStringSegment *segment =
					g_ptr_array_index (mstring->segments, j);

			NXGLOGV("%*s    Compression:0x%x", spacing, "",
					segment->compression_type);
			NXGLOGV("%*s    Mode:0x%x", spacing, "", segment->mode);
			NXGLOGV("%*s    Len:%u", spacing, "", segment->compressed_data_size);
			NXGLOGV("%*s    %s", spacing, "",
					gst_mpegts_atsc_string_segment_get_string (segment));
		}
	}
}

static void
dump_atsc_eit (GstMpegtsSection * section)
{
	const GstMpegtsAtscEIT *eit = gst_mpegts_section_get_atsc_eit (section);
	guint i, len;

	g_assert (eit);

	NXGLOGV("     event_id            : 0x%04x", eit->source_id);
	NXGLOGV("     protocol_version    : %u", eit->protocol_version);

	len = eit->events->len;
	NXGLOGV("     %d Event(s):", len);
	for (i = 0; i < len; i++) {
		GstMpegtsAtscEITEvent *event = g_ptr_array_index (eit->events, i);

		NXGLOGV("     %d)", i);
		NXGLOGV("       event_id: 0x%04x", event->event_id);
		NXGLOGV("       start_time: %u", event->start_time);
		NXGLOGV("       etm_location: 0x%x", event->etm_location);
		NXGLOGV("       length_in_seconds: %u", event->length_in_seconds);
		NXGLOGV("       Title(s):");
		dump_atsc_mult_string (event->titles, 9);
		dump_descriptors (event->descriptors, 9);
	}
}

static void
dump_ett (GstMpegtsSection * section)
{
	const GstMpegtsAtscETT *ett = gst_mpegts_section_get_atsc_ett (section);
	guint len;

	g_assert (ett);

	NXGLOGV("     ett_table_id_ext    : 0x%04x", ett->ett_table_id_extension);
	NXGLOGV("     protocol_version    : 0x%04x", ett->protocol_version);
	NXGLOGV("     etm_id              : 0x%04x", ett->etm_id);

	len = ett->messages->len;
	NXGLOGV("     %d Messages(s):", len);
	dump_atsc_mult_string (ett->messages, 9);
}

static void
dump_stt (GstMpegtsSection * section)
{
	const GstMpegtsAtscSTT *stt = gst_mpegts_section_get_atsc_stt (section);
	GstDateTime *dt;
	gchar *dt_str = NULL;

	g_assert (stt);

	dt = gst_mpegts_atsc_stt_get_datetime_utc ((GstMpegtsAtscSTT *) stt);
	if (dt)
		dt_str = gst_date_time_to_iso8601_string (dt);

	NXGLOGV("     protocol_version    : 0x%04x", stt->protocol_version);
	NXGLOGV("     system_time         : 0x%08x", stt->system_time);
	NXGLOGV("     gps_utc_offset      : %d", stt->gps_utc_offset);
	NXGLOGV("     daylight saving     : %d day:%d hour:%d", stt->ds_status,
			stt->ds_dayofmonth, stt->ds_hour);
	NXGLOGV("     utc datetime        : %s", dt_str);

	g_free (dt_str);
	gst_date_time_unref (dt);
}

static void
dump_nit (GstMpegtsSection * section)
{
	const GstMpegtsNIT *nit = gst_mpegts_section_get_nit (section);
	guint i, len;

	g_assert (nit);

	NXGLOGV("     network_id     : 0x%04x", section->subtable_extension);
	NXGLOGV("     actual_network : %s",
			nit->actual_network ? "TRUE" : "FALSE");
	dump_descriptors (nit->descriptors, 7);
	len = nit->streams->len;
	NXGLOGV("     %d Streams:", len);
	for (i = 0; i < len; i++) {
		GstMpegtsNITStream *stream = g_ptr_array_index (nit->streams, i);
		NXGLOGV
				("       transport_stream_id:0x%04x , original_network_id:0x%02x",
				stream->transport_stream_id, stream->original_network_id);
		dump_descriptors (stream->descriptors, 9);
	}
}

static void
dump_bat (GstMpegtsSection * section)
{
	const GstMpegtsBAT *bat = gst_mpegts_section_get_bat (section);
	guint i, len;

	g_assert (bat);

	NXGLOGV("     bouquet_id     : 0x%04x", section->subtable_extension);
	dump_descriptors (bat->descriptors, 7);
	len = bat->streams->len;
	NXGLOGV("     %d Streams:", len);
	for (i = 0; i < len; i++) {
		GstMpegtsBATStream *stream = g_ptr_array_index (bat->streams, i);
		NXGLOGV
				("       transport_stream_id:0x%04x , original_network_id:0x%02x",
				stream->transport_stream_id, stream->original_network_id);
		dump_descriptors (stream->descriptors, 9);
	}
}

static void
dump_sdt (GstMpegtsSection * section)
{
	const GstMpegtsSDT *sdt = gst_mpegts_section_get_sdt (section);
	guint i, len;

	g_assert (sdt);

	NXGLOGV("     original_network_id : 0x%04x", sdt->original_network_id);
	NXGLOGV("     actual_ts           : %s",
			sdt->actual_ts ? "TRUE" : "FALSE");
	len = sdt->services->len;
	NXGLOGV("     %d Services:", len);
	for (i = 0; i < len; i++) {
		GstMpegtsSDTService *service = g_ptr_array_index (sdt->services, i);
		NXGLOGV
				("       service_id:0x%04x, EIT_schedule_flag:%d, EIT_present_following_flag:%d",
				service->service_id, service->EIT_schedule_flag,
				service->EIT_present_following_flag);
		NXGLOGV
				("       running_status:0x%02x (%s), free_CA_mode:%d (%s)",
				service->running_status,
				enum_name (GST_TYPE_MPEGTS_RUNNING_STATUS, service->running_status),
				service->free_CA_mode,
				service->free_CA_mode ? "MAYBE SCRAMBLED" : "NOT SCRAMBLED");
		dump_descriptors (service->descriptors, 9);
	}
}

static void
dump_tdt (GstMpegtsSection * section)
{
	GstDateTime *date = gst_mpegts_section_get_tdt (section);

	if (date) {
		gchar *str = gst_date_time_to_iso8601_string (date);
		NXGLOGV("     utc_time : %s", str);
		g_free (str);
		gst_date_time_unref (date);
	} else {
		NXGLOGV("     No utc_time present");
	}
}

static void
dump_tot (GstMpegtsSection * section)
{
	const GstMpegtsTOT *tot = gst_mpegts_section_get_tot (section);
	gchar *str = gst_date_time_to_iso8601_string (tot->utc_time);

	NXGLOGV("     utc_time : %s", str);
	dump_descriptors (tot->descriptors, 7);
	g_free (str);
}

static void
dump_mgt (GstMpegtsSection * section)
{
	const GstMpegtsAtscMGT *mgt = gst_mpegts_section_get_atsc_mgt (section);
	gint i;

	NXGLOGV("     protocol_version    : %u", mgt->protocol_version);
	NXGLOGV("     tables number       : %d", mgt->tables->len);
	for (i = 0; i < mgt->tables->len; i++) {
		GstMpegtsAtscMGTTable *table = g_ptr_array_index (mgt->tables, i);
		NXGLOGV("     table %d)", i);
		NXGLOGV("       table_type    : %u", table->table_type);
		NXGLOGV("       pid           : 0x%x", table->pid);
		NXGLOGV("       version_number: %u", table->version_number);
		NXGLOGV("       number_bytes  : %u", table->number_bytes);
		dump_descriptors (table->descriptors, 9);
	}
	dump_descriptors (mgt->descriptors, 7);
}

static void
dump_vct (GstMpegtsSection * section)
{
	const GstMpegtsAtscVCT *vct;
	gint i;

	if (GST_MPEGTS_SECTION_TYPE (section) == GST_MPEGTS_SECTION_ATSC_CVCT) {
		vct = gst_mpegts_section_get_atsc_cvct (section);
	} else {
		/* GST_MPEGTS_SECTION_ATSC_TVCT */
		vct = gst_mpegts_section_get_atsc_tvct (section);
	}

	g_assert (vct);

	NXGLOGV("     transport_stream_id : 0x%04x", vct->transport_stream_id);
	NXGLOGV("     protocol_version    : %u", vct->protocol_version);
	NXGLOGV("     %d Sources:", vct->sources->len);
	for (i = 0; i < vct->sources->len; i++) {
		GstMpegtsAtscVCTSource *source = g_ptr_array_index (vct->sources, i);
		NXGLOGV("       short_name: %s", source->short_name);
		NXGLOGV("       major_channel_number: %u, minor_channel_number: %u",
				source->major_channel_number, source->minor_channel_number);
		NXGLOGV("       modulation_mode: %u", source->modulation_mode);
		NXGLOGV("       carrier_frequency: %u", source->carrier_frequency);
		NXGLOGV("       channel_tsid: %u", source->channel_TSID);
		NXGLOGV("       program_number: %u", source->program_number);
		NXGLOGV("       ETM_location: %u", source->ETM_location);
		NXGLOGV("       access_controlled: %u", source->access_controlled);
		NXGLOGV("       hidden: %u", source->hidden);
		if (section->table_id == GST_MPEGTS_SECTION_ATSC_CVCT) {
			NXGLOGV("       path_select: %u", source->path_select);
			NXGLOGV("       out_of_band: %u", source->out_of_band);
		}
		NXGLOGV("       hide_guide: %u", source->hide_guide);
		NXGLOGV("       service_type: %u", source->service_type);
		NXGLOGV("       source_id: %u", source->source_id);

		dump_descriptors (source->descriptors, 9);
	}
	dump_descriptors (vct->descriptors, 7);
}

static void
dump_cat (GstMpegtsSection * section)
{
	GPtrArray *descriptors;

	descriptors = gst_mpegts_section_get_cat (section);
	g_assert (descriptors);
	dump_descriptors (descriptors, 7);
	g_ptr_array_unref (descriptors);
}

static const gchar *
scte_descriptor_name (guint8 tag)
{
	switch (tag) {
		case 0x00:
			return "avail";
		case 0x01:
			return "DTMF";
		case 0x02:
			return "segmentation";
		case 0x03:
			return "time";
		case 0x04:
			return "audio";
		default:
			return "UNKNOWN";
	}
}

static void
dump_scte_descriptors (GPtrArray * descriptors, guint spacing)
{
	guint i;

	for (i = 0; i < descriptors->len; i++) {
		GstMpegtsDescriptor *desc = g_ptr_array_index (descriptors, i);
		NXGLOGV("%*s [scte descriptor 0x%02x (%s) length:%d]", spacing, "",
				desc->tag, scte_descriptor_name (desc->tag), desc->length);
		if (DUMP_DESCRIPTORS)
			dump_memory_content (desc, spacing + 2);
		/* FIXME : Add parsing of SCTE descriptors */
	}
}

static void
dump_section (GstMpegtsSection * section, MpegTsSt *handle)
{
	switch (GST_MPEGTS_SECTION_TYPE (section)) {
		case GST_MPEGTS_SECTION_PAT:
			dump_pat (section, handle);
			break;
		case GST_MPEGTS_SECTION_PMT:
			dump_pmt (section, handle);
			break;
		case GST_MPEGTS_SECTION_CAT:
			dump_cat (section);
			break;
		case GST_MPEGTS_SECTION_TDT:
			dump_tdt (section);
			break;
		case GST_MPEGTS_SECTION_TOT:
			dump_tot (section);
			break;
		case GST_MPEGTS_SECTION_SDT:
			dump_sdt (section);
			break;
		case GST_MPEGTS_SECTION_NIT:
			dump_nit (section);
			break;
		case GST_MPEGTS_SECTION_BAT:
			dump_bat (section);
			break;
		case GST_MPEGTS_SECTION_EIT:
			dump_eit (section);
			break;
		case GST_MPEGTS_SECTION_ATSC_MGT:
			dump_mgt (section);
			break;
		case GST_MPEGTS_SECTION_ATSC_CVCT:
		case GST_MPEGTS_SECTION_ATSC_TVCT:
			dump_vct (section);
			break;
		case GST_MPEGTS_SECTION_ATSC_EIT:
			dump_atsc_eit (section);
			break;
		case GST_MPEGTS_SECTION_ATSC_ETT:
			dump_ett (section);
			break;
		case GST_MPEGTS_SECTION_ATSC_STT:
			dump_stt (section);
			break;
		default:
			NXGLOGI("     Unknown section type");
			break;
	}
}

static void
dump_collection (GstStreamCollection * collection, MpegTsSt *handle)
{
	guint i;
	GstTagList *tags;
	GstCaps *caps;
	GstStructure *structure;
	gint cur_pro_idx = handle->program_index;
	if (-1 == cur_pro_idx) {
		NXGLOGE("No matched program number");
		return;
	}

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
			gst_tag_list_foreach (tags, print_tag_foreach, GUINT_TO_POINTER (3));
		}

		NXGLOGI("MIME-type (%s)", mime_type);

		if (stype & GST_STREAM_TYPE_AUDIO)
		{
			gint audio_mpegversion, channels, samplerate;
			AUDIO_TYPE audio_type = get_audio_codec_type(mime_type);
			gchar* lang = NULL;
			gst_tag_list_get_string (tags, GST_TAG_LANGUAGE_CODE, &lang);
			int32_t a_idx = handle->media_info->ProgramInfo[cur_pro_idx].n_audio;

			handle->media_info->ProgramInfo[cur_pro_idx].AudioInfo[a_idx].type = audio_type;
			if (gst_structure_get_int (structure, "mpegversion", &audio_mpegversion))
			{
				if (audio_mpegversion == 1) {
					handle->media_info->ProgramInfo[cur_pro_idx].AudioInfo[a_idx].type = AUDIO_TYPE_MPEG_V1;
				} else if (audio_mpegversion == 2) {
					handle->media_info->ProgramInfo[cur_pro_idx].AudioInfo[a_idx].type = AUDIO_TYPE_MPEG_V2;
				}
			}
			handle->media_info->ProgramInfo[cur_pro_idx].AudioInfo[a_idx].language_code = g_strdup(lang);
			handle->media_info->ProgramInfo[cur_pro_idx].AudioInfo[a_idx].stream_id = g_strdup(stream_id);
			handle->media_info->ProgramInfo[cur_pro_idx].n_audio++;
			NXGLOGI("n_audio(%d), audio type(%d), languague_code(%s), stream_id(%s)",
					handle->media_info->ProgramInfo[cur_pro_idx].n_audio,
					handle->media_info->ProgramInfo[cur_pro_idx].AudioInfo[a_idx].type,
					(lang ? lang:""), (stream_id ? stream_id:""));
		}
		else if (stype & GST_STREAM_TYPE_VIDEO)
		{
			gint video_mpegversion, num, den = 0;
			VIDEO_TYPE video_type = get_video_codec_type(mime_type);
			int32_t v_idx = handle->media_info->ProgramInfo[cur_pro_idx].n_video;

			handle->media_info->ProgramInfo[cur_pro_idx].VideoInfo[v_idx].type = video_type;
			if ((structure != NULL) && (video_type == VIDEO_TYPE_MPEG_V4))
			{
				gst_structure_get_int (structure, "mpegversion", &video_mpegversion);
				if (video_mpegversion == 1) {
					handle->media_info->ProgramInfo[cur_pro_idx].VideoInfo[v_idx].type = VIDEO_TYPE_MPEG_V1;
				} else if (video_mpegversion == 2) {
					handle->media_info->ProgramInfo[cur_pro_idx].VideoInfo[v_idx].type = VIDEO_TYPE_MPEG_V2;
				}
			}
			handle->media_info->ProgramInfo[cur_pro_idx].VideoInfo[v_idx].stream_id = g_strdup(stream_id);
			handle->media_info->ProgramInfo[cur_pro_idx].n_video++;

			NXGLOGI("n_video(%d), video type(%d) stream_id(%s)",
					handle->media_info->ProgramInfo[cur_pro_idx].n_video,
					handle->media_info->ProgramInfo[cur_pro_idx].VideoInfo[v_idx].type,
					(stream_id ? stream_id:""));
		}
		else if (stype & GST_STREAM_TYPE_TEXT)
		{
			SUBTITLE_TYPE sub_type = get_subtitle_codec_type(mime_type);
			gchar* lang = NULL;
			gst_tag_list_get_string (tags, GST_TAG_LANGUAGE_CODE, &lang);
			int32_t sub_idx = handle->media_info->ProgramInfo[cur_pro_idx].n_subtitle;

			handle->media_info->ProgramInfo[cur_pro_idx].SubtitleInfo[sub_idx].type = sub_type;
			handle->media_info->ProgramInfo[cur_pro_idx].SubtitleInfo[sub_idx].language_code = g_strdup(lang);
			handle->media_info->ProgramInfo[cur_pro_idx].SubtitleInfo[sub_idx].stream_id = g_strdup(stream_id);
			handle->media_info->ProgramInfo[cur_pro_idx].n_subtitle++;

			NXGLOGI("n_subtitle(%d), subtitle_type(%d), language_code(%s), stream_id(%s)",
                handle->media_info->ProgramInfo[cur_pro_idx].n_subtitle,
                handle->media_info->ProgramInfo[cur_pro_idx].SubtitleInfo[sub_idx].type,
                handle->media_info->ProgramInfo[cur_pro_idx].SubtitleInfo[sub_idx].language_code,
				(stream_id ? stream_id:""));
		}

		if (tags) {
			gst_tag_list_unref (tags);
		}
	}
}

static void
on_bus_message_program (GstBus * bus, GstMessage * message, MpegTsSt *handle)
{
	//NXGLOGV("Got message %s", GST_MESSAGE_TYPE_NAME (message));

	switch (GST_MESSAGE_TYPE (message)) {
		case GST_MESSAGE_ERROR:
		case GST_MESSAGE_EOS:
			g_main_loop_quit (handle->loop);
			break;
		case GST_MESSAGE_ELEMENT:
		{
			GstMpegtsSection *section;
			if ((section = gst_message_parse_mpegts_section(message))) {
				GstMpegtsSectionType section_type = GST_MPEGTS_SECTION_TYPE(section);
				if (GST_MPEGTS_SECTION_PAT == section_type) {
					dump_pat(section, handle);
					gst_mpegts_section_unref(section);
					g_main_loop_quit(handle->loop);
				}
			}
			break;
		}
        case GST_MESSAGE_STATE_CHANGED:
        {
            GstState old_state, new_state;

            gst_message_parse_state_changed (message, &old_state, &new_state, NULL);
			if(g_strcmp0("pipe", GST_OBJECT_NAME (message->src)) == 0) {
            	NXGLOGI("Element '%s' changed state from  '%s' to '%s'"
						, GST_OBJECT_NAME (message->src)
						, gst_element_state_get_name (old_state)
						, gst_element_state_get_name (new_state));
			}
            break;
        }
		default:
			break;
	}
}

static void
on_bus_message_simple (GstBus * bus, GstMessage * message, MpegTsSt *handle)
{
	NXGLOGV("Got message %s", GST_MESSAGE_TYPE_NAME (message));

	switch (GST_MESSAGE_TYPE (message)) {
		case GST_MESSAGE_ERROR:
		case GST_MESSAGE_EOS:
			g_main_loop_quit (handle->loop);
			break;
		case GST_MESSAGE_STREAM_COLLECTION:
		{
			GstStreamCollection *collection = NULL;
			GstObject *src = GST_MESSAGE_SRC(message);

			gst_message_parse_stream_collection(message, &collection);
			if (collection)
			{
				NXGLOGI("Got a collection from %s",
						src ? GST_OBJECT_NAME (src) : "Unknown");
				dump_collection(collection, handle);
				gst_object_unref (collection);
				g_main_loop_quit (handle->loop);
				NXGLOGI("exit simple bus loop");
			}
			break;
		}
		default:
			break;
	}
}

static void
on_bus_message_detail (GstBus * bus, GstMessage * message, MpegTsSt *handle)
{
	NXGLOGV("Got message %s", GST_MESSAGE_TYPE_NAME (message));

	switch (GST_MESSAGE_TYPE (message)) {
		case GST_MESSAGE_ERROR:
		case GST_MESSAGE_EOS:
			g_main_loop_quit (handle->loop);
			break;
		default:
			break;
	}
}

static void
demux_pad_added(GstElement *element, GstPad *pad, gpointer data)
{
	gchar *name;
	GstCaps *caps;
	GstStructure *structure;
	GstElement *target_sink;
	GstPad *target_sink_pad;

	MpegTsSt * handle = (MpegTsSt *)data;
	name = gst_pad_get_name(pad);
	NXGLOGV("A new pad %s was created for %s", name, gst_element_get_name(element));
	g_free(name);

	caps = gst_pad_get_current_caps(pad);
	if (caps == NULL) {
		NXGLOGE("Failed to get current caps");
		return;
	}

	structure = gst_caps_get_structure(caps, 0);
	if (structure == NULL) {
		NXGLOGE("Failed to get current caps");
		gst_caps_unref (caps);
		return;
	}

	const char *mime_type = gst_structure_get_name(structure);
	if (g_str_has_prefix(mime_type, "video"))
	{
		target_sink = handle->video_queue;
		target_sink_pad = gst_element_get_static_pad(target_sink, "sink");
		if (!gst_pad_is_linked (target_sink_pad))
		{
			GstPadLinkReturn ret = gst_pad_link(pad, target_sink_pad);
			NXGLOGI("   ==> %s to link %s:%s to %s:%s",
						(ret != GST_PAD_LINK_OK) ? "Failed":"Succeed",
						GST_DEBUG_PAD_NAME(pad),
						GST_DEBUG_PAD_NAME(target_sink_pad));
		}
		gst_object_unref (target_sink_pad);
	}
	else if (g_str_has_prefix(mime_type, "audio"))
	{
		// Do not link pad. It's enough to get stream info from dump_collection() with video
	}
	gst_caps_unref (caps);
}


static void
decodebin_pad_added(GstElement *element, GstPad *pad, gpointer data)
{
	gchar *name;
	GstCaps *caps;
	GstStructure *structure;
	GstElement *target_sink;
	GstPad *target_sink_pad;
	MpegTsSt * handle = (MpegTsSt *)data;

	name = gst_pad_get_name(pad);
	NXGLOGV("A new pad %s was created for %s\n", name, gst_element_get_name(element));
	g_free(name);

	caps = gst_pad_get_current_caps(pad);
	if (caps == NULL) {
		NXGLOGE("Failed to get current caps");
		return;
	}

	structure = gst_caps_get_structure(caps, 0);
	if (structure == NULL) {
		NXGLOGE("Failed to get current caps");
		gst_caps_unref (caps);
		return;
	}

	const char *mime_type = gst_structure_get_name(structure);
	NXGLOGV("MIME-type:%s", mime_type);
	if (g_str_has_prefix(mime_type, "video/"))
	{
		gint video_mpegversion, width, height, num, den;

		target_sink = handle->typefind;
		target_sink_pad = gst_element_get_static_pad(target_sink, "sink");
		GstPadLinkReturn ret = gst_pad_link(pad, target_sink_pad);
		NXGLOGI("  ==> %s to link %s:%s to %s:%s",
						(ret != GST_PAD_LINK_OK) ? "Failed":"Succeed",
						GST_DEBUG_PAD_NAME(pad),
						GST_DEBUG_PAD_NAME(target_sink_pad));
		gst_object_unref (target_sink_pad);
	}
	gst_caps_unref (caps);
}

static void
demux_video_pad_added_detail (GstElement *element, GstPad *pad, gpointer data)
{
	gchar *name;
	GstCaps *caps;
	GstStructure *structure;
	GstElement *target_sink;
	GstPad *target_sink_pad;

	MpegTsSt *handle = (MpegTsSt *)data;

	name = gst_pad_get_name(pad);
	NXGLOGV("A new pad %s was created for %s\n", name, gst_element_get_name(element));
	g_free(name);

	caps = gst_pad_get_current_caps(pad);
	if (caps == NULL) {
		NXGLOGE("Failed to get current caps");
		return;
	}

	structure = gst_caps_get_structure(caps, 0);
	if (structure == NULL) {
		NXGLOGE("Failed to get current caps");
		gst_caps_unref (caps);
		return;
	}

	const char *mime_type = gst_structure_get_name(structure);
	int pIdx = handle->program_index;

	if (g_str_has_prefix(mime_type, "video"))
	{
		NXGLOGV("handle->select_video_idx(%d) handle->pad_added_video_num(%d)",
				handle->select_video_idx, handle->pad_added_video_num);

		if (handle->select_video_idx != handle->pad_added_video_num) {
			gst_caps_unref (caps);
			handle->pad_added_video_num++;
			return;
		}

		if (handle->select_video_idx == handle->pad_added_video_num)
		{
			target_sink = handle->video_queue;

			target_sink_pad = gst_element_get_static_pad(target_sink, "sink");
			GstPadLinkReturn ret = gst_pad_link(pad, target_sink_pad);
			NXGLOGI("%s to link %s:%s to %s:%s",
						(ret != GST_PAD_LINK_OK) ? "Failed":"Succeed",
						GST_DEBUG_PAD_NAME(pad),
						GST_DEBUG_PAD_NAME(target_sink_pad));
			gst_object_unref (target_sink_pad);
		}
	}
	gst_caps_unref (caps);
}

static void
demux_audio_pad_added_detail (GstElement *element, GstPad *pad, gpointer data)
{
	gchar *name;
	GstCaps *caps;
	GstStructure *structure;
	GstElement *target_sink;
	GstPad *target_sink_pad;

	MpegTsSt *handle = (MpegTsSt *)data;

	name = gst_pad_get_name(pad);
	NXGLOGV("A new pad %s was created for %s\n", name, gst_element_get_name(element));
	g_free(name);

	caps = gst_pad_get_current_caps(pad);
	if (caps == NULL) {
		NXGLOGE("Failed to get current caps");
		return;
	}

	structure = gst_caps_get_structure(caps, 0);
	if (structure == NULL) {
		NXGLOGE("Failed to get current caps");
		gst_caps_unref (caps);
		return;
	}

	const char *mime_type = gst_structure_get_name(structure);
	if (g_str_has_prefix(mime_type, "audio"))
	{
		NXGLOGV("handle->select_audio_idx(%d) handle->pad_added_audio_num(%d)",
				handle->select_audio_idx, handle->pad_added_audio_num);

		if (handle->select_audio_idx == handle->pad_added_audio_num)
		{
			target_sink = handle->audio_queue;
			target_sink_pad = gst_element_get_static_pad(target_sink, "sink");
			GstPadLinkReturn ret = gst_pad_link(pad, target_sink_pad);
			NXGLOGI("  ==> %s to link %s:%s to %s:%s",
						(ret != GST_PAD_LINK_OK) ? "Failed":"Succeed",
						GST_DEBUG_PAD_NAME(pad),
						GST_DEBUG_PAD_NAME(target_sink_pad));
			gst_object_unref (target_sink_pad);
		}
		handle->pad_added_audio_num++;
	}
	gst_caps_unref (caps);
}

static void
decodebin_video_pad_added_detail(GstElement *element, GstPad *pad, gpointer data)
{
	gchar *name;
	GstCaps *caps;
	GstStructure *structure;
	GstElement *target_sink;
	GstPad *target_sink_pad;
	MpegTsSt * handle = (MpegTsSt *)data;
	gint cur_pro_idx = handle->program_index;

	FUNC_IN();

	if (-1 == cur_pro_idx) {
		NXGLOGE("Not found matched program idx");
		return;
	}

	name = gst_pad_get_name(pad);
	NXGLOGV("A new pad %s was created for %s", name, gst_element_get_name(element));
	g_free(name);

	caps = gst_pad_get_current_caps(pad);
	if (caps == NULL) {
		NXGLOGE("Failed to get current caps");
		return;
	}

	structure = gst_caps_get_structure(caps, 0);
	if (structure == NULL) {
		NXGLOGE("Failed to get current caps");
		gst_caps_unref (caps);
		return;
	}

	const char *mime_type = gst_structure_get_name(structure);
	NXGLOGV("MIME-type:%s", mime_type);
	if (g_str_has_prefix(mime_type, "video"))
	{
		// Get video detail info
		gint video_mpegversion, width, height, num, den;
		int32_t n_video = handle->media_info->ProgramInfo[cur_pro_idx].n_video;
		int32_t vIdx = handle->select_video_idx;
		if (gst_structure_get_int(structure, "width", &width) &&
			gst_structure_get_int(structure, "height", &height)) {
			handle->media_info->ProgramInfo[cur_pro_idx].VideoInfo[vIdx].width = width;
			handle->media_info->ProgramInfo[cur_pro_idx].VideoInfo[vIdx].height = height;
		}

		if (gst_structure_get_fraction(structure, "framerate", &num, &den)) {
			handle->media_info->ProgramInfo[cur_pro_idx].VideoInfo[vIdx].framerate_num = num;
			handle->media_info->ProgramInfo[cur_pro_idx].VideoInfo[vIdx].framerate_denom = den;
		}

		// Link the last video pad
		target_sink = handle->typefind;
		target_sink_pad = gst_element_get_static_pad(target_sink, "sink");
		GstPadLinkReturn ret = gst_pad_link(pad, target_sink_pad);
		NXGLOGI(" ==> %s to link %s:%s to %s:%s",
				(ret != GST_PAD_LINK_OK) ? "Failed":"Succeed",
				GST_DEBUG_PAD_NAME(pad),
				GST_DEBUG_PAD_NAME(target_sink_pad));
		gst_object_unref (target_sink_pad);
	}

	gst_caps_unref (caps);

	FUNC_OUT();
}

static void
decodebin_audio_pad_added_detail(GstElement *element, GstPad *pad, gpointer data)
{
	gchar *name;
	GstCaps *caps;
	GstStructure *structure;
	GstElement *target_sink;
	GstPad *target_sink_pad;
	MpegTsSt * handle = (MpegTsSt *)data;
	gint cur_pro_idx = handle->program_index;

	FUNC_IN();

	if (-1 == cur_pro_idx) {
		NXGLOGE("Not found matched program idx");
		return;
	}

	name = gst_pad_get_name(pad);
	NXGLOGV("A new pad %s was created for %s\n", name, gst_element_get_name(element));
	g_free(name);

	caps = gst_pad_get_current_caps(pad);
	if (caps == NULL) {
		NXGLOGE("Failed to get current caps");
		return;
	}

	structure = gst_caps_get_structure(caps, 0);
	if (structure == NULL) {
		NXGLOGE("Failed to get current caps");
		gst_caps_unref (caps);
		return;
	}

	const char *mime_type = gst_structure_get_name(structure);
	NXGLOGI("MIME-type:%s", mime_type);
	if (g_str_has_prefix(mime_type, "audio"))
	{
		NXGLOGV("handle->select_audio_idx(%d)", handle->select_audio_idx);

		// Get audio detail information
		gint audio_mpegversion, channels, samplerate;

		int32_t n_audio = handle->media_info->ProgramInfo[cur_pro_idx].n_audio;
		int32_t aIdx = handle->select_audio_idx;
		AUDIO_TYPE audio_type = get_audio_codec_type(mime_type);
		handle->media_info->ProgramInfo[cur_pro_idx].AudioInfo[0].type = audio_type;
		if (gst_structure_get_int (structure, "mpegversion", &audio_mpegversion))
		{
			NXGLOGI("audio_mpegversion(%d)", audio_mpegversion);
			if (audio_mpegversion == 1) {
				handle->media_info->ProgramInfo[cur_pro_idx].AudioInfo[aIdx].type = AUDIO_TYPE_MPEG_V1;
			} else if (audio_mpegversion == 2) {
				handle->media_info->ProgramInfo[cur_pro_idx].AudioInfo[aIdx].type = AUDIO_TYPE_MPEG_V2;
			}
		}
		if (gst_structure_get_int(structure, "channels", &channels)) {
			handle->media_info->ProgramInfo[cur_pro_idx].AudioInfo[aIdx].n_channels = channels;
		}
		if (gst_structure_get_int(structure, "rate", &samplerate)) {
			handle->media_info->ProgramInfo[cur_pro_idx].AudioInfo[aIdx].samplerate = samplerate;
		}

		NXGLOGI("n_channels(%d), samplerate(%d), type(%d)",
				channels, samplerate,
				handle->media_info->ProgramInfo[cur_pro_idx].AudioInfo[aIdx].type);

		if (g_strcmp0(gst_element_get_name(element), "audio_decodebin") == 0){
			target_sink = handle->audio_typefind;
		}

		target_sink_pad = gst_element_get_static_pad(target_sink, "sink");
		GstPadLinkReturn ret = gst_pad_link(pad, target_sink_pad);
		NXGLOGI("  ==> %s to link %s:%s to %s:%s",
				(ret != GST_PAD_LINK_OK) ? "Failed":"Succeed",
				GST_DEBUG_PAD_NAME(pad),
				GST_DEBUG_PAD_NAME(target_sink_pad));
		gst_object_unref (target_sink_pad);
	}

	gst_caps_unref (caps);

	FUNC_OUT();
}

static void
cb_typefind_details (GstElement *typefind, guint probability,
			GstCaps *caps, gpointer data)
{
	MpegTsSt *handle = (MpegTsSt *)data;
	g_main_loop_quit(handle->loop);
	NXGLOGI("Exit loop for details");
}

gint
get_program_info(const char* filePath, struct GST_MEDIA_INFO *media_info)
{
	GMainContext *worker_context;
	GError *error = NULL;
	gboolean ret = FALSE;
	MpegTsSt handle;

	FUNC_IN();

	// Initialize struct 'MpegTsSt'
	memset(&handle, 0, sizeof(MpegTsSt));
	handle.media_info = media_info;

	if (DEMUX_TYPE_MPEGTSDEMUX != media_info->demux_type) {
		NXGLOGE("Failed to get program info because it's not ts file");
		return -1;
	}

	// init GStreamer
	if(!gst_is_initialized()) {
		gst_init(NULL, NULL);
	}

	// init mpegts library
	gst_mpegts_initialize();

	worker_context = g_main_context_new();
	g_main_context_push_thread_default(worker_context);
	handle.loop = g_main_loop_new (worker_context, FALSE);

	//  gst-launch-1.0 -v filesrc location=/tmp/media/sda1/VIDEO_MPEG2/bbc010906.ts ! tsdemux name=demux demux. ! queue !  decodebin ! typefind ! fakesink
	// Create file source and typefind element
	handle.pipeline = gst_pipeline_new("pipe");

	/* Put a bus handler */
	handle.bus = gst_pipeline_get_bus (GST_PIPELINE (handle.pipeline));
	gst_bus_add_watch (handle.bus, (GCallback) on_bus_message_program, &handle);
	gst_object_unref (GST_OBJECT (handle.bus));

	handle.filesrc = gst_element_factory_make ("filesrc", "source");
	g_object_set (G_OBJECT (handle.filesrc), "location", filePath, NULL);
	handle.demuxer = gst_element_factory_make ("tsdemux", "tsdemux");	
	handle.video_queue = gst_element_factory_make ("queue2", "queue2");
	handle.decodebin = gst_element_factory_make ("decodebin", "decodebin");
	handle.typefind = gst_element_factory_make ("typefind", "typefind");
	handle.fakesink = gst_element_factory_make ("fakesink", "sink");

	// Add & link elements
	gst_bin_add_many (GST_BIN (handle.pipeline), handle.filesrc,
					handle.demuxer, handle.video_queue,
					handle.decodebin, handle.typefind, handle.fakesink, NULL);

	// filesrc <--> demuxer
	ret = gst_element_link_many (handle.filesrc, handle.demuxer, NULL);
	NXGLOGV("(%d) %s to link filesrc<-->demuxer", __LINE__, (!ret) ? "Failed":"Succeed");
	// demuxer <==> video_queue, demuxer <==> audio_queue
	g_signal_connect(handle.demuxer, "pad-added", G_CALLBACK(demux_pad_added), &handle);
	// video_queue <-> decodebin
	ret = gst_element_link_many (handle.video_queue, handle.decodebin, NULL);
	NXGLOGV("(%d) %s to link video_queue<-->decodebin", __LINE__, (!ret) ? "Failed":"Succeed");
	// decodebin <==> typefind
	g_signal_connect(handle.decodebin, "pad-added", G_CALLBACK(decodebin_pad_added), &handle);
	// typefind <--> fakesink
	ret = gst_element_link_many (handle.typefind, handle.fakesink, NULL);
	NXGLOGV("(%d) %s to link typefind<-->fakesink", __LINE__, (!ret) ? "Failed":"Succeed");

	g_type_class_ref (GST_TYPE_MPEGTS_SECTION_TYPE);
	g_type_class_ref (GST_TYPE_MPEGTS_SECTION_TABLE_ID);
	g_type_class_ref (GST_TYPE_MPEGTS_RUNNING_STATUS);
	g_type_class_ref (GST_TYPE_MPEGTS_DESCRIPTOR_TYPE);
	g_type_class_ref (GST_TYPE_MPEGTS_DVB_DESCRIPTOR_TYPE);
	g_type_class_ref (GST_TYPE_MPEGTS_ATSC_DESCRIPTOR_TYPE);
	g_type_class_ref (GST_TYPE_MPEGTS_ISDB_DESCRIPTOR_TYPE);
	g_type_class_ref (GST_TYPE_MPEGTS_MISC_DESCRIPTOR_TYPE);
	g_type_class_ref (GST_TYPE_MPEGTS_ISO639_AUDIO_TYPE);
	g_type_class_ref (GST_TYPE_MPEGTS_DVB_SERVICE_TYPE);
	g_type_class_ref (GST_TYPE_MPEGTS_DVB_TELETEXT_TYPE);
	g_type_class_ref (GST_TYPE_MPEGTS_STREAM_TYPE);
	g_type_class_ref (GST_TYPE_MPEGTS_SECTION_DVB_TABLE_ID);
	g_type_class_ref (GST_TYPE_MPEGTS_SECTION_ATSC_TABLE_ID);
	g_type_class_ref (GST_TYPE_MPEGTS_SECTION_SCTE_TABLE_ID);
	g_type_class_ref (GST_TYPE_MPEGTS_MODULATION_TYPE);
	g_type_class_ref (GST_TYPE_MPEGTS_DVB_CODE_RATE);
	g_type_class_ref (GST_TYPE_MPEGTS_CABLE_OUTER_FEC_SCHEME);
	g_type_class_ref (GST_TYPE_MPEGTS_TERRESTRIAL_TRANSMISSION_MODE);
	g_type_class_ref (GST_TYPE_MPEGTS_TERRESTRIAL_GUARD_INTERVAL);
	g_type_class_ref (GST_TYPE_MPEGTS_TERRESTRIAL_HIERARCHY);
	g_type_class_ref (GST_TYPE_MPEGTS_DVB_LINKAGE_TYPE);
	g_type_class_ref (GST_TYPE_MPEGTS_DVB_LINKAGE_HAND_OVER_TYPE);
	g_type_class_ref (GST_TYPE_MPEGTS_COMPONENT_STREAM_CONTENT);
	g_type_class_ref (GST_TYPE_MPEGTS_CONTENT_NIBBLE_HI);
	g_type_class_ref (GST_TYPE_MPEGTS_SCTE_STREAM_TYPE);
	g_type_class_ref (GST_TYPE_MPEGTS_SECTION_SCTE_TABLE_ID);

	// Set the state to PLAYING
	gst_element_set_state (GST_ELEMENT (handle.pipeline), GST_STATE_PLAYING);

	g_main_loop_run (handle.loop);

	// unset
    gst_element_set_state (GST_ELEMENT (handle.pipeline), GST_STATE_NULL);
    gst_object_unref (GST_OBJECT (handle.pipeline));

	g_main_loop_unref(handle.loop);
	g_main_context_pop_thread_default(worker_context);
	g_main_context_unref(worker_context);

	FUNC_OUT();
	return 0;
}

int32_t get_program_index(struct GST_MEDIA_INFO *media_info, gint program_number)
{
	gint cur_pro_idx = -1;
	for (int i=0; i<media_info->n_program; i++)
	{
		if (media_info->program_number[i] == program_number) {
			cur_pro_idx = i;
			NXGLOGI("Found matched program number! idx:%d", i);
			break;
		}
	}
	return cur_pro_idx;
}

gint
get_stream_simple_info(const char* filePath, gint program_number, struct GST_MEDIA_INFO *media_info)
{
	GMainContext *worker_context;
	MpegTsSt handle;
	GError *error = NULL;
	gboolean ret = FALSE;

	NXGLOGI("START with program_number(%d)", program_number);
	if (DEMUX_TYPE_MPEGTSDEMUX != media_info->demux_type) {
		NXGLOGE("Failed to get program info because it's not ts file");
		return -1;
	}

	// init GStreamer
	if(!gst_is_initialized()) {
		gst_init(NULL, NULL);
	}

	// init mpegts library
	gst_mpegts_initialize();

	// Initialize struct 'MpegTsSt'
	memset(&handle, 0, sizeof(MpegTsSt));
	handle.media_info = media_info;

	handle.program_index = get_program_index(media_info, program_number);

	worker_context = g_main_context_new();
	g_main_context_push_thread_default(worker_context);
	handle.loop = g_main_loop_new (worker_context, FALSE);

	// Create elements & set program-number to tsdemux
	handle.pipeline = gst_pipeline_new("pipe");
	handle.filesrc = gst_element_factory_make ("filesrc", "source");
	g_object_set (G_OBJECT (handle.filesrc), "location", filePath, NULL);
	handle.demuxer = gst_element_factory_make ("tsdemux", "tsdemux");
	g_object_set (G_OBJECT (handle.demuxer), "program-number", program_number, NULL);
	handle.video_queue = gst_element_factory_make ("queue2", "queue2");
	handle.decodebin = gst_element_factory_make ("decodebin", "decodebin");
	handle.typefind = gst_element_factory_make ("typefind", "typefind");
	handle.fakesink = gst_element_factory_make ("fakesink", "sink");

	// Add & link elements
	gst_bin_add_many (GST_BIN (handle.pipeline), handle.filesrc,
					handle.demuxer, handle.video_queue,
					handle.decodebin, handle.typefind, handle.fakesink, NULL);

	// filesrc <--> demuxer
	ret = gst_element_link_many (handle.filesrc, handle.demuxer, NULL);
	NXGLOGV("(%d) %s to link filesrc<-->demuxer", __LINE__, (!ret) ? "Failed":"Succeed");
	// demuxer <==> video_queue, demuxer <==> audio_queue
	// Can get simple program info when the pad is added in demuxer
	g_signal_connect(handle.demuxer, "pad-added", G_CALLBACK(demux_pad_added), &handle);
	// video_queue <-> decodebin
	if (!gst_element_link_many (handle.video_queue, handle.decodebin, NULL))
	NXGLOGV("(%d) %s to link video_queue<-->decodebin", __LINE__, (!ret) ? "Failed":"Succeed");
	// decodebin <==> typefind
	g_signal_connect(handle.decodebin, "pad-added", G_CALLBACK(decodebin_pad_added), &handle);
	// typefind <--> fakesink
	if (!gst_element_link_many (handle.typefind, handle.fakesink, NULL))
	NXGLOGV("(%d) %s to link typefind<-->fakesink", __LINE__, (!ret) ? "Failed":"Succeed");

	/* Put a bus handler */
	handle.bus = gst_pipeline_get_bus (GST_PIPELINE (handle.pipeline));
	gst_bus_add_signal_watch (handle.bus);
	g_signal_connect (handle.bus, "message", (GCallback) on_bus_message_simple, &handle);
	gst_object_unref (GST_OBJECT (handle.bus));

	g_type_class_ref (GST_TYPE_MPEGTS_SECTION_TYPE);
	g_type_class_ref (GST_TYPE_MPEGTS_SECTION_TABLE_ID);
	g_type_class_ref (GST_TYPE_MPEGTS_RUNNING_STATUS);
	g_type_class_ref (GST_TYPE_MPEGTS_DESCRIPTOR_TYPE);
	g_type_class_ref (GST_TYPE_MPEGTS_DVB_DESCRIPTOR_TYPE);
	g_type_class_ref (GST_TYPE_MPEGTS_ATSC_DESCRIPTOR_TYPE);
	g_type_class_ref (GST_TYPE_MPEGTS_ISDB_DESCRIPTOR_TYPE);
	g_type_class_ref (GST_TYPE_MPEGTS_MISC_DESCRIPTOR_TYPE);
	g_type_class_ref (GST_TYPE_MPEGTS_ISO639_AUDIO_TYPE);
	g_type_class_ref (GST_TYPE_MPEGTS_DVB_SERVICE_TYPE);
	g_type_class_ref (GST_TYPE_MPEGTS_DVB_TELETEXT_TYPE);
	g_type_class_ref (GST_TYPE_MPEGTS_STREAM_TYPE);
	g_type_class_ref (GST_TYPE_MPEGTS_SECTION_DVB_TABLE_ID);
	g_type_class_ref (GST_TYPE_MPEGTS_SECTION_ATSC_TABLE_ID);
	g_type_class_ref (GST_TYPE_MPEGTS_SECTION_SCTE_TABLE_ID);
	g_type_class_ref (GST_TYPE_MPEGTS_MODULATION_TYPE);
	g_type_class_ref (GST_TYPE_MPEGTS_DVB_CODE_RATE);
	g_type_class_ref (GST_TYPE_MPEGTS_CABLE_OUTER_FEC_SCHEME);
	g_type_class_ref (GST_TYPE_MPEGTS_TERRESTRIAL_TRANSMISSION_MODE);
	g_type_class_ref (GST_TYPE_MPEGTS_TERRESTRIAL_GUARD_INTERVAL);
	g_type_class_ref (GST_TYPE_MPEGTS_TERRESTRIAL_HIERARCHY);
	g_type_class_ref (GST_TYPE_MPEGTS_DVB_LINKAGE_TYPE);
	g_type_class_ref (GST_TYPE_MPEGTS_DVB_LINKAGE_HAND_OVER_TYPE);
	g_type_class_ref (GST_TYPE_MPEGTS_COMPONENT_STREAM_CONTENT);
	g_type_class_ref (GST_TYPE_MPEGTS_CONTENT_NIBBLE_HI);
	g_type_class_ref (GST_TYPE_MPEGTS_SCTE_STREAM_TYPE);
	g_type_class_ref (GST_TYPE_MPEGTS_SECTION_SCTE_TABLE_ID);

	// Set the state to PLAYING
	gst_element_set_state (GST_ELEMENT (handle.pipeline), GST_STATE_PLAYING);
	g_main_loop_run (handle.loop);

	// unset
    gst_element_set_state (GST_ELEMENT (handle.pipeline), GST_STATE_NULL);
    gst_object_unref (GST_OBJECT (handle.pipeline));

	g_main_loop_unref(handle.loop);
	g_main_context_pop_thread_default(worker_context);
	g_main_context_unref(worker_context);

	FUNC_OUT();
	return 0;
}

gint
get_video_stream_details_info(const char* filePath,
	gint program_number, gint video_index, struct GST_MEDIA_INFO *media_info)
{
	GError *error = NULL;
	gboolean ret = FALSE;
	gint cur_pro_idx;
	GMainContext *worker_context;
	MpegTsSt handle;

	// Initialize struct 'MpegTsSt'
	memset(&handle, 0, sizeof(MpegTsSt));
	handle.media_info = media_info;
	handle.program_index = get_program_index(media_info, program_number);
	handle.select_video_idx = video_index;
	
	FUNC_IN();

	cur_pro_idx = get_program_index(media_info, program_number);
	if (cur_pro_idx == -1) {
		NXGLOGE("Not found matched program number");
		return -1;
	}
	NXGLOGI("program_number(%d) --> pIdx(%d)",
			program_number, handle.program_index);

	if (DEMUX_TYPE_MPEGTSDEMUX != media_info->demux_type) {
		NXGLOGE("Failed to get program info because it's not ts file");
		return -1;
	}
	if (media_info->ProgramInfo[cur_pro_idx].n_video == 0) {
		NXGLOGE("Skip to get video info because no video file");
		return -1;
	}
	// init GStreamer
	if(!gst_is_initialized()) {
		gst_init(NULL, NULL);
	}

	// init mpegts library
	gst_mpegts_initialize();

	worker_context = g_main_context_new();
	g_main_context_push_thread_default(worker_context);
	handle.loop = g_main_loop_new (worker_context, FALSE);

	// Create elements & set program-number to tsdemux
	handle.pipeline = gst_pipeline_new("pipe");
	handle.filesrc = gst_element_factory_make ("filesrc", "source");
	g_object_set (G_OBJECT (handle.filesrc), "location", filePath, NULL);
	handle.demuxer = gst_element_factory_make ("tsdemux", "tsdemux");
	g_object_set (G_OBJECT (handle.demuxer), "program-number", program_number, NULL);
	handle.video_queue = gst_element_factory_make ("queue2", "queue2");
	handle.decodebin = gst_element_factory_make ("decodebin", "decodebin");
	handle.typefind = gst_element_factory_make ("typefind", "typefind");
	handle.fakesink = gst_element_factory_make ("fakesink", "sink");

	// Add & link elements
	gst_bin_add_many (GST_BIN (handle.pipeline), handle.filesrc,
					handle.demuxer, handle.video_queue,
					handle.decodebin, handle.typefind, handle.fakesink, NULL);
	// filesrc <--> demuxer
	ret = gst_element_link_many (handle.filesrc, handle.demuxer, NULL);
	NXGLOGV("(%d) %s to link filesrc<-->demuxer", __LINE__, (!ret) ? "Failed":"Succeed");
	// demuxer <==> video_queue, demuxer <==> audio_queue
	// Can get simple program info when the pad is added in demuxer
	g_signal_connect(handle.demuxer, "pad-added", G_CALLBACK(demux_video_pad_added_detail), &handle);
	// video_queue <-> decodebin
	ret = gst_element_link_many (handle.video_queue, handle.decodebin, NULL);
	NXGLOGV("(%d) %s to link video_queue<-->decodebin", __LINE__, (!ret) ? "Failed":"Succeed");
	// decodebin <==> typefind
	g_signal_connect(handle.decodebin, "pad-added", G_CALLBACK(decodebin_video_pad_added_detail), &handle);
	// typefind <--> fakesink
	ret = gst_element_link_many (handle.typefind, handle.fakesink, NULL);
	NXGLOGV("(%d) %s to link typefind<-->fakesink", __LINE__, (!ret) ? "Failed":"Succeed");
	g_signal_connect (handle.typefind, "have-type", G_CALLBACK(cb_typefind_details), &handle);

	/* Put a bus handler */
	handle.bus = gst_pipeline_get_bus (GST_PIPELINE (handle.pipeline));
	gst_bus_add_signal_watch (handle.bus);
	g_signal_connect (handle.bus, "message", (GCallback) on_bus_message_detail, &handle);
	gst_object_unref (GST_OBJECT (handle.bus));

	g_type_class_ref (GST_TYPE_MPEGTS_SECTION_TYPE);
	g_type_class_ref (GST_TYPE_MPEGTS_SECTION_TABLE_ID);
	g_type_class_ref (GST_TYPE_MPEGTS_RUNNING_STATUS);
	g_type_class_ref (GST_TYPE_MPEGTS_DESCRIPTOR_TYPE);
	g_type_class_ref (GST_TYPE_MPEGTS_DVB_DESCRIPTOR_TYPE);
	g_type_class_ref (GST_TYPE_MPEGTS_ATSC_DESCRIPTOR_TYPE);
	g_type_class_ref (GST_TYPE_MPEGTS_ISDB_DESCRIPTOR_TYPE);
	g_type_class_ref (GST_TYPE_MPEGTS_MISC_DESCRIPTOR_TYPE);
	g_type_class_ref (GST_TYPE_MPEGTS_ISO639_AUDIO_TYPE);
	g_type_class_ref (GST_TYPE_MPEGTS_DVB_SERVICE_TYPE);
	g_type_class_ref (GST_TYPE_MPEGTS_DVB_TELETEXT_TYPE);
	g_type_class_ref (GST_TYPE_MPEGTS_STREAM_TYPE);
	g_type_class_ref (GST_TYPE_MPEGTS_SECTION_DVB_TABLE_ID);
	g_type_class_ref (GST_TYPE_MPEGTS_SECTION_ATSC_TABLE_ID);
	g_type_class_ref (GST_TYPE_MPEGTS_SECTION_SCTE_TABLE_ID);
	g_type_class_ref (GST_TYPE_MPEGTS_MODULATION_TYPE);
	g_type_class_ref (GST_TYPE_MPEGTS_DVB_CODE_RATE);
	g_type_class_ref (GST_TYPE_MPEGTS_CABLE_OUTER_FEC_SCHEME);
	g_type_class_ref (GST_TYPE_MPEGTS_TERRESTRIAL_TRANSMISSION_MODE);
	g_type_class_ref (GST_TYPE_MPEGTS_TERRESTRIAL_GUARD_INTERVAL);
	g_type_class_ref (GST_TYPE_MPEGTS_TERRESTRIAL_HIERARCHY);
	g_type_class_ref (GST_TYPE_MPEGTS_DVB_LINKAGE_TYPE);
	g_type_class_ref (GST_TYPE_MPEGTS_DVB_LINKAGE_HAND_OVER_TYPE);
	g_type_class_ref (GST_TYPE_MPEGTS_COMPONENT_STREAM_CONTENT);
	g_type_class_ref (GST_TYPE_MPEGTS_CONTENT_NIBBLE_HI);
	g_type_class_ref (GST_TYPE_MPEGTS_SCTE_STREAM_TYPE);
	g_type_class_ref (GST_TYPE_MPEGTS_SECTION_SCTE_TABLE_ID);

	// Set the state to PLAYING
	gst_element_set_state (GST_ELEMENT (handle.pipeline), GST_STATE_PLAYING);
	g_main_loop_run (handle.loop);

	// unset
    gst_element_set_state (GST_ELEMENT (handle.pipeline), GST_STATE_NULL);
    gst_object_unref (GST_OBJECT (handle.pipeline));

	g_main_loop_unref(handle.loop);
	g_main_context_pop_thread_default(worker_context);
	g_main_context_unref(worker_context);

	FUNC_OUT();

	return 0;
}

gint
get_audio_stream_detail_info(const char* filePath, gint program_number, gint audio_index, struct GST_MEDIA_INFO *media_info)
{
	GMainContext *worker_context;
	GError *error = NULL;
	gboolean ret = FALSE;
	// Initialize struct 'MpegTsSt'
	MpegTsSt handle;
	memset(&handle, 0, sizeof(MpegTsSt));
	handle.media_info = media_info;
	handle.program_index = get_program_index(media_info, program_number);
	handle.select_audio_idx = audio_index;

	if (DEMUX_TYPE_MPEGTSDEMUX != media_info->demux_type) {
		NXGLOGE("Failed to get program info because it's not ts file");
		return -1;
	}

	// init GStreamer
	if(!gst_is_initialized()) {
		gst_init(NULL, NULL);
	}

	// init mpegts library
	gst_mpegts_initialize();

	worker_context = g_main_context_new();
	g_main_context_push_thread_default(worker_context);
	handle.loop = g_main_loop_new (worker_context, FALSE);

	// Create elements & set program-number to tsdemux
	handle.pipeline = gst_pipeline_new("pipe");
	handle.filesrc = gst_element_factory_make ("filesrc", "source");
	g_object_set (G_OBJECT (handle.filesrc), "location", filePath, NULL);
	handle.demuxer = gst_element_factory_make ("tsdemux", "tsdemux");
	g_object_set (G_OBJECT (handle.demuxer), "program-number", program_number, NULL);
	handle.audio_queue = gst_element_factory_make ("queue2", "audio_queue");
	handle.audio_decodebin = gst_element_factory_make ("decodebin", "audio_decodebin");
	handle.audio_typefind = gst_element_factory_make ("typefind", "audio_typefind");
	handle.audio_fakesink = gst_element_factory_make ("fakesink", "audio_fakesink");	

	// Add & link elements
	gst_bin_add_many (GST_BIN (handle.pipeline), handle.filesrc,
					handle.demuxer, handle.audio_queue,
					handle.audio_decodebin, handle.audio_typefind, handle.audio_fakesink, NULL);
	// filesrc <--> demuxer
	ret = gst_element_link_many (handle.filesrc, handle.demuxer, NULL);
	NXGLOGV("(%d) %s to link filesrc<-->demuxer", __LINE__, (!ret) ? "Failed":"Succeed");
	// demuxer <==> audio_queue
	// Can get simple program info when the pad is added in demuxer
	g_signal_connect(handle.demuxer, "pad-added", G_CALLBACK(demux_audio_pad_added_detail), &handle);
	// audio_queue <-> decodebin
	ret = gst_element_link_many (handle.audio_queue, handle.audio_decodebin, NULL);
	NXGLOGV("(%d) %s to link audio_queue<-->audio_decodebin", __LINE__, (!ret) ? "Failed":"Succeed");
	// decodebin <==> typefind
	g_signal_connect(handle.audio_decodebin, "pad-added", G_CALLBACK(decodebin_audio_pad_added_detail), &handle);
	// typefind <--> fakesink
	ret = gst_element_link_many (handle.audio_typefind, handle.audio_fakesink, NULL);
	NXGLOGV("(%d) %s to link audio_typefind<-->audio_fakesink", __LINE__, (!ret) ? "Failed":"Succeed");
	g_signal_connect (handle.audio_typefind, "have-type", G_CALLBACK(cb_typefind_details), &handle);

	/* Put a bus handler */
	handle.bus = gst_pipeline_get_bus (GST_PIPELINE (handle.pipeline));
	gst_bus_add_signal_watch (handle.bus);
	g_signal_connect (handle.bus, "message", (GCallback) on_bus_message_detail, &handle);
	gst_object_unref (GST_OBJECT (handle.bus));

	g_type_class_ref (GST_TYPE_MPEGTS_SECTION_TYPE);
	g_type_class_ref (GST_TYPE_MPEGTS_SECTION_TABLE_ID);
	g_type_class_ref (GST_TYPE_MPEGTS_RUNNING_STATUS);
	g_type_class_ref (GST_TYPE_MPEGTS_DESCRIPTOR_TYPE);
	g_type_class_ref (GST_TYPE_MPEGTS_DVB_DESCRIPTOR_TYPE);
	g_type_class_ref (GST_TYPE_MPEGTS_ATSC_DESCRIPTOR_TYPE);
	g_type_class_ref (GST_TYPE_MPEGTS_ISDB_DESCRIPTOR_TYPE);
	g_type_class_ref (GST_TYPE_MPEGTS_MISC_DESCRIPTOR_TYPE);
	g_type_class_ref (GST_TYPE_MPEGTS_ISO639_AUDIO_TYPE);
	g_type_class_ref (GST_TYPE_MPEGTS_DVB_SERVICE_TYPE);
	g_type_class_ref (GST_TYPE_MPEGTS_DVB_TELETEXT_TYPE);
	g_type_class_ref (GST_TYPE_MPEGTS_STREAM_TYPE);
	g_type_class_ref (GST_TYPE_MPEGTS_SECTION_DVB_TABLE_ID);
	g_type_class_ref (GST_TYPE_MPEGTS_SECTION_ATSC_TABLE_ID);
	g_type_class_ref (GST_TYPE_MPEGTS_SECTION_SCTE_TABLE_ID);
	g_type_class_ref (GST_TYPE_MPEGTS_MODULATION_TYPE);
	g_type_class_ref (GST_TYPE_MPEGTS_DVB_CODE_RATE);
	g_type_class_ref (GST_TYPE_MPEGTS_CABLE_OUTER_FEC_SCHEME);
	g_type_class_ref (GST_TYPE_MPEGTS_TERRESTRIAL_TRANSMISSION_MODE);
	g_type_class_ref (GST_TYPE_MPEGTS_TERRESTRIAL_GUARD_INTERVAL);
	g_type_class_ref (GST_TYPE_MPEGTS_TERRESTRIAL_HIERARCHY);
	g_type_class_ref (GST_TYPE_MPEGTS_DVB_LINKAGE_TYPE);
	g_type_class_ref (GST_TYPE_MPEGTS_DVB_LINKAGE_HAND_OVER_TYPE);
	g_type_class_ref (GST_TYPE_MPEGTS_COMPONENT_STREAM_CONTENT);
	g_type_class_ref (GST_TYPE_MPEGTS_CONTENT_NIBBLE_HI);
	g_type_class_ref (GST_TYPE_MPEGTS_SCTE_STREAM_TYPE);
	g_type_class_ref (GST_TYPE_MPEGTS_SECTION_SCTE_TABLE_ID);

	// Set the state to PLAYING
	gst_element_set_state (GST_ELEMENT (handle.pipeline), GST_STATE_PLAYING);
	g_main_loop_run (handle.loop);

	// unset
    gst_element_set_state (GST_ELEMENT (handle.pipeline), GST_STATE_NULL);
    gst_object_unref (GST_OBJECT (handle.pipeline));

	g_main_loop_unref(handle.loop);
	g_main_context_pop_thread_default(worker_context);
	g_main_context_unref(worker_context);

	FUNC_OUT();

	return 0;
}