#ifndef CNX_Util_H
#define CNX_Util_H

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <assert.h>
#include <stdlib.h>

#include <unistd.h>
#include <sys/time.h>

#define		NX_ESEM					-1
#define		NX_ESEM_OVERFLOW		-2					//	Exceed Max Value

#define TIME_FORMAT "%u:%02u:%02u"

#define MSEC_TO_SEC(a)         a / 1000
#define NANOSEC_TO_MSEC(a)     a / (1000 * 1000)
#define NANOSEC_TO_SEC(a)      a / (1000 * 1000 * 1000)

#define MSEC_TO_NANOSEC(a)      a * (1000 * 1000)
#define SEC_TO_NANOSEC(a)      a * (1000 * 1000 * 1000)

////////////////////////////////////////////////////////////////////////////////
//
//	NX_CMutex
//
class CNX_AutoLock {
public:
	CNX_AutoLock( pthread_mutex_t *pLock )
		: m_pLock(pLock)
	{
		pthread_mutex_lock( m_pLock );
	}
	~CNX_AutoLock()
	{
		pthread_mutex_unlock( m_pLock );
	}

protected:
	pthread_mutex_t *m_pLock;

private:
	CNX_AutoLock (const CNX_AutoLock &Ref);
	CNX_AutoLock &operator=(CNX_AutoLock &Ref);
};


class NX_CMutex
{
public:
	NX_CMutex()
	{
		pthread_mutex_init( &m_hMutex, NULL );
	}

	~NX_CMutex()
	{
		pthread_mutex_destroy( &m_hMutex );
	}

public:
	void Lock()
	{
		pthread_mutex_lock( &m_hMutex );
	}

	void Unlock()
	{
		pthread_mutex_unlock( &m_hMutex );
	}

private:
	pthread_mutex_t		m_hMutex;

private:
	NX_CMutex (const NX_CMutex &Ref);
	NX_CMutex &operator=(const NX_CMutex &Ref);
};

////////////////////////////////////////////////////////////////////////////////

struct _NX_VDEC_SEMAPHORE{
	uint				value;
	pthread_cond_t		cond;
	pthread_mutex_t		mutex;
};
typedef struct _NX_VDEC_SEMAPHORE NX_VDEC_SEMAPHORE;

NX_VDEC_SEMAPHORE *VDecSemCreate( int init );
void VDecSemDestroy( NX_VDEC_SEMAPHORE *pSem );
int VDecSemPend( NX_VDEC_SEMAPHORE *pSem );
int VDecSemPost( NX_VDEC_SEMAPHORE *pSem );
int VDecSemSignal( NX_VDEC_SEMAPHORE *pSem );


//
//	Semaphore functions
//
class NX_CSemaphore{
public:
	NX_CSemaphore()
		: m_iValue	( 0 )
		, m_iMax	( 1 )
		, m_iInit	( 0 )
		, m_bReset	( false )
		, m_iTot    ( 0 )
	{
		pthread_cond_init ( &m_hCond,  NULL );
		pthread_mutex_init( &m_hMutex, NULL );
	}

	NX_CSemaphore( int32_t iMax, int32_t iInit )
		: m_iValue	( iInit )
		, m_iMax	( iMax )
		, m_iInit	( iInit )
		, m_bReset	( false )
	{
		pthread_cond_init ( &m_hCond,  NULL );
		pthread_mutex_init( &m_hMutex, NULL );
	}

	~NX_CSemaphore()
	{
		ResetSignal();
		pthread_cond_destroy( &m_hCond );
		pthread_mutex_destroy( &m_hMutex );
	}

public:
	int32_t Post()
	{
		int32_t iRet = 0;

		pthread_mutex_lock( &m_hMutex );
		if(m_iValue >= m_iMax)
		{
			iRet = NX_ESEM_OVERFLOW;
		}
		else
		{
			m_iValue ++;
		}
		pthread_cond_signal( &m_hCond );
		pthread_mutex_unlock( &m_hMutex );

		return iRet;
	}

	int32_t Pend()
	{
		int32_t iRet = 0;
		pthread_mutex_lock( &m_hMutex );

		if( m_iValue == 0 )
		{
			iRet = pthread_cond_wait(&m_hCond,&m_hMutex);

			if( 0 != iRet )
			{
				iRet = NX_ESEM;
			}
			else
			{
				m_iValue--;
			}
		}
		pthread_mutex_unlock( &m_hMutex );
		return iRet;
	}

	void ResetSignal()
	{
		pthread_mutex_lock ( &m_hMutex );
		m_bReset = true;

		for (int32_t i = 0; i<m_iMax; i++)
		{
			pthread_cond_signal( &m_hCond );
		}

		m_iTot = 0;
		pthread_mutex_unlock( &m_hMutex );
	}

	void ResetValue()
	{
		pthread_mutex_lock( &m_hMutex );
		m_iValue = m_iInit;
		m_bReset = false;
		pthread_mutex_unlock( &m_hMutex );
	}

	int32_t GetValue()
	{
		pthread_mutex_lock( &m_hMutex );
		int32_t iValue = m_iValue;
		pthread_mutex_unlock( &m_hMutex );
		return iValue;
	}

private:
	enum { MAX_SEM_VALUE = 1024 };

	pthread_cond_t  m_hCond;
	pthread_mutex_t m_hMutex;
	int32_t			m_iValue;
	int32_t			m_iMax;
	int32_t			m_iInit;
	int32_t			m_bReset;
	int32_t			m_iTot;
private:
	NX_CSemaphore (const NX_CSemaphore &Ref);
	NX_CSemaphore &operator=(const NX_CSemaphore &Ref);
};


////////////////////////////////////////////////////////////////////////////////
//
//	NX_CGetTickCount
//
class NX_GetTickCount
{
public:
	NX_GetTickCount()
	{
	}

	~NX_GetTickCount()
	{
	}

public:
	int64_t GetTime()   //ms
	{
		int64_t Ret;
		struct timeval	tv;
		struct timezone	zv;
		gettimeofday( &tv, &zv );
		Ret = ((int64_t)tv.tv_sec)*1000 + (int64_t)(tv.tv_usec/1000);
		return Ret;
	}

private:
	NX_GetTickCount (const NX_GetTickCount &Ref);
	NX_GetTickCount &operator=(const NX_GetTickCount &Ref);
};

#endif // CNX_Util_H
