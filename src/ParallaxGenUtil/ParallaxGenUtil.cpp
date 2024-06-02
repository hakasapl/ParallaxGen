#include "ParallaxGenUtil/ParallaxGenUtil.hpp"

#include <boost/property_tree/ini_parser.hpp>
#include <shlobj.h>
#include <iostream>
#include <fstream>

using namespace std;
namespace fs = filesystem;

namespace ParallaxGenUtil {
	void mergePropertyTrees(boost::property_tree::ptree& pt1, const boost::property_tree::ptree& pt2) {
		for (const auto& kv : pt2) {
			const auto& key = kv.first;
			const auto& value = kv.second;

			// Check if the value is a subtree or a single value
			if (value.empty()) {
				pt1.put(key, value.data()); // Overwrite single value
			}
			else {
				mergePropertyTrees(pt1.put_child(key, boost::property_tree::ptree()), value); // Recurse for subtrees
			}
		}
	}

	boost::property_tree::ptree readINIFile(const fs::path& ini_path, const bool required = false)
	{
		boost::property_tree::ptree pt_out;

		// open ini file handle
		ifstream f = openFileHandle(ini_path, required);
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
}
