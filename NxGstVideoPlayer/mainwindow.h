#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include <QMediaPlayer>
#include <QTime>
#include <QTimer>
#include <QDebug>
#include <QTouchEvent>

#include <QEvent>
//Message
#include <QFrame>
#include <QLabel>
#include <QPushButton>

#include "NX_CFileList.h"
#include "CNX_GstMoviePlayer.h"
#include "CNX_SubtitleParser.h"
#include "media/CNX_UeventManager.h"
#include "CNX_Util.h"
#include "playlistwindow.h"

#include <NX_GstIface.h>

#include <QObject>
class CallBackSignal : public QObject
{
	Q_OBJECT

public:
	CallBackSignal() {}

public slots:
	void statusChanged(int eventType, int eventData, void* param)
	{
		emit mediaStatusChanged(eventType, eventData, param);
	}

signals:
	void mediaStatusChanged(int eventType, int eventData, void* param);
};

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	explicit MainWindow(QWidget *parent = 0);
	~MainWindow();

	//
	//	Mouse Event
	//
	void getAspectRatio(int srcWidth, int srcHeight,
						int scrWidth, int scrHeight,
						int *pWidth, int *pHeight);
	void setResize(int dspStatus);
	void ImageUpdate(void *pImg);
	void displayTouchEvent();

	// save previous state
	int32_t SaveInfo();
	bool SeekToPrev(int* iSavedPosition, const char *pPrevPath, int* iFileIdx);

	// HDMI Status Event
	void HDMIStatusChanged(int status);
	//	MediaPlayer
	bool PauseVideo();
	bool StopVideo();
	bool PlayVideo();
	bool CloseVideo();
	bool SeekVideo(int32_t iSec);
	bool PlayNextVideo();
	bool PlayPreviousVideo();
	void PlaySeek();

	//	SubTitle
	int	 OpenSubTitle();
	void PlaySubTitle();
	void StopSubTitle();

public:
	static QString btn_enabled;
	static QString btn_disabled;

private:
    bool            dbg;
	int64_t			m_iDuration;
	QTimer			m_PosUpdateTimer;
	QTimer			*m_SubtitleDismissTimer;
	bool			m_bIsInitialized;
	qint64			m_savePosition;
	CNX_GstMoviePlayer *m_pNxPlayer;
	NX_CFileList    m_FileList;
	QTimer          *m_pTimer;
	int             m_DspMode;
	bool			m_bFindVideoFile;
	CNX_UeventManager *m_pUeventManager;

	//	Progress Bar
	bool	m_bSeekReady;
	bool    m_bVoumeCtrlReady;
	bool    m_bButtonHide;
	int     m_iCurFileListIdx;

	bool	m_bTryFlag;			//try playing back last status
	NX_CMutex	m_listMutex;	//for storage event

	// Subtitle
	CNX_SubtitleParser  *m_pSubtitleParser;
	bool                m_bSubThreadFlag;
	QTimer              *m_pSubTitleTimer;
	QTextCodec*         m_pCodec;

	// Video Speed
	float m_fSpeed;
	bool m_bNotSupportSpeed;

	//Message
	QFrame *m_pMessageFrame;
	QLabel *m_pMessageLabel;
	QPushButton *m_pMessageButton;

	// Display Info
	DISPLAY_INFO 	m_dspInfo;
	MP_DRM_PLANE_INFO m_idSecondDisplay;
	bool			m_bHDMIConnected;
	bool			m_bHDMIModeSet;

	// Dual display
	CNX_DrmInfo* 	m_pDrmInfo;

	// For test : Program, stream selection index
	int 	m_select_program;
	int		m_select_audio;

	NX_MEDIA_STATE m_current_status;
private:
	//	event filter
	bool eventFilter(QObject *watched, QEvent *event);
	void SetupUI();
	void UpdateDurationInfo(int64_t position, int64_t duration);

	//  Update Progress Bar
private:
	void updateProgressBar(QMouseEvent *event, bool bReleased);

private slots:
	// uevent
	void slotDetectUEvent(QString action, QString devNode);

	void updateSubTitle();
	void statusChanged(int eventType, int eventData, void* param);
	void DoPositionUpdate();
	void dismissSubtitle();

	void on_prevButton_released();
	void on_playButton_released();
	void on_pauseButton_released();
	void on_nextButton_released();
	void on_stopButton_released();

	//	Playlist Button & Close Button
	void on_closeButton_released();
	void on_playListButton_released();

	//Message
	void slotOk();

	void on_speedButton_released();
	void on_switchStreamButton_released();
	void on_nextProgramButton_released();
protected:
	void resizeEvent(QResizeEvent *event);

private:
	Ui::MainWindow *ui;
};

#endif // MAINWINDOW_H
