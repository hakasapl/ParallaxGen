#ifndef PARALLAXGEN_H
#define PARALLAXGEN_H

#include <filesystem>
#include <array>
#include <NifFile.hpp>
#include <miniz.h>
#include <unordered_map>
#include <tuple>

#include "ParallaxGenDirectory/ParallaxGenDirectory.hpp"
#include "ParallaxGenD3D/ParallaxGenD3D.hpp"

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

public:
	static inline const std::string parallax_state_file = "PARALLAXGEN_DONTDELETE";

    // constructor
    ParallaxGen(const std::filesystem::path output_dir, ParallaxGenDirectory* pgd, ParallaxGenD3D* pgd3d, bool optimize_meshes = false, bool ignore_parallax = false, bool ignore_complex_material = false);
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

private:
    // processes a NIF file (enable parallax if needed)
	bool processNIF(const std::filesystem::path& nif_file);
	// processes a shape within a NIF file
	bool processShape(const std::filesystem::path& nif_file, nifly::NifFile& nif, const uint32_t shape_block_id, nifly::NiShape* shape, bool& nif_modified, const bool has_attached_havok);
	// check if complex material should be enabled on shape
	bool shouldEnableComplexMaterial(const std::filesystem::path& nif_file, nifly::NifFile& nif, const uint32_t shape_block_id, nifly::NiShape* shape, nifly::NiShader* shader, const std::string& search_prefix);
	// check if vanilla parallax should be enabled on shape
	bool shouldEnableParallax(const std::filesystem::path& nif_file, nifly::NifFile& nif, const uint32_t shape_block_id, nifly::NiShape* shape, nifly::NiShader* shader, const std::string& search_prefix, const bool has_attached_havok);
	// enables complex material on a shape in a NIF
	bool enableComplexMaterialOnShape(nifly::NifFile& nif, nifly::NiShape* shape, nifly::NiShader* shader, const std::string& search_prefix, bool dynCubemaps, bool& nif_modified);
	// enables parallax on a shape in a NIF
	bool enableParallaxOnShape(nifly::NifFile& nif, nifly::NiShape* shape, nifly::NiShader* shader, const std::string& search_prefix, bool& nif_modified);

	static std::vector<std::string> getSearchPrefixes(nifly::NifFile& nif, nifly::NiShape* shape);

	// zip methods
	void addFileToZip(mz_zip_archive& zip, const std::filesystem::path& filePath, const std::filesystem::path& zipPath);
	void zipDirectory(const std::filesystem::path& dirPath, const std::filesystem::path& zipPath);
};

#endif
