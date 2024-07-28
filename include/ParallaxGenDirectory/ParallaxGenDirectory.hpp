#pragma once

#include <vector>
#include <filesystem>
#include <array>
#include <nlohmann/json.hpp>

#include "BethesdaDirectory/BethesdaDirectory.hpp"

class ParallaxGenDirectory : public BethesdaDirectory {
private:
	std::filesystem::path EXE_PATH;
	static inline const std::wstring LO_PGCONFIG_PATH = L"parallaxgen";

	std::vector<std::filesystem::path> heightMaps;
	std::vector<std::filesystem::path> complexMaterialMaps;
	std::vector<std::filesystem::path> meshes;
  std::vector<nlohmann::json> truePBRConfigs;
  
	nlohmann::json PG_config;

	static inline const std::vector<std::string> truePBR_filename_fields = { "match_normal", "match_diffuse", "rename" };

public:
	static inline const std::filesystem::path default_cubemap_path = "textures\\cubemaps\\dynamic1pxcubemap_black.dds";

	// constructor - calls the BethesdaDirectory constructor
	ParallaxGenDirectory(BethesdaGame bg, std::filesystem::path EXE_PATH);

	// searches for height maps in the data directory
	void findHeightMaps();
	// searches for complex material maps in the data directory
	void findComplexMaterialMaps();
	// searches for meshes in the data directory
	void findMeshes();
	// find truepbr config files
	void findTruePBRConfigs();

	// get the parallax gen config
	void loadPGConfig(bool load_default);

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

	// Helpers
	static void merge_json_smart(nlohmann::json& target, const nlohmann::json& source);
	static std::vector<std::wstring> jsonArrayToWString(const nlohmann::json& json_array);
	static void replaceForwardSlashes(nlohmann::json& j);
};
