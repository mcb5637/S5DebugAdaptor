#include "pch.h"
#include "shok.h"

static inline bool(__thiscall* const dirfilesys_filter_allowedI)(BB::CDirectoryFileSystem::FilterData* th, const char* f) = reinterpret_cast<bool(__thiscall*)(BB::CDirectoryFileSystem::FilterData*, const char*)>(0x5532C4);
bool BB::CDirectoryFileSystem::FilterData::IsAllowedCaseInsensitive(const char* filename)
{
	return dirfilesys_filter_allowedI(this, filename);
}
static inline bool(__thiscall* const dirfilesys_filter_allowed)(BB::CDirectoryFileSystem::FilterData* th, const char* f) = reinterpret_cast<bool(__thiscall*)(BB::CDirectoryFileSystem::FilterData*, const char*)>(0x5532F8);
bool BB::CDirectoryFileSystem::FilterData::IsAllowed(const char* filename)
{
	return dirfilesys_filter_allowed(this, filename);
}

inline BB::CBBArchiveFile::DirectoryEntry* (__thiscall* const archivefile_searchbyhash)(BB::CBBArchiveFile* th, const char* fn) = reinterpret_cast<BB::CBBArchiveFile::DirectoryEntry * (__thiscall*)(BB::CBBArchiveFile*, const char*)>(0x5512E9);
BB::CBBArchiveFile::DirectoryEntry* BB::CBBArchiveFile::SearchByHash(const char* filename)
{
	return archivefile_searchbyhash(this, filename);
}
BB::CBBArchiveFile::DirectoryEntry* BB::CBBArchiveFile::GetByOffset(size_t offset)
{
	DirectoryEntry* r = nullptr;
	void* d = DirectoryData;
	size_t o = offset;
	__asm {
		mov eax, d;
		add eax, o;
		add eax, 4;
		mov r, eax;
	};
	return r;
}

static inline void(__cdecl* const filesystem_addfolder)(const char* p, char* d) = reinterpret_cast<void(__cdecl*)(const char*, char*)>(0x546514);
void BB::CFileSystemMgr::AddFolder(const char* path)
{
	filesystem_addfolder(path, nullptr);
	auto v = LoadOrder.SaveVector();
	size_t l = v.Vector.size();
	BB::IFileSystem* last = v.Vector[l - 1];
	for (int i = l - 1; i > 0; i--) {
		v.Vector[i] = v.Vector[i - 1];
	}
	v.Vector[0] = last;
}

std::unique_ptr<BB::CBBArchiveFile, CppLogic::DestroyCaller<BB::CBBArchiveFile>> BB::CBBArchiveFile::CreateUnique()
{
	return std::unique_ptr<CBBArchiveFile, CppLogic::DestroyCaller<CBBArchiveFile>>(Create());
}

void BB::CFileSystemMgr::AddArchive(const char* path)
{
	AddArchiveI(path, true);
}

bool BB::CFileSystemMgr::OpenFileAsHandle(const char* path, int& handle, size_t& size)
{
	return OpenFileHandle(path, &handle, &size);
}

static inline bool(__cdecl* const file_closehandle)(int h) = reinterpret_cast<bool(__cdecl*)(int)>(0x54E9EC);
bool BB::CFileSystemMgr::CloseHandle(int handle)
{
	return file_closehandle(handle);
}

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

BB::CFileStream::CFileStream()
{
	*reinterpret_cast<int*>(this) = vtp;
}
inline void(__thiscall* const filestream_dtor)(BB::CFileStream* f) = reinterpret_cast<void(__thiscall*)(BB::CFileStream*)>(0x548DBB);
BB::CFileStream::~CFileStream()
{
	filestream_dtor(this);
}
static inline bool(__thiscall* const filestream_open)(BB::CFileStream* th, const char* n, BB::IStream::Flags f) = reinterpret_cast<bool(__thiscall*)(BB::CFileStream*, const char*, BB::IStream::Flags)>(0x548DEE);
bool BB::CFileStream::OpenFile(const char* name, Flags mode)
{
	return filestream_open(this, name, mode);
}

inline void(__thiscall* const memstream_dtor)(BB::CMemoryStream* th) = reinterpret_cast<void(__thiscall*)(BB::CMemoryStream*)>(0x54EEB0);
BB::CMemoryStream::~CMemoryStream()
{
	memstream_dtor(this);
}
BB::CMemoryStream::CMemoryStream()
{
	*reinterpret_cast<int*>(this) = vtp;
}
inline void(__thiscall* const memstream_copyto)(const BB::CMemoryStream* th, BB::IStream* to) = reinterpret_cast<void(__thiscall*)(const BB::CMemoryStream*, BB::IStream*)>(0x5A19D1);
void BB::CMemoryStream::CopyToStream(IStream& to) const
{
	memstream_copyto(this, &to);
}
inline void(__thiscall* const memstream_copyfrom)(BB::CMemoryStream* th, BB::IStream* fr) = reinterpret_cast<void(__thiscall*)(BB::CMemoryStream*, BB::IStream*)>(0x5A1B04);
void BB::CMemoryStream::CopyFromStream(IStream& from)
{
	memstream_copyfrom(this, &from);
}

std::unique_ptr<BB::IStream> BB::IFileSystem::OpenFileStreamUnique(const char* path, BB::IStream::Flags mode)
{
	return std::unique_ptr<IStream>(OpenFileStream(path, mode));
}

static inline bool(__thiscall* const shok_BB_CFileStreamEx_OpenFile)(BB::CFileStreamEx* th, const char* name, BB::CFileStreamEx::Flags m) = reinterpret_cast<bool(__thiscall*)(BB::CFileStreamEx*, const char*, BB::CFileStreamEx::Flags)>(0x54924D);
static inline void(__thiscall* const shok_BB_CFileStreamEx_Close)(BB::CFileStreamEx* th) = reinterpret_cast<void(__thiscall*)(BB::CFileStreamEx*)>(0x54920A);
BB::CFileStreamEx::CFileStreamEx()
{
	*reinterpret_cast<int*>(this) = BB::CFileStreamEx::vtp;
}
inline void(__thiscall* const filestreamex_dtor)(BB::CFileStreamEx* f) = reinterpret_cast<void(__thiscall*)(BB::CFileStreamEx*)>(0x549215);
BB::CFileStreamEx::~CFileStreamEx()
{
	filestreamex_dtor(this);
}
bool BB::CFileStreamEx::OpenFile(const char* filename, Flags mode)
{
	return shok_BB_CFileStreamEx_OpenFile(this, filename, mode);
}
void BB::CFileStreamEx::Close()
{
	shok_BB_CFileStreamEx_Close(this);
}

const char* BB::CFileSystemMgr::ReadFileToString(const char* name, size_t* size)
{
	char* buff = nullptr;
	try
	{
		BB::CFileStreamEx filestr{};
		if (filestr.OpenFile(name, BB::CFileStreamEx::Flags::DefaultRead)) {
			size_t s = filestr.GetSize();
			if (size)
				*size = s;
			if (s > 0) {
				buff = new char[s + 1];
				memset(buff, 0, s + 1);
				filestr.Read(buff, s);
			}
			filestr.Close();
		}
	}
	catch (...)
	{
		if (buff)
			delete[] buff;
		return nullptr;
	}
	return buff;
}

bool BB::CFileSystemMgr::DoesFileExist(const char* name)
{
	FileInfo i{};
	(*GlobalObj)->GetFileInfo(&i, name, 0);
	return i.Found && !i.IsDirectory;
}
static inline void(__thiscall* const str_ctor)(shok::String* th, const char* s) = reinterpret_cast<void(__thiscall*)(shok::String*, const char*)>(0x4018C6);
shok::String::String(const char* s)
{
	str_ctor(this, s);
}
static inline void(__thiscall* const str_ctorcopy)(shok::String* th, const shok::String* ot) = reinterpret_cast<void(__thiscall*)(shok::String*, const shok::String*)>(0x401808);
shok::String::String(const shok::String& c)
{
	str_ctorcopy(this, &c);
}
shok::String::String(const std::string& s)
{
	*this = s;
}
shok::String::String(const std::string_view& s)
{
	*this = s;
}
const char* shok::String::c_str() const
{
	if (allocated < 16)
		return data.inlin;
	else
		return data.alloc;
}
size_t shok::String::size() const
{
	return size_v;
}
shok::String::~String()
{
	if (allocated >= 16)
		shok::Free(data.alloc);
}
static inline void(__thiscall* const str_assign)(shok::String* th, const char* c) = reinterpret_cast<void(__thiscall*)(shok::String*, const char*)>(0x40182E);
void shok::String::assign(const char* s)
{
	str_assign(this, s);
}
inline void(__thiscall* const str_assign_l)(shok::String* th, const char* c, size_t l) = reinterpret_cast<void(__thiscall*)(shok::String*, const char*, size_t)>(0x401799);
void shok::String::assign(const char* s, size_t len)
{
	str_assign_l(this, s, len);
}
shok::String::String() : shok::String("")
{
}

std::strong_ordering shok::String::operator<=>(const String& r) const
{
	return std::string_view{ this->c_str(), this->size() } <=> std::string_view{ r.c_str(), r.size() };
}
bool shok::String::operator==(const String& r) const
{
	return (*this <=> r) == std::strong_ordering::equal;
}
void shok::String::operator=(const String& s)
{
	assign(s.c_str(), s.size());
}
void shok::String::operator=(const std::string& s)
{
	assign(s.c_str(), s.size());
}
void shok::String::operator=(const std::string_view& s)
{
	assign(s.data(), s.size());
}
void shok::String::operator=(const char* s)
{
	assign(s);
}
shok::String::operator std::string_view() const
{
	return { c_str(), size() };
}

std::strong_ordering shok::operator<=>(const String& a, const char* b)
{
	return std::string_view{ a.c_str(), a.size() } <=> std::string_view{ b };
}
bool shok::operator==(const String& a, const char* b)
{
	return (a <=> b) == std::strong_ordering::equal;
}
std::strong_ordering shok::operator<=>(const char* a, const String& b)
{
	return std::string_view{ a } <=> std::string_view{ b.c_str(), b.size() };
}
bool shok::operator==(const char* a, const String& b)
{
	return (a <=> b) == std::strong_ordering::equal;
}

void ECore::IReplayStreamExtension::unknown0()
{
}

inline bool(__thiscall* skeys_checknorm)(const Framework::SKeys* th, const Framework::SKeys* map) = reinterpret_cast<bool(__thiscall*)(const Framework::SKeys*, const Framework::SKeys*)>(0x51B9F7);
bool Framework::SKeys::Check(const SKeys& map) const
{
    return skeys_checknorm(this, &map);
}
inline bool(__thiscall* skeys_checksp)(const Framework::SKeys* th, const Framework::SKeys* map) = reinterpret_cast<bool(__thiscall*)(const Framework::SKeys*, const Framework::SKeys*)>(0x51BA3C);
bool Framework::SKeys::CheckSP(const SKeys& map) const
{
    return skeys_checksp(this, &map);
}

inline GS3DTools::CMapData* (__thiscall* const mapdata_assign)(GS3DTools::CMapData* th, const GS3DTools::CMapData* o) = reinterpret_cast<GS3DTools::CMapData * (__thiscall*)(GS3DTools::CMapData*, const GS3DTools::CMapData*)>(0x4029B8);
GS3DTools::CMapData& GS3DTools::CMapData::operator=(const GS3DTools::CMapData& o)
{
    if (this == &o)
        return *this;
    return *mapdata_assign(this, &o);
}

int(__thiscall* const cinfo_getindex)(Framework::CampagnInfo* th, const char* s) = reinterpret_cast<int(__thiscall* const)(Framework::CampagnInfo*, const char*)>(0x51A23B);
int Framework::CampagnInfo::GetMapIndexByName(const char* s)
{
    return cinfo_getindex(this, s);
}

Framework::MapInfo* Framework::CampagnInfo::GetMapInfoByName(const char* n)
{
    int i = GetMapIndexByName(n);
    if (i < 0)
        return nullptr;
    return &Maps[i];
}

inline bool(__thiscall* const savedata_load)(Framework::SavegameSystem* th, const char* s) = reinterpret_cast<bool(__thiscall* const)(Framework::SavegameSystem*, const char*)>(0x403253);
bool Framework::SavegameSystem::LoadSaveData(const char* name)
{
    return savedata_load(this, name);
}

inline void(__thiscall* const savesys_save)(Framework::SavegameSystem* th, const char* slot, GS3DTools::CMapData* mapdata, const char* name, bool debugSave) = reinterpret_cast<void(__thiscall*)(Framework::SavegameSystem*, const char*, GS3DTools::CMapData*, const char*, bool)>(0x4031C4);
void Framework::SavegameSystem::SaveGame(const char* slot, GS3DTools::CMapData* mapdata, const char* name, bool debugSave)
{
    savesys_save(this, slot, mapdata, name, debugSave);
}

Framework::CampagnInfo* Framework::CMain::CIH::GetCampagnInfo(shok::MapType i, const char* n)
{
    int i2 = static_cast<int>(i);
    if (i2 < -1 || i2 >= 4)
        return nullptr;
    Framework::CampagnInfo* r = nullptr;
    auto* th = this;
    __asm {
        push n;
        mov ecx, th;
        mov eax, i2;
        mov edx, 0x40BB16; // this thing has first param in eax, no known calling convention, so i have to improvise
        call edx;
        pop edx;
        mov r, eax;
    }
    return r;
}
Framework::CampagnInfo* (__thiscall* const framew_getcinfo)(void* th, const GS3DTools::CMapData* s) = reinterpret_cast<Framework::CampagnInfo * (__thiscall* const)(void*, const GS3DTools::CMapData*)>(0x5190D5);
Framework::CampagnInfo* Framework::CMain::CIH::GetCampagnInfo(GS3DTools::CMapData* d)
{
    return framew_getcinfo(this, d);
}
