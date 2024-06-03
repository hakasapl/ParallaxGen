#ifndef PARALLAXGENUTIL_H
#define PARALLAXGENUTIL_H

#include <filesystem>
#include <boost/property_tree/ptree.hpp>
#include <windows.h>
#include <unordered_set>
#include <algorithm>
#include <random>

namespace ParallaxGenUtil
{
	// reads ini file into ptree
	boost::property_tree::wptree readINIFile(const std::filesystem::path& ini_path, const bool required = false, const bool logging = false);

	// opens a file handle for a file
	std::ifstream openFileHandle(const std::filesystem::path& file_path, const bool required);

	// merges two ptrees
	void mergePropertyTrees(boost::property_tree::wptree& pt1, const boost::property_tree::wptree& pt2);

	// gets the system path for a folder (from windows.h)
	std::filesystem::path getSystemPath(const GUID& folder_id);

	// terminals usually auto exit when program ends, this function waits for user input before exiting
	void exitWithUserInput(const int exit_code);

	// converts fs::path to a lowercase variant (for case-insensitive comparison)
	void pathLower(std::filesystem::path& path);

	// converts a string to a wstring
	std::wstring convertToWstring(const std::string str);

	std::string wstring_to_utf8(const std::wstring& str);

	std::vector<std::byte> getFileBytes(const std::filesystem::path& file_path);

	// concatenates two vectors without duplicates
	template <typename T>
	void concatenateVectorsWithoutDuplicates(std::vector<T>& vec1, const std::vector<T>& vec2) {
		std::unordered_set<T> set(vec1.begin(), vec1.end());

		for (const auto& element : vec2) {
			if (set.find(element) == set.end()) {
				vec1.push_back(element);
				set.insert(element);
			}
		}
	};

	// adds an element to a vector if it is not already present
	template<typename T>
	void addUniqueElement(std::vector<T>& vec, const T& element) {
		if (std::find(vec.begin(), vec.end(), element) == vec.end()) {
			vec.push_back(element);
		}
	}
}

#endif // !PARALLAXGENUTIL_H
