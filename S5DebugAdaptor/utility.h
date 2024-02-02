#pragma once
#include <string_view>

namespace debug_lua {
	class EnsureBbaLoaded {
		bool NeedsPop = false;

	public:
		EnsureBbaLoaded(std::string_view file);
		EnsureBbaLoaded(const EnsureBbaLoaded&) = delete;
		EnsureBbaLoaded(EnsureBbaLoaded&&) = delete;
		void operator=(const EnsureBbaLoaded&) = delete;
		void operator=(EnsureBbaLoaded&&) = delete;
		~EnsureBbaLoaded();
	};

	std::string ANSIToUTF8(std::string_view data);
	std::string UTF8ToANSI(std::string_view data);

	std::string EnsureUTF8(std::string_view data);
}