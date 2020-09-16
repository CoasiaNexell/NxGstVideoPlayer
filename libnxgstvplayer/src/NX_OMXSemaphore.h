//------------------------------------------------------------------------------
//
//	Copyright (C) 2010 Nexell co., Ltd All Rights Reserved
//
//	Module      : Semaphore Module.
//	File        : 
//	Description :
//	Author      : Seong-O Park (ray@nexell.co.kr)
//	History     :
//------------------------------------------------------------------------------
#ifndef __NX_Semaphore_h__
#define __NX_Semaphore_h__

#include <stdint.h>

//	use semarphore instead of loop
//#define USE_SEMAPHORE	( 1 )

#define		NX_ESEM_TIMEOUT			ETIMEDOUT			//	Timeout
#define		NX_ESEM					-1
#define		NX_ESEM_OVERFLOW		-2					//	Exceed Max Value

typedef struct _NX_SEMAPHORE NX_SEMAPHORE;

struct _NX_SEMAPHORE{
	uint32_t nValue;
	uint32_t nMaxValue;
	pthread_cond_t hCond;
	pthread_mutex_t hMutex;
};

NX_SEMAPHORE *NX_CreateSem( uint32_t initValue, uint32_t maxValue );
void NX_DestroySem( NX_SEMAPHORE *hSem );
int32_t NX_PendSem( NX_SEMAPHORE *hSem );
int32_t NX_PostSem( NX_SEMAPHORE *hSem );
int32_t NX_PendTimedSem( NX_SEMAPHORE *hSem, uint32_t milliSeconds );

#endif	//	__NX_Semaphore_h__
