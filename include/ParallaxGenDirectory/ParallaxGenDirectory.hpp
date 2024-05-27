#ifndef PARALLAXGENDIRECTORYITERATOR_H
#define PARALLAXGENDIRECTORYITERATOR_H

#include <vector>
#include <filesystem>

#include "BethesdaDirectory/BethesdaDirectory.hpp"

class ParallaxGenDirectory : public BethesdaDirectory {
private:
	std::vector<std::filesystem::path> heightMaps;
	std::vector<std::filesystem::path> complexMaterialMaps;
	std::vector<std::filesystem::path> meshes;

public:
	ParallaxGenDirectory(BethesdaGame bg);

	std::vector<std::filesystem::path> findHeightMaps() const;
	std::vector<std::filesystem::path> findComplexMaterialMaps() const;
	std::vector<std::filesystem::path> findMeshes() const;
};

#endif