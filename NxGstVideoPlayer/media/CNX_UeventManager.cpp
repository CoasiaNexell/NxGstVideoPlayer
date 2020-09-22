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

#include <string.h>
#include <libudev.h>

#include "CNX_UeventManager.h"

#define BUF_SIZE 16*1024

#define LOG_TAG "[uevent]"
#include <NX_Log.h>

//------------------------------------------------------------------------------
CNX_UeventManager::CNX_UeventManager()
{
	m_udev = udev_new();
	if (!m_udev)
	{
		NXLOGE("udev_new()");
		return;
	}

	m_monitor = udev_monitor_new_from_netlink(m_udev, "udev");
	if (!m_monitor)
	{
		NXLOGE("udev_monitor_new_from_netlink");
		return;
	}

	udev_monitor_filter_add_match_subsystem_devtype(m_monitor, "block", NULL);
	udev_monitor_filter_add_match_subsystem_devtype(m_monitor, "drm", NULL);
	udev_monitor_enable_receiving(m_monitor);

	m_fd  = udev_monitor_get_fd(m_monitor);
	if (m_fd  < 0)
	{
		NXLOGE("udev_monitor_get_fd");
		return;
	}

	m_pSocketNotifier = new QSocketNotifier(m_fd, QSocketNotifier::Read, this);
	connect(m_pSocketNotifier, SIGNAL(activated(int)), this, SLOT(slotNotification()));
}

//------------------------------------------------------------------------------
CNX_UeventManager::~CNX_UeventManager()
{

}

void CNX_UeventManager::slotNotification()
{
	struct udev_device *dev;
	struct udev_list_entry *entries;
	const char *action;
	const char *node;
	const char *subsystem;
	const char *type;
	const char *sysname;

	if (!m_monitor)
		return;

	dev = udev_monitor_receive_device(m_monitor);
	if (!dev)
		goto cleanup;

	action = udev_device_get_action(dev);
	if (!action) {
		NXLOGE("udev_device_get_action");
		goto cleanup;
	}

	node = udev_device_get_devnode(dev);
	if (!node) {
		NXLOGE("udev_device_get_devnode");
		goto cleanup;
	}

	subsystem = udev_device_get_subsystem(dev);
	if (!subsystem) {
		NXLOGE("udev_device_get_subsystem");
		goto cleanup;
	}

	type = udev_device_get_devtype(dev);
	if (!type) {
		NXLOGE("udev_device_get_devtype");
		goto cleanup;
	}

	sysname = udev_device_get_sysname(dev);
	if (!sysname) {
		NXLOGE("udev_device_get_sysname");
		goto cleanup;
	}

	entries = udev_device_get_properties_list_entry(dev);
	if (!entries) {
		NXLOGE("udev_device_get_properties_list_entry");
		goto cleanup;
	}

#if 0
	NXLOGI("[%s] action = %s", __func__, action);
	NXLOGI("[%s] dev node = %s", __func__, node);
	NXLOGI("[%s] subsystem = %s", __func__, subsystem);
	NXLOGI("[%s] driver = %s", __func__, driver);
	NXLOGI("[%s] type = %s", __func__, type);
	NXLOGI("[%s] sysname = %s", __func__, sysname);
#endif

	if (0 == strcmp(type, "partition"))
	{
		/*******************************************************************
		 * NOTIFY
		 *  1. action : add or remove
		 *  2. device node : /dev/sda1
		 ******************************************************************/
		if (0 == strcmp(action, "add"))
		{
			emit signalDetectUEvent(action, node);
		}
		else if (0 == strcmp(action, "remove"))
		{
			emit signalDetectUEvent(action, node);
		}
	}
	else if (0 == strcmp(type, "drm_minor"))
	{
		/*******************************************************************
		 * NOTIFY
		 *  1. action : change
		 *  2. device node : /dev/dri/card0
		 ******************************************************************/
		if (0 == strcmp(action, "change"))
		{
			emit signalDetectUEvent(action, node);
		}
	}

cleanup:
	udev_device_unref(dev);
}
