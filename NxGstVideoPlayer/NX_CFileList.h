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

#ifndef NX_CFILELIST_H
#define NX_CFILELIST_H

#include <QTime>
#include <dirent.h>

#include <vector>
#include <QString>
#include <QVector>
#include <QMutexLocker>

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "[CNX_FileList]"
#include <NX_Log.h>

class NX_CFileList
{
public:
	NX_CFileList();
	~NX_CFileList();

public:
	QVector <QString> m_FileList;
	QVector <QString> m_FileNameList;

	void MakeFileList(const char *pDir, const char **pExtension, int numExtension);

	//for scanner
	//add, remove  ... cannot use when MakeFileList used
	void AddItem(QString path)
	{
		QMutexLocker lock(&m_ListLock);
		if(m_bIsMadeByLocal)
		{
			NXLOGD("m_bIsMadeByLocal check \n");
			return;
		}
		m_FileList.push_back(path);
	}

	void RemoveItem(int index)
	{
		QMutexLocker lock(&m_ListLock);
		if(m_bIsMadeByLocal) return;
		m_FileList.remove(index);
	}

	void ClearList()
	{
		QMutexLocker lock(&m_ListLock);
		m_FileList.clear();
		m_FileNameList.clear();
	}

	int32_t GetSize()
	{
		QMutexLocker lock(&m_ListLock);
		return m_FileList.size();
	}

	QString GetList( int32_t index )
	{
		QMutexLocker lock(&m_ListLock);
		if( index<0 ||  index >= m_FileList.size() )
		{
			return NULL;
		}
		return m_FileList[index];
	}

	QString GetFileName( int32_t index )
	{
		QMutexLocker lock(&m_ListLock);
		if( index<0 ||  index >= m_FileNameList.size() )
		{
			return NULL;
		}
		return m_FileNameList[index];
	}

	int GetPathIndex(QString path)
	{
		QMutexLocker lock(&m_ListLock);
		if( path.isNull() || path.isEmpty() )
		{
			return -1;
		}
		return m_FileList.indexOf(path);
	}

private:
	int32_t IsDir(QString path);
	int32_t GetDir(QString dir);

	//	FileList Database
private:
	int32_t m_iNumExtension;
	const char** m_ppExtentsions;
	QMutex m_ListLock;		//	File List Mutex
	bool m_bIsMadeByLocal;			//	set true when MakeFileList called
};

#endif // NX_CFILELIST_H
