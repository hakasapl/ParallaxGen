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
	boost::property_tree::ptree readINIFile(const std::filesystem::path& ini_path, bool required);
	std::ifstream openFileHandle(const std::filesystem::path& file_path, bool required);
	void mergePropertyTrees(boost::property_tree::ptree& pt1, const boost::property_tree::ptree& pt2);

	std::filesystem::path getSystemPath(const GUID& folder_id);

	void exitWithUserInput(int exit_code);

	template <typename T>
	std::vector<T> getSubVector(const std::vector<T>& vec, size_t a, size_t b) {
		// Ensure a and b are within bounds and a <= b
		if (a > b || a >= vec.size() || b >= vec.size()) {
			throw std::out_of_range("Invalid indices for subvector");
		}

		// Create the subvector using vector's range constructor
		std::vector<T> subVector(vec.begin() + a, vec.begin() + b + 1);
		return subVector;
	}

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

	template<typename T>
	void addUniqueElement(std::vector<T>& vec, const T& element) {
		if (std::find(vec.begin(), vec.end(), element) == vec.end()) {
			vec.push_back(element);
		}
	}

	template<typename T>
	void shuffleVector(std::vector<T>& vec) {
		// Create a random number generator
		std::random_device rd;  // Seed
		std::mt19937 g(rd());   // Generator

		// Shuffle the vector
		std::shuffle(vec.begin(), vec.end(), g);
	}
}

#endif // !PARALLAXGENUTIL_H
