#ifndef PARALLAXGENUTIL_H
#define PARALLAXGENUTIL_H

#include <filesystem>
#include <windows.h>
#include <unordered_set>
#include <algorithm>
#include <random>

namespace ParallaxGenUtil
{
	// opens a file handle for a file
	std::ifstream openFileHandle(const std::filesystem::path& file_path, const bool required);

	// get a single value from ini file
	std::wstring readINIValue(const std::filesystem::path& ini_path, const std::wstring& section, const std::wstring& key, const bool logging, const bool first_ini_read);

	// terminals usually auto exit when program ends, this function waits for user input before exiting
	void exitWithUserInput(const int exit_code);

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
