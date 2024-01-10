#include "pch.h"
#include "utility.h"
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
