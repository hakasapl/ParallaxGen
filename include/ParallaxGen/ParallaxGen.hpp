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
	const nifly::NifSaveOptions nif_save_options = { false, true };

    // which texture indices to search when mapping mesh shapes to parallax maps
	static inline const std::array<uint32_t, 2> texture_maps_idx_search = { 0, 1 };

    // bools to ignore parallax and/or complex material
	bool ignore_parallax;
	bool ignore_complex_material;

	size_t num_completed;

	// Hash function for std::tuple
	struct TupleHash {
		template <typename T1, typename T2>
		std::size_t operator()(const std::tuple<T1, T2>& t) const {
			auto hash1 = std::hash<T1>{}(std::get<0>(t));
			auto hash2 = std::hash<T2>{}(std::get<1>(t));
			return hash1 ^ (hash2 << 1); // Combining the hash values
		}
	};

	// store already checked height map checks
	std::unordered_map<std::tuple<std::filesystem::path, std::filesystem::path>, double, TupleHash> height_map_checks;

public:
	static inline const std::string parallax_state_file = "PARALLAXGEN_DONTDELETE";

    // constructor
    ParallaxGen(const std::filesystem::path output_dir, ParallaxGenDirectory* pgd, ParallaxGenD3D* pgd3d);
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
