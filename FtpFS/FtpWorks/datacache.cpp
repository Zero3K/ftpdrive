
#include "stdafx.h"
#include <winsock.h>
#include <process.h>
#include <assert.h>
#include "../ntdefs.h"
#include "../../APIHooks/NtDllExports.h"
#include "../TrafficCounter.h"
#include "../../FtpSettings/settings.h"
#include "../../FtpSettings/defs.h"
#include "../../FtpFS/ntdefs.h"
#include "../../AnyFS/AnyFS.h"
#include "../../AnyFS/SomeUsefull.h"
#include "../../DebugLog/DebugLog.h"
#include "ftpworks.h"
#include <boost/shared_ptr.hpp>
#define CACHE_PAGE_SIZE		0x100000

namespace FtpWorks
{
	class Storage
	{
	private:
		typedef std::vector<DWORD> DWORDVECTOR;
		
		HANDLE			_handle;
		DWORD			_capacity;
		
	public:
		Storage(const std::wstring &filepath)
			:_capacity(0)
		{
			for(size_t i=0;i<0xfff;i++)
			{
				_handle = ::CreateFile(filepath.c_str(),GENERIC_READ|GENERIC_WRITE,
					FILE_SHARE_READ,NULL,CREATE_ALWAYS,
					FILE_ATTRIBUTE_NORMAL|FILE_FLAG_DELETE_ON_CLOSE,NULL);
				if(_handle!=INVALID_HANDLE_VALUE)break;
			}
		}
		
		virtual ~Storage()
		{
			if(_handle!=INVALID_HANDLE_VALUE)::CloseHandle(_handle);
		}
		
		DWORD Allocate()
		{
			if ((_capacity*(CACHE_PAGE_SIZE/0x100000)) >= FtpSettings::cache_max_file_size)
			{
				return 0xffffffff;
			}

			if (_capacity)
			{
				LARGE_INTEGER liFileSize={_capacity+1,0};
				liFileSize.QuadPart*=CACHE_PAGE_SIZE;
				IO_STATUS_BLOCK isb;
				NTSTATUS s = ZwSetInformationFile(_handle,&isb,&liFileSize,
					sizeof(liFileSize),FileEndOfFileInformation);
				
				if (NT_ERROR(s))
					return 0xffffffff;
				
				
				s = ZwSetInformationFile(_handle,&isb,&liFileSize,
					sizeof(liFileSize),FileAllocationInformation);
				
				if (NT_ERROR(s))
					return 0xffffffff;
			}

			return _capacity++;
		}
		
		bool Write(DWORD page, DWORD offset, DWORD size, const void *buf)
		{
			if (_handle==INVALID_HANDLE_VALUE)
				return false;

			LARGE_INTEGER liPos={page,0};
			liPos.QuadPart*=(LONGLONG)CACHE_PAGE_SIZE;
			liPos.QuadPart+=(LONGLONG)offset;
			OVERLAPPED ovl={0,0,liPos.LowPart,liPos.HighPart,NULL};
			
			DWORD dw=0;
			if (!::WriteFile(_handle,buf,size,&dw,&ovl))
				return false;
			
			return (dw==size);			
		}
		
		bool Read(DWORD page, DWORD offset, DWORD size, void *buf)
		{
			if (_handle==INVALID_HANDLE_VALUE)
				return false;
			
			LARGE_INTEGER liPos={page,0};
			liPos.QuadPart*=(LONGLONG)CACHE_PAGE_SIZE;
			liPos.QuadPart+=(LONGLONG)offset;
			OVERLAPPED ovl={0,0,liPos.LowPart,liPos.HighPart,NULL};
			
			DWORD dw=0;
			if (!::ReadFile(_handle,buf,size,&dw,&ovl))
				return false;
			
			return (dw==size);
		}
	};
	
	typedef boost::shared_ptr<Storage>	StoragePtr;
	
	///////////////////////////////////////////////////////////////////////	
	
	class PageRange
	{
	public:
		inline PageRange(ULONG ofs_begin_, ULONG ofs_end_)
			:ofs_begin(ofs_begin_),ofs_end(ofs_end_)
		{
		}
		
		ULONG ofs_begin;
		ULONG ofs_end;
	};
	
	///////////////////////////////////////////////////////////////////////	
	
	class Cache
	{
	private:
		class PageRanges : public std::vector<PageRange>
		{
		private:
			DWORD _page;
			DWORD _last_read;
			
		public:
			PageRanges(DWORD page)
				:_page(page),
				_last_read(::GetTickCount())
			{
			}

			DWORD Page(){return _page;};
			
			ULONG HaveRange(ULONG ofs_begin, ULONG ofs_end)
			{
				for (iterator i=begin();i!=end();++i)
				{
					if (i->ofs_begin<=ofs_begin
						&&i->ofs_end>ofs_begin)
					{
						if (ofs_end>i->ofs_end)
						{
							return (i->ofs_end - ofs_begin);
						}
						
						return (ofs_end - ofs_begin);
					}
				}
				return 0;
			}
			
			void EnforceRange(ULONG ofs_begin, ULONG ofs_end)
			{
				iterator j = end();
				for(iterator i=begin();i!=end();++i)
				{
					if (i->ofs_begin<=ofs_begin&&i->ofs_end>=ofs_end)
					{//this range assimilates needed, nothing to do
						return;
					}
					
					if (i->ofs_begin>=ofs_begin&&i->ofs_end<=ofs_end)
					{//needed assimilates this range
						i->ofs_begin = 0xffffffff;
						i->ofs_end = 0xffffffff;
						j = i;
						continue;
					}
					
					if (i->ofs_begin<=ofs_begin&&i->ofs_end>=ofs_begin)
					{//needed begin in/be-side of this range
						ofs_begin = i->ofs_begin;
						i->ofs_begin = 0xffffffff;
						i->ofs_end = 0xffffffff;
						j = i;
						continue;
					}
					
					if (i->ofs_begin<=ofs_end&&i->ofs_end>=ofs_end)
					{//needed end in/be-side of this range
						ofs_end = i->ofs_end;
						i->ofs_begin = 0xffffffff;
						i->ofs_end = 0xffffffff;
						j = i;
						continue;
					}
				}
				
				if (j!=end())
				{
					j->ofs_begin = ofs_begin;
					j->ofs_end = ofs_end;
				}
				else
				{
					push_back(PageRange(ofs_begin, ofs_end));
				}
			}
			
			inline DWORD GetLastRead()
			{
				return _last_read;
			}

			inline DWORD UpdateLastRead()
			{
				return _last_read;
			}

		};
		
		typedef boost::shared_ptr<PageRanges>	PageRangesPtr;
		typedef std::map<ULONG, PageRangesPtr>	Segment2PageMap;
		
		StoragePtr		_storage;
		Segment2PageMap	_fseg_2_page;
		
		DWORD FreeOldestPage()
		{
			DWORD max_ticks = 0xffffffff;
			Segment2PageMap::iterator j=_fseg_2_page.end();
			for(Segment2PageMap::iterator i=_fseg_2_page.begin();i!=_fseg_2_page.end();++i)
			{
				if (i->second->GetLastRead()<max_ticks)
				{
					j = i;
					max_ticks = i->second->GetLastRead();
				}
			}

			if (j==_fseg_2_page.end())
				return 0xffffffff;

			j->second->clear();
			j->second->UpdateLastRead();
			DWORD out = j->second->Page();
			_fseg_2_page.erase(j);
			return out;
		}

		void WriteAligned(ULONG segment, ULONG ofs, ULONG size, const void *buf)
		{
			assert(size);
			Segment2PageMap::iterator i=_fseg_2_page.find(segment);
			PageRangesPtr ranges;
			if (i==_fseg_2_page.end())
			{
				DWORD page = _storage->Allocate();
				if (page==0xffffffff)
				{
					page = FreeOldestPage();
					if (page==0xffffffff)
						return;
				}

				ranges.reset(new PageRanges(page));
				
				_fseg_2_page.insert(
					Segment2PageMap::value_type(segment,ranges));
			}
			else
			{
				ranges = i->second;
			}
			
			if (_storage->Write(ranges->Page(),ofs,size,buf))
			{
				ranges->EnforceRange(ofs, ofs + size);
			}			
		}
		
		ULONG ReadAligned(ULONG segment, ULONG ofs, ULONG size, void *buf)
		{
			assert(size);
			Segment2PageMap::iterator i=_fseg_2_page.find(segment);
			if (i==_fseg_2_page.end())
				return 0;
			
			size = i->second->HaveRange(ofs, ofs + size);
			i->second->UpdateLastRead();

			if (!size)
				return 0;
			
			if (!_storage->Read(i->second->Page(),ofs,size,buf))
				return 0;

			return size;
		}
		
	public:
		Cache(StoragePtr storage)
			:_storage(storage)
		{
		}
		
		virtual ~Cache()
		{
		}
		
		void Write(const LONGLONG pos, ULONG size, const void *buf)
		{
			ULONG segment = (ULONG)(LONGLONG)(pos/CACHE_PAGE_SIZE);
			ULONG ofs = (ULONG)(LONGLONG)(pos%CACHE_PAGE_SIZE);
			
			while (size)
			{
				ULONG inpage_size = CACHE_PAGE_SIZE - ofs;
				if (inpage_size>size)
					inpage_size = size;
				
				WriteAligned(segment, ofs, inpage_size, buf);
				
				segment++;
				buf = (const char *)buf+inpage_size;
				size -= inpage_size;
				ofs = 0;
			}
		}
		
		ULONG Read(const LONGLONG pos, ULONG size, void *buf)
		{
			ULONG segment = (ULONG)(LONGLONG)(pos/CACHE_PAGE_SIZE);
			ULONG ofs = (ULONG)(LONGLONG)(pos%CACHE_PAGE_SIZE);
			ULONG out = 0;
			
			while (size)
			{
				ULONG inpage_size = CACHE_PAGE_SIZE - ofs;
				if (inpage_size>size)
					inpage_size = size;

				ULONG size_ok  = ReadAligned(segment, ofs, inpage_size, buf);
				out += size_ok;
				
				if (size_ok!=inpage_size)break;
				
				segment++;
				buf = (char *)buf+inpage_size;
				size -= inpage_size;
				ofs = 0;
			}
			
			return out;
		}
		
	};
	
	typedef boost::shared_ptr<Cache>	CachePtr;
	
	///////////////////////////////////////////////////////////////////////	
	
	
	class CachedFile
	{
	private:
		DWORD _expire_at;
		CachePtr _cache;

	public:
		CachedFile(const std::wstring &store_path)
			:_cache(new Cache(StoragePtr(new Storage(store_path))))
		{
			update_expire();
		}

		void update_expire()
		{
			_expire_at = ::GetTickCount()+(1000*FtpSettings::cache_file_expire);
		}

		DWORD expire_at()
		{
			return _expire_at;
		}

		CachePtr cache()
		{
			return _cache;
		}
	};

	typedef boost::shared_ptr<CachedFile>		CachedFilePtr;
	
	typedef std::map<std::wstring,CachedFilePtr,wstring_less_no_case> CachedFiles;
	CachedFiles cached_files;
	
	FastCriticalSection data_cache_cs;
	
	void ForceCleanCaches()
	{
		CleanEnumCache(true);
		CloseCleanFtpConnectionsCache(FtpFilePtr(),true);
		CleanDataCache(true);
		InaccessibleClean();
	}
	
	unsigned int ReadFromCache(
		FtpFilePtr file,
		LPVOID lpBuffer,
		const LARGE_INTEGER *iPointer,
		DWORD nNumberOfBytesToRead)
	{
		if (!nNumberOfBytesToRead)
			return 0;

		DWORD out=0;
		if (FtpSettings::adv_flags&FTPS_ADV_FILECACHE)
		{
			std::wstring filename(file->MakeFilePathName(false));
			AutoCriticalSection cslock(data_cache_cs);
			CachedFiles::iterator i=cached_files.find(filename);
			if (i!=cached_files.end())
			{
				out = i->second->cache()->Read(iPointer->QuadPart,nNumberOfBytesToRead,lpBuffer);
				i->second->update_expire();
			}
		}
		//if(out)ftp_cache_counter.OnRecv((int)out);
		return out;
	}
	
	void WriteToCache(
		FtpFilePtr file,
		LPCVOID lpBuffer,
		const LARGE_INTEGER *iPointer,
		DWORD nNumberOfBytesToWrite)
	{
		if (!nNumberOfBytesToWrite)
			return;
		
		//&&(liPointer->QuadPart<(__int64)1024*(__int64)1024*(__int64)FtpSettings::cache_max_file_size))
		if(FtpSettings::adv_flags&FTPS_ADV_FILECACHE)
		{
			std::wstring filename(file->MakeFilePathName(false));
			AutoCriticalSection cslock(data_cache_cs);
			static unsigned __int64 LastId=0;
			CachedFiles::iterator i=cached_files.find(filename);
			CachedFilePtr cached_file_ptr;
			if (i==cached_files.end())
			{
				LastId++;
				::CreateDirectory(tempdir.c_str(),NULL);
				std::vector<TCHAR> temp(tempdir.size()+32);
				wsprintf(&temp[0],TEXT("%ws\\%I64x.fdc"),
					tempdir.c_str(),LastId);
				
				cached_file_ptr.reset(new CachedFile(&temp[0]));
				cached_files.insert(CachedFiles::value_type(filename,cached_file_ptr));	
			}
			else
			{
				cached_file_ptr = i->second;
			}
			
			cached_file_ptr->cache()->Write(iPointer->QuadPart,nNumberOfBytesToWrite,lpBuffer);
			cached_file_ptr->update_expire();
		}
	}
	
	void CleanDataCache(bool forced)
	{
		DWORD tm=::GetTickCount();
		STRINGVECTOR todelete;
		AutoCriticalSection cslock(data_cache_cs);
		for(CachedFiles::iterator i=cached_files.begin();i!=cached_files.end();++i)
		{
			if(forced||(tm>i->second->expire_at()))todelete.push_back(i->first);
		}
		for(STRINGVECTOR::iterator j=todelete.begin();j!=todelete.end();++j)
		{
			cached_files.erase(*j);
		}
	}

	void CleanFileFromDataCache(FtpFilePtr file)
	{
		std::wstring filename(file->MakeFilePathName(false));
		AutoCriticalSection cslock(data_cache_cs);
		CachedFiles::iterator i=cached_files.find(filename);
		if(i!=cached_files.end())
			cached_files.erase(i);
	}
 }