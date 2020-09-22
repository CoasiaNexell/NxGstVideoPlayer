#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QTextCodec>
#include <QDesktopWidget>

#include <NX_GstIface.h>
#define LOG_TAG "[MainWindow]"
#include <NX_Log.h>

static const char *NX_VIDEO_EXTENSION[] = {
	".mp4",  ".avi", ".mkv",
	".divx", ".mov", ".3gpp"
	".m2ts", ".ts", ".3gp"
};

QString MainWindow::btn_enabled = QString(
	"QPushButton{background-color: rgba(255, 255, 255, 50%);"
	"color: rgba(1, 1, 1);} ");
QString MainWindow::btn_disabled = QString(
	"QPushButton{background-color: rgba(189, 189, 189, 50%);"
	"color: rgba(255, 255, 255);} ");

// Display Info
#define DEFAULT_DSP_WIDTH	1024
#define DEFAULT_DSP_HEIGHT	600

// HDMI Display Info
#define DEFAULT_SUB_DSP_WIDTH	1920
#define DEFAULT_SUB_DSP_HEIGHT	1080

enum NxMediaEvent {
	NX_EVENT_MEDIA_UNKNOWN = -1,
	NX_EVENT_MEDIA_HDMI_CONNECTED,
	NX_EVENT_MEDIA_HDMI_DISCONNECTED
};

////////////////////////////////////////////////////////////////////////////////
//
//	Event Callback
//
static    CallBackSignal mediaStateCb;

//CallBack Eos, Error
static void cbEventCallback(void *privateDesc, unsigned int EventType, unsigned int EventData, void* param)
{
	mediaStateCb.statusChanged(EventType, EventData, param);
}

////////////////////////////////////////////////////////////////////////////////

MainWindow::MainWindow(QWidget *parent)
	: QMainWindow(parent)
	, dbg(false)
	, m_bSubThreadFlag(false)
	, m_iDuration    (0)
	, m_bIsInitialized(false)
	, m_savePosition(0)
	, m_pNxPlayer   (NULL)
	, m_DspMode     (DISPLAY_MODE_NONE)
	, m_bFindVideoFile (false)
	, m_bSeekReady  (false)
	, m_bVoumeCtrlReady (false)
	, m_bButtonHide (false)
	, m_bHDMIConnected(false)
	, m_bHDMIModeSet(false)
	, m_iCurFileListIdx (0)
	, m_bTryFlag(false)
	, m_pSubtitleParser (NULL)
	, m_pSubTitleTimer (NULL)
	, m_fSpeed(1.0)
	, m_bNotSupportSpeed (false)
	, m_pMessageFrame(NULL)
	, m_pMessageLabel(NULL)
	, m_pMessageButton(NULL)
	, m_select_program(0)
	, m_select_audio(0)
	, m_current_status(MP_STATE_STOPPED)
	, ui(new Ui::MainWindow)
{
	ui->setupUi(this);

	const QRect screen = QApplication::desktop()->screenGeometry();
	if ((width() != screen.width()) || (height() != screen.height()))
	{
		setFixedSize(screen.width(), screen.height());
	}

	//Find file list
	m_FileList.MakeFileList("/run/media",(const char **)&NX_VIDEO_EXTENSION, sizeof(NX_VIDEO_EXTENSION) / sizeof(NX_VIDEO_EXTENSION[0]) );

	NXLOGI("<<< Total file list = %d\n", m_FileList.GetSize());

	m_pUeventManager = new CNX_UeventManager();
	connect(m_pUeventManager, SIGNAL(signalDetectUEvent(QString,QString)), this, SLOT(slotDetectUEvent(QString,QString)));

	//	Connect Solt Functions
	connect(&mediaStateCb, SIGNAL(mediaStatusChanged(int, int, void*)), SLOT(statusChanged(int, int, void*)));

	ui->graphicsView->viewport()->installEventFilter(this);
	ui->progressBar->installEventFilter(this);
	ui->progressBar->setRange(0,100);
	ui->progressBar->setValue(0);

	// Dual Display
	memset(&m_dspInfo, 0, sizeof(DISPLAY_INFO));
	m_pDrmInfo = new CNX_DrmInfo();
	m_pDrmInfo->OpenDrm();
	m_bHDMIConnected = m_pDrmInfo->isHDMIConnected();
	if (m_bHDMIConnected)
	{
		m_bHDMIModeSet = m_pDrmInfo->setMode(CRTC_IDX_SECONDARY, PLANE_TYPE_VIDEO,
											DEFAULT_RGB_LAYER_IDX, DEFAULT_SUB_DSP_WIDTH,
											DEFAULT_SUB_DSP_HEIGHT);
	}

	m_dspInfo.primary_dsp_width = width();
	m_dspInfo.primary_dsp_height = height();
	m_dspInfo.dspMode = (m_bHDMIConnected && m_bHDMIModeSet) ? DISPLAY_MODE_LCD_HDMI : DISPLAY_MODE_LCD_ONLY;
	NXLOGI("%s() dsp_mode(%s)", __FUNCTION__, (m_dspInfo.dspMode==DISPLAY_MODE_LCD_ONLY) ? "LCD Only":"LCD_HDMI");
	m_dspInfo.secondary_dsp_width = DEFAULT_SUB_DSP_WIDTH;
	m_dspInfo.secondary_dsp_height = DEFAULT_SUB_DSP_HEIGHT;

	m_pNxPlayer = new CNX_GstMoviePlayer();
	ui->durationlabel->setStyleSheet("QLabel { color : white; }");
	ui->appNameLabel->setStyleSheet("QLabel { color : white; }");
	ui->subTitleLabel->setStyleSheet("QLabel { color : white; }");
	ui->subTitleLabel2->setStyleSheet("QLabel { color : white; }");

	m_pTimer = new QTimer();

	//Update position timer
	connect(&m_PosUpdateTimer, SIGNAL(timeout()), this, SLOT(DoPositionUpdate()));
	//Update Subtitle
	connect(&m_PosUpdateTimer, SIGNAL(timeout()), this, SLOT(updateSubTitle()));

	m_SubtitleDismissTimer = new QTimer();
	connect(m_SubtitleDismissTimer, SIGNAL(timeout()), this, SLOT(dismissSubtitle()));

	//Message
	m_pMessageFrame = new QFrame(this);

	m_pMessageFrame->setGeometry(340, 190, 271, 120);
	m_pMessageFrame->setStyleSheet("background: white;");
	m_pMessageFrame->hide();

	m_pMessageLabel = new QLabel(m_pMessageFrame);
	m_pMessageLabel->setGeometry(0, 0, m_pMessageFrame->width(), 100);
	m_pMessageLabel->setText("text");

	m_pMessageButton = new QPushButton(m_pMessageFrame);
	m_pMessageButton->setGeometry(m_pMessageFrame->width()/2-100/2, m_pMessageFrame->height()-30, 80, 30);
	m_pMessageButton->setText("Ok");
	connect(m_pMessageButton, SIGNAL(clicked(bool)), this, SLOT(slotOk()));

	setAttribute(Qt::WA_AcceptTouchEvents, true);
}

MainWindow::~MainWindow()
{
	if (m_PosUpdateTimer.isActive())
	{
		m_PosUpdateTimer.stop();
	}

	if(m_pNxPlayer)
	{
        NX_MEDIA_STATE state = m_pNxPlayer->GetState();
        if( (MP_STATE_PLAYING == state)||(MP_STATE_PAUSED == state) )
		{
			StopVideo();
		}

		delete m_pNxPlayer;
		m_pNxPlayer = NULL;
	}

	if (m_pUeventManager)
	{
		delete m_pUeventManager;
	}

	if (m_pDrmInfo)
	{
		m_pDrmInfo->CloseDrm();
		m_pDrmInfo = NULL;
	}

	if(m_pSubtitleParser)
	{
		delete m_pSubtitleParser;
		m_pSubtitleParser = NULL;
	}

	if(m_pMessageButton)
	{
		delete m_pMessageButton;
	}

	if(m_pMessageLabel)
	{
		delete m_pMessageLabel;
	}

	if(m_pMessageFrame)
	{
		delete m_pMessageFrame;
	}

	delete ui;
}

////////////////////////////////////////////////////////////////////
//
//      Update Player Progress Bar
//
////////////////////////////////////////////////////////////////////
void MainWindow::updateProgressBar(QMouseEvent *event, bool bReleased)
{
	NXLOGI("%s() %s", __FUNCTION__, bReleased ? "button_released":"button_pressed");
    if(bReleased)
	{
		//	 Do Seek
        if(m_bSeekReady)
		{
			NX_MEDIA_STATE state = (m_pNxPlayer) ? m_pNxPlayer->GetState() : MP_STATE_STOPPED;
			if(MP_STATE_PAUSED == state || MP_STATE_PLAYING == state)
			{
				double ratio = (double)event->x()/ui->progressBar->width();
				qint64 position = ratio * NANOSEC_TO_MSEC(m_iDuration);
                NXLOGI("%s() ratio: %lf, m_iDuration: %lld, conv_msec:%lld",
                       __FUNCTION__, ratio, m_iDuration, NANOSEC_TO_SEC(m_iDuration));
				if (m_fSpeed > 1.0)
				{
					if (0 > m_pNxPlayer->SetVideoSpeed(1.0))
					{

					}
					else
					{
						m_fSpeed = 1.0;
						ui->speedButton->setText("x 1");
					}
				}
                SeekVideo(position);

				//seek subtitle
				ui->subTitleLabel->setText("");
				ui->subTitleLabel2->setText("");
				m_pNxPlayer->SeekSubtitle(position);

				NXLOGD("Position = %lld", position);
			}
			NXLOGD("Do Seek !!!");
			DoPositionUpdate();
		}
		m_bSeekReady = false;
	}
	else
	{
		m_bSeekReady = true;
		NXLOGD("Ready to Seek");
	}
}

void MainWindow::HDMIStatusChanged(int status)
{
	if (status == NX_EVENT_MEDIA_HDMI_CONNECTED)
	{
		if (false == m_bHDMIModeSet) {
			m_bHDMIModeSet = m_pDrmInfo->setMode(CRTC_IDX_SECONDARY, PLANE_TYPE_VIDEO,
									DEFAULT_RGB_LAYER_IDX, DEFAULT_SUB_DSP_WIDTH,
									DEFAULT_SUB_DSP_HEIGHT);
		}
		m_bHDMIConnected = m_bHDMIModeSet;

		if (DISPLAY_MODE_LCD_ONLY == m_dspInfo.dspMode)
		{
			m_dspInfo.dspMode = m_bHDMIConnected ? DISPLAY_MODE_LCD_HDMI : DISPLAY_MODE_LCD_ONLY;
		}
		else if (DISPLAY_MODE_NONE == m_dspInfo.dspMode)
		{
			m_dspInfo.dspMode = m_bHDMIConnected ? DISPLAY_MODE_HDMI_ONLY : DISPLAY_MODE_NONE;
		}
	}
	else if (status == NX_EVENT_MEDIA_HDMI_DISCONNECTED)
	{
		m_bHDMIConnected = false;

		if (DISPLAY_MODE_LCD_HDMI == m_dspInfo.dspMode)
		{
			m_dspInfo.dspMode = DISPLAY_MODE_LCD_ONLY;
		}
		else if (DISPLAY_MODE_HDMI_ONLY == m_dspInfo.dspMode)
		{
			m_dspInfo.dspMode = DISPLAY_MODE_NONE;
		}
	}
	else
	{
		m_bHDMIConnected = false;

		m_dspInfo.dspMode = DISPLAY_MODE_LCD_ONLY;
	}

	m_pNxPlayer->SetDisplayMode(m_dspInfo.dspMode);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
	if (watched == ui->graphicsView->viewport())
	{
		if (event->type() == QEvent::MouseButtonPress)
		{
		}
		else if (event->type() == QEvent::MouseButtonRelease)
		{
			displayTouchEvent();
		}
	}
	else if (watched == ui->progressBar)
	{
		if (event->type() == QEvent::MouseButtonPress)
		{
			QMouseEvent *pMouseEvent = static_cast<QMouseEvent *>(event);
			updateProgressBar(pMouseEvent, false);
		}
		else if (event->type() == QEvent::MouseButtonRelease)
		{
			QMouseEvent *pMouseEvent = static_cast<QMouseEvent *>(event);
			updateProgressBar(pMouseEvent, true);
		}
	}

	return QWidget::eventFilter(watched, event);
}

/* Prev button */
void MainWindow::on_prevButton_released()
{
	PlayPreviousVideo();
}

/* Play button */
void MainWindow::on_playButton_released()
{
	PlayVideo();
}

/* Pause button */
void MainWindow::on_pauseButton_released()
{
	PauseVideo();
}

/* Next button */
void MainWindow::on_nextButton_released()
{
	PlayNextVideo();
}

/* Stop button */
void MainWindow::on_stopButton_released()
{
	if (m_pNxPlayer)
		m_pNxPlayer->resetStreamIndex();

	StopVideo();
}

/* switch stream button */
void MainWindow::on_switchStreamButton_released()
{
	NXLOGI("on_switchStreamButton_released");
	SaveInfo();
	StopVideo();
	m_select_audio = m_pNxPlayer->SetNextAudioStream(m_select_audio);
	PlaySeek();
}

/* next program button */
void MainWindow::on_nextProgramButton_released()
{
	NXLOGI("on_nextProgramButton_released");

	if (m_pNxPlayer->isProgramSelectable())
	{
		StopVideo();
		m_select_program = m_pNxPlayer->SetNextProgramIdx(m_select_program);
		PlayVideo();
	}
}

/* Playback speed button */
void MainWindow::on_speedButton_released()
{
    NX_MEDIA_STATE state = (m_pNxPlayer) ? m_pNxPlayer->GetState() : MP_STATE_STOPPED;
    if((MP_STATE_PLAYING != state) && (MP_STATE_PAUSED != state))
	{
		NXLOGI("Works when in play state.\n");
		ui->speedButton->setText("x 1");
		return;
	}

	double old_speed = 1.0;
	double new_speed = 1.0;

	old_speed = m_fSpeed;
	new_speed = m_fSpeed * 2;
	if(new_speed > 16)
		new_speed = 1.0;

	if (0 > m_pNxPlayer->SetVideoSpeed(new_speed))
	{
		m_fSpeed = old_speed;
	}
	else
	{
		m_fSpeed = new_speed;
		if (MP_STATE_PLAYING == state)
		{
			m_pNxPlayer->Play();
		}
	}

	if(m_fSpeed == 1.0) ui->speedButton->setText("x 1");
	else if(m_fSpeed == 2.0) ui->speedButton->setText("x 2");
	else if(m_fSpeed == 4.0) ui->speedButton->setText("x 4");
	else if(m_fSpeed == 8.0) ui->speedButton->setText("x 8");
	else if(m_fSpeed == 16.0) ui->speedButton->setText("x 16");
}

void MainWindow::DoPositionUpdate()
{
	if(NULL == m_pNxPlayer)
	{
		NXLOGW("%s(), line: %d, m_pNxPlayer is NULL \n", __FUNCTION__, __LINE__);
		ui->progressBar->setRange(0, 100);
		ui->progressBar->setValue(0);
		UpdateDurationInfo( 0, 0 );
		return;
	}

	if(m_bIsInitialized)
	{
		int64_t iDuration = 0;
		int64_t iPosition = 0;

		NX_MEDIA_STATE state = m_pNxPlayer->GetState();
		if (state == MP_STATE_PAUSED ||
			state == MP_STATE_PLAYING)
		{
			iDuration = m_pNxPlayer->GetMediaDuration();
			iPosition = m_pNxPlayer->GetMediaPosition();
		}

		if( (0 > iDuration) || (0 > iPosition) )
		{
			iPosition = 0;
			iDuration = 0;
		}

		//	ProgressBar
		ui->progressBar->setValue(NANOSEC_TO_SEC(iPosition));
        UpdateDurationInfo(iPosition, iDuration);
    }
	else
	{
		ui->progressBar->setRange(0, 100);
		ui->progressBar->setValue(0);
		UpdateDurationInfo( 0, 0 );
	}
}


void MainWindow::UpdateDurationInfo(int64_t position, int64_t duration)
{
	qint64 cur_pos = NANOSEC_TO_SEC(position);
	qint64 dur = NANOSEC_TO_SEC(duration);

	QString tStr;
	if (cur_pos || dur)
	{
		QTime currentTime((cur_pos/3600)%60, (cur_pos/60)%60, cur_pos%60, (cur_pos*1000)%1000);
		QTime totalTime((dur/3600)%60, (dur/60)%60, dur%60, (dur*1000)%1000);
		QString format = "mm:ss";
		if (dur > 3600)
		{
			format = "hh:mm:ss";
		}
		tStr = currentTime.toString(format) + " / " + totalTime.toString(format);
	}
	ui->durationlabel->setText(tStr);
}

const char *NxGstEvent2String(NX_GST_EVENT event)
{
	switch(event)
	{
		case MP_EVENT_EOS:
			return "MP_EVENT_EOS";
		case MP_EVENT_DEMUX_LINK_FAILED:
			return "MP_EVENT_DEMUX_LINK_FAILED";
		case MP_EVENT_NOT_SUPPORTED:
			return "MP_EVENT_NOT_SUPPORTED";
		case MP_EVENT_GST_ERROR:
			return "MP_EVENT_GST_ERROR";
		case MP_EVENT_STATE_CHANGED:
			return "MP_EVENT_STATE_CHANGED";
		case MP_EVENT_SUBTITLE_UPDATED:
			return "MP_EVENT_SUBTITLE_UPDATED";
		case MP_EVENT_ERR_OPEN_AUDIO_DEVICE:
			return "MP_EVENT_ERR_OPEN_AUDIO_DEVICE";
		default:
			return NULL;
	};
	return NULL;
}

/* UEvent */
void MainWindow::slotDetectUEvent(QString action, QString devNode)
{
	if (!m_pDrmInfo)
	{
		NXLOGE("m_pDrmInfo is NULL");
		return;
	}

	if (action == "change")
	{
		int32_t ret = m_pDrmInfo->isHDMIConnected();
		if (ret == 1)
		{
			// connected
			HDMIStatusChanged(NX_EVENT_MEDIA_HDMI_CONNECTED);
		} else if (ret == 0)
		{
			// disconnected
			HDMIStatusChanged(NX_EVENT_MEDIA_HDMI_DISCONNECTED);
		}
	}
}

void MainWindow::statusChanged(int eventType, int eventData, void* param)
{
    NXLOGI("%s() eventType '%s'", __FUNCTION__, NxGstEvent2String((NX_GST_EVENT)eventType));
	switch (eventType)
	{
	case MP_EVENT_EOS:
	{
		NXLOGI("******** EndOfMedia !!!\n");
		PlayNextVideo();
		break;
	}
	case MP_EVENT_DEMUX_LINK_FAILED:
	{
		NXLOGW("******** MP_MSG_DEMUX_ERR !!!\n");
		PlayNextVideo();
		break;
	}
	case MP_EVENT_GST_ERROR:
	{
		NXLOGE("******** MP_EVENT_GST_ERROR !!!\n");
		StopVideo();
		break;
	}
	case MP_EVENT_ERR_OPEN_AUDIO_DEVICE:
	{
		NXLOGI("******** MP_MSG_ERR_OPEN_AUDIO_DEVICE\n");
		// message
		m_pMessageFrame->show();
		m_pMessageLabel->setText("ERR_OPEN_AUDIO_DEVICE");

		ui->progressBar->setValue(0);
		UpdateDurationInfo(0, 0);
		StopVideo();
		break;
	}
	case MP_EVENT_STATE_CHANGED:
	{
		m_current_status = (NX_MEDIA_STATE)eventData;
		NXLOGI("%s() New state [%d] m_fSpeed(%lf)", __FUNCTION__, m_current_status, m_fSpeed);
		bool isPlaying = (m_current_status == MP_STATE_PLAYING);
		bool isStopped = (m_current_status == MP_STATE_STOPPED);
		bool isSeekable = m_pNxPlayer->isSeekable();
		bool isVideoSpeedSupport = m_pNxPlayer->GetVideoSpeedSupport();

		ui->playButton->setEnabled(!isPlaying || (m_fSpeed != 1.0));
		//ui->playButton->setStyleSheet(!isPlaying ? btn_enabled:btn_disabled);
		ui->pauseButton->setEnabled(isPlaying);
		//ui->pauseButton->setStyleSheet(isPlaying ? btn_enabled:btn_disabled);
		ui->stopButton->setEnabled(!isStopped);
		//ui->stopButton->setStyleSheet(!isStopped ? btn_enabled:btn_disabled);
		ui->speedButton->setEnabled(isVideoSpeedSupport);
		ui->speedButton->setStyleSheet(isVideoSpeedSupport ? btn_enabled:btn_disabled);

		if (isPlaying)
		{
			bool isProSelectable = m_pNxPlayer->isProgramSelectable();
			bool isStrSelectable = m_pNxPlayer->isStreamSelectable();

			ui->nextProgramButton->setEnabled(isProSelectable);
			ui->nextProgramButton->setStyleSheet(isProSelectable ? btn_enabled:btn_disabled);

			ui->switchStreamButton->setEnabled(isStrSelectable);
			ui->switchStreamButton->setStyleSheet(isStrSelectable ? btn_enabled:btn_disabled);

			m_iDuration = m_pNxPlayer->GetMediaDuration();
			if (-1 == m_iDuration) {
				ui->progressBar->setMaximum(0);
			} else {
				ui->progressBar->setMaximum(NANOSEC_TO_SEC(m_iDuration));
			}
			ui->appNameLabel->setText(m_FileList.GetList(m_iCurFileListIdx));

			m_PosUpdateTimer.start(300);
		}
		else if (m_current_status == MP_STATE_PAUSED)
		{
			m_PosUpdateTimer.stop();
			m_SubtitleDismissTimer->stop();
		}

		if (m_current_status == MP_STATE_STOPPED)
		{
			DoPositionUpdate();
		}
		break;
	}
	case MP_EVENT_SUBTITLE_UPDATED:
	{
		SUBTITLE_INFO* m_pSubtitle = (SUBTITLE_INFO*)param;

		m_pCodec = QTextCodec::codecForName("EUC-KR");
		QString encResult = m_pCodec->toUnicode(m_pSubtitle->subtitleText);
		encResult.replace("&apos;", "'");
		ui->subTitleLabel->setText(encResult);
		
		int time_msec = (int)(m_pSubtitle->duration / (1000000));
	
		m_SubtitleDismissTimer->setSingleShot(true);
		m_SubtitleDismissTimer->start(time_msec);

		if (m_pSubtitle->subtitleText)
			free(m_pSubtitle->subtitleText);
		if (m_pSubtitle)
			free(m_pSubtitle);
		break;
	}
	default:
		break;
	}
}

void MainWindow::dismissSubtitle()
{
	ui->subTitleLabel->setText("");
	ui->subTitleLabel2->setText("");

	m_SubtitleDismissTimer->stop();
}

int32_t MainWindow::SaveInfo()
{
	if(NULL == m_pNxPlayer)
	{
		NXLOGW("%s(), line: %d, m_pNxPlayer is NULL \n", __FUNCTION__, __LINE__);
		return -1;
	}

	//save current media path
	m_listMutex.Lock();
	QString curPath = m_FileList.GetList(m_iCurFileListIdx);
	m_listMutex.Unlock();
	if(curPath.isEmpty() || curPath.isNull())
	{
		NXLOGE("current path is not valid\n");
		return -1;
	}

	// encode pCurPath(QString , unicode) to UTF-8
	QTextCodec* pCodec = QTextCodec::codecForName("UTF-8");		//pCodec  271752
	QTextEncoder* pEncoder = pCodec->makeEncoder();
	QByteArray encodedByteArray = pEncoder->fromUnicode(curPath);
	char* pCurPath = (char*)encodedByteArray.data();
	NXLOGI("Current path(%s)", pCurPath);

	//save current media position
    NX_MEDIA_STATE state = (m_pNxPlayer) ? m_pNxPlayer->GetState() : MP_STATE_STOPPED;
    if((MP_STATE_PLAYING == state)||(MP_STATE_PAUSED == state))
	{
		qint64 iCurPos = m_pNxPlayer->GetMediaPosition();
		if(0 > iCurPos)
		{
			NXLOGW("current position is not valid  iCurPos : %lld is set to 0", iCurPos);
			iCurPos = 0;
		}
		return iCurPos;
    }
    else if(MP_STATE_STOPPED == state)
	{
		return 0;
	}

	return 0;
}

bool MainWindow::SeekToPrev(int* iSavedPosition, const char *pPrevPath, int* iFileIdx)
{
	if(NULL == m_pNxPlayer)
	{
		NXLOGE("%s(), line: %d, m_pNxPlayer is NULL \n", __FUNCTION__, __LINE__);
		return false;
	}

	//find index in file list by path
	m_listMutex.Lock();
	if(0 < m_FileList.GetSize())
	{
		//media file list is valid
		//find pPrevPath in list

		int iIndex = m_FileList.GetPathIndex(QString::fromUtf8(pPrevPath));
		if(0 > iIndex)
		{
			NXLOGE("saved path does not exist in FileList\n");
			m_listMutex.Unlock();
			return false;
		}
		*iFileIdx = iIndex;
		m_listMutex.Unlock();
		return true;
	}
	else
	{
		NXLOGD("File List is not valid.. no media file or media scan is not done\n");
		NXLOGD("just try last path : %s", pPrevPath);
		m_bTryFlag = true;
		m_FileList.AddItem(QString::fromUtf8(pPrevPath));
		*iFileIdx = 0;
		m_listMutex.Unlock();
		return true;
	}

	return false;
}

bool MainWindow::StopVideo()
{
	if(NULL == m_pNxPlayer)
	{
		NXLOGW("%s(), line: %d, m_pNxPlayer is NULL \n", __FUNCTION__, __LINE__);
		return false;
	}

	if (-1 < m_pNxPlayer->Stop())
	{
		NXLOGI("%s() send MP_EVENT_STATE_CHANGED with stopped", __FUNCTION__);
		statusChanged((int)MP_EVENT_STATE_CHANGED, (int)MP_STATE_STOPPED, NULL);
	}
	CloseVideo();

	m_fSpeed = 1.0;
	ui->speedButton->setText("x 1");
	return true;
}

bool MainWindow::CloseVideo()
{
	NXLOGI("%s()", __FUNCTION__);

	if(NULL == m_pNxPlayer)
	{
		NXLOGW("%s(), line: %d, m_pNxPlayer is NULL \n", __FUNCTION__, __LINE__);
		return false;
	}
	m_bIsInitialized = false;
	ui->appNameLabel->setText("");

	StopSubTitle();

	if(0 > m_pNxPlayer->CloseHandle())
	{
		NXLOGE("%s(), line: %d, CloseHandle failed \n", __FUNCTION__, __LINE__);
		return false;
	}

	return true;
}

bool MainWindow::PlayNextVideo()
{
	NXLOGI("%s()", __FUNCTION__);
	if(NULL == m_pNxPlayer)
	{
		NXLOGW("%s(), line: %d, m_pNxPlayer is NULL \n", __FUNCTION__, __LINE__);
		return false;
	}

	if (m_pNxPlayer)
		m_pNxPlayer->resetStreamIndex();

	StopVideo();

	//	find next index
	if(0 != m_FileList.GetSize())
	{
		m_iCurFileListIdx = (m_iCurFileListIdx+1) % m_FileList.GetSize();
	}
	return PlayVideo();
}

bool MainWindow::PlayPreviousVideo()
{
	NXLOGI("%s()", __FUNCTION__);
	if(NULL == m_pNxPlayer)
	{
		NXLOGW("%s(), line: %d, m_pNxPlayer is NULL \n", __FUNCTION__, __LINE__);
		return false;
	}
	if (m_pNxPlayer)
		m_pNxPlayer->resetStreamIndex();

	StopVideo();

	//	Find previous index
	if(0 != m_FileList.GetSize())
	{
		m_iCurFileListIdx --;
		if( 0 > m_iCurFileListIdx )
			m_iCurFileListIdx = m_FileList.GetSize() -1;
	}
	return PlayVideo();
}

void MainWindow::PlaySeek()
{
	bool seekflag = false;
	int iSavedPosition = 0;

	NXLOGI("%s()", __FUNCTION__);
	if(NULL == m_pNxPlayer)
	{
		NXLOGW("%s(), line: %d, m_pNxPlayer is NULL \n", __FUNCTION__, __LINE__);
		return;
	}

	QString filePath = m_FileList.GetList(m_iCurFileListIdx);
	seekflag = SeekToPrev(&iSavedPosition, filePath.toStdString().c_str(), &m_iCurFileListIdx);

	PlayVideo();

	if(seekflag)
	{
		if (m_fSpeed > 1.0)
		{
			if (0 > m_pNxPlayer->SetVideoSpeed(1.0))
			{
				
			}
			else
			{
				m_fSpeed = 1.0;
				ui->speedButton->setText("x 1");
			}
		}
		//seek video
		SeekVideo( iSavedPosition );

		//seek subtitle
		ui->subTitleLabel->setText("");
		ui->subTitleLabel2->setText("");
		m_pNxPlayer->SeekSubtitle(iSavedPosition);
	}
}

bool MainWindow::PlayVideo()
{
	NX_MEDIA_STATE state;

	NXLOGI("%s()", __FUNCTION__);

	if(NULL == m_pNxPlayer) {
        NXLOGW("%s(), line: %d, m_pNxPlayer is NULL", __FUNCTION__, __LINE__);
		return false;
	}

	if( 0 == m_FileList.GetSize()) {
		NXLOGI("%s() Not found the file to play", __FUNCTION__);
		return false;
	}

	state = m_pNxPlayer->GetState();
	NXLOGI("%s() The previous state before playing is %d", __FUNCTION__, state);

	if(MP_STATE_PLAYING == state)
	{
		double video_speed = m_pNxPlayer->GetVideoSpeed();
		NXLOGW("%s() The current video speed(%f) in PLYAING state", __FUNCTION__, video_speed);
		if(1.0 == video_speed)
		{
			NXLOGW("%s() already playing", __FUNCTION__);
			return true;
		}
		else
		{
			/* When pressing 'play' button while playing the video with the speed x2, x4, ..., x16,
			 * play the video with the normal speed (x1).
			 */
			if (0 > m_pNxPlayer->SetVideoSpeed(1.0))
			{
				NXLOGE("%s() Failed to set video speed as 1.0", __FUNCTION__);
				return false;
			}
			else
			{
				m_fSpeed = 1.0;
				ui->speedButton->setText("x 1");
				m_pNxPlayer->Play();
			}
			return true;
		}
	}

	/* When pressing 'play' button in the paused state with the specific playback speed,
	 * it needs to play with the same playback speed.
	 */
	if((MP_STATE_PAUSED == state) || (MP_STATE_READY == state))
	{
		m_pNxPlayer->Play();
		return true;
	}
	else if(MP_STATE_STOPPED == state)
	{
		m_listMutex.Lock();
		if(0 < m_FileList.GetSize())
		{
			int iResult = -1;
			int iTryCount = 0;
			int iMaxCount = m_FileList.GetSize();
			while(0 > iResult)
			{
				m_fSpeed = 1.0;
				ui->speedButton->setText("x 1");

				// Test code for Thumbnail
				//m_pNxPlayer->MakeThumbnail(m_FileList.GetList(m_iCurFileListIdx).toStdString().c_str(),
				//							20 * 1000, 200, "/nexell/daudio/NxGstVideoPlayer/snapshot.jpg");

				ui->appNameLabel->setText(m_FileList.GetList(m_iCurFileListIdx));

				iResult = m_pNxPlayer->InitMediaPlayer(cbEventCallback, NULL,
													   m_FileList.GetList(m_iCurFileListIdx).toStdString().c_str(),
													   m_dspInfo);

                NXLOGI("%s() filepath:%s", __FUNCTION__, m_FileList.GetList(m_iCurFileListIdx).toStdString().c_str());
                if(iResult != 0) {
					NXLOGE("%s() Error! Failed to setup GStreamer(ret:%d)", __FUNCTION__, iResult);
                }

				iTryCount++;
				if(0 == iResult)
				{
                    NXLOGI("%s() *********** media init done! *********** ", __FUNCTION__);
					m_bIsInitialized = true;

					if( 0 == OpenSubTitle() )
					{
						PlaySubTitle();
					}

					if(0 > m_pNxPlayer->Play())
					{
						NXLOGE("NX_MPPlay() failed !!!");
                        iResult = -1; //retry with next file..
                    }
                    else
                    {					
						if(1.0 == m_pNxPlayer->GetVideoSpeed())
						{
							ui->speedButton->setText("x 1");
						}

						NXLOGI("%s() *********** media play done! *********** ", __FUNCTION__);
						m_listMutex.Unlock();
						return true;
					}
				}

				if(m_bTryFlag)
				{
					//This case is for playing back to last file
					//When there is no available file list but last played file path exists in config.xml,
					//videoplayer tries playing path that saved in config.xml .
					//If trying playing is failed, videoplayer should stop trying next file.
                    NXLOGI("%s(): Have no available contents!!", __FUNCTION__);
					m_bTryFlag = false;
					m_FileList.ClearList();
					m_listMutex.Unlock();
					return false;
				}

				if( iTryCount == iMaxCount )
				{
					//all list is tried, but nothing succeed.
                    NXLOGI("%s(): Have no available contents!!", __FUNCTION__);
					m_listMutex.Unlock();
					return false;
				}

                NXLOGW("%s(): MediaPlayer Initialization fail.... retry with next file", __FUNCTION__);
				m_iCurFileListIdx = (m_iCurFileListIdx+1) % m_FileList.GetSize();
				CloseVideo();
				NXLOGW("%s() Closed video and try to play next video '%s'"
						, __FUNCTION__, m_FileList.GetList(m_iCurFileListIdx).toStdString().c_str());
				m_pNxPlayer->resetStreamIndex();
			}	// end of while(0 > iResult)
		}		// end of if(0 < m_FileList.GetSize())
		else
		{
            NXLOGW("%s(): Have no available contents!! InitMediaPlayer is not tried", __FUNCTION__);
			m_listMutex.Unlock();
			return false;
		}
	}			// end of else if(MP_STATE_STOPPED == state)
	NXLOGW("%s() ------------------------------------------", __FUNCTION__);
	return true;
}

bool MainWindow::PauseVideo()
{
	NXLOGI("%s()", __FUNCTION__);
	if(NULL == m_pNxPlayer) {
		NXLOGE("%s(), line: %d, m_pNxPlayer is NULL", __FUNCTION__, __LINE__);
		return false;
	}

	NX_MEDIA_STATE state = m_pNxPlayer->GetState();
	if((MP_STATE_STOPPED == state) || (MP_STATE_READY == state)) {
		return false;
	}

    if(0 > m_pNxPlayer->Pause()) {
        NXLOGE( "%s(): Error! Failed to pause", __FUNCTION__);
		return false;
    }

	return true;
}

bool MainWindow::SeekVideo(int32_t mSec)
{
	NXLOGI("%s()", __FUNCTION__);
	if(NULL == m_pNxPlayer)
	{
		NXLOGE("%s(), line: %d, m_pNxPlayer is NULL \n", __FUNCTION__, __LINE__);
		return false;
	}

	NXLOGI("%s() seek to %d mSec", __FUNCTION__, mSec);
	if(0 > m_pNxPlayer->Seek(mSec))
	{
		return false;
	}

	return true;
}

void MainWindow::displayTouchEvent()
{
	if(false == m_bButtonHide)
	{
		m_bButtonHide = true;
		ui->progressBar->hide();
		ui->prevButton->hide();
		ui->playButton->hide();
		ui->pauseButton->hide();
		ui->nextButton->hide();
		ui->stopButton->hide();
		ui->playListButton->hide();
		ui->closeButton->hide();
		ui->durationlabel->hide();
		ui->appNameLabel->hide();
		ui->speedButton->hide();
		ui->nextProgramButton->hide();
		ui->switchStreamButton->hide();
		NXLOGI("**************** MainWindow:: Hide \n ");
	}
	else
	{
		NXLOGI("**************** MainWindow:: Show \n ");
		ui->progressBar->show();
		ui->prevButton->show();
		ui->playButton->show();
		ui->pauseButton->show();
		ui->nextButton->show();
		ui->stopButton->show();
		ui->playListButton->show();
		ui->closeButton->show();
		ui->durationlabel->show();
		ui->appNameLabel->show();
		ui->speedButton->show();
		ui->nextProgramButton->show();
		ui->switchStreamButton->show();
		m_bButtonHide = false;
	}
}

void MainWindow::on_closeButton_released()
{
	StopVideo();
	this->close();
}


//
//		Play Util
//

void MainWindow::getAspectRatio(int srcWidth, int srcHeight,
								int scrWidth, int scrHeight,
								int *pWidth,  int *pHeight)
{
	// Calculate Video Aspect Ratio
	int dspWidth = 0, dspHeight = 0;
	double xRatio = (double)scrWidth / (double)srcWidth;
	double yRatio = (double)scrHeight / (double)srcHeight;

	if( xRatio > yRatio )
	{
		dspWidth    = (int)((double)srcWidth * yRatio);
		dspHeight   = scrHeight;
	}
	else
	{
		dspWidth    = scrWidth;
		dspHeight   = (int)((double)srcHeight * xRatio);
	}

	*pWidth     = dspWidth;
	*pHeight    = dspHeight;
}


//
//		Play List Button
//
void MainWindow::on_playListButton_released()
{
	PlayListWindow *pPlayList = new PlayListWindow();
	pPlayList->setWindowFlags(Qt::Window|Qt::FramelessWindowHint);

	pPlayList->setList(&m_FileList);
	pPlayList->setCurrentIndex(m_iCurFileListIdx);

	if( pPlayList->exec() )
	{
		NXLOGI("OK ~~~~~~");
		m_iCurFileListIdx = pPlayList->getCurrentIndex();
		on_stopButton_released();
		on_playButton_released();
	}
	else
	{
		NXLOGI("Cancel !!!!!");
	}
	delete pPlayList;
}

//
// Subtitle Display Routine
//

void MainWindow::updateSubTitle()
{
	if (m_bSubThreadFlag)
	{
		NX_MEDIA_STATE state = m_pNxPlayer->GetState();
		if ((m_pNxPlayer) && (MP_STATE_PLAYING == state || MP_STATE_PAUSED == state))
		{
			QString encResult;
			int idx;
			qint64 curPos = m_pNxPlayer->GetMediaPosition();
			for (idx = m_pNxPlayer->GetSubtitleIndex(); idx <= m_pNxPlayer->GetSubtitleMaxIndex(); idx++)
			{
				if (dbg) {
					NXLOGD("%s GetSubtitleStartTime(%d) curPos: %lld",
							__FUNCTION__, m_pNxPlayer->GetSubtitleStartTime(), NANOSEC_TO_MSEC(curPos));
				}

				if (m_pNxPlayer->GetSubtitleStartTime() < NANOSEC_TO_MSEC(curPos))
				{
					char *pBuf = m_pNxPlayer->GetSubtitleText();
					encResult = m_pCodec->toUnicode(pBuf);
					//HTML
					//encResult = QString("%1").arg(m_pCodec->toUnicode(pBuf));	//&nbsp; not detected
					//encResult.replace( QString("<br>"), QString("\n")  );		//detected
					encResult.replace(QString("&nbsp;"), QString(" "));
					if (m_bButtonHide == false)
					{
						ui->subTitleLabel->setText(encResult);
						ui->subTitleLabel2->setText("");
					}
					else
					{
						ui->subTitleLabel->setText("");
						ui->subTitleLabel2->setText(encResult);
					}
				}
				else
				{
					break;
				}
				m_pNxPlayer->IncreaseSubtitleIndex();
			}
		}
	}
}

int MainWindow::OpenSubTitle()
{
	QString path = m_FileList.GetList(m_iCurFileListIdx);
	int lastIndex = path.lastIndexOf(".");
	char tmpStr[1024]={0};
	if((lastIndex == 0))
	{
		return -1;  //this case means there is no file that has an extension..
	}
	strncpy(tmpStr, (const char*)path.toStdString().c_str(), lastIndex);
	QString pathPrefix(tmpStr);
	QString subtitlePath;

	subtitlePath = pathPrefix + ".smi";

	//call library method
	int openResult = m_pNxPlayer->OpenSubtitle( (char *)subtitlePath.toStdString().c_str() );

	if ( 1 == openResult )
	{
		// case of open succeed
		m_pCodec = QTextCodec::codecForName(m_pNxPlayer->GetBestSubtitleEncode());
		if (NULL == m_pCodec)
			m_pCodec = QTextCodec::codecForName("EUC-KR");
		return 0;
	}else if( -1 == openResult )
	{
		//smi open tried but failed while fopen (maybe smi file does not exist)
		//should try opening srt
		subtitlePath = pathPrefix + ".srt";
		if( 1 == m_pNxPlayer->OpenSubtitle( (char *)subtitlePath.toStdString().c_str() ) )
		{
			m_pCodec = QTextCodec::codecForName(m_pNxPlayer->GetBestSubtitleEncode());
			if (NULL == m_pCodec)
				m_pCodec = QTextCodec::codecForName("EUC-KR");
			return 0;
		}else
		{
			//smi and srt both cases are tried, but open failed
			return -1;
		}
	}else
	{
		NXLOGI("parser lib OpenResult : %d\n",openResult);
		//other err cases
		//should check later....
		return -1;
	}
	return -1;
}

void MainWindow::PlaySubTitle()
{
	m_bSubThreadFlag = true;
}

void MainWindow::StopSubTitle()
{
	if(m_bSubThreadFlag)
	{
		m_bSubThreadFlag = false;
	}

	m_pNxPlayer->CloseSubtitle();

	ui->subTitleLabel->setText("");
	ui->subTitleLabel2->setText("");
}

void MainWindow::slotOk()
{
	if(m_bNotSupportSpeed)
	{
		m_bNotSupportSpeed = false;
		m_pMessageFrame->hide();
	}
	else
	{
		ui->progressBar->setValue(0);
		UpdateDurationInfo(0, 0);
		StopVideo();
		m_pMessageFrame->hide();
	}
}

// reSize
void MainWindow::resizeEvent(QResizeEvent *)
{
	if ((width() != DEFAULT_DSP_WIDTH) || (height() != DEFAULT_DSP_HEIGHT))
	{
		SetupUI();
	}
}

void MainWindow::SetupUI()
{
	ui->graphicsView->setGeometry(0,0,width(),height());

	float widthRatio = (float)width() / DEFAULT_DSP_WIDTH;
	float heightRatio = (float)height() / DEFAULT_DSP_HEIGHT;
	int rx, ry, rw, rh;

	rx = widthRatio * ui->progressBar->x();
	ry = heightRatio * ui->progressBar->y();
	rw = widthRatio * ui->progressBar->width();
	rh = heightRatio * ui->progressBar->height();
	ui->progressBar->setGeometry(rx, ry, rw, rh);

	rx = widthRatio * ui->prevButton->x();
	ry = heightRatio * ui->prevButton->y();
	rw = widthRatio * ui->prevButton->width();
	rh = heightRatio * ui->prevButton->height();
	ui->prevButton->setGeometry(rx, ry, rw, rh);

	rx = widthRatio * ui->playButton->x();
	ry = heightRatio * ui->playButton->y();
	rw = widthRatio * ui->playButton->width();
	rh = heightRatio * ui->playButton->height();
	ui->playButton->setGeometry(rx, ry, rw, rh);

	rx = widthRatio * ui->pauseButton->x();
	ry = heightRatio * ui->pauseButton->y();
	rw = widthRatio * ui->pauseButton->width();
	rh = heightRatio * ui->pauseButton->height();
	ui->pauseButton->setGeometry(rx, ry, rw, rh);

	rx = widthRatio * ui->nextButton->x();
	ry = heightRatio * ui->nextButton->y();
	rw = widthRatio * ui->nextButton->width();
	rh = heightRatio * ui->nextButton->height();
	ui->nextButton->setGeometry(rx, ry, rw, rh);

	rx = widthRatio * ui->stopButton->x();
	ry = heightRatio * ui->stopButton->y();
	rw = widthRatio * ui->stopButton->width();
	rh = heightRatio * ui->stopButton->height();
	ui->stopButton->setGeometry(rx, ry, rw, rh);

	rx = widthRatio * ui->playListButton->x();
	ry = heightRatio * ui->playListButton->y();
	rw = widthRatio * ui->playListButton->width();
	rh = heightRatio * ui->playListButton->height();
	ui->playListButton->setGeometry(rx, ry, rw, rh);

	rx = widthRatio * ui->durationlabel->x();
	ry = heightRatio * ui->durationlabel->y();
	rw = widthRatio * ui->durationlabel->width();
	rh = heightRatio * ui->durationlabel->height();
	ui->durationlabel->setGeometry(rx, ry, rw, rh);

	rx = widthRatio * ui->subTitleLabel->x();
	ry = heightRatio * ui->subTitleLabel->y();
	rw = widthRatio * ui->subTitleLabel->width();
	rh = heightRatio * ui->subTitleLabel->height();
	ui->subTitleLabel->setGeometry(rx, ry, rw, rh);

	rx = widthRatio * ui->subTitleLabel2->x();
	ry = heightRatio * ui->subTitleLabel2->y();
	rw = widthRatio * ui->subTitleLabel2->width();
	rh = heightRatio * ui->subTitleLabel2->height();
	ui->subTitleLabel2->setGeometry(rx, ry, rw, rh);

	rx = widthRatio * ui->closeButton->x();
	ry = heightRatio * ui->closeButton->y();
	rw = widthRatio * ui->closeButton->width();
	rh = heightRatio * ui->closeButton->height();
	ui->closeButton->setGeometry(rx, ry, rw, rh);

	rx = widthRatio * ui->speedButton->x();
	ry = heightRatio * ui->speedButton->y();
	rw = widthRatio * ui->speedButton->width();
	rh = heightRatio * ui->speedButton->height();
	ui->speedButton->setGeometry(rx, ry, rw, rh);

	rx = widthRatio * ui->switchStreamButton->x();
	ry = heightRatio * ui->switchStreamButton->y();
	rw = widthRatio * ui->switchStreamButton->width();
	rh = heightRatio * ui->switchStreamButton->height();
	ui->switchStreamButton->setGeometry(rx, ry, rw, rh);

	rx = widthRatio * ui->nextProgramButton->x();
	ry = heightRatio * ui->nextProgramButton->y();
	rw = widthRatio * ui->nextProgramButton->width();
	rh = heightRatio * ui->nextProgramButton->height();
	ui->nextProgramButton->setGeometry(rx, ry, rw, rh);
}
