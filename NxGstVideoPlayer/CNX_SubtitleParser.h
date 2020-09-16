//	simple routine
//
//	Open() --> loop start --> GetAPI() --> loop end --> Close()
//

//	simple example pseudo code:
//
// <make variables..>
//
//  CNX_SubtitleParser SubtitleParser = new CNX_SubtitleParser();
//  CNX_SubtitleParser.PARSED_SUBTITLE Subtitle;
//
// <call Open Method>
//  if( 1 == SubtitleParser.NX_SPOpen("PATH_of_Subtitle_File") )
//  {
//	  int startIdx = 0;
//	  int endIdx = SubtitleParser.NX_SPGetMaxIndex();
//	  for(int idx = startIdx ; idx <= endIdx ; idx++ )
//	  {
//		  Subtitle = SubtitleParser.NX_SPGetParsedSubtitleArray(idx);
//
// <wait until below if() true>
//		  if(Subtitle.startTime < CurrentVideoPlaytime)
//		  {
//			  UI.show (Subtitle.subtitleTextString);
//		  }
//	  }
//  }
//	SubtitleParser.NX_SPClose();
//  delete SubtitleParser;

#ifndef SUBTITLEPARSER_H
#define SUBTITLEPARSER_H
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unicode/ucnv.h>
#include <unicode/ucsdet.h>
#include <unicode/ustring.h>

struct PARSED_SUBTITLE{
	int startTime;
	int endTime;
	char* subtitleTextString;
};

struct TEXTCODECLIST{
	const char* encode;
	int confidence;
};

class CNX_SubtitleParser
{

public:
	CNX_SubtitleParser();
	virtual ~CNX_SubtitleParser();
	enum{
		FILE_TYPE_ERR = -1,
		FILE_TYPE_NONE,
		FILE_TYPE_SMI,
		FILE_TYPE_SRT,
	};

	//  return case of Open Function
	//  -1 : fopen (file) error, check file path
	//  -2 : file is too big = out of memory
	//  -3 : input file is neither smi nor srt subtitle file
	//  -4 : file structure is smi but
	//       subtitle does not exist or has wrong syntax form
	//  -5 : file structure is srt but
	//       subtitle does not exist or has wrong syntax form
	//   1 : open succeed
	int NX_SPOpen(const char* fullpath);

	// only after Open function succeed, below functions will work
	void NX_SPIncreaseIndex();		// increase m_iCurrentIndex
	int NX_SPGetMaxIndex();			// returns m_iMaxIndex
	int NX_SPGetIndex();			// returns m_iCurrentIndex
	void NX_SPSetIndex(int idx);	// set m_iCurrentIndex

	int NX_SPGetStartTime();		// returns m_pStartTimeArray[m_iCurrentIndex]
	int NX_SPGetEndTime();			// returns m_pEndTimeArray[m_iCurrentIndex]
	char* NX_SPGetSubtitle();		// returns m_ppSubtitleTextArray[m_iCurrentIndex]

	int NX_SPGetSubtitleSync();		// returns m_iSyncTime
	void NX_SPChangeSubtitleSync(int milliseconds);	// add input(milliseconds) to time vars
	int NX_SPSeekSubtitleIndex(int milliseconds);	// return index.. need to test... especially srt case

	PARSED_SUBTITLE NX_SPGetParsedSubtitleArray();			// return m_pParsedSubtitleArray[m_iCurrentIndex]
	PARSED_SUBTITLE NX_SPGetParsedSubtitleArray(int index);	// return m_pParsedSubtitleArray[index]

	int NX_SPGetTEXTCODECLIST( TEXTCODECLIST ** codec );	//returns m_iSizeOfTEXTCODECLIST or -1(error)
	const char* NX_SPGetBestTextEncode();					//returns best encode
	const char* NX_SPFindStringEncode(const char* str);		//returns best encode for input str

	void NX_SPClose();

	bool NX_SPIsParsed();			// return m_bIsParsed (true if API is ready to use(is Open completed))

private:
	bool m_bIsParsed;				// change to true when subtitle is parsed
	int m_iMaxIndex;				// will be set when subtitle is parsed (same as the size of Parsing Result arrays)
	int m_iCurrentIndex;			// will be set to 0 when subtitle is parsed
	int m_iLastStartTime;			// will be set when subtitle is parsed
	int m_iSyncTime;				// will be set when NX_SPChangeSubtitleSync(int) is called
	int32_t m_hFileSize;			// total size of subtitle file
	unsigned char *m_pFileBuf;		// pointer that indicates start point of whole subtitle file
	bool IsSAMI();					// return if file is SAMI
	bool IsSrt();					// return if file is SubRip
	bool ParsingSamiSubtitle();		// return if SAMI file is parsed
	bool ParsingSrtSubtitle();		// return if Subrip file is parsed

	void SetParsedSubtitleArray();	// set m_pParsedSubtitleArray at the end of Open method
	void FindEncodingType(); 		// detect encoding info by ICU lib

	// Parsing Result
private:
	int m_hFileType;				// will be set to smi or srt when subtitle is parsed
	int* m_pStartTimeArray;			// stores StartTime of each parsed subtitle
	int* m_pEndTimeArray;			// stores EndTime of each parsed subtitle
	char** m_ppSubtitleTextArray;	// stores Text of each parsed subtitle
	PARSED_SUBTITLE* m_pParsedSubtitleArray;	// structure of above information (startTime+endTime+text)

	TEXTCODECLIST* m_pTEXTCODECLIST;	// structure of encode info (encode, confidence)
	int m_iSizeOfTEXTCODECLIST;			// index of TEXTCODECLIST* m_pTEXTCODECLIST

};

#endif // SUBTITLEPARSER_H
