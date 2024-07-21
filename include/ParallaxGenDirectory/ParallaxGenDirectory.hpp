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

	std::vector<std::filesystem::path> heightMaps;
	std::vector<std::filesystem::path> complexMaterialMaps;
	std::vector<std::filesystem::path> meshes;

public:
	// constructor - calls the BethesdaDirectory constructor
	ParallaxGenDirectory(BethesdaGame bg);

	// searches for height maps in the data directory
	void findHeightMaps();
	// searches for complex material maps in the data directory
	void findComplexMaterialMaps();
	// searches for meshes in the data directory
	void findMeshes();

	// add methods
	void addHeightMap(std::filesystem::path path);
	void addComplexMaterialMap(std::filesystem::path path);
	void addMesh(std::filesystem::path path);

	// is methods
	bool isHeightMap(std::filesystem::path path) const;
	bool isComplexMaterialMap(std::filesystem::path path) const;
	bool isMesh(std::filesystem::path) const;

	// get methods
	const std::vector<std::filesystem::path> getHeightMaps() const;
	const std::vector<std::filesystem::path> getComplexMaterialMaps() const;
	const std::vector<std::filesystem::path> getMeshes() const;

	// helpers
	static std::filesystem::path changeDDSSuffix(std::filesystem::path path, std::wstring suffix);
};

#endif
