#ifndef PARALLAXGENDIRECTORYITERATOR_H
#define PARALLAXGENDIRECTORYITERATOR_H

#include <vector>
#include <filesystem>
#include <array>

#include "BethesdaDirectory/BethesdaDirectory.hpp"

class ParallaxGenDirectory : public BethesdaDirectory {
private:
	// list of mesh block names to ignore (if the path has any of these as a component it is ignored)
	static inline const std::vector<std::string> mesh_blocklist = {
		"actors",
		"cameras",
		"decals",
		"effects",
		"interface",
		"lod",
		"magic",
		"markers",
		"mps",
		"sky",
		"water"
	};

public:
	// constructor - calls the BethesdaDirectory constructor
	ParallaxGenDirectory(BethesdaGame bg);

	// searches for height maps in the data directory
	std::vector<std::filesystem::path> findHeightMaps() const;
	// searches for complex material maps in the data directory
	std::vector<std::filesystem::path> findComplexMaterialMaps() const;
	// searches for meshes in the data directory
	std::vector<std::filesystem::path> findMeshes() const;
};

#endif
