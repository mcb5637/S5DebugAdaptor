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
}