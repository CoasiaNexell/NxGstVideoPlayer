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
//	Module		: 
//	File		: 
//	Description	: 
//	Author		: 
//	Export		: 
//	History		: 
//
//------------------------------------------------------------------------------

#ifndef __NX_DDRMINFO_H__
#define __NX_DRMINFO_H__

#include <stdint.h>

#define SET_CRTC	1

#define PLANE_TYPE_VIDEO		0
#define PLANE_TYPE_RGB			1

#define CRTC_IDX_PRIMARY		0
#define CRTC_IDX_SECONDARY		(CRTC_IDX_PRIMARY+1)

#define DEFAULT_RGB_LAYER_IDX	1

typedef struct MP_DRM_PLANE_INFO {
	int32_t		iConnectorID;		//  Dsp Connector ID
	int32_t		iPlaneId;			//  DRM Plane ID
	int32_t		iCrtcId;			//  DRM CRTC ID
	int32_t		iDrmFd;				// 	DRM FD
} MP_DRM_PLANE_INFO;

class CNX_DrmInfo
{
public:
	CNX_DrmInfo();
	~CNX_DrmInfo();

private:
	int32_t find_video_plane(int fd, int crtcIdx,
                              uint32_t *connId, uint32_t *crtcId,
                              uint32_t *planeId);
	int32_t find_rgb_plane(int fd, int32_t crtcIdx,
								  int32_t layerIdx, uint32_t *connId,
                        		  uint32_t *crtcId, uint32_t *planeId);

public:
	bool 	OpenDrm();
	void 	CloseDrm();

	bool 	isHDMIConnected();
	bool 	setMode(int crtcIdx, int findRgb, int layerIdx, int width, int height);
	int32_t FindPlaneForDisplay(int32_t in_crtcIdx,
        	     			    int32_t in_findRgb,
            	            	int32_t in_layerIdx,
                	        	MP_DRM_PLANE_INFO *pDrmPlaneInfo);
#ifdef SET_CRTC
	int32_t SetCrtc(uint32_t crtcIdx, uint32_t crtcID, uint32_t connId,
					uint32_t width, uint32_t height);
#endif

private:
	int32_t	m_drmFd;
	MP_DRM_PLANE_INFO m_display;
};

#endif	// __NX_DRMINFO_H__