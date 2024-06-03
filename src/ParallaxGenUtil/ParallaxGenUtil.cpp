#include "ParallaxGenUtil/ParallaxGenUtil.hpp"

#include <spdlog/spdlog.h>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/algorithm/string.hpp>
#include <shlobj.h>
#include <iostream>
#include <fstream>
#include <codecvt>

using namespace std;
namespace fs = filesystem;

namespace ParallaxGenUtil {
	void mergePropertyTrees(boost::property_tree::wptree& pt1, const boost::property_tree::wptree& pt2) {
		for (const auto& kv : pt2) {
			const auto& key = kv.first;
			const auto& value = kv.second;

			// Check if the value is a subtree or a single value
			if (value.empty()) {
				pt1.put(key, value.data()); // Overwrite single value
			}
			else {
				mergePropertyTrees(pt1.put_child(key, boost::property_tree::wptree()), value); // Recurse for subtrees
			}
		}
	}

	boost::property_tree::wptree readINIFile(const fs::path& ini_path, const bool required, const bool logging)
	{
		boost::property_tree::wptree pt_out;

		// open ini file handle
		wifstream f(ini_path);

		if (!f.is_open()) {
			if (required) {
				if (logging) {
					spdlog::critical("Unable to open INI: {}", ini_path.string());
					exitWithUserInput(1);
				}
				else {
					throw runtime_error("Unable to open file: " + ini_path.string());
				}
			}

			return pt_out;
		}

		read_ini(f, pt_out);
		f.close();

		return pt_out;
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

	fs::path getSystemPath(const GUID& folder_id)
	{
		PWSTR path = NULL;
		HRESULT result = SHGetKnownFolderPath(folder_id, 0, NULL, &path);
		if (SUCCEEDED(result)) {
			wstring appDataPath(path);
			CoTaskMemFree(path);  // Free the memory allocated for the path

			return fs::path(path);
		}
		else {
			// Handle error
			return fs::path("");
		}
	}

	void exitWithUserInput(const int exit_code)
	{
		cout << "Press any key to exit...";
		cin.get();
		exit(exit_code);
	}

	void pathLower(fs::path& path) {
		wstring path_str = path.wstring();
		boost::algorithm::to_lower(path_str);
		path = fs::path(path_str);
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
}
