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
//	Module		: Thumbnail Class
//	File		:
//	Description	:
//	Author		:
//	Export		:
//	History		:
//
//------------------------------------------------------------------------------
#include <stdlib.h>
#include <stdio.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm/drm_fourcc.h>

#include <QDir>
#include <QTextStream>
#include <CNX_DrmInfo.h>
#define LOG_TAG "[CNX_DrmInfo]"
#include <NX_Log.h>

#include <errno.h>

#ifdef SET_CRTC
#include <dp.h>
struct dp_framebuffer* m_DefaultFb;
struct dp_device *	   m_DrmDevice;
#endif

CNX_DrmInfo::CNX_DrmInfo()
    :m_drmFd(-1)
{

}

CNX_DrmInfo::~CNX_DrmInfo()
{

}

bool
CNX_DrmInfo::OpenDrm()
{
    m_drmFd = drmOpen("nexell", NULL);
    if(0 > m_drmFd)
	{
        NXLOGE("%s() Fail, drmOpen().", __FUNCTION__);
        return false;
    }
    return true;
}

void
CNX_DrmInfo::CloseDrm()
{
    NXLOGI("%s()", __FUNCTION__);
    // ReleaseFb
    if(m_DefaultFb)
	{
		dp_framebuffer_delfb2(m_DefaultFb);
		dp_framebuffer_free(m_DefaultFb);
		m_DefaultFb = NULL;
	}

    // drmClose
    if (-1 != m_drmFd)
    {
        drmClose(m_drmFd);
        m_drmFd = -1;
    }
}

bool
CNX_DrmInfo::isHDMIConnected()
{
	const char* path = "/sys/devices/platform/c0000000.soc/c0102800.display_drm/drm/card0/card0-HDMI-A-1/status";

    QFile file(path);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        NXLOGE("%s() Failed to open file", __FUNCTION__);
        return false;
    }
	else
	{
		QTextStream stream(&file);
		const char* status = stream.readAll().toStdString().c_str();
		file.close();

		NXLOGI("%s() %s", __FUNCTION__, status);
		int length = strlen(status)-1;
		if (strncmp(status, "connected", length) == 0) {
			return true;
		} else if (strncmp(status, "disconnected", length) == 0) {
			return false;
		} else {
			return false;
		}
    }
	return false;
}

bool
CNX_DrmInfo::setMode(int crtcIdx, int findRgb, int layerIdx, int width, int height)
{
    int32_t ret = -1;

    m_display.iConnectorID = -1;
	m_display.iCrtcId      = -1;
	m_display.iPlaneId     = -1;
	m_display.iDrmFd	   = -1;

    // VIDEO : connId = 51, crtcId = 39, planeId = 40
    if (0 > FindPlaneForDisplay(crtcIdx, findRgb,
                            layerIdx, &m_display)) {
        NXLOGE("%s() cannot found video format for %dth crtc", __FUNCTION__, crtcIdx);
    } else {
        NXLOGI("%s() m_display(%d, %d, %d, %d)", __FUNCTION__,
                m_display.iConnectorID, m_display.iCrtcId,
                m_display.iPlaneId, m_display.iDrmFd);
        ret = SetCrtc(crtcIdx, m_display.iCrtcId, m_display.iConnectorID,
                     width, height);
    }

    NXLOGI("%s, ret(%d)", __FUNCTION__, ret);
    return (ret == -1) ? false:true;
}

int32_t
CNX_DrmInfo::find_video_plane(int fd, int crtcIdx,
                              uint32_t *connId, uint32_t *crtcId,
                              uint32_t *planeId)
{
    uint32_t possible_crtcs = 0;
    drmModeRes *res;
    drmModePlaneRes *pr;
    drmModePlane *plane;
    uint32_t i, j;
    int32_t found = 0;

    res = drmModeGetResources(fd);

    if( crtcIdx >= res->count_crtcs )
        goto ErrorExit;

    *crtcId = res->crtcs[ crtcIdx ];
    *connId = res->connectors[ crtcIdx ];

    possible_crtcs = 1<<crtcIdx;

    drmSetClientCap(fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);
    pr = drmModeGetPlaneResources( fd );

    for( i=0 ; i<pr->count_planes ; ++i )
    {
        plane = drmModeGetPlane( fd, pr->planes[i] );
        if( plane->possible_crtcs & possible_crtcs )
        {
            for( j=0 ; j<plane->count_formats ; j++ )
            {
                if( plane->formats[j]==DRM_FORMAT_YUV420 ||
                    plane->formats[j]==DRM_FORMAT_YVU420 ||
                    plane->formats[j]==DRM_FORMAT_UYVY ||
                    plane->formats[j]==DRM_FORMAT_VYUY ||
                    plane->formats[j]==DRM_FORMAT_YVYU ||
                    plane->formats[j]==DRM_FORMAT_YUYV )
                {
                    found = 1;
                    *planeId = plane->plane_id;
                }
            }
        }
    }
    drmModeFreeResources(res);
    return found?0:-1;
ErrorExit:
    drmModeFreeResources(res);
    return -1;
}

int32_t 
CNX_DrmInfo::find_rgb_plane(int fd, int32_t crtcIdx,
                            int32_t layerIdx, uint32_t *connId,
                            uint32_t *crtcId, uint32_t *planeId)
{
    uint32_t possible_crtcs = 0;
    drmModeRes *res;
    drmModePlaneRes *pr;
    drmModePlane *plane;
    uint32_t i, j;
    int32_t found = 0;
    int32_t findIdx = 0;
    int32_t isRgb = 0;

    res = drmModeGetResources(fd);
    if( crtcIdx >= res->count_crtcs )
        goto ErrorExit;

    *crtcId = res->crtcs[ crtcIdx ];
    *connId = res->connectors[ crtcIdx ];

    possible_crtcs = 1<<crtcIdx;

    drmSetClientCap(fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);
    pr = drmModeGetPlaneResources( fd );

    for( i=0 ; i<pr->count_planes ; i++ )
    {
        plane = drmModeGetPlane( fd, pr->planes[i] );
        if( plane->possible_crtcs & possible_crtcs )
        {
            isRgb = 0;
            for( j=0 ; j<plane->count_formats ; j++ )
            {
                if( plane->formats[j]==DRM_FORMAT_ABGR8888 ||
                    plane->formats[j]==DRM_FORMAT_RGBA8888 ||
                    plane->formats[j]==DRM_FORMAT_XBGR8888 ||
                    plane->formats[j]==DRM_FORMAT_RGBX8888 ||
                    plane->formats[j]==DRM_FORMAT_RGB888 ||
                    plane->formats[j]==DRM_FORMAT_BGR888 )
                {
                    isRgb = 1;
                }
            }

            if( isRgb )
            {
                if( findIdx == layerIdx )
                {
                    found = 1;
                    *planeId = plane->plane_id;
                    break;
                }
                findIdx++;
            }
        }
    }
    drmModeFreeResources(res);
    return found?0:-1;
ErrorExit:
    drmModeFreeResources(res);
    return -1;
}

int32_t
CNX_DrmInfo::FindPlaneForDisplay(int32_t in_crtcIdx,
        	                     int32_t in_findRgb, int32_t in_layerIdx,
                	             MP_DRM_PLANE_INFO *out_pDrmPlaneInfo)
{
    uint32_t connId = 0;
    uint32_t crtcId = 0;
    uint32_t planeId = 0;

    if(0 > m_drmFd)
	{
        NXLOGE("%s() Fail, drmOpen().", __FUNCTION__);
        return -1;
    }

    if(in_findRgb)   // RGB Layer
    {
        if(0 == find_rgb_plane(m_drmFd, in_crtcIdx, in_layerIdx, &connId, &crtcId, &planeId))
        {
			NXLOGI("%s() [RGB%d] : in_crtcIdx = %d(%s) connId = %d, crtcId = %d, planeId = %d",
                    __FUNCTION__, in_layerIdx, in_crtcIdx, (in_crtcIdx==0)?"PRIMARY":"SECONDARY", connId, crtcId, planeId);
        }
        else
        {
            NXLOGE("%s() cannot found video format for %dth crtc", __FUNCTION__, in_crtcIdx);
            return -1;
        }
    }
    else            // Video Layer
    {
        if(0 == find_video_plane(m_drmFd, in_crtcIdx, &connId, &crtcId, &planeId))
        {
            NXLOGI("%s() [VIDEO] : in_crtcIdx = %d(%s) connId = %d, crtcId = %d, planeId = %d",
                    __FUNCTION__, in_crtcIdx, (in_crtcIdx==0)?"PRIMARY":"SECONDARY", connId, crtcId, planeId);
        }
        else
        {
            NXLOGE("%s() cannot found video format for %dth crtc", __FUNCTION__, in_crtcIdx);
            return -1;
        }
    }

    out_pDrmPlaneInfo->iConnectorID     = connId;
    out_pDrmPlaneInfo->iCrtcId        	= crtcId;
    out_pDrmPlaneInfo->iPlaneId         = planeId;
    out_pDrmPlaneInfo->iDrmFd           = m_drmFd;

    return 0;
}

#ifdef SET_CRTC
int32_t
CNX_DrmInfo::SetCrtc(uint32_t crtcIdx, uint32_t crtcId, uint32_t connId, uint32_t width, uint32_t height)
{
    int ret = -1;
	m_DefaultFb = NULL;
	drmModeCrtcPtr pCrtc = NULL;

	pCrtc = drmModeGetCrtc(m_drmFd , crtcId);
	if(pCrtc->buffer_id == 0 &&  crtcIdx == CRTC_IDX_SECONDARY)
	{
		drmModeRes *resources = NULL;
		drmModeModeInfo* mode = NULL;
		drmModeConnector *connector = NULL;
		int32_t i, area;
		struct dp_plane *plane;
		uint32_t format;
		int32_t err;
		int32_t d_idx = 0, p_idx = 1;

		resources = drmModeGetResources(m_drmFd);

		m_DrmDevice = dp_device_open(m_drmFd);

		plane = dp_device_find_plane_by_index(m_DrmDevice, d_idx, p_idx);
		if (!plane) {
			NXLOGE("%s() no overlay plane found", __FUNCTION__);
			return ret;
		}

		err = dp_plane_supports_format(plane, DRM_FORMAT_ARGB8888);
		if (!err) {
			NXLOGE("%s() fail : no matching format found", __FUNCTION__);
			return ret;
		} else {
			format = DRM_FORMAT_ARGB8888;
		}

		m_DefaultFb = dp_framebuffer_create(m_DrmDevice, format, width, height, 0);
		if (!m_DefaultFb) {
			NXLOGE("%s() fail : framebuffer create Fail", __FUNCTION__);
			return ret;
		}
		err = dp_framebuffer_addfb2(m_DefaultFb);

		if (err < 0) {
			NXLOGE("%s() fail : framebuffer add Fail", __FUNCTION__);
			if (m_DefaultFb)
				dp_framebuffer_free(m_DefaultFb);
			return ret;
		}

		for (i = 0; i < resources->count_connectors; i++) {
			connector = drmModeGetConnector(m_drmFd, resources->connectors[i]);
			if ((connector->connector_id == connId) && (connector->connection == DRM_MODE_CONNECTED)) {
				/* it's connected, let's use this! */
                NXLOGI("%s() it's connected(%d), let's use this!", __FUNCTION__, connId);
				break;
			}

			drmModeFreeConnector(connector);
			connector = NULL;
		}

		if (!connector) {
			/* we could be fancy and listen for hotplug events and wait for
			* a connector..
			*/
			NXLOGE("%s() no connected connector!", __FUNCTION__);
			return ret;
		}

		for (i = 0, area = 0; i < connector->count_modes; i++) {
			drmModeModeInfo *current_mode = &connector->modes[i];

			if (current_mode->type & DRM_MODE_TYPE_PREFERRED) {
				mode = current_mode;
			}

			int current_area = current_mode->hdisplay * current_mode->vdisplay;
			if (current_area > area) {
				mode = current_mode;
				area = current_area;
			}
		}

		ret = drmModeSetCrtc(m_drmFd, crtcId, m_DefaultFb->id, 0, 0,
				&connId, 1, mode);
        if (ret) {
                NXLOGE("%s() failed to set mode: %s", __FUNCTION__, strerror(errno));
                return -1;
        } else {
            NXLOGI("%s() succeed to set mode, crtcId(%d), connId(%d)", __FUNCTION__, crtcId, connId);
        }
	}
    return ret;
}
#endif