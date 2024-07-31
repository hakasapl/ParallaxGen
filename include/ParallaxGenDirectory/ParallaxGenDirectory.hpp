#pragma once

#include <array>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <vector>

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

  // Validation is hardcoded to make it user-proof. Match the schema of the
  // config, but instead of array leafs, string leafs with regex patterns for
  // acceptable list values are used
  static inline const std::string PG_config_validation_raw = R"(
	{
		"parallax_lookup": {
			"archive_blocklist": "^[^\\/\\\\]*$",
			"allowlist": ".*_.*\\.dds$",
			"blocklist": ".*"
		},
		"complexmaterial_lookup": {
			"archive_blocklist": "^[^\\/\\\\]*$",
			"allowlist": ".*_.*\\.dds$",
			"blocklist": ".*"
		},
		"truepbr_cfg_lookup": {
			"archive_blocklist": "^[^\\/\\\\]*$",
			"allowlist": ".*\\.json$",
			"blocklist": ".*"
		},
		"nif_lookup": {
			"archive_blocklist": "^[^\\/\\\\]*$",
			"allowlist": ".*\\.nif$",
			"blocklist": ".*"
		}
	}
	)";
  static inline const nlohmann::json PG_config_validation =
      nlohmann::json::parse(PG_config_validation_raw);

  static inline const std::vector<std::string> truePBR_filename_fields = {
      "match_normal", "match_diffuse", "rename"};

public:
  static inline const std::filesystem::path default_cubemap_path =
      "textures\\cubemaps\\dynamic1pxcubemap_black.dds";

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

  const std::string getHeightMapFromBase(const std::string &base) const;
  const std::string
  getComplexMaterialMapFromBase(const std::string &base) const;

  // Helpers
  static void merge_json_smart(nlohmann::json &target,
                               const nlohmann::json &source,
                               const nlohmann::json &validation);
  static bool validate_json(const nlohmann::json &item,
                            const nlohmann::json &validation,
                            const std::string &key);
  static std::vector<std::wstring>
  jsonArrayToWString(const nlohmann::json &json_array);
  static void replaceForwardSlashes(nlohmann::json &j);
  static std::filesystem::path
  matchBase(const std::string &base,
            const std::vector<std::filesystem::path> &search_list);
};
