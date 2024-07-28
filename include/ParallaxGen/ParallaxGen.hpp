#pragma once

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
    std::filesystem::path output_dir;  // ParallaxGen output directory

	// Dependency objects
    ParallaxGenDirectory* pgd;
	ParallaxGenD3D* pgd3d;

    // sort blocks enabled, optimize disabled (for now)
	nifly::NifSaveOptions nif_save_options = { false, true };

    // which texture indices to search when mapping mesh shapes to parallax maps
	static inline const std::array<uint32_t, 2> texture_maps_idx_search = { 0, 1 };

	// If nif or texture paths have these components dynamic cubemaps will be ignored for CM patching
	// TODO make this configurable
	static inline const std::vector<std::wstring> dynCubemap_ignore_list = { L"armor", L"weapons" };

    // store CLI arguments
	bool ignore_parallax;
	bool ignore_complex_material;
	bool ignore_truepbr;

	// store the suffixes for each texture slot. First element of each vector is used when creating maps
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

	//
	// The following methods are called from main.cpp and are public facing
	//

    // constructor
    ParallaxGen(
		const std::filesystem::path output_dir,
		ParallaxGenDirectory* pgd,
		ParallaxGenD3D* pgd3d,
		const bool optimize_meshes = false,
		const bool ignore_parallax = false,
		const bool ignore_complex_material = false,
		const bool ignore_truepbr = false);
	// upgrades textures whenever possible
	void upgradeShaders() const;
    // enables parallax on relevant meshes
	void patchMeshes() const;
	// zips all meshes and removes originals
	void zipMeshes() const;
	// deletes generated meshes
	void deleteMeshes() const;
	// deletes entire output folder
	void deleteOutputDir() const;
	// init output folder
	void initOutputDir() const;

private:
	//
	// The following methods are private and are called from the public facing methods
	//

	// upgrades a height map to complex material
	ParallaxGenTask::PGResult convertHeightMapToComplexMaterial(const std::filesystem::path& height_map) const;
    // processes a NIF file (enable parallax if needed)
	ParallaxGenTask::PGResult processNIF(const std::filesystem::path& nif_file) const;
	// processes a shape within a NIF file
	ParallaxGenTask::PGResult processShape(
		const std::filesystem::path& nif_file,
		nifly::NifFile& nif,
		const uint32_t shape_block_id,
		nifly::NiShape* shape,
		bool& nif_modified,
		const bool has_attached_havok
	) const;
	// check if truepbr should be enabled on shape
	ParallaxGenTask::PGResult shouldApplyTruePBRConfig(
		const std::filesystem::path& nif_file,
		nifly::NifFile& nif,
		const uint32_t shape_block_id,
		nifly::NiShape* shape,
		nifly::NiShader* shader,
		const std::array<std::string, 9>& search_prefixes,
		bool& enable_result,
		nlohmann::json& truepbr_data,
		std::string& matched_path
	) const;
	// check if complex material should be enabled on shape
	static inline const int cm_slot_search[2] = { 0, 1 };  // Checks match for diffuse first, then normal
	ParallaxGenTask::PGResult shouldEnableComplexMaterial(
		const std::filesystem::path& nif_file,
		nifly::NifFile& nif,
		const uint32_t shape_block_id,
		nifly::NiShape* shape,
		nifly::NiShader* shader,
		const std::array<std::string, 9>& search_prefixes,
		bool& enable_result,
		std::string& matched_path
	) const;
	// check if vanilla parallax should be enabled on shape
	static inline const int parallax_slot_search[2] = { 0, 1 };  // Checks match for diffuse first, then normal
	ParallaxGenTask::PGResult shouldEnableParallax(
		const std::filesystem::path& nif_file,
		nifly::NifFile& nif,
		const uint32_t shape_block_id,
		nifly::NiShape* shape,
		nifly::NiShader* shader,
		const std::array<std::string, 9>& search_prefixes,
		const bool has_attached_havok,
		bool& enable_result,
		std::string& matched_path
	) const;
	// applies truepbr config on a shape in a NIF (always applies with config, but maybe PBR is disabled)
	ParallaxGenTask::PGResult applyTruePBRConfigOnShape(
		nifly::NifFile& nif,
		nifly::NiShape* shape,
		nifly::NiShader* shader,
		nlohmann::json& truepbr_data,
		const std::string& matched_path,
		bool& nif_modified
	) const;
	// enables truepbr on a shape in a NIF (If PBR is enabled)
	ParallaxGenTask::PGResult enableTruePBROnShape(
		nifly::NifFile& nif,
		nifly::NiShape* shape,
		nifly::NiShader* shader,
		nlohmann::json& truepbr_data,
		const std::string& matched_path,
		bool& nif_modified
	) const;
	// enables complex material on a shape in a NIF
	ParallaxGenTask::PGResult enableComplexMaterialOnShape(
		nifly::NifFile& nif,
		nifly::NiShape* shape,
		nifly::NiShader* shader,
		const std::string& matched_path,
		bool dynCubemaps,
		bool& nif_modified
	) const;
	// enables parallax on a shape in a NIF
	ParallaxGenTask::PGResult enableParallaxOnShape(
		nifly::NifFile& nif,
		nifly::NiShape* shape,
		nifly::NiShader* shader,
		const std::string& matched_path,
		bool& nif_modified
	) const;

	// Gets all the texture prefixes for a textureset. ie. _n.dds is removed etc. for each slot
	static const std::array<std::string, 9> getSearchPrefixes(nifly::NifFile& nif, nifly::NiShape* shape);

	// Zip methods
	void addFileToZip(
		mz_zip_archive& zip,
		const std::filesystem::path& filePath,
		const std::filesystem::path& zipPath
	) const;
	void zipDirectory(
		const std::filesystem::path& dirPath,
		const std::filesystem::path& zipPath
	) const;

	// TruePBR Helpers
	// Calculates 2d absolute value of a vector2
	static nifly::Vector2 abs2(nifly::Vector2 v);
	// Auto UV scale magic that I don't understand
	static nifly::Vector2 auto_uv_scale(const std::vector<nifly::Vector2>* uvs, const std::vector<nifly::Vector3>* verts, std::vector<nifly::Triangle>& tris);
	// Checks if a json object has a key
	static bool flag(nlohmann::json& json, const char* key);
};
