#pragma once
#include <optional>
#include <string>

namespace debug_lua {
	void ProcessBasicWindowEvents();

	class FileDialog {
	public:
		bool Open = true;
		const char* Filter = "All Files (*.*)\0*.*\0";
		const char* Title = nullptr;
		std::string SelectedPath;

		bool Show();
	};

	std::optional<std::string> GetEnvVariable(const char* var);
}
