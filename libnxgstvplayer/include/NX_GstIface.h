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

#ifndef __NX_GSTIFACE_H
#define __NX_GSTIFACE_H

#include <glib.h>
#include "NX_GstTypes.h"

#ifdef __cplusplus
extern "C" {
#endif	//	__cplusplus

/*!
 * \fn NX_GST_RET NX_GSTMP_Open(MP_HANDLE *handle,
 * void (*cb)(void *owner, unsigned int msg, unsigned int param1, void* param),
 * void *cbOwner);
 *
 * \brief This is used to allocate the required resources and register a callback.
 * GStreamer library is also initialized in this method.
 *
 * \param [in]  handle    Movie player handle
 * \param [in]  void (*cb)(void owner, unsigned int msgType, unsigned int msgData, void param) 
 * The application will receive various messages such as EOS message, 
 * state changed message from movie player library.
 * \param [in]  cbOwner  Not currently used. Can be set NULL.
 *
 * \retval NX_GST_RET_ERROR On failure.
 * \retval NX_GST_RET_OK On succee.
 */
NX_GST_RET NX_GSTMP_Open(MP_HANDLE *handle,
        void (*cb)(void *owner, unsigned int msg, unsigned int param1, void* param),
        void *cbOwner);

/*!
 * \fn NX_GST_RET NX_GSTMP_SetDisplayMode(MP_HANDLE handle, enum DISPLAY_MODE in_mode);
 *
 * \brief This is used to set display mode.
 * If the application doesn’t call this API, DISPLAY_MODE_LCD_ONLY is set as default.
 *
 * \param [in]  handle    Movie player handle
 * \param [in]  in_mode   The display mode to set
 *
 * \retval NX_GST_RET_ERROR On failure.
 * \retval NX_GST_RET_OK On succee.
 */
NX_GST_RET NX_GSTMP_SetDisplayMode(MP_HANDLE handle, enum DISPLAY_MODE in_mode);

/*!
 * \fn NX_GST_RET NX_GSTMP_SetUri(MP_HANDLE handle, const char *filePath);
 *
 * \brief This is used to set the file path to play. 
 * If the content does not support playback due to certain reason(such as not supported container format,
 * video codoec, audio codec and video size, etc), then it returns NX_GST_RET_ERROR.
 *
 * \param [in]  handle    Movie player handle
 * \param [in]  filePath  The file path to play
 *
 * \retval NX_GST_RET_ERROR On failure.
 * \retval NX_GST_RET_OK On succee.
 */
NX_GST_RET NX_GSTMP_SetUri(MP_HANDLE handle, const char *filePath);

/*!
 * \fn NX_GST_RET NX_GSTMP_GetMediaInfo(MP_HANDLE handle, const char* filePath, struct GST_MEDIA_INFO *pInfo);
 *
 * \brief This is used to get media information.
 * The application can use the media information to limit the codec types to support or
 * to calculate the display rect information according to aspect ratio.
 *
 * \param [in]  handle    Movie player handle
 * \param [in]  filePath  Media filepath
 * \param [in]  pInfo     Media information
 *
 * \retval NX_GST_RET_ERROR On failure.
 * \retval NX_GST_RET_OK On succee.
 */
NX_GST_RET NX_GSTMP_GetMediaInfo(MP_HANDLE handle, const char* filePath, GST_MEDIA_INFO *pGstMInfo);

/*!
 * \fn NX_GST_RET NX_GSTMP_SetDisplayInfo(MP_HANDLE handle, enum DISPLAY_TYPE type,
 * int dspWidth, int dspHeight, struct DSP_RECT rect);
 *
 * \brief This is used to set the display area information according to the each display type.
 *
 * \param [in]  handle    Movie player handle
 * \param [in]  type      The display type (ex. DISPLAY_TYPE_PRIMARY, DISPLAY_TYPE_SECONDARY)
 * \param [in]  dspWidth  The display width
 * \param [in]  dspHeight The display height
 * \param [in]  rect      Display area information calculated according to aspect ratio
 *
 * \retval NX_GST_RET_ERROR On failure.
 * \retval NX_GST_RET_OK On succee.
 */
NX_GST_RET NX_GSTMP_SetDisplayInfo(MP_HANDLE handle, enum DISPLAY_TYPE type,
                                    int dspWidth, int dspHeight, struct DSP_RECT rect);

/*!
 * \fn void NX_GSTMP_Prepare(MP_HANDLE handle);
 *
 * \brief This is used to configure GStreamer pipeline
 * After configuring GStreamer pipeline, it sets the state to ‘READY’.
 *
 * \param [in]  handle    Movie player handle
 *
 */
NX_GST_RET NX_GSTMP_Prepare(MP_HANDLE handle);

/*!
 * \fn void NX_GSTMP_Close(MP_HANDLE handle);
 *
 * \brief This is used to deallocate all the resources allocated.
 *
 * \param [in]  handle    Movie player handle
 *
 */
void NX_GSTMP_Close(MP_HANDLE handle);

/*!
 * \fn NX_GST_RET NX_GSTMP_Play(MP_HANDLE handle);
 *
 * \brief This is used to start playback.
 *
 * \param [in]  handle    Movie player handle
 *
 * \retval NX_GST_RET_ERROR On failure.
 * \retval NX_GST_RET_OK On succee.
 */
NX_GST_RET NX_GSTMP_Play(MP_HANDLE handle);

/*!
 * \fn NX_GST_RET NX_GSTMP_Pause(MP_HANDLE handle);
 *
 * \brief This is used to pause playback.
 *
 * \param [in]  handle    Movie player handle
 *
 * \retval NX_GST_RET_ERROR On failure.
 * \retval NX_GST_RET_OK On succee.
 */
NX_GST_RET NX_GSTMP_Pause(MP_HANDLE hande);

/*!
 * \fn NX_GST_RET NX_GSTMP_Stop(MP_HANDLE handle);
 *
 * \brief This is used to stop playback.
 *
 * \param [in]  handle    Movie player handle
 *
 * \retval NX_GST_RET_ERROR On failure.
 * \retval NX_GST_RET_OK On succee.
 */
NX_GST_RET NX_GSTMP_Stop(MP_HANDLE hande);

/*!
 * \fn NX_GST_RET NX_GSTMP_Seek(MP_HANDLE hande, gint64 seekTime);
 *
 * \brief This is used to seek(jump) to a certain position(time).
 *
 * \param [in]  handle    Movie player handle
 * \param [in]  seekTime    Seek time in milliseconds
 *
 * \retval NX_GST_RET_ERROR On failure.
 * \retval NX_GST_RET_OK On succee.
 */
NX_GST_RET NX_GSTMP_Seek(MP_HANDLE hande, int64_t seekTime);

/*!
 * \fn gint64 NX_GSTMP_GetDuration(MP_HANDLE handle);
 *
 * \brief This is used to get the total stream duration in nanoseconds.
 *
 * \param [in]  handle    Movie player handle
 *
 * \retval The stream duration in nanoseconds
 */
int64_t NX_GSTMP_GetDuration(MP_HANDLE handle);

/*!
 * \fn gint64 NX_GSTMP_GetPosition(MP_HANDLE handle);
 *
 * \brief This is used to get the current stream position in nanoseconds.
 *
 * \param [in]  handle    Movie player handle
 *
 * \retval The stream duration in nanoseconds
 */
int64_t NX_GSTMP_GetPosition(MP_HANDLE handle);

/*!
 * \fn enum NX_MEDIA_STATE NX_GSTMP_GetState(MP_HANDLE handle);
 *
 * \brief This is used for getting the current state playback.
 *
 * \param [in]  handle    Movie player handle
 *
 * \retval Playback state (NX_MEDIA_STATE)
 * \retval MP_STATE_VOID_PENDING    
 * \retval MP_STATE_STOPPED         STOPPED state
 * \retval MP_STATE_READY           READY state
 * \retval MP_STATE_PAUSED          PAUSED state
 * \retval MP_STATE_PLAYING         PLAYING state
 */
enum NX_MEDIA_STATE NX_GSTMP_GetState(MP_HANDLE handle);

/*!
 * \fn NX_GST_RET NX_GSTMP_VideoMute(MP_HANDLE handle, int32_t bOnoff);
 *
 * \brief This is used to turn on/off the video mute.
 *
 * \param [in]  handle    Movie player handle
 *
 * \retval NX_GST_RET_ERROR On failure.
 * \retval NX_GST_RET_OK On succee.
 */
NX_GST_RET NX_GSTMP_VideoMute(MP_HANDLE handle, int32_t bOnoff);

/*!
 * \fn NX_GST_RET NX_GSTMP_SetVideoSpeed(MP_HANDLE handle, double rate);
 *
 * \brief This is used to control the playback rate.
 * It’s available in PAUSED or PLAYING state.
 *
 * \param [in]  handle    Movie player handle
 * \param [in]  rate      The playback speed rate.
 * (ex. 0.0 : not allowed, 1.0 : the normal playback, 2.0 : double speed)
 *
 * \retval NX_GST_RET_ERROR On failure.
 * \retval NX_GST_RET_OK On succee.
 */
NX_GST_RET NX_GSTMP_SetVideoSpeed(MP_HANDLE handle, double rate);

/*!
 * \fn double NX_GSTMP_GetVideoSpeed(MP_HANDLE handle);
 *
 * \brief This is used to get video playback speed rate.
 *
 * \param [in]  handle    Movie player handle
 *
 * \retval Video playback speed rate.
 */
double NX_GSTMP_GetVideoSpeed(MP_HANDLE handle);

/*!
 * \fn NX_GST_RET NX_GSTMP_SelectStream(MP_HANDLE handle);
 *
 * \brief This is used to select the specific stream.
 *
 * \param [in]  handle    Movie player handle
 * \param [in]  type      The stream type to select
 * \param [in]  idx       The stream index to select
 *
 * \retval NX_GST_RET_ERROR On failure.
 * \retval NX_GST_RET_OK On succee.
 */
NX_GST_RET NX_GSTMP_SelectStream(MP_HANDLE handle, STREAM_TYPE type, int32_t idx);

/*!
 * \fn const char* NX_GSTMP_MakeThumbnail(const gchar *uri, int64_t pos_msec,
 * int32_t width, const char *outPath);
 *
 * \brief This is used to make thumbnail for a certain position.
 *
 * \param [in]  uri         URI
 * \param [in]  pos_msec    Video position to make thumbnail
 * \param [in]  width       Width of thumbnail to create
 * \param [in]  outPath     File path of thumbnail to create
 *
 * \retval NX_GST_RET_ERROR On failure.
 * \retval NX_GST_RET_OK On succee.
 */
NX_GST_RET NX_GSTMP_MakeThumbnail(const char *uri, int64_t pos_msec,
                        int32_t width, const char *outPath);

#ifdef __cplusplus
}
#endif

#endif // __NX_GSTIFACE_H