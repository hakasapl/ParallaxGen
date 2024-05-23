#ifndef PARALLAXGENUTIL_H
#define PARALLAXGENUTIL_H

#include <filesystem>
#include <boost/property_tree/ptree.hpp>
#include <windows.h>
#include <unordered_set>

namespace ParallaxGenUtil
{
	boost::property_tree::ptree readINIFile(const std::filesystem::path& ini_path, bool required);
	std::ifstream openFileHandle(const std::filesystem::path& file_path, bool required);
	void mergePropertyTrees(boost::property_tree::ptree& pt1, const boost::property_tree::ptree& pt2);

	std::filesystem::path getSystemPath(const GUID& folder_id);

	void exitWithUserInput(int exit_code);

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
}

#endif // !PARALLAXGENUTIL_H
