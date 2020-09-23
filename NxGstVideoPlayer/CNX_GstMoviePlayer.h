/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef CNX_GstMoviePlayer_H
#define CNX_GstMoviePlayer_H

#include <QTime>
#include <QDebug>
#include "CNX_Util.h"
#include <NX_GstIface.h>
#include "CNX_SubtitleParser.h"
#include "CNX_DrmInfo.h"

typedef struct DISPLAY_INFO {
	int 			primary_dsp_width;
	int 			primary_dsp_height;
	DISPLAY_MODE	dspMode;
	int				secondary_dsp_width;
	int				secondary_dsp_height;
} DISPLAY_INFO;

class CNX_GstMoviePlayer
{

public:
	CNX_GstMoviePlayer();
	~CNX_GstMoviePlayer();

public:
	// MediaPlayer commomn Initialize , close
	//mediaType is MP_TRACK_VIDEO or MP_TRACK_AUDIO
	int InitMediaPlayer(void (*pCbEventCallback)(void *privateDesc, unsigned int EventType,
												 unsigned int EventData, void* param),
						void *pCbPrivate, const char *pUri,
						DISPLAY_INFO dspinfo);
	int SetDisplayMode(DISPLAY_MODE mode);
	int CloseHandle();

	// MediaPlayer common Control
	int Play();
	int Seek(qint64 position);
	int Pause();
	int Stop();

	// MediaPlayer common information
	int64_t GetMediaPosition();
	int64_t GetMediaDuration();
	NX_MEDIA_STATE GetState();
	int SetDisplayInfo(enum DISPLAY_TYPE type, int dspWidth, int dspHeight, DSP_RECT rect);
	// Thumbnail
	int MakeThumbnail(const char *pUri, int64_t pos_msec, int32_t width, const char *outPath);
	// Test API for program, Stream selection
	int SetNextAudioStream(int aIdx);
	bool isProgramSelectable();
	bool isStreamSelectable();
	int SetNextProgramIdx(int pIdx);
	void resetStreamIndex();
	// The video mute
	int DrmVideoMute(int bOnOff);

	// The playback speed
	int SetVideoSpeed(double rate);
	double GetVideoSpeed();
	bool GetVideoSpeedSupport();
	bool isSeekable();

	// The subtitle
	bool HasSubTitleStream();

private:
	// MediaPlayer InitMediaPlayer
	int OpenHandle(void (*pCbEventCallback)(void *privateDesc, unsigned int EventType,
											unsigned int EventData, void* param),
				   void *cbPrivate);
	int SetUri(const char *pUri);
	int GetMediaInfo(const char* filePath);
	int SelectStream(STREAM_TYPE type, int idx);
	int Prepare();
	void GetAspectRatio(int srcWidth, int srcHeight,
						int dspWidth, int dspHeight,
						DSP_RECT *pDspDstRect);
	void PrintMediaInfo(GST_MEDIA_INFO media_info, const char* filePath);
	int SetAspectRatio(DISPLAY_INFO dspInfo);

	//vars
	bool    debug;
	pthread_mutex_t	m_hLock;
	MP_HANDLE		m_hPlayer;

    int m_X, m_Y, m_Width, m_Height;

	int				m_iMediaType;
	int             m_bVideoMute;

	GST_MEDIA_INFO	m_MediaInfo;

	char			*m_pAudioDeviceName;
	double			m_fSpeed;
	int				m_select_program;
	int				m_select_audio;
	int				m_select_video;
public:
	int IsCbQtUpdateImg();

public:
	// Subtitle
	CNX_SubtitleParser* m_pSubtitleParser;
	pthread_mutex_t m_SubtitleLock;
	pthread_t m_subtitleThread;
	int m_iSubtitleSeekTime;

	void	CloseSubtitle();
	int		OpenSubtitle(char * subtitlePath);
	int		GetSubtitleStartTime();
	void	SetSubtitleIndex(int idx);
	int		GetSubtitleIndex();
	int		GetSubtitleMaxIndex();
	void	IncreaseSubtitleIndex();
	void	SeekSubtitle(int milliseconds);
	char*	GetSubtitleText();
	bool	IsSubtitleAvailable();
	const char*	GetBestSubtitleEncode();
	const char* GetBestStringEncode(const char* str);

private:
	//
	// Subtitle
	static void* ThreadWrapForSubtitleSeek(void *Obj);
	void SeekSubtitleThread(void);
};

#endif // CNX_GstMoviePlayer_H
