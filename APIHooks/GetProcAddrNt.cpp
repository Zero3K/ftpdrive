#include "stdafx.h"
#include <excpt.h>
#include "../FtpFS/ntdefs.h"
#include "../AnyFS/AnyFS.h"
#include "HookImpl.h"
#include "NtDllExports.h"


namespace HookImpl
{
	/*
typedef NTSTATUS (__stdcall *LdrGetProcedureAddress_t)
 (
 IN PVOID DllHandle,
 IN PANSI_STRING ProcedureName OPTIONAL,
 IN ULONG ProcedureNumber OPTIONAL,
 OUT PVOID *ProcedureAddress
 );
static volatile LdrGetProcedureAddress_t pLdrGetProcedureAddress=LdrGetProcedureAddress;
*/

#include "PSHPACK1.H"
	DWORD FindExportOffset(const char *buff,unsigned int base_size, const char *fn)
	{		
		if (base_size<(sizeof(IMAGE_DOS_HEADER)+sizeof(IMAGE_NT_HEADERS)))
			return 0;
		
		BYTE* pImageBase = (BYTE*)&buff[0];
		
		IMAGE_DOS_HEADER* pDOSHeader=(IMAGE_DOS_HEADER*)pImageBase;
		IMAGE_NT_HEADERS* pNTHeader = (IMAGE_NT_HEADERS*)((DWORD)pDOSHeader + (DWORD)pDOSHeader->e_lfanew);	
		if (*(LPWORD)pNTHeader!='EP')
			return 0;
		
		DWORD StartRVA = pNTHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
		if (!StartRVA)
			return 0;
		
		IMAGE_EXPORT_DIRECTORY *pExport = (IMAGE_EXPORT_DIRECTORY *)(pImageBase+StartRVA);
		DWORD NumberOfFunctions=pExport->NumberOfFunctions;
		DWORD AddressOfFunctions=pExport->AddressOfFunctions;
		DWORD AddressOfNameOrdinals=pExport->AddressOfNameOrdinals;
		DWORD AddressOfNames=pExport->AddressOfNames;
		LPWORD FunctionOrdinal= (LPWORD)(pImageBase+AddressOfNameOrdinals);
		LPDWORD FunctionAddr= (LPDWORD)(pImageBase+AddressOfFunctions);
		LPDWORD NameAddr= (LPDWORD)(pImageBase+AddressOfNames);
		for(DWORD n=0;n<NumberOfFunctions;n++)
		{
			if (!strcmp(fn,(char *)(pImageBase+*NameAddr)))
			{
				return FunctionAddr[*FunctionOrdinal];
			}
			FunctionOrdinal++;
			NameAddr++;
		}
		
		return 0;
	}
#include "POPPACK.H"


	PVOID NtGetProcAddr(HMODULE hmod, const char *fn)
	{
		//HMODULE hmod = GetModuleHandle(dllname);
		//if(!hmod)
		//	return NULL;
		
		TCHAR dllfile[MAX_PATH+1]={0};
		GetModuleFileName(hmod,dllfile,MAX_PATH);
		
		PBYTE out = NULL;
		
		HANDLE f=CreateFile(dllfile,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,
			NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
		if (f!=INVALID_HANDLE_VALUE)
		{
			DWORD sz=GetFileSize(f,NULL);
			HANDLE fm=CreateFileMapping(f,NULL,SEC_IMAGE|PAGE_READONLY,0,0,NULL);//
			if(fm)
			{
				PCHAR buff=(PCHAR)MapViewOfFile(fm,FILE_MAP_READ,0,0,0);
				if(buff)
				{
					__try
					{
						DWORD export=FindExportOffset(buff,sz, fn);
						if(export)
						{
							out = (((LPBYTE)hmod)+export);
						}
					}__except(EXCEPTION_EXECUTE_HANDLER){}
					UnmapViewOfFile(buff);
				}
				CloseHandle(fm);
			}
			CloseHandle(f);
		}
		
		return out;
	}
	
/*
PVOID NtGetProcAddr(HMODULE hDll, const char *pszProcName)
 {
 ANSI_STRING pn;
 pn.Length=pn.MaximumLength=strlen(pszProcName);
 pn.Buffer=(PCHAR)pszProcName;
 PVOID out = NULL;
 pLdrGetProcedureAddress(hDll,&pn,0,&out);
 return out;
 }
*/



bool PathMatchSpec(const TCHAR *pszFileParam, const TCHAR *pszSpec)
 {
 if (pszSpec && pszFileParam)
  {
  // Special case empty string, "*", and "*.*"...
  //
  if (*pszSpec == 0)
   {
   return true;
   }
  
  do
   {
   const TCHAR *pszFile = pszFileParam;
   
   // Strip leading spaces from each spec.  This is mainly for commdlg
   // support;  the standard format that apps pass in for multiple specs
   // is something like "*.bmp; *.dib; *.pcx" for nicer presentation to
   // the user.
   while (*pszSpec == ' ')
    pszSpec++;
   
   while (*pszFile && *pszSpec && *pszSpec != ';')
    {
    switch (*pszSpec)
     {
     case '?':
      pszFile++;
      pszSpec++;      // NLS: We know that this is a SBCS
      break;
      
     case '*':
      
      // We found a * so see if this is the end of our file spec
      // or we have *.* as the end of spec, in which case we
      // can return true.
      //
      if (*(pszSpec + 1) == 0 || *(pszSpec + 1) == ';')   // "*" matches everything
       return true;
      
      
      // Increment to the next character in the list
      pszSpec++;
      
      // If the next character is a . then short circuit the
      // recursion for performance reasons
      if (*pszSpec == '.')
       {
       pszSpec++;  // Get beyond the .
       
       // Now see if this is the *.* case
       if ((*pszSpec == '*') &&
        ((*(pszSpec+1) == '\0') || (*(pszSpec+1) == ';')))
        return true;
       
       // find the extension (or end in the file name)
       while (*pszFile)
        {
        // If the next char is a dot we try to match the
        // part on down else we just increment to next item
        if (*pszFile == '.')
         {
         pszFile++;
         
         if (PathMatchSpec(pszFile, pszSpec))
          return true;
         
         }
        else
         pszFile++;
        }
       
       goto NoMatch;   // No item found so go to next pattern
       }
      else
       {
       // Not simply looking for extension, so recurse through
       // each of the characters until we find a match or the
       // end of the file name
       while (*pszFile)
        {
        // recurse on our self to see if we have a match
        if (PathMatchSpec(pszFile, pszSpec))
         return true;
        pszFile++;
        }
       
       goto NoMatch;   // No item found so go to next pattern
       }
      
     default:
      if ((*pszSpec==*pszFile) || (towupper(*pszSpec) ==towupper(*pszFile)))
       {
       pszFile++;
       pszSpec++;
       }
      else goto NoMatch;
     }
    }
   
   // If we made it to the end of both strings, we have a match...
   //
   if (!*pszFile)
    {
    if ((!*pszSpec || *pszSpec == ';'))
     return true;
    
    // Also special case if things like foo should match foo*
    // as well as foo*; for foo*.* or foo*.*;
    if ( (*pszSpec == '*') &&
     ( (*(pszSpec+1) == '\0') || (*(pszSpec+1) == ';') ||
     ((*(pszSpec+1) == '.') &&  (*(pszSpec+2) == '*') &&
     ((*(pszSpec+3) == '\0') || (*(pszSpec+3) == ';')))))
     
     return true;
    }
   
   // Skip to the end of the path spec...
   //
NoMatch:
    while (*pszSpec && *pszSpec != ';')
     pszSpec++;
    
    // If we have more specs, keep looping...
    //
        } while (*pszSpec++ == ';');
    }
    
    return false;
}

}

