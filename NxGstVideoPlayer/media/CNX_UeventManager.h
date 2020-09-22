//------------------------------------------------------------------------------
//
//	Copyright (C) 2017 Nexell Co. All Rights Reserved
//	Nexell Co. Proprietary & Confidential
//
//	NEXELL INFORMS THAT THIS CODE AND INFORMATION IS PROVIDED "AS IS" BASE
//  AND	WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING
//  BUT NOT LIMITED TO THE IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS
//  FOR A PARTICULAR PURPOSE.
//
//	Module		:
//	File		:
//	Description	:
//	Author		:
//	Export		:
//	History		:
//
//------------------------------------------------------------------------------

#ifndef __CNX_UEVENTMANAGER_H__
#define __CNX_UEVENTMANAGER_H__

#include <QObject>
#include <QSocketNotifier>

class CNX_UeventManager : public QObject
{
	Q_OBJECT

signals:
	void signalDetectUEvent(QString action, QString devNode);

private slots:
	void slotNotification();

public:
	CNX_UeventManager();
	~CNX_UeventManager();

private:
	struct udev *m_udev;
	struct udev_monitor *m_monitor;
	int m_fd;
	QSocketNotifier *m_pSocketNotifier;
};

#endif	// __CNX_UEVENTMANAGER_H__
