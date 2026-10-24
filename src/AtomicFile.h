#pragma once

#include <Windows.h>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <system_error>

namespace OStrands
{
	inline void WriteFileAtomically(const std::filesystem::path& destination, std::string_view contents)
	{
		static std::atomic<unsigned long long> sequence{ 0 };
		std::filesystem::path temporary;
		for (;;) {
			temporary = destination;
			temporary += L".tmp." + std::to_wstring(GetCurrentProcessId()) + L"." + std::to_wstring(sequence++);
			HANDLE file = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
			if (file != INVALID_HANDLE_VALUE) {
				CloseHandle(file);
				break;
			}
			const auto error = GetLastError();
			if (error != ERROR_FILE_EXISTS && error != ERROR_ALREADY_EXISTS) {
				throw std::system_error(static_cast<int>(error), std::system_category(), "create temporary configuration");
			}
		}
		try {
			std::ofstream stream;
			stream.exceptions(std::ios::failbit | std::ios::badbit);
			stream.open(temporary, std::ios::binary | std::ios::trunc);
			stream.write(contents.data(), static_cast<std::streamsize>(contents.size()));
			stream.flush();
			stream.close();
			if (!MoveFileExW(temporary.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
				throw std::system_error(static_cast<int>(GetLastError()), std::system_category(), "replace configuration");
			}
		} catch (...) {
			std::error_code ignored;
			std::filesystem::remove(temporary, ignored);
			throw;
		}
	}
}
