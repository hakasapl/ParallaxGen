#ifndef PARALLAXGENDIRECTORYITERATOR_H
#define PARALLAXGENDIRECTORYITERATOR_H

#include <vector>
#include <filesystem>
#include <array>

#include "BethesdaDirectory/BethesdaDirectory.hpp"

class ParallaxGenDirectory : public BethesdaDirectory {
private:
	// list of mesh block names to ignore (if the path has any of these as a component it is ignored)
	static inline const std::vector<std::wstring> mesh_blocklist = {
		L"actors",
		L"cameras",
		L"decals",
		L"effects",
		L"interface",
		L"loadscreenart",
		L"lod",
		L"magic",
		L"markers",
		L"mps",
		L"sky",
		L"water"
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
