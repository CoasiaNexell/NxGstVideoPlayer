#include "CNX_SubtitleParser.h"

#ifdef __SYSLOG_D__
#define LOG_TAG "[SubtitleParser]"
#include <NX_Log.h>
#else
#define NXLOGV(...)	printf(__VA_ARGS__)
#define NXLOGD(...)	printf(__VA_ARGS__)
#define NXLOGI(...)	printf(__VA_ARGS__)
#define NXLOGN(...)	printf(__VA_ARGS__)
#define NXLOGW(...)	printf(__VA_ARGS__)
#define NXLOGE(...)	printf(__VA_ARGS__)
#endif

CNX_SubtitleParser::CNX_SubtitleParser()
//	Initialize Parameter
	: m_bIsParsed(false)
	, m_iSyncTime(0)
	, m_hFileSize(0)
	, m_pFileBuf(NULL)
	, m_hFileType(FILE_TYPE_NONE)
	, m_pStartTimeArray(NULL)
	, m_pEndTimeArray(NULL)
	, m_ppSubtitleTextArray(NULL)
	, m_pParsedSubtitleArray(NULL)
	, m_pTEXTCODECLIST(NULL)
	, m_iSizeOfTEXTCODECLIST(0)
{
}

CNX_SubtitleParser::~CNX_SubtitleParser()
{
	NX_SPClose();
}

void CNX_SubtitleParser::NX_SPIncreaseIndex()
{
	if(m_bIsParsed)
	{
		if( (m_iCurrentIndex+1) <= m_iMaxIndex )
		{
			this->m_iCurrentIndex++;
		}
		else
		{
			this->m_iCurrentIndex = m_iMaxIndex;
		}
	}
	else
	{
		return;
	}
}

int CNX_SubtitleParser::NX_SPGetMaxIndex()
{
	if(m_bIsParsed)
	{
		return this->m_iMaxIndex;
	}
	else
	{
		return 0;
	}
}

int CNX_SubtitleParser::NX_SPGetIndex()
{
	if(m_bIsParsed)
	{
		return this->m_iCurrentIndex;
	}
	else
	{
		return 0;
	}
}

void CNX_SubtitleParser::NX_SPSetIndex(int idx)
{
	if(m_bIsParsed)
	{
		if( idx < 0)
		{
			m_iCurrentIndex = 0;
		}
		else
		{
			if( idx > m_iMaxIndex)
			{
				m_iCurrentIndex = m_iMaxIndex;
			}
			else
			{
				m_iCurrentIndex = idx;
			}
		}
	}
	else
	{
		return;
	}
}

int CNX_SubtitleParser::NX_SPOpen(const char* fullpath)
{
	if(m_bIsParsed)
	{
		NX_SPClose();
	}
	//
	//	Load all data from subtile file to m_pFileBuf
	//
	FILE *hFile = fopen(fullpath, "r");
	if(NULL == hFile)
	{
		return -1;
	}
	//	Initialize file type int none
	m_hFileType = FILE_TYPE_NONE;
	//	find file size
	fseek(hFile, 0, SEEK_END);
	long position = ftell(hFile);
	fseek(hFile, 0, SEEK_SET);

	//printf("File (%s) size = %d", fullpath, position);
	m_hFileSize = position;
	if( m_pFileBuf )
		free(m_pFileBuf);
	m_pFileBuf = (unsigned char *)malloc(m_hFileSize);
	if( NULL == m_pFileBuf )
	{
		//	out of memory
		fclose( hFile );
		return -2;
	}
	fread(m_pFileBuf, 1, m_hFileSize, hFile); //whole texts are read and saved at m_pFileBuf
	fclose( hFile );

	//	check smi or srt
	//	Main subtile parsing
	if( !IsSAMI() )
	{
		if( !IsSrt() ){
			return -3;
		}
		else
		{
			if(!ParsingSrtSubtitle()) return -5;
		}
	}
	else
	{
		if(!ParsingSamiSubtitle()) return -4;
	}
	m_bIsParsed = true;
	m_iCurrentIndex = 0;
	SetParsedSubtitleArray();
	FindEncodingType();
	NXLOGD("==================Subtitle(%d) Open Succeed=================\n",NX_SPGetMaxIndex());
	NXLOGD("================================================================\n");
	return 1;
}//Open

bool CNX_SubtitleParser::IsSAMI()
{
	if( m_pFileBuf==NULL || m_hFileSize==0 )
	{
		return false;
	}

	char *pBuf = (char *)m_pFileBuf;
	//	Find From start to end "<SAMI"
	for( int i=0 ; i<m_hFileSize-5 ; i++ )
	{
		if( 0 == strncasecmp("<SAMI", pBuf, 5) )
		{
			m_hFileType = FILE_TYPE_SMI;
			NXLOGD("\n==================================================================\n");
			NXLOGD("==========================SAMI found==============================\n");

			return true;
		}
		pBuf++;
	}
	return false;
}

bool CNX_SubtitleParser::IsSrt()
{
	//srt syntax is "starttime" --> "endtime"
	//check starttime and endtime
	//
	//if (endtime-starttime) >= 0   , the file is srt
	int startHour, startMinute, startSeconds, startMilSeconds, endHour, endMinute, endSeconds, endMilSeconds;
	int startTimeMilliseconds = 0;
	int endTimeMilliseconds = 0;
	int result = 0;

	//printf("IsSrt() , m_hFileSize : %d",m_hFileSize);
	if( m_pFileBuf==NULL || m_hFileSize==0 )
	{
		return false;
	}

	char *pBuf = (char *)m_pFileBuf;

	//printf("IsSrt() loop start ============================================");
	for( int i=0 ; i<m_hFileSize-3 ; i++ )
	{
		if( 0 == strncasecmp(":", pBuf, 1) )
		{
			//printf(" : found ============================================");
			// 00:00:00,000 --> 00:00:00,000
			// ':' position-2 == start of line..
			// sscanf(start of line , filter , var1,var2,,,,)

			result = sscanf( (pBuf-2), "%d:%d:%d,%d --> %d:%d:%d,%d",
							 &startHour, &startMinute, &startSeconds, &startMilSeconds,
							 &endHour, &endMinute, &endSeconds, &endMilSeconds);

			//printf("============================================sscanf result  : %d\n" , result);
			//printf("============================================\n");
			//printf("startHour  : %d\n" , startHour );
			//printf("startMinute  : %d\n" , startMinute );
			//printf("startSeconds  : %d\n" , startSeconds );
			//printf("startMilSeconds  : %d\n" , startMilSeconds );
			//printf("endHour  : %d\n" , endHour );
			//printf("endMinute  : %d\n" , endMinute );
			//printf("endSeconds  : %d\n" , endSeconds );
			//printf("endMilSeconds  : %d\n" , endMilSeconds );

			startTimeMilliseconds = ( startHour * 3600*1000 + startMinute * 60*1000 + startSeconds * 1000 + startMilSeconds );
			endTimeMilliseconds  = ( endHour * 3600*1000 + endMinute * 60*1000 + endSeconds * 1000 + endMilSeconds );
			//printf("==================================================================\n");
			//printf("startTimeMilliseconds : %d\n" , startTimeMilliseconds );
			//printf("endTimeMilliseconds : %d\n" , endTimeMilliseconds);

			if(result == 8)
			{
				if( (endTimeMilliseconds - startTimeMilliseconds) > 0 )
				{
					m_hFileType = FILE_TYPE_SRT;
					NXLOGD("\n==================================================================\n");
					NXLOGD("===========================srt found==============================\n");

					return true;
				}
			}
		}
		pBuf++;
	}
	return false;
}//IsSrt

bool CNX_SubtitleParser::ParsingSamiSubtitle()
{
	bool isSubtitleFounded = false;
	char *pBuf = (char *)m_pFileBuf;
	if( m_pFileBuf==NULL || m_hFileSize==0 )
	{
		NXLOGD("ParsingSubtitle() , result of fread is nothing");
		return isSubtitleFounded;
	}
	// ========================================================================================
	// case SAMI
	// <SYNC Start=500><P Class=KRCC>    28 chars
	int maxIndex = ((m_hFileSize - (m_hFileSize%30))/30 +1 );
	int currentIndex = 0;
	int result = 0;

	this->m_iMaxIndex = maxIndex; //will be updated when parsing finished
	// results.. ===============================================================================================
	m_pStartTimeArray = (int *)malloc( sizeof(int)*maxIndex );
	m_ppSubtitleTextArray = (char **)malloc( sizeof(char *)*maxIndex );
	memset(m_ppSubtitleTextArray, 0, sizeof(char *)*maxIndex);

	// parsing smi start========================================================================================
	// parse smi
	//	시작점, 사이즈  ==> Database
	//	SAMI ----------- HEAD  ---- TITLE
	//			+              +
	//			+              +--- STYLE
	//			+
	//		    + ------ BODY  ----- SYNC
	//			               +---- SYNC
	//					       +---- SYNC
	//						     ...
	//	<HEAD, <BODY
	//	Find From start to end "<SAMI"

	// for detecting multi language var start ==========
	//	bool isMultiLanguageDetermined = false;
	//	int countLanguage = 0;
	// for detecting multi language var end ==========

	for( int i=0 ; i<m_hFileSize-12 ; i++ )
	{
		if( 0 == strncasecmp("<HEAD", pBuf, 5) )
		{
			//printf("Found HEAD!!!\n");
			//DETECT STYLE FOR MULTILANGUAGE
			// detect lang:%s
			// .classname { Name:한국어; lang:ko-KR; SAMIType:CC; }

			//			m_iNumberOfLanguage = 0;
			//			char* langPosition = (pBuf);
			//			char* templanguage;

			//			while( true )
			//			{//what if language info is not exists

			//				if(0 == strncasecmp("</STYLE>", langPosition, 8))
			//				{
			//					break;
			//				}
			//				if(0 == strncasecmp("</HEAD>", langPosition, 7))
			//				{
			//					break;
			//				}
			//				if(0 == strncasecmp("<BODY>", langPosition, 6))
			//				{
			//					break;
			//				}
			//				if(0 == strncasecmp("<SYNC>", langPosition, 6))
			//				{
			//					break;
			//				}

			//				if( 1 == sscanf( (langPosition), "lang:%s;", &templanguage) )
			//				{
			//					m_iNumberOfLanguage++;
			//					printf("detected language : %s\n",templanguage);
			//				}
			//				langPosition++;
			//			}
			//			//m_iNumberOfLanguage == the number of language that subtitle file supports
			//			//classname will be needed if subtitle supports multilanguage..

			//			if(0==m_iNumberOfLanguage)
			//			{
			//				//there is no STYLE information about language,, wrong syntax subtitle file
			//				printf("no language detected.. subtitle file could have wrong syntax\n");
			//			}

			//			// if the number of language <= 1  .. code behavior is same as before..

			//			if(languageCount>1)
			//			{
			//				m_bIsMultiLanguageDetected = true;
			//				language = (char*)malloc(sizeof(char)*languageCount);
			//			}



		}//detect head for multi language
		//		else if ( 0 == strncasecmp("<BODY", pBuf, 5) )
		//		{
		//			//m_BodyStart = pBuf;
		//		}

		else if( 0 == strncasecmp("<SYNC Start=", pBuf, 12) )
		{
			//printf("Found <SYNC Start=\n");
			int time = 0;
			result = sscanf( (pBuf+6), "Start=%d>", &time );

			if(result==1)
			{
				//printf("founded start time : %d\n", time);
				m_pStartTimeArray[currentIndex] = time;
			}
		}
		else if( 0 == strncasecmp("<P Class=", pBuf, 9) )
		{
			char* subStartPosition = (pBuf);
			char* subEndPosition = (pBuf+1);

			while( 0 != strncasecmp(">", subStartPosition, 1) )
			{
				subStartPosition++;
			}
			while( true )
			{
				if(0 == strncasecmp("<SYNC", subEndPosition, 5)) break;
				if(0 == strncasecmp("</SYNC", subEndPosition, 6)) break;
				if(0 == strncasecmp("</BODY", subEndPosition, 6)) break;
				if(0 == strncasecmp("</SAMI", subEndPosition, 6)) break;
				if(1 == (((char *)m_pFileBuf+m_hFileSize)-subEndPosition)) break;

				subEndPosition++;
			}
			subStartPosition++;
			subEndPosition--;
			int range = (subEndPosition-subStartPosition);

			if(range > 0)
			{
				m_ppSubtitleTextArray[currentIndex] = (char*)malloc( sizeof(char)*(range+1) );
				memcpy(m_ppSubtitleTextArray[currentIndex], subStartPosition, range);
				m_ppSubtitleTextArray[currentIndex][range] = '\0';

				//printf("m_ppSubtitleTextArray[%d] : %s\n", currentIndex,m_ppSubtitleTextArray[currentIndex]);
				currentIndex++;
				isSubtitleFounded = true;
			}else
			{
				//printf("founded range : %d\n", range);
				//err!!
				//do nothing
				//skip to next
				//printf("time founded but subtitle parsing occurs error\n");
			}
		}
		pBuf++;
	}

	//printf("used Index : %d\n", currentIndex);
	this->m_iMaxIndex = (currentIndex-1);
	this->m_iLastStartTime = m_pStartTimeArray[m_iMaxIndex];
	// parsing smi end========================================================================================


	return isSubtitleFounded;
}//ParsingSubtitle

bool CNX_SubtitleParser::ParsingSrtSubtitle()
{
	bool isSubtitleFounded = false;
	char *pBuf = (char *)m_pFileBuf;
	if( m_pFileBuf==NULL || m_hFileSize==0 )
	{
		//printf("ParsingSubtitle() , result of fread is nothing\n");
		return isSubtitleFounded;
	}
	// ========================================================================================
	//case : srt
	//startTime --> endTime
	//1									== 1  char
	//00:00:00,000 --> 00:00:00,000		== 27 chars
	//00:00:00,000						== 12 chars
	//subtitles
	// so if the file is srt subtitle file,
	//whole file size in bytes can be devided by 27
	//the number of avobe calculation, can be used in caculation of malloc parameter
	//the number of subtitle is less than (m_hFileSize - (m_hFileSize%28))/28 +1
	int maxIndex = ((m_hFileSize - (m_hFileSize%27))/27 +1 );
	int currentIndex = 0;
	int startHour, startMinute, startSeconds, startMilSeconds, endHour, endMinute, endSeconds, endMilSeconds;
	int result = 0;

	this->m_iMaxIndex = maxIndex; //will be updated when parsing finished
	//for subtitle text
	bool timeFlag = false;
	bool subFlag = false;

	char* subStartPosition = pBuf;
	char* subEndPosition;
	// results.. ===============================================================================================
	m_pStartTimeArray = (int *)malloc( sizeof(int)*maxIndex );
	m_pEndTimeArray = (int *)malloc( sizeof(int)*maxIndex );
	m_ppSubtitleTextArray = (char **)malloc( sizeof(char*)*maxIndex );
	memset(m_ppSubtitleTextArray, 0, sizeof(char*)*maxIndex);

	// parsing srt start========================================================================================

	for( int i=0 ; i<m_hFileSize ; i++ )
	{
		//printf("current char == *pBuf : %c\n", *pBuf);
		if( 0 == strncasecmp(":", pBuf, 1) )
		{
			// 00:00:00,000 --> 00:00:00,000
			// ':' position-2 == start of line..
			// sscanf(start of line , filter , var1,var2,,,,)
			result = sscanf( (pBuf-2), "%d:%d:%d,%d --> %d:%d:%d,%d",
							 &startHour, &startMinute, &startSeconds, &startMilSeconds,
							 &endHour, &endMinute, &endSeconds, &endMilSeconds);


			if(result == 8)
			{
				//subflag is set to true after time period founded
				if(subFlag)
				{
					isSubtitleFounded = true;
					//find subtitle number and set subtitle endposition
					//2 cases
					//01 subtitle number exists
					//02 subtitle number does not exist
					//printf("finding endpoisition of subtitle\n");
					int iteratedNumber = 0;

					char* tempTextEndPosition = (pBuf-3);
					bool numberFounded = false;
					while( (tempTextEndPosition-iteratedNumber) > subStartPosition )//subtitleTextStartPosition[currentIndex] )
					{
						//check below
						//0~9 number , space , enter
						//when the other char is founded,
						//set EndPosition immediately

						char value = *(tempTextEndPosition-iteratedNumber);
						if( ((0<=(value-'0'))&&((value-'0')<=9)) || (value==' ') || ((value=='\n') || (value=='\r')) )
						{
							//01 subtitle number exists
							if((0<=(value-'0'))&&((value-'0')<=9))
							{
								numberFounded = true;
							}
							if(numberFounded)
							{
								if(( (value=='\n') || (value=='\r') ))
								{
									numberFounded = false;
									//set end
									subEndPosition = tempTextEndPosition-iteratedNumber;
									int sizeOfSubtitleText = subEndPosition - subStartPosition;
									//printf("sizeOfSubtitleText : %d\n", sizeOfSubtitleText);

									m_ppSubtitleTextArray[currentIndex] = (char *)malloc(sizeof(char)*(sizeOfSubtitleText+1));
									memcpy(m_ppSubtitleTextArray[currentIndex], subStartPosition, sizeOfSubtitleText);
									m_ppSubtitleTextArray[currentIndex][sizeOfSubtitleText] = '\0';

									//printf("founded subtitle : %s\n", m_ppSubtitleTextArray[currentIndex]);
									subFlag = false;
									currentIndex++;
									break;
								}
							}
						}else
						{
							//set end
							subEndPosition = tempTextEndPosition-iteratedNumber;
							int sizeOfSubtitleText = subEndPosition - subStartPosition;

							m_ppSubtitleTextArray[currentIndex] = (char *)malloc(sizeof(char)*(sizeOfSubtitleText+1));
							memcpy(m_ppSubtitleTextArray[currentIndex], subStartPosition, sizeOfSubtitleText);
							m_ppSubtitleTextArray[currentIndex][sizeOfSubtitleText] = '\0';

							//printf("founded subtitle : %s\n", m_ppSubtitleTextArray[currentIndex]);
							subFlag = false;
							currentIndex++;
							break;
						}
						iteratedNumber++;
					}
				}
				timeFlag = true;
				m_pStartTimeArray[currentIndex] = ( startHour * 3600*1000 + startMinute * 60*1000 + startSeconds * 1000 + startMilSeconds );
				m_pEndTimeArray[currentIndex] = ( endHour * 3600*1000 + endMinute * 60*1000 + endSeconds * 1000 + endMilSeconds );
				//printf("m_pStartTimeArray[%d]: %d\n", currentIndex, m_pStartTimeArray[currentIndex]);
				//printf("m_pEndTimeArray[%d] : %d\n", currentIndex, m_pEndTimeArray[currentIndex]);
				//printf("currentIndex : %d\n", currentIndex);
			}
		}

		//check if enter start ========================================================================================
		//window '\r\n' , linux '\n' , mac '\r'
		if( ((pBuf[0]=='\r')&&(pBuf[1]=='\n')) || (*(pBuf)=='\n') || (*(pBuf)=='\r') )
		{
			//if timeFlag is true
			//next pointer is startpoint of subtitle area
			if( timeFlag )
			{
				subFlag = true;
				//subtitle start point is pBuf+1
				subStartPosition = (pBuf+1);

				timeFlag = false;
				//if time period again, subStartPosition = (pBuf+1); refreshed
			}
		}
		//check if enter end ========================================================================================
		pBuf++;
	}

	//when file read loop finished, set last subtitle endposition
	subEndPosition = (char *)m_pFileBuf+m_hFileSize-1;
	int sizeOfSubtitleText = subEndPosition - subStartPosition;

	m_ppSubtitleTextArray[currentIndex] = (char *)malloc(sizeof(char)*(sizeOfSubtitleText+1));
	memcpy(m_ppSubtitleTextArray[currentIndex], subStartPosition, sizeOfSubtitleText );
	m_ppSubtitleTextArray[currentIndex][sizeOfSubtitleText] = '\0';

	//printf("founded subtitle : %s\n", m_ppSubtitleTextArray[currentIndex]);

	this->m_iMaxIndex = currentIndex;
	this->m_iLastStartTime = m_pStartTimeArray[m_iMaxIndex];

	// parsing srt end========================================================================================
	return isSubtitleFounded;
}//ParsingSrtSubtitle

int CNX_SubtitleParser::NX_SPGetStartTime()
{
	if(m_bIsParsed)
	{
		return (this->m_pStartTimeArray[m_iCurrentIndex]+ m_iSyncTime);
	}
	else
	{
		return 0;
	}
}

int CNX_SubtitleParser::NX_SPGetEndTime()
{
	if(m_bIsParsed)
	{
		if(m_hFileType == FILE_TYPE_SRT)
		{
			return (this->m_pEndTimeArray[m_iCurrentIndex]+ m_iSyncTime);
		}
		else if(m_hFileType == FILE_TYPE_SMI)
		{
			if(m_iCurrentIndex == m_iMaxIndex)
			{
				return -1;//INT_MAX;  // could be problem
			}
			else
			{//next start time - 1 millisecond
				return (this->m_pStartTimeArray[m_iCurrentIndex+1]+ m_iSyncTime -1);
			}
		}
	}
	else
	{
		return 0;
	}

	return 0;
}

char* CNX_SubtitleParser::NX_SPGetSubtitle()
{
	if(m_bIsParsed)
	{
		return this->m_ppSubtitleTextArray[m_iCurrentIndex];
	}
	else
	{
		return NULL;
	}
}

void CNX_SubtitleParser::NX_SPChangeSubtitleSync(int milliseconds)
{
	if(m_bIsParsed)
	{
		m_iSyncTime = milliseconds;
	}
	else
	{
		return;
	}
}

int CNX_SubtitleParser::NX_SPGetSubtitleSync()
{
	if(m_bIsParsed)
	{
		return m_iSyncTime;
	}
	else
	{
		return 0;
	}
}

int CNX_SubtitleParser::NX_SPSeekSubtitleIndex(int milliseconds)
{
	//returns index for input time
	//smi is okay but,
	//in srt case, further check needed (check endTime)
	if(m_bIsParsed)
	{
		if(milliseconds > m_iLastStartTime)
		{
			return m_iMaxIndex;
		}

		int centerIdx = (m_iMaxIndex - m_iMaxIndex%2)/2;

		int firstIdx = 0;
		int lastIdx = 0;
		int idx;

		if(milliseconds < (m_pStartTimeArray[centerIdx] + m_iSyncTime) )
		{
			lastIdx = centerIdx-1;
		}
		else
		{
			firstIdx = centerIdx;
			lastIdx = m_iMaxIndex;
		}

		bool seekFlag = false;
		for( idx = firstIdx ; idx < (lastIdx+1) ; idx++ )
		{
			if( milliseconds <= (m_pStartTimeArray[idx]+ m_iSyncTime) )
			{
				if( (idx-1)<0 )
				{
					this->m_iCurrentIndex = 0;
				}
				else
				{
					this->m_iCurrentIndex = (idx-1);
				}
				seekFlag = true;
			}

			if(seekFlag) break;
		}
		NXLOGD("NX_SPSeekSubtitleIndex seek done, founded Index : %d\n",m_iCurrentIndex);
		return m_iCurrentIndex;
	}
	else
	{
		NXLOGD("NX_SPSeekSubtitleIndex did not work\n");
		return 0;
	}
}//NX_SPSeekSubtitleIndex

void CNX_SubtitleParser::SetParsedSubtitleArray()
{
	if(m_bIsParsed)
	{
		m_pParsedSubtitleArray = (PARSED_SUBTITLE*)malloc( sizeof(PARSED_SUBTITLE)*(m_iMaxIndex+1 ) );
		int idx;
		for(idx = 0 ; idx <= m_iMaxIndex ; idx++)
		{
			NX_SPSetIndex(idx);
			m_pParsedSubtitleArray[idx].startTime = NX_SPGetStartTime();
			m_pParsedSubtitleArray[idx].endTime = NX_SPGetEndTime();
			m_pParsedSubtitleArray[idx].subtitleTextString = NX_SPGetSubtitle();
			NX_SPIncreaseIndex();
		}
		NX_SPSetIndex(0);
	}
	else
	{
		return;
	}
}

PARSED_SUBTITLE CNX_SubtitleParser::NX_SPGetParsedSubtitleArray()
{
	if(m_bIsParsed)
	{
		return m_pParsedSubtitleArray[m_iCurrentIndex];
	}
	else
	{
		NXLOGW("NX_SPGetParsedSubtitleArray err\n");
		m_pParsedSubtitleArray = NULL;
		return m_pParsedSubtitleArray[0];
	}
}

PARSED_SUBTITLE CNX_SubtitleParser::NX_SPGetParsedSubtitleArray(int index)
{
	if(m_bIsParsed)
	{
		if(index < 0)
		{
			return m_pParsedSubtitleArray[0];
		}
		else if(index > m_iMaxIndex)
		{
			return m_pParsedSubtitleArray[m_iMaxIndex];
		}
		else
		{
			return m_pParsedSubtitleArray[index];
		}
	}
	else
	{
		NXLOGW("subtitle file is not opened, useage err\n");
		NXLOGW("NX_SPGetParsedSubtitleArray err\n");
		m_pParsedSubtitleArray = NULL;
		return m_pParsedSubtitleArray[0];
	}
}

void CNX_SubtitleParser::NX_SPClose()
{
	if(m_pFileBuf)
	{
		free(m_pFileBuf);
		m_pFileBuf = NULL;
	}

	if(m_pStartTimeArray)
	{
		free(m_pStartTimeArray);
		m_pStartTimeArray = NULL;
	}

	if(m_pStartTimeArray)
	{
		free(m_pStartTimeArray);
		m_pStartTimeArray = NULL;
	}

	if(m_pEndTimeArray)
	{
		free(m_pEndTimeArray);
		m_pEndTimeArray = NULL;
	}

	if(m_ppSubtitleTextArray)
	{
		int i;
		for(i = 0 ; i <= m_iMaxIndex ; i++)
		{
			free(m_ppSubtitleTextArray[i]);
		}
		free(m_ppSubtitleTextArray);
		m_ppSubtitleTextArray = NULL;
	}

	if(m_pParsedSubtitleArray)
	{
		free(m_pParsedSubtitleArray);
		m_pParsedSubtitleArray = NULL;
	}

	if(m_pTEXTCODECLIST)
	{
		free(m_pTEXTCODECLIST);
		m_pTEXTCODECLIST = NULL;
	}

	m_bIsParsed = false;
	m_iMaxIndex = 0;
	m_iCurrentIndex = 0;
	m_iLastStartTime = 0;
	m_iSyncTime = 0;
	m_hFileSize = 0;
	m_hFileType = FILE_TYPE_NONE;
	m_iSizeOfTEXTCODECLIST = 0;
}

bool CNX_SubtitleParser::NX_SPIsParsed()
{
	return m_bIsParsed;
}

int CNX_SubtitleParser::NX_SPGetTEXTCODECLIST( TEXTCODECLIST ** codec )
{
	if( NULL == m_pTEXTCODECLIST)
		return -1;

	*codec = m_pTEXTCODECLIST;
	return m_iSizeOfTEXTCODECLIST;
}

const char* CNX_SubtitleParser::NX_SPGetBestTextEncode()
{
	if(NULL == m_pTEXTCODECLIST)
	{
		NXLOGI("no codec list.. return EUC-KR\n");
		return "EUC-KR";
	}

	return m_pTEXTCODECLIST[0].encode;
}

void CNX_SubtitleParser::FindEncodingType()
{
	int32_t matches;
	char buf[2048];
	UErrorCode status = U_ZERO_ERROR;
	UCharsetDetector *csd = ucsdet_open( &status );

	int usedlength = 0;
	int sublength = 0;

	if( m_bIsParsed )
	{
		buf[0] = 0;
		NX_SPSetIndex(0);
		for( int32_t idx = 0 ; idx <= NX_SPGetMaxIndex() ; idx++ )
		{
			usedlength = strlen(buf);
			sublength = strlen(NX_SPGetSubtitle());
			//printf("FindEncodingType usedlength = %d\n",usedlength);
			//printf("FindEncodingType sublength = %d\n",sublength);
			if( (usedlength+sublength) >= (2048-2) )
			{
				NXLOGW("FindEncodingType break at idx : %d\n",idx);
				NXLOGW("FindEncodingType usedlength = %d\n",usedlength);
				break;
			}

			strncat(buf, NX_SPGetSubtitle(), sizeof(buf));

			NX_SPIncreaseIndex();
		}
		NX_SPSetIndex(0);
	}

	//
	ucsdet_setText( csd, buf, strlen(buf), &status );
	const UCharsetMatch **ucma = ucsdet_detectAll( csd, &matches, &status );

	if( 0 != matches)
	{
		m_pTEXTCODECLIST = (TEXTCODECLIST*)malloc(sizeof(TEXTCODECLIST)*matches);

		if (NULL == m_pTEXTCODECLIST) return;

		m_iSizeOfTEXTCODECLIST = matches;
	}

	for( int32_t i= 0 ; i < matches ; i++ )
	{
		const char *encname = ucsdet_getName(ucma[i], &status);
		int confidence = ucsdet_getConfidence(ucma[i], &status);
		//printf("%d : %s , %d\n", i, encname, confidence);

		m_pTEXTCODECLIST[i].encode = encname;
		m_pTEXTCODECLIST[i].confidence = confidence;
	}
	NXLOGI("Founded BestEncode : %s , Confidence : %d\n", m_pTEXTCODECLIST[0].encode, m_pTEXTCODECLIST[0].confidence);
	ucsdet_close(csd);
}

const char* CNX_SubtitleParser::NX_SPFindStringEncode(const char* str)
{
	int32_t matches;
	UErrorCode status = U_ZERO_ERROR;
	UCharsetDetector *csd = ucsdet_open( &status );

	int strlength = 0;

	strlength = strlen(str);

	if( (strlength) >= (255-2) )
	{
		printf("FindStringEncode input string is too long! input str length : %d\n",strlength);
		printf("default (EUC-KR) returned\n");
		ucsdet_close(csd);
		return "EUC-KR";
	}

	//ucsdet_setText( csd, buf, strlen(buf), &status );
	ucsdet_setText( csd, str, strlen(str), &status );
	const UCharsetMatch **ucma = ucsdet_detectAll( csd, &matches, &status );

	if( 0 != matches)
	{
		const char* encode = ucsdet_getName(ucma[0], &status);
		int confidence = ucsdet_getConfidence(ucma[0], &status);
		ucsdet_close(csd);
		printf("Founded BestEncode : %s , Confidence : %d\n", encode, confidence);
		return encode;
	}else
	{
		printf("ICU library could not found encode..\n");
		printf("default (EUC-KR) returned\n");
		ucsdet_close(csd);
		return "EUC-KR";
	}
}
