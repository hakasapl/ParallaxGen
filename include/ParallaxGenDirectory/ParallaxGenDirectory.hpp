#ifndef PARALLAXGENDIRECTORYITERATOR_H
#define PARALLAXGENDIRECTORYITERATOR_H

#include <vector>
#include <filesystem>
#include <array>
#include <nlohmann/json.hpp>

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

	static inline const std::vector<std::string> truePBR_filename_fields = { "match_normal", "match_diffuse", "rename" };
	std::vector<nlohmann::json> truePBRConfigs;

public:
	static inline const std::filesystem::path default_cubemap_path = "textures\\cubemaps\\dynamic1pxcubemap_black.dds";

	// constructor - calls the BethesdaDirectory constructor
	ParallaxGenDirectory(BethesdaGame bg);

	// searches for height maps in the data directory
	void findHeightMaps();
	// searches for complex material maps in the data directory
	void findComplexMaterialMaps();
	// searches for meshes in the data directory
	void findMeshes();
	// find truepbr config files
	void findTruePBRConfigs();

	// add methods
	void addHeightMap(std::filesystem::path path);
	void addComplexMaterialMap(std::filesystem::path path);
	void addMesh(std::filesystem::path path);

	// is methods
	bool isHeightMap(std::filesystem::path path) const;
	bool isComplexMaterialMap(std::filesystem::path path) const;
	bool isMesh(std::filesystem::path) const;

	bool defCubemapExists();

	// get methods
	const std::vector<std::filesystem::path> getHeightMaps() const;
	const std::vector<std::filesystem::path> getComplexMaterialMaps() const;
	const std::vector<std::filesystem::path> getMeshes() const;
	const std::vector<nlohmann::json> getTruePBRConfigs() const;
};

#endif
