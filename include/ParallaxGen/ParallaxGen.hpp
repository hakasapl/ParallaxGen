#ifndef PARALLAXGEN_H
#define PARALLAXGEN_H

#include <filesystem>
#include <array>
#include <NifFile.hpp>
#include <miniz.h>
#include "ParallaxGenDirectory/ParallaxGenDirectory.hpp"

class ParallaxGen
{
private:
    std::filesystem::path output_dir;
    ParallaxGenDirectory* pgd;

    // we don't want any other change on the NIF file so we disable all save options from nifly
	const nifly::NifSaveOptions nif_save_options = { false, true };

    // which texture indices to search when mapping mesh shapes to parallax maps
	static inline const std::array<uint32_t, 2> texture_maps_idx_search = { 0, 1 };

    // bools to ignore parallax and/or complex material
	bool ignore_parallax;
	bool ignore_complex_material;

public:
	static inline const std::string parallax_state_file = "PARALLAXGEN_DONTDELETE";

    // constructor
    ParallaxGen(const std::filesystem::path output_dir, ParallaxGenDirectory* pgd);
    // enables parallax on relevant meshes
	void patchMeshes(std::vector<std::filesystem::path>& meshes, std::vector<std::filesystem::path>& heightMaps, std::vector<std::filesystem::path>& complexMaterialMaps);
	// zips all meshes and removes originals
	void zipMeshes();
	// deletes generated meshes
	void deleteMeshes();
	// deletes entire output folder
	void deleteOutputDir();

private:
    // processes a NIF file (enable parallax if needed)
	void processNIF(const std::filesystem::path& nif_file, std::vector<std::filesystem::path>& heightMaps, std::vector<std::filesystem::path>& complexMaterialMaps);
	// enables complex material on a shape in a NIF
	bool enableComplexMaterialOnShape(nifly::NifFile& nif, nifly::NiShape* shape, nifly::NiShader* shader, const std::string& search_prefix);
	// enables parallax on a shape in a NIF
	bool enableParallaxOnShape(nifly::NifFile& nif, nifly::NiShape* shape, nifly::NiShader* shader, const std::string& search_prefix);

	// zip methods
	void addFileToZip(mz_zip_archive& zip, const std::filesystem::path& filePath, const std::filesystem::path& zipPath);
	void zipDirectory(const std::filesystem::path& dirPath, const std::filesystem::path& zipPath);
};

#endif
