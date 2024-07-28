#include "ParallaxGenUtil/ParallaxGenUtil.hpp"

#include <spdlog/spdlog.h>
#include <boost/algorithm/string.hpp>
#include <iostream>
#include <fstream>
#include <codecvt>
#include <DirectXTex.h>

using namespace std;
namespace fs = filesystem;

namespace ParallaxGenUtil {
	wstring readINIValue(const fs::path& ini_path, const wstring& section, const wstring& key, const bool logging, const bool first_ini_read)
	{
		if (!fs::exists(ini_path)) {
			if (logging && first_ini_read) {
				spdlog::warn(L"INI file does not exist (ignoring): {}", ini_path.wstring());
			}
			return L"";
		}

		wifstream f(ini_path);
		if (!f.is_open()) {
			if (logging && first_ini_read) {
				spdlog::warn(L"Unable to open INI (ignoring): {}", ini_path.wstring());
			}
			return L"";
		}

		wstring cur_line;
		wstring cur_section;
		bool foundSection = false;

		while(getline(f, cur_line)) {
			boost::trim(cur_line);

			// ignore comments
			if (cur_line.empty() || cur_line[0] == ';' || cur_line[0] == '#') {
            	continue;
        	}

			// Check if it's a section
			if (cur_line.front() == '[' && cur_line.back() == ']') {
				cur_section = cur_line.substr(1, cur_line.size() - 2);
				continue;
			}

			// Check if it's the correct section
			if (boost::iequals(cur_section, section)) {
				foundSection = true;
			}

			if (!boost::iequals(cur_section, section)) {
				// exit if already checked section
				if (foundSection) {
					break;
				}
				continue;
			}

			// check key
			size_t pos = cur_line.find('=');
			if (pos != std::string::npos) {
				// found key value pair
				wstring cur_key = cur_line.substr(0, pos);
				boost::trim(cur_key);
				if (boost::iequals(cur_key, key)) {
					wstring cur_value = cur_line.substr(pos + 1);
					boost::trim(cur_value);
					return cur_value;
				}
			}
		}

		return L"";
	}

	ifstream openFileHandle(const fs::path& file_path, const bool required = false) {
		ifstream file(file_path);
		if (!file.is_open()) {
			if (required) {
				throw runtime_error("Unable to open file: " + file_path.string());
			}
		}

		return file;
	}

	void exitWithUserInput(const int exit_code)
	{
		cout << "Press any key to exit...";
		cin.get();
		exit(exit_code);
	}

	wstring convertToWstring(const string str) {
		size_t length = str.length() + 1; // Including null terminator
		std::vector<wchar_t> wbuffer(length);
		size_t convertedChars = 0;

		errno_t err = mbstowcs_s(&convertedChars, wbuffer.data(), wbuffer.size(), str.c_str(), length - 1);
		if (err != 0) {
			throw std::runtime_error("Conversion failed");
		}

		return std::wstring(wbuffer.data());
	}

	string wstring_to_utf8(const wstring& str) {
		if (str.empty()) return std::string();
		int size_needed = WideCharToMultiByte(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0, NULL, NULL);
		std::string str_to(size_needed, 0);
		WideCharToMultiByte(CP_UTF8, 0, &str[0], (int)str.size(), &str_to[0], size_needed, NULL, NULL);
		return str_to;
	}

	vector<std::byte> getFileBytes(const fs::path& file_path) {
		ifstream inputFile(file_path, ios_base::binary);

		inputFile.seekg(0, ios_base::end);
		auto length = inputFile.tellg();
		inputFile.seekg(0, ios_base::beg);

		// Make a buffer of the exact size of the file and read the data into it.
		vector<std::byte> buffer(length);
		inputFile.read(reinterpret_cast<char*>(buffer.data()), length);

		inputFile.close();

		return buffer;
	}

	filesystem::path replaceLastOf(const filesystem::path& path, const wstring& to_replace, const wstring& replace_with) {
		wstring path_str = path.wstring();
		size_t pos = path_str.rfind(to_replace);
		if (pos == wstring::npos) {
			return path;
		}

		path_str.replace(pos, to_replace.size(), replace_with);
		return filesystem::path(path_str);
	}

	string replaceLastOf(const string& path, const string& to_replace, const string& replace_with) {
		size_t pos = path.rfind(to_replace);
		if (pos == wstring::npos) {
			return path;
		}

		string out_path = path;
		out_path.replace(pos, to_replace.size(), replace_with);
		return out_path;
	}
}
