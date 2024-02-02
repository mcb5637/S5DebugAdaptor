#include "pch.h"
#include "utility.h"

#include <stdexcept>

#include <uni_algo/all.h>

#include "shok.h"

debug_lua::EnsureBbaLoaded::EnsureBbaLoaded(std::string_view file)
{
	if (file.empty())
		return;
	BB::CFileSystemMgr* m = *BB::CFileSystemMgr::GlobalObj;
	for (const auto* f : m->LoadOrder) {
		if (const auto* a = dynamic_cast<const BB::CBBArchiveFile*>(f)) {
			if (a->ArchiveFile.Filename == file)
				return;
		}
	}
	m->AddArchive(file.data());
	NeedsPop = true;
}
debug_lua::EnsureBbaLoaded::~EnsureBbaLoaded()
{
	if (NeedsPop) {
		NeedsPop = false;
		(*BB::CFileSystemMgr::GlobalObj)->RemoveTopArchive();
	}
}

template<UINT CP>
std::wstring To16(std::string_view data) {
	size_t len = MultiByteToWideChar(CP, 0, data.data(), data.size(), nullptr, 0);
	std::wstring r{};
	r.resize(len);
	if (!MultiByteToWideChar(CP, 0, data.data(), data.size(), r.data(), r.size()))
		throw std::invalid_argument("data");
	return r;
}
template<UINT CP>
std::string To8(std::wstring_view data) {
	size_t len = WideCharToMultiByte(CP, 0, data.data(), data.size(), nullptr, 0, 0, nullptr);
	std::string r{};
	r.resize(len);
	if (!WideCharToMultiByte(CP, 0, data.data(), data.size(), r.data(), r.size(), 0, nullptr))
		throw std::invalid_argument("data");
	return r;
}

std::string debug_lua::ANSIToUTF8(std::string_view data)
{
	if (data.empty())
		return "";

	return To8<CP_UTF8>(To16<CP_ACP>(data));
}

std::string debug_lua::UTF8ToANSI(std::string_view data)
{
	if (data.empty())
		return "";

	return To8<CP_ACP>(To16<CP_UTF8>(data));
}

std::string debug_lua::EnsureUTF8(std::string_view data)
{
	if (una::is_valid_utf8(data))
		return std::string{ data };
	return una::norm::to_nfc_utf8(data);
}
