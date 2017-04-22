#include "stdafx.h"
#include "winsock.h"
#include "process.h"
#include "../TrafficCounter.h"
#include "../../FtpSettings/settings.h"
#include "../../FtpSettings/defs.h"
#include "../../FtpFS/ntdefs.h"
#include "../../AnyFS/AnyFS.h"
#include "../../AnyFS/SomeUsefull.h"
#include "FtpWorks.h"
#include "../../DebugLog/DebugLog.h"
#include "../UNCSupport.h"

namespace FtpWorks
{
#define MAX_YEAR_SUPPORTED  2100
#define TOKEN_BUFFER_LENGTH 128
#define RELATIVELY_SMALL_AMOUNT_OF_LS_DATA  128 // arbitrary, but allow for
#define MAX_FILES_COUNT		0xc350	//don't eat more that 32 mb of memory for FtpFindData's vector
	// prolix error text
	
	//
	//  types
	//
	
	typedef enum {
		State_Start,
			State_Error,
			State_Continue,
			State_Done
	} PARSE_STATE;
	
	typedef PARSE_STATE (*DIR_PARSER)(LPSTR*, LPDWORD, LPWIN32_FIND_DATAA);
	
	//
	//  macros
	//
	
#define ClearFileTime(fileTime) \
	(fileTime).dwLowDateTime = 0; \
	(fileTime).dwHighDateTime = 0;
	
#define ClearFindDataFields(lpFind) \
	ClearFileTime((lpFind)->ftCreationTime); \
	ClearFileTime((lpFind)->ftLastAccessTime); \
	(lpFind)->dwReserved0 = 0; \
	(lpFind)->dwReserved1 = 0; \
	(lpFind)->cAlternateFileName[0] = '\0';
	
	//
	//  prototypes
	//
	
	BOOL DetermineDirectoryFormat(
		IN OUT LPSTR* lpBuffer,
		IN OUT LPDWORD lpBufferLength,
		OUT DIR_PARSER* ParserFunction
		);
	
	
	BOOL IsNtDateFormat(
		IN LPSTR lpBuffer,
		IN DWORD dwBufferLength
		);
	
	
	BOOL GetToken(
		IN LPSTR lpszBuffer,
		IN DWORD dwBufferLength,
		OUT LPSTR lpszToken,
		IN OUT LPDWORD lpdwTokenLength
		);
	
	
	BOOL IsUnixAttributeFormat(
		IN LPSTR lpBuffer,
		IN DWORD dwBufferLength
		);
	
	
	PARSE_STATE ParseNtDirectory(
		IN OUT LPSTR* lpBuffer,
		IN OUT LPDWORD lpBufferLength,
		IN OUT LPWIN32_FIND_DATAA lpFindData
		);
	
	
	PARSE_STATE ParseUnixDirectory(
		IN OUT LPSTR* lpBuffer,
		IN OUT LPDWORD lpBufferLength,
		IN OUT LPWIN32_FIND_DATAA lpFindData
		);
	
	
	PARSE_STATE ParseOs2Directory(
		IN OUT LPSTR* lpBuffer,
		IN OUT LPDWORD lpBufferLength,
		IN OUT LPWIN32_FIND_DATAA lpFindData
		);
	
	
	PARSE_STATE ParseMacDirectory(
		IN OUT LPSTR* lpBuffer,
		IN OUT LPDWORD lpBufferLength,
		IN OUT LPWIN32_FIND_DATAA lpFindData
		);
	
	
	bool ExtractFileSize(
		IN OUT LPSTR* lpBuffer,
		IN OUT LPDWORD lpBufferLength,
		IN OUT LPWIN32_FIND_DATAA lpFindData
		);
	
	
	BOOL _ExtractFilename(
		IN OUT LPSTR* lpBuffer,
		IN OUT LPDWORD lpBufferLength,
		IN OUT LPWIN32_FIND_DATAA lpFindData
		);
	
	
	BOOL ExtractNtDate(
		IN OUT LPSTR* lpBuffer,
		IN OUT LPDWORD lpBufferLength,
		IN OUT LPWIN32_FIND_DATAA lpFindData
		);
	
	
	BOOL ExtractUnixDate(
		IN OUT LPSTR* lpBuffer,
		IN OUT LPDWORD lpBufferLength,
		IN OUT LPWIN32_FIND_DATAA lpFindData
		);
	
	
	BOOL ExtractOs2Attributes(
		IN OUT LPSTR* lpBuffer,
		IN OUT LPDWORD lpBufferLength,
		IN OUT LPWIN32_FIND_DATAA lpFindData
		);
	
	
	BOOL ParseWord(
		IN OUT LPSTR* lpBuffer,
		IN OUT LPDWORD lpBufferLength,
		IN WORD LowerBound,
		IN WORD UpperBound,
		OUT LPWORD lpNumber
		);
	
	
	BOOL ExtractInteger(
		IN OUT LPSTR* lpBuffer,
		IN OUT LPDWORD lpBufferLength,
		OUT LPINT lpNumber
		);
	
	//
	//  data
	//
	
	//
	// DefaultSystemTime - if we fail to parse the time/date field for any reason,
	// we will return this default time
	//
	
	static SYSTEMTIME DefaultSystemTime = {1980, 1, 0, 1, 12, 0, 0, 0};
	
	//
	// functions
	//
	
	BOOL SkipWhitespace(
		IN OUT LPSTR* lpBuffer,
		IN OUT LPDWORD lpBufferLength
		)
	{
		while ((*lpBufferLength != 0) && isspace(**lpBuffer)) {
			++*lpBuffer;
			--*lpBufferLength;
		}
		return *lpBufferLength != 0;
	}
	
	BOOL SkipSpaces(
		IN OUT LPSTR* lpBuffer,
		IN OUT LPDWORD lpBufferLength
		)
	{
		while ((*lpBufferLength != 0) && (**lpBuffer == ' ')) {
			++*lpBuffer;
			--*lpBufferLength;
		}
		return *lpBufferLength != 0;
	}
	
	BOOL SkipLine(
		IN OUT LPSTR* lpBuffer,
		IN OUT LPDWORD lpBufferLength
		)
	{
		while ((*lpBufferLength != 0) && (**lpBuffer != '\r') && (**lpBuffer != '\n')) {
			++*lpBuffer;
			--*lpBufferLength;
		}
		while ((*lpBufferLength != 0) && ((**lpBuffer == '\r') || (**lpBuffer == '\n'))) {
			++*lpBuffer;
			--*lpBufferLength;
		}
		return *lpBufferLength != 0;
	}
	
	BOOL FindToken(
		IN OUT LPSTR* lpBuffer,
		IN OUT LPDWORD lpBufferLength
		)
	{
		while ((*lpBufferLength != 0) && !isspace(**lpBuffer)) {
			++*lpBuffer;
			--*lpBufferLength;
		}
		while ((*lpBufferLength != 0) && isspace(**lpBuffer)) {
			++*lpBuffer;
			--*lpBufferLength;
		}
		return *lpBufferLength != 0;
	}
	
	BOOL FindTokenSpecial(
		IN OUT LPSTR* lpBuffer,
		IN OUT LPDWORD lpBufferLength
		)
	{
		while ((*lpBufferLength != 0) && !isspace(**lpBuffer)) {
			++*lpBuffer;
			--*lpBufferLength;
		}
		while (*lpBufferLength != 0) {
			CHAR ch =  **lpBuffer;
			if (((ch < 0x09) || (ch > 0x0d)) && (ch!=0x20))
				break;
			++*lpBuffer;
			--*lpBufferLength;
		}
		return *lpBufferLength != 0;
	}
	
	
	DWORD ParseDirList(
		IN LPSTR lpBuffer,
		IN DWORD dwBufferLength,
		FindVector &found,
		CPConv *conv)                       
	{
		DIR_PARSER directoryParser;
		
		if (!DetermineDirectoryFormat(&lpBuffer, &dwBufferLength, &directoryParser)) 
		{   
			if (dwBufferLength <= RELATIVELY_SMALL_AMOUNT_OF_LS_DATA) 
			{
				return 1;
			}else 
			{
				return 2;
			}
		}
		
		while (dwBufferLength != 0)
		{   
			WIN32_FIND_DATAA Find={0};
			if( directoryParser(&lpBuffer, &dwBufferLength, &Find)==State_Error)break;   
			if(stricmp(Find.cFileName,"desktop.ini") && found.size()<MAX_FILES_COUNT)
			{
				found.push_back(
					FtpFindDataPtr(
					new FtpFindData(Find,conv)));
			}
		}
		
		return ERROR_SUCCESS;
	}
	/*debug
	VOID ClearFindList(IN PLIST_ENTRY lpList)
	{
	
	  while (!IsListEmpty(lpList)) {
	  
		PLIST_ENTRY lpHead;
		
		  lpHead = RemoveHeadList(lpList);
		  
			
			  FREE_MEMORY(lpHead);
			  }
			  
				}
	*/
	//
	//  functions
	//
	
	BOOL
		DetermineDirectoryFormat(
		IN LPSTR* lpBuffer,
		IN LPDWORD lpdwBufferLength,
		OUT DIR_PARSER* lpfnParserFunction
		)
		
	{
		BOOL success;
		
		if (!SkipWhitespace(lpBuffer, lpdwBufferLength)) {
			success = FALSE;
			goto quit;
		}
		
		if (IsNtDateFormat(*lpBuffer, *lpdwBufferLength)) {
			
			*lpfnParserFunction = ParseNtDirectory;
			success = TRUE;
			goto quit;
		}
		
		//
		// we think the directory output is from Unix. The listing probably
		// starts with "total #" or a number, or other random garbage. We
		// know that a Unix dir listing starts with the ls attributes, so
		// we'll search for those, but keep our search within a reasonable
		// distance of the start
		//
		
		LPSTR buffer;
		DWORD length;
		char tokenBuffer[TOKEN_BUFFER_LENGTH];
		int lengthChecked;
		int iteration;
		int dataLength;
		
		buffer = *lpBuffer;
		length = *lpdwBufferLength;
		lengthChecked = 0;
		iteration = 0;
		dataLength = min((int)*lpdwBufferLength, RELATIVELY_SMALL_AMOUNT_OF_LS_DATA);
		
		while (lengthChecked < dataLength) {
			
			DWORD tokenLength;
			DWORD previousLength;
			
			tokenLength = sizeof(tokenBuffer);
			if (!GetToken(buffer,
				length,
				tokenBuffer,
				&tokenLength)) {
				success = FALSE;
				goto quit;
			}
			lengthChecked += tokenLength;
			if (IsUnixAttributeFormat(tokenBuffer, tokenLength)) {
				
				*lpfnParserFunction = ParseUnixDirectory;
				*lpBuffer = buffer;
				*lpdwBufferLength = length;
				success = TRUE;
				goto quit;
			} else if ((iteration == 0)
				&& (tokenLength == 5)
				&& !strnicmp(tokenBuffer, "total", 5)) {
				
				//
				// there may be nothing in the directory listing, except
				// "total 0". If this is this case, then we recognize the
				// format
				//
				
				buffer += tokenLength;
				length -= tokenLength;
				tokenLength = sizeof(tokenBuffer) - 1;  // for '\0'
				if (!GetToken(buffer,
					length,
					tokenBuffer,
					&tokenLength)) {
					success = FALSE;
					goto quit;
				}
				tokenBuffer[tokenLength] = '\0';
				if (isdigit(tokenBuffer[0]) && (atoi(tokenBuffer) == 0)) {
					
					*lpfnParserFunction = ParseUnixDirectory;
					SkipLine(&buffer, &length);
					*lpBuffer = buffer;
					*lpdwBufferLength = length;
					success = TRUE;
					goto quit;
				}
			}
			
			//
			// try the next line
			//
			
			previousLength = length;
			SkipLine(&buffer, &length);
			lengthChecked += previousLength - length;
			++iteration;
		}
		
		//
		// not NT or Unix. Lets try for OS/2. The format of an OS/2 directory entry
		// is:
		//
		//      [<ws>]<length>[DIR|<attribute>]<date><time><filename>
		//
		// we just try to parse the first line. If this succeeds, we assume OS/2
		// format
		//
		
		WIN32_FIND_DATAA findData;
		PARSE_STATE state;
		
		buffer = *lpBuffer;
		length = *lpdwBufferLength;
		state = ParseOs2Directory(&buffer, &length, &findData);
		if ((state == State_Continue) || (state == State_Done)) {
			
			success = TRUE;
			*lpfnParserFunction = ParseOs2Directory;
			goto quit;
		}
		
		//
		// Mac? Mac servers return Unix-like output which will (should) have already
		// been handled by the Unix listing check, and a very simple format which
		// just consists of names with an optional '/' appended, indicating a
		// directory
		//
		
		//
		// the Telnet 2.6 FTP server (which just reports the ultra-simple listing
		// format) returns a weird 'hidden' entry at the start of the listing which
		// consists of "\x03\x02\x01". We will skip all leading control characters
		//
		
		buffer = *lpBuffer;
		length = *lpdwBufferLength;
		while (length && (*buffer < ' ')) {
			++buffer;
			--length;
		}
		
		LPSTR buffer_;
		DWORD length_;
		
		buffer_ = buffer;
		length_ = length;
		
		state = ParseMacDirectory(&buffer, &length, &findData);
		if ((state == State_Continue) || (state == State_Done)) {
			
			*lpBuffer = buffer_;
			*lpdwBufferLength = length_;
			success = TRUE;
			*lpfnParserFunction = ParseMacDirectory;
			goto quit;
		}
		
		//
		// failed to determine the format
		//
		
		success = FALSE;
		
quit:
		
		return success;
}

BOOL
IsNtDateFormat(
               IN LPSTR lpBuffer,
               IN DWORD dwBufferLength
               )               
{
	if (dwBufferLength > 8) {
		
		LPSTR buffer;
		WORD number;
		
		buffer = lpBuffer;
		if (!ParseWord(&buffer, &dwBufferLength, 1, 12, &number)) {
			return FALSE;
		}
		if (!((dwBufferLength > 0) && (*buffer == '-'))) {
			return FALSE;
		}
		++buffer;
		--dwBufferLength;
		if (!ParseWord(&buffer, &dwBufferLength, 1, 31, &number)) {
			return FALSE;
		}
		if (!((dwBufferLength > 0) && (*buffer == '-'))) {
			return FALSE;
		}
		++buffer;
		--dwBufferLength;
		return ParseWord(&buffer, &dwBufferLength, 0, MAX_YEAR_SUPPORTED, &number);
	}
	return FALSE;
}

BOOL
GetToken(
         IN LPSTR lpszBuffer,
         IN DWORD dwBufferLength,
         OUT LPSTR lpszToken,
         IN OUT LPDWORD lpdwTokenLength
         )
{
	DWORD length;
	
	if (!SkipSpaces(&lpszBuffer, &dwBufferLength)) {
		return FALSE;
	}
	
	if (dwBufferLength == 0) {
		return FALSE;
	}
	
	length = *lpdwTokenLength;
	while (!isspace(*lpszBuffer) && (dwBufferLength != 0) && (length != 0)) {
		*lpszToken++ = *lpszBuffer++;
		--dwBufferLength;
		--length;
	}
	*lpdwTokenLength -= length;
	return TRUE;
}

BOOL
IsUnixAttributeFormat(
                      IN LPSTR lpBuffer,
                      IN DWORD dwBufferLength
                      )
{
	int i;
	int hits;
	
	if (dwBufferLength != 10) {
		return FALSE;
	}
	
	//
	// the first character contains 'd' for directory, 'l' for link, '-' for
	// file, and may contain other, unspecified characters, so we just ignore
	// it. So long as the next 9 characters are in the set [-rwx] then we have
	// a Unix ls attribute field.
	//
	// N.B. it turns out that the first character can be in the set [-bcdlp]
	// and the attribute characters can be in the set [-lrsStTwx] (as of
	// 08/18/95)
	//
	
	++lpBuffer;
	hits = 0;
	for (i = 0; i < 9; ++i) {
		
		char ch;
		
		ch = tolower(*lpBuffer);
		++lpBuffer;
		
		if ((ch == '-')
			|| (ch == 'l')
			|| (ch == 'r')
			|| (ch == 's')
			|| (ch == 't')
			|| (ch == 'w')
			|| (ch == 'x')) {
			++hits;
		}
	}
	
	//
	// new scheme: we decide if the token was a Unix attribute field based on
	// probability. If the hit rate was greater than 1 in 2 (5 out of 9 or
	// higher) then we say that the field was probably a Unix attribute. This
	// scheme allows us to accept future enhancements or non-standard Unix
	// implementations without changing this code (for a while) (Make the
	// attribute set a registry value (!))
	//
	
	return hits >= 5;
}

PARSE_STATE
ParseNtDirectory(
                 IN OUT LPSTR* lpBuffer,
                 IN OUT LPDWORD lpBufferLength,
                 IN OUT LPWIN32_FIND_DATAA lpFindData
                 )                                         
{
	//
	// not expecting the line to start with spaces, but we check anyway
	//
	
	if (!SkipSpaces(lpBuffer, lpBufferLength)) {
		goto done;
	}
	
	if (!ExtractNtDate(lpBuffer, lpBufferLength, lpFindData)) {
		goto done;
	}
	
	if (!strnicmp(*lpBuffer, "<DIR>", 5)) {
		lpFindData->dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
		lpFindData->nFileSizeHigh = 0;
		lpFindData->nFileSizeLow = 0;
		FindTokenSpecial(lpBuffer, lpBufferLength);
	} else {
		lpFindData->dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
		ExtractFileSize(lpBuffer, lpBufferLength, lpFindData);
		SkipSpaces(lpBuffer, lpBufferLength);
	}
	
	_ExtractFilename(lpBuffer, lpBufferLength, lpFindData);
	
	//
	// we expect the filename to be the last thing on the line
	//
	
done:
	
	return SkipLine(lpBuffer, lpBufferLength) ? State_Continue : State_Done;
}



void
ReadUnixPermissions(
                    IN LPCSTR pszBuffer,
                    IN DWORD cbBufferSize,
                    OUT LPDWORD pdwPermissions)
{
	// Format: rwxrwxrwx <Owner><Group><All>
	*pdwPermissions = 0;
	
	if (cbBufferSize > 10)
	{
		if ('r' == pszBuffer[1])
			*pdwPermissions |= 0x00000400;
		
		if ('w' == pszBuffer[2])
			*pdwPermissions |= 0x00000200;
		
		if ('x' == pszBuffer[3])
			*pdwPermissions |= 0x00000100;
		
		if ('r' == pszBuffer[4])
			*pdwPermissions |= 0x00000040;
		
		if ('w' == pszBuffer[5])
			*pdwPermissions |= 0x00000020;
		
		if ('x' == pszBuffer[6])
			*pdwPermissions |= 0x00000010;
		
		if ('r' == pszBuffer[7])
			*pdwPermissions |= 0x00000004;
		
		if ('w' == pszBuffer[8])
			*pdwPermissions |= 0x00000002;
		
		if ('x' == pszBuffer[9])
			*pdwPermissions |= 0x00000001;
	}
}

PARSE_STATE
ParseUnixDirectory(
                   IN OUT LPSTR* lpBuffer,
                   IN OUT LPDWORD lpBufferLength,
                   IN OUT LPWIN32_FIND_DATAA lpFindData
                   )
{
	int i;
	BOOL symbolicLink;
	char ch;
	
	//
	// not expecting the line to start with spaces, but we check anyway
	//
	
	if (!SkipSpaces(lpBuffer, lpBufferLength)) {
		goto done;
	}
	
	//
	// if the item is a symbolic link then we have to trim the 'filename' below
	//
	
	ch = tolower(**lpBuffer);
	symbolicLink = (ch == 'l');
	
	//
	// attributes are first thing on line
	//
	
	lpFindData->dwFileAttributes = (ch == 'd') ? FILE_ATTRIBUTE_DIRECTORY
		: (ch == '-') ? FILE_ATTRIBUTE_NORMAL
		: 0;
	
	//
	// skip over the attributes and over the owner/creator fields to the file
	// size
	//
	
	// Read the Attributes and put them in the WIN32_FIND_DATA.dwReserved0 attributes.
	// It's OK to use FILE_ATTRIBUTE_REPARSE_POINT because it's unused unless
	// WIN32_FIND_DATA.dwFileAttributes contains yyy, which we don't set.
	ReadUnixPermissions(*lpBuffer, *lpBufferLength, &(lpFindData->dwReserved0));
	
	LPSTR lpszLastToken;
	DWORD dwLastToken;
	
	for (i = 0; i < 4; ++i) {
		lpszLastToken = *lpBuffer;
		dwLastToken = *lpBufferLength;
		if (!FindToken(lpBuffer, lpBufferLength)) {
			goto done;
		}
	}
	
	if (!ExtractFileSize(lpBuffer, lpBufferLength, lpFindData)) {
		ExtractFileSize(&lpszLastToken, &dwLastToken, lpFindData);
	}
	
	SkipSpaces(lpBuffer, lpBufferLength);
	
	if (!ExtractUnixDate(lpBuffer, lpBufferLength, lpFindData)) {
		goto done;
	}
	
	SkipSpaces(lpBuffer, lpBufferLength);
	
	//
	// we expect the filename to be the last thing on the line
	//
	
	_ExtractFilename(lpBuffer, lpBufferLength, lpFindData);
	
	//
	// if the item is a symbolic link, then remove everything after the " -> "
	//
	
	if (symbolicLink) {
		
		LPSTR lpArrow;
		
		lpArrow = strstr(lpFindData->cFileName, " -> ");
		if (lpArrow != NULL) {
			*lpArrow = '\0';
			lpFindData->dwFileAttributes|=FILE_ATTRIBUTE_REPARSE_POINT;
		}
	}
	
done:
	
	return SkipLine(lpBuffer, lpBufferLength) ? State_Continue : State_Done;
}


PARSE_STATE
ParseOs2Directory(
				  IN OUT LPSTR* lpBuffer,
				  IN OUT LPDWORD lpBufferLength,
				  IN OUT LPWIN32_FIND_DATAA lpFindData
				  )
{
	PARSE_STATE state;
	
	if (!SkipSpaces(lpBuffer, lpBufferLength)) {
		goto skip;
	}
	
	if (!ExtractFileSize(lpBuffer, lpBufferLength, lpFindData)) {
		state = State_Error;
		goto done;
	}
	
	if (!SkipSpaces(lpBuffer, lpBufferLength)) {
		goto skip;
	}
	
	if (!strnicmp(*lpBuffer, "DIR", 3)) {
		lpFindData->dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
		FindToken(lpBuffer, lpBufferLength);
	} else if (!isdigit(**lpBuffer)) {
		if (!ExtractOs2Attributes(lpBuffer, lpBufferLength, lpFindData)) {
			state = State_Error;
			goto done;
		}
		SkipSpaces(lpBuffer, lpBufferLength);
    } else {
		lpFindData->dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
	}
    
    if (!ExtractNtDate(lpBuffer, lpBufferLength, lpFindData)) {
		state = State_Error;
		goto done;
	}
    
    _ExtractFilename(lpBuffer, lpBufferLength, lpFindData);
    
    //
    // we expect the filename to be the last thing on the line
    //
    
skip:
    
    state = SkipLine(lpBuffer, lpBufferLength) ? State_Continue : State_Done;
    
done:
    
    return state;
}

PARSE_STATE
ParseMacDirectory(
				  IN OUT LPSTR* lpBuffer,
				  IN OUT LPDWORD lpBufferLength,
				  IN OUT LPWIN32_FIND_DATAA lpFindData
				  )                              
{
	PARSE_STATE state;
	
	if (!SkipSpaces(lpBuffer, lpBufferLength)) {
		goto skip;
	}
	
	_ExtractFilename(lpBuffer, lpBufferLength, lpFindData);
	
	int len;
	
	len = strlen(lpFindData->cFileName);
	if (lpFindData->cFileName[len - 1] == '/') {
		lpFindData->cFileName[len - 1] = '\0';
		lpFindData->dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY;
	} else {
		lpFindData->dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
    }
	if ((*lpBufferLength != 0)
		&& !((**lpBuffer == '\r') || (**lpBuffer == '\n'))) {
		state = State_Error;
		goto done;
    }
	
	//
	// this directory format has no size or time information
	//
	
	lpFindData->nFileSizeLow = 0;
	lpFindData->nFileSizeHigh = 0;
	SystemTimeToFileTime(&DefaultSystemTime, &lpFindData->ftLastWriteTime);
	
	//
	// we expect the filename to be the last thing on the line
	//
	
skip:
	
	state = SkipLine(lpBuffer, lpBufferLength) ? State_Continue : State_Done;
	
done:
	
	return state;
}

void my_strtoui64(char *buf, char **end, ULARGE_INTEGER &out)
{
	typedef unsigned __int64 (__cdecl *t_strtoui64)
		(
		const char *nptr,
		char **endptr,
		int base 
		);
	static t_strtoui64 p_strtoui64 = 
		(t_strtoui64)::GetProcAddress(
		::LoadLibrary(TEXT("msvcrt.dll")),"_strtoui64");

	if (p_strtoui64)
	{
		out.QuadPart = p_strtoui64(buf,end,10);
		return;
	}

	out.QuadPart = (ULONGLONG)(LONGLONG)_atoi64(buf);
	while (isdigit(*buf))buf++;
	*end = buf;
}

bool ExtractFileSize(
				IN OUT LPSTR* lpBuffer,
				IN OUT LPDWORD lpBufferLength,
				IN OUT LPWIN32_FIND_DATAA lpFindData)
{
	LPSTR last = *lpBuffer;
	if (!isdigit(*last)) 
		return false;
	
	ULARGE_INTEGER sz64;
	my_strtoui64(*lpBuffer,&last,sz64);
	
	lpFindData->nFileSizeLow = sz64.LowPart;
	lpFindData->nFileSizeHigh = sz64.HighPart;
	*lpBufferLength -= (DWORD) (last - *lpBuffer);
	*lpBuffer = last;
	return true;
}

BOOL
_ExtractFilename(
				 IN OUT LPSTR* lpBuffer,
				 IN OUT LPDWORD lpBufferLength,
				 IN OUT LPWIN32_FIND_DATAA lpFindData
				 )
{
	LPSTR dest;
	DWORD destLength;
	
	dest = lpFindData->cFileName;
	destLength = sizeof(lpFindData->cFileName) - 1;
	while ((*lpBufferLength != 0)
		&& (destLength != 0)
		&& !((**lpBuffer == '\r') || (**lpBuffer == '\n'))) {
		*dest++ = *(*lpBuffer)++;
		--*lpBufferLength;
		--destLength;
	}
	*dest = '\0';
	return TRUE;
}

BOOL
ExtractNtDate(
			  IN OUT LPSTR* lpBuffer,
			  IN OUT LPDWORD lpBufferLength,
			  IN OUT LPWIN32_FIND_DATAA lpFindData
			  )
{
	SYSTEMTIME systemTime;
	LPSYSTEMTIME lpSystemTime;
	
	lpSystemTime = &DefaultSystemTime;
	
	//
	// BUGBUG - what about internationalization? E.g. does UK FTP server return
	//          the date as e.g. 27/07/95? Other formats?
	//
	
	//
	// month ::= 1..12
	//
	
	if (!ParseWord(lpBuffer, lpBufferLength, 1, 12, &systemTime.wMonth)) {
		goto done;
	}
	if (!((*lpBufferLength > 0) && (**lpBuffer == '-'))) {
		goto done;
	}
	++*lpBuffer;
	--*lpBufferLength;
	
	//
	// date ::= 1..31
	//
	
	if (!ParseWord(lpBuffer, lpBufferLength, 1, 31, &systemTime.wDay)) {
		goto done;
	}
	if (!((*lpBufferLength > 0) && (**lpBuffer == '-'))) {
		goto done;
	}
	++*lpBuffer;
	--*lpBufferLength;
	
	//
	// year ::= 0..2100
	//
	
	if (!ParseWord(lpBuffer, lpBufferLength, 0, MAX_YEAR_SUPPORTED, &systemTime.wYear)) {
		goto done;
	}
	if (!((*lpBufferLength > 0) && (**lpBuffer == ' '))) {
		goto done;
	}
	
	//
	// the oldest file can be dated 1980. We allow the following:
	//
	//  1-1-79              => 1-1-2079
	//  1-1-80..12-31-99    => 1-1-1980..12-31-1999
	//  1-1-00              => 1-1-2000
	//  1-1-1995            => 1-1-1995
	//  1-1-2001            => 1-1-2001
	//  etc.
	//
	
	systemTime.wYear += (systemTime.wYear < 80)
		? 2000
		: (systemTime.wYear <= 99)
		? 1900
		: 0
		;
	
	//
	// find start of time (er, Professor Hawking..?)
	//
	
	if (!FindToken(lpBuffer, lpBufferLength)) {
		goto done;
	}
	
	//
	// hour ::= 0..23 | 1..12
	//
	
	if (!ParseWord(lpBuffer, lpBufferLength, 0, 23, &systemTime.wHour)) {
		goto done;
	}
	if (!((*lpBufferLength > 0) && (**lpBuffer == ':'))) {
		goto done;
	}
	++*lpBuffer;
	--*lpBufferLength;
	
	//
	// minute ::= 0..59
	//
	
	if (!ParseWord(lpBuffer, lpBufferLength, 0, 59, &systemTime.wMinute)) {
		goto done;
	}
	
	//
	// if the time is followed by AM or PM then convert to 24-hour time if PM
	// and skip to end of token in both cases
	//
	
	if (*lpBufferLength >= 2) {
		
		char ch_p;
		
		ch_p = tolower(**lpBuffer);
		if ((ch_p == 'p') || (ch_p == 'a')) {
			
			char ch_m;
			
			ch_m = tolower(*(*lpBuffer + 1));
			if ((ch_p == 'p') && (ch_m == 'm')) {
				// 12 PM = 12, 1PM = 13, 2PM = 14, etc
				if ( systemTime.wHour < 12 ) {
					systemTime.wHour += 12;
				}
			} else if ( systemTime.wHour == 12 ) {
				// 12 AM == 0:00 24hr
				//debug//INET_ASSERT((ch_p == 'a') && (ch_m == 'm'));
				systemTime.wHour = 0;
			}
		}
	}
	
	//
	// seconds, milliseconds and weekday always zero
	//
	
	systemTime.wSecond = 0;
	systemTime.wMilliseconds = 0;
	systemTime.wDayOfWeek = 0;
	
	//
	// get ready to convert the parsed date/time to a FILETIME
	//
	
	lpSystemTime = &systemTime;
	
done:
	
	//
	// convert the system time to file time and move the buffer pointer/length
	// to the next line
	//
	
	if (!SystemTimeToFileTime(lpSystemTime, &lpFindData->ftLastWriteTime)) {
		SystemTimeToFileTime(&DefaultSystemTime, &lpFindData->ftLastWriteTime);
	}
	FindToken(lpBuffer, lpBufferLength);
	return TRUE;
}

BOOL
ExtractUnixDate(
                IN OUT LPSTR* lpBuffer,
                IN OUT LPDWORD lpBufferLength,
                IN OUT LPWIN32_FIND_DATAA lpFindData
                )                                      
{
	SYSTEMTIME systemTime;
	LPSYSTEMTIME lpSystemTime;
	static LPSTR Months = "janfebmaraprmayjunjulaugsepoctnovdec";
	char monthStr[4];
	int i;
	LPSTR offset;
	WORD wnum;
	
	lpSystemTime = &DefaultSystemTime;
	
	//
	// month ::= Jan..Dec
	//
	
	for (i = 0; i < 3; ++i) {
		if (*lpBufferLength == 0) {
			goto done;
		}
		monthStr[i] = *(*lpBuffer)++;
		monthStr[i] = tolower(monthStr[i]);
		--*lpBufferLength;
	}
	monthStr[i] = '\0';
	offset = strstr(Months, monthStr);
	if (offset == NULL) {
		goto done;
	}
	systemTime.wMonth = (unsigned short) ((offset-Months) / 3 + 1);
	
	FindToken(lpBuffer, lpBufferLength);
	
	//
	// date ::= 1..31
	//
	
	if (!ParseWord(lpBuffer, lpBufferLength, 1, 31, &systemTime.wDay)) {
		goto done;
	}
	
	SkipSpaces(lpBuffer, lpBufferLength);
	
	//
	// year or hour
	//
	
	if (!ParseWord(lpBuffer, lpBufferLength, 0, 65535, &wnum)) {
		goto done;
	}
	
	if ((*lpBufferLength != 0) && (**lpBuffer == ':')) {
		
		SYSTEMTIME timeNow;
		
		systemTime.wHour = wnum;
		
		//
		// we found the hour field, now get the minutes
		//
		
		++*lpBuffer;
		--*lpBufferLength;
		if (!ParseWord(lpBuffer, lpBufferLength, 0, 59, &systemTime.wMinute)) {
			goto done;
		}
		
		//
		// a date-time with an hour:minute field is based in this year. We need
		// to get the current year from the system. There is a slight problem in
		// that if this machine's just had a new year and the FTP server is
		// behind us, then the file can be a year out of date.
		//
		// There is no guarantees about the basis of time used by the FTP server,
		// so we'll get the UTC (aka Greenwich Mean Time) time
		//
		
		GetSystemTime(&timeNow);
		
		//
		// apparently its not quite as straightforward as first we'd believed.
		// If the date/month is in the future then the year is last year
		//
		
		BOOL bLastYear = FALSE;
		
		if (systemTime.wMonth > timeNow.wMonth) {
			bLastYear = TRUE;
		} else if (systemTime.wMonth == timeNow.wMonth) {
			
			//
			// BUGBUG - leap year? I believe that in this case, because the time
			//          difference is 1 year minus 1 day, then that is great
			//          enough for the date format including year to have been
			//          used and thus making this moot. Need to prove it.
			//          Note, by that logic, everything from here on down should
			//          also be moot - we should only have to concern ourselves
			//          with the month
			//
			
			if (systemTime.wDay > timeNow.wDay) {
				bLastYear = TRUE;
			} else if (systemTime.wDay == timeNow.wDay) {
				if (systemTime.wHour > timeNow.wHour) {
					bLastYear = TRUE;
				} else if (systemTime.wHour == timeNow.wHour) {
					if (systemTime.wMinute > timeNow.wMinute) {
						bLastYear = TRUE;
					} else if (systemTime.wMinute == timeNow.wMinute) {
						if (systemTime.wSecond > timeNow.wSecond) {
							bLastYear = TRUE;
						}
					}
				}
			}
		}
		systemTime.wYear = timeNow.wYear;
		if (bLastYear) {
			--systemTime.wYear;
		}
	} else {
		
		//
		// next field is the year
		//
		
		systemTime.wYear = wnum;
		
		//
		// time for a file with only a year is 00:00 (mitternacht)
		//
		
		systemTime.wHour = 0;
		systemTime.wMinute = 0;
	}
	
	//
	// seconds, milliseconds and weekday always zero
	//
	
	systemTime.wSecond = 0;
	systemTime.wMilliseconds = 0;
	systemTime.wDayOfWeek = 0;
	
	//
	// get ready to convert the parsed date/time to a FILETIME
	//
	
	lpSystemTime = &systemTime;
	
done:
	
	//
	// convert the system time to file time and move the buffer pointer/length
	// to the next line
	//
	
	if (!SystemTimeToFileTime(lpSystemTime, &lpFindData->ftLastWriteTime)) {
		SystemTimeToFileTime(&DefaultSystemTime, &lpFindData->ftLastWriteTime);
	}
	FindTokenSpecial(lpBuffer, lpBufferLength);
	return TRUE;
}

BOOL
ExtractOs2Attributes(
                     IN OUT LPSTR* lpBuffer,
                     IN OUT LPDWORD lpBufferLength,
                     IN OUT LPWIN32_FIND_DATAA lpFindData
                     )
					 
{
	DWORD attributes = 0;
	BOOL done = FALSE;
	
	while (*lpBufferLength && !done) {
		
		char ch = **lpBuffer;
		
		switch (toupper(ch)) {
		case 'A':
			attributes |= FILE_ATTRIBUTE_ARCHIVE;
			break;
			
		case 'H':
			attributes |= FILE_ATTRIBUTE_HIDDEN;
			break;
			
		case 'R':
			attributes |= FILE_ATTRIBUTE_READONLY;
			break;
			
		case 'S':
			attributes |= FILE_ATTRIBUTE_SYSTEM;
			break;
			
		case ' ':
			
			//
			// if there is only one space, we will be pointing at the next token
			// after this function completes. Okay so long as we will be calling
			// SkipSpaces() next (which doesn't expect to be pointing at a space)
			//
			
			done = TRUE;
			break;
		}
		--*lpBufferLength;
		++*lpBuffer;
	}
	
	//
	// if we are here there must have been some characters which looked like
	// OS/2 file attributes
	//
	
	//debug//INET_ASSERT(attributes != 0);
	
	lpFindData->dwFileAttributes = attributes;
	
	return TRUE;
}

BOOL ParseWord(
			   IN OUT LPSTR* lpBuffer,
			   IN OUT LPDWORD lpBufferLength,
			   IN WORD LowerBound,
			   IN WORD UpperBound,
			   OUT LPWORD lpNumber
			   )
{
	int number;
	
	if (ExtractInteger(lpBuffer, lpBufferLength, &number)) {
		if ((number >= (int)LowerBound) && (number <= (int)UpperBound)) {
			*lpNumber = (WORD)number;
			return TRUE;
		}
	}
	return FALSE;
}

BOOL ExtractInteger(
					IN OUT LPSTR* lpBuffer,
					IN OUT LPDWORD lpBufferLength,
					OUT LPINT lpNumber
					)
{
	BOOL success;
	int number;
	
	number = 0;
	if ((*lpBufferLength > 0) && isdigit(**lpBuffer)) {
		while (isdigit(**lpBuffer) && (*lpBufferLength != 0)) {
			number = number * 10 + (int)(**lpBuffer - '0');
			++*lpBuffer;
			--*lpBufferLength;
		}
		success = TRUE;
	} else {
		success = FALSE;
	}
	*lpNumber = number;
	return success;
}
}