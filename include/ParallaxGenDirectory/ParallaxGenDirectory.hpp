#ifndef PARALLAXGENDIRECTORYITERATOR_H
#define PARALLAXGENDIRECTORYITERATOR_H

#include <vector>
#include <filesystem>
#include <NifFile.hpp>
#include <array>

#include "BethesdaDirectory/BethesdaDirectory.hpp"

class ParallaxGenDirectory : public BethesdaDirectory {
private:
	// stores the heightmap files in the data directory ("_p.dds")
	std::vector<std::filesystem::path> heightMaps;
	// stores the complex material maps in the data directory ("_m.dds" that have alpha channel)
	std::vector<std::filesystem::path> complexMaterialMaps;
	// stores the meshes in the data directory (.nif)
	std::vector<std::filesystem::path> meshes;

	// bools to ignore parallax and/or complex material
	bool ignore_parallax;
	bool ignore_complex_material;

	// which texture indices to search when mapping mesh shapes to parallax maps
	static inline const std::array<uint32_t, 2> texture_maps_idx_search = { 0, 1 };

    // we don't want any other change on the NIF file so we disable all save options from nifly
	const nifly::NifSaveOptions nif_save_options = { false, false };

	// list of mesh block names to ignore (if the path has any of these as a component it is ignored)
	static inline const std::vector<std::string> mesh_blocklist = {
		"actors",
		"cameras",
		"decals",
		"effects",
		"interface",
		"lod",
		"magic",
		"markers",
		"mps",
		"sky",
		"water"
	};

public:
	// constructor - calls the BethesdaDirectory constructor
	ParallaxGenDirectory(BethesdaGame bg, const bool ignore_parallax, const bool ignore_complex_material);
	// searches for the relevant files required for patching meshes
	// this must be run before patchMeshes()
	void buildFileVectors();
	// enables parallax on relevant meshes
	void patchMeshes();

private:
	// searches for height maps in the data directory
	void findHeightMaps();
	// searches for complex material maps in the data directory
	void findComplexMaterialMaps();
	// searches for meshes in the data directory
	void findMeshes();

	// processes a NIF file (enable parallax if needed)
	void processNIF(std::filesystem::path nif_file);
	// enables complex material on a shape in a NIF
	bool enableComplexMaterialOnShape(nifly::NifFile& nif, nifly::NiShape* shape, nifly::NiShader* shader, const std::string& search_prefix);
	// enables parallax on a shape in a NIF
	bool enableParallaxOnShape(nifly::NifFile& nif, nifly::NiShape* shape, nifly::NiShader* shader, const std::string& search_prefix);
};

#endif
