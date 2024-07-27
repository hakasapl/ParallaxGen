#ifndef PARALLAXGEN_H
#define PARALLAXGEN_H

#include <filesystem>
#include <array>
#include <NifFile.hpp>
#include <miniz.h>
#include <unordered_map>
#include <tuple>
#include <nlohmann/json.hpp>

#include "ParallaxGenDirectory/ParallaxGenDirectory.hpp"
#include "ParallaxGenD3D/ParallaxGenD3D.hpp"
#include "ParallaxGenTask/ParallaxGenTask.hpp"

class ParallaxGen
{
private:
    std::filesystem::path output_dir;
    ParallaxGenDirectory* pgd;
	ParallaxGenD3D* pgd3d;

    // we don't want any other change on the NIF file so we disable all save options from nifly
	nifly::NifSaveOptions nif_save_options = { false, true };

    // which texture indices to search when mapping mesh shapes to parallax maps
	static inline const std::array<uint32_t, 2> texture_maps_idx_search = { 0, 1 };

	static inline const std::vector<std::wstring> dynCubemap_ignore_list = { L"armor", L"weapons" };

    // bools to ignore parallax and/or complex material
	bool ignore_parallax;
	bool ignore_complex_material;
	bool ignore_truepbr;

	static inline const std::vector<std::vector<std::string>> default_suffixes = {
		{".dds"},
		{"_n.dds"},
		{"_g.dds", "_sk.dds"},
		{"_p.dds"},
		{"_e.dds"},
		{"_m.dds", "_em.dds"},
		{"_i.dds"},
		{"_s.dds", "_b.dds", "_bl.dds"},
		{}
	};

	static inline std::string empty_path = "";

public:
	static inline const std::string parallax_state_file = "PARALLAXGEN_DONTDELETE";

    // constructor
    ParallaxGen(const std::filesystem::path output_dir, ParallaxGenDirectory* pgd, ParallaxGenD3D* pgd3d, bool optimize_meshes = false, bool ignore_parallax = false, bool ignore_complex_material = false, bool ignore_truepbr = false);
	// upgrades textures whenever possible
	void upgradeShaders();
    // enables parallax on relevant meshes
	void patchMeshes();
	// zips all meshes and removes originals
	void zipMeshes();
	// deletes generated meshes
	void deleteMeshes();
	// deletes entire output folder
	void deleteOutputDir();
	// init output folder
	void initOutputDir();

private:
// TODO what can be const here?
	ParallaxGenTask::PGResult convertHeightMapToComplexMaterial(const std::filesystem::path& height_map);
    // processes a NIF file (enable parallax if needed)
	ParallaxGenTask::PGResult processNIF(const std::filesystem::path& nif_file);
	// processes a shape within a NIF file
	ParallaxGenTask::PGResult processShape(const std::filesystem::path& nif_file, nifly::NifFile& nif, const uint32_t shape_block_id, nifly::NiShape* shape, bool& nif_modified, const bool has_attached_havok);
	// check if truepbr should be enabled on shape
	ParallaxGenTask::PGResult shouldApplyTruePBRConfig(const std::filesystem::path& nif_file, nifly::NifFile& nif, const uint32_t shape_block_id, nifly::NiShape* shape, nifly::NiShader* shader, const std::array<std::string, 9>& search_prefixes, bool& enable_result, nlohmann::json& truepbr_data, std::string& matched_path);
	// check if complex material should be enabled on shape
	static inline const int cm_slot_search[2] = { 0, 1 };
	ParallaxGenTask::PGResult shouldEnableComplexMaterial(const std::filesystem::path& nif_file, nifly::NifFile& nif, const uint32_t shape_block_id, nifly::NiShape* shape, nifly::NiShader* shader, const std::array<std::string, 9>& search_prefixes, bool& enable_result, std::string& matched_path);
	// check if vanilla parallax should be enabled on shape
	static inline const int parallax_slot_search[2] = { 0, 1 };
	ParallaxGenTask::PGResult shouldEnableParallax(const std::filesystem::path& nif_file, nifly::NifFile& nif, const uint32_t shape_block_id, nifly::NiShape* shape, nifly::NiShader* shader, const std::array<std::string, 9>& search_prefixes, const bool has_attached_havok, bool& enable_result, std::string& matched_path);
	// applies truepbr config on a shape in a NIF
	ParallaxGenTask::PGResult applyTruePBRConfigOnShape(nifly::NifFile& nif, nifly::NiShape* shape, nifly::NiShader* shader, nlohmann::json& truepbr_data, const std::string& matched_path, bool& nif_modified);
	// enables truepbr on a shape in a NIF
	ParallaxGenTask::PGResult enableTruePBROnShape(nifly::NifFile& nif, nifly::NiShape* shape, nifly::NiShader* shader, nlohmann::json& truepbr_data, const std::string& matched_path, bool& nif_modified);
	// enables complex material on a shape in a NIF
	ParallaxGenTask::PGResult enableComplexMaterialOnShape(nifly::NifFile& nif, nifly::NiShape* shape, nifly::NiShader* shader, const std::string& matched_path, bool dynCubemaps, bool& nif_modified);
	// enables parallax on a shape in a NIF
	ParallaxGenTask::PGResult enableParallaxOnShape(nifly::NifFile& nif, nifly::NiShape* shape, nifly::NiShader* shader, const std::string& matched_path, bool& nif_modified);

	static const std::array<std::string, 9> getSearchPrefixes(nifly::NifFile& nif, nifly::NiShape* shape);

	// Helpers
	static nifly::Vector2 abs2(nifly::Vector2 v);
	static nifly::Vector2 auto_uv_scale(const std::vector<nifly::Vector2>* uvs, const std::vector<nifly::Vector3>* verts, std::vector<nifly::Triangle>& tris);
	static bool flag(nlohmann::json& json, const char* key);

	// zip methods
	void addFileToZip(mz_zip_archive& zip, const std::filesystem::path& filePath, const std::filesystem::path& zipPath);
	void zipDirectory(const std::filesystem::path& dirPath, const std::filesystem::path& zipPath);
};

#endif
