#include "pch.h"
#include "shok.h"

bool __stdcall BB::IStream::rettrue()
{
	return true;
}
bool __stdcall BB::IStream::rettrue1()
{
	return true;
}
bool __stdcall BB::IStream::rettrue2()
{
	return true;
}
const char* __stdcall BB::IStream::GetFileName()
{
	return (*reinterpret_cast<const char* (__stdcall***)(BB::IStream*)>(this))[4](this);
}
int64_t __stdcall BB::IStream::GetLastWriteTime()
{
	return (*reinterpret_cast<int64_t(__stdcall***)(BB::IStream*)>(this))[5](this);
}
size_t __stdcall BB::IStream::GetSize()
{
	return (*reinterpret_cast<size_t(__stdcall***)(BB::IStream*)>(this))[6](this);
}
void __stdcall BB::IStream::SetFileSize(long size)
{
	return (*reinterpret_cast<void(__stdcall***)(BB::IStream*, long)>(this))[7](this, size);
}
long __stdcall BB::IStream::GetFilePointer()
{
	return (*reinterpret_cast<long(__stdcall***)(BB::IStream*)>(this))[8](this);
}
void __stdcall BB::IStream::SetFilePointer(long fp)
{
	return (*reinterpret_cast<void(__stdcall***)(BB::IStream*, long)>(this))[9](this, fp);
}
long __stdcall BB::IStream::Read(void* buff, long numBytesToRead)
{
	return (*reinterpret_cast<long(__stdcall***)(BB::IStream*, void*, long)>(this))[10](this, buff, numBytesToRead);
}
void __stdcall BB::IStream::Seek(long seek, SeekMode mode)
{
	return (*reinterpret_cast<void(__stdcall***)(BB::IStream*, long, SeekMode)>(this))[11](this, seek, mode);
}
void __stdcall BB::IStream::Write(const void* buff, long numBytesToWrite)
{
	return (*reinterpret_cast<void(__stdcall***)(BB::IStream*, const void*, long)>(this))[12](this, buff, numBytesToWrite);
}
static inline bool(__thiscall* const shok_BB_CFileStreamEx_OpenFile)(BB::CFileStreamEx* th, const char* name, BB::CFileStreamEx::Flags m) = reinterpret_cast<bool(__thiscall*)(BB::CFileStreamEx*, const char*, BB::CFileStreamEx::Flags)>(0x54924D);
static inline void(__thiscall* const shok_BB_CFileStreamEx_Close)(BB::CFileStreamEx* th) = reinterpret_cast<void(__thiscall*)(BB::CFileStreamEx*)>(0x54920A);
BB::CFileStreamEx::CFileStreamEx()
{
	*reinterpret_cast<int*>(this) = BB::CFileStreamEx::vtp;
}
bool BB::CFileStreamEx::OpenFile(const char* filename, Flags mode)
{
	return shok_BB_CFileStreamEx_OpenFile(this, filename, mode);
}
void BB::CFileStreamEx::Close()
{
	shok_BB_CFileStreamEx_Close(this);
}