#ifndef PARALLAXGENDIRECTORYITERATOR_H
#define PARALLAXGENDIRECTORYITERATOR_H

#include <vector>
#include <filesystem>
#include <NifFile.hpp>
#include <array>

#include "BethesdaDirectory/BethesdaDirectory.hpp"

class ParallaxGenDirectory : public BethesdaDirectory {
private:
	std::vector<std::filesystem::path> heightMaps;
	std::vector<std::filesystem::path> complexMaterialMaps;
	std::vector<std::filesystem::path> meshes;

	static inline const std::array<uint32_t, 2> texture_maps_idx_search = { 0, 1 };

	size_t completed_tasks;

	nifly::NifSaveOptions nif_save_options = { false, false };

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
	ParallaxGenDirectory(BethesdaGame bg);

private:
	void monitorThread(size_t num_workers);

	void findHeightMaps();
	void findComplexMaterialMaps();
	void findMeshes();
	void mapTexturesToMeshes(bool threaded);
	void processNIFBatch(std::vector<std::filesystem::path> nif_files);
	void processNIF(std::filesystem::path nif_file);
	bool enableComplexMaterialOnShape(nifly::NifFile& nif, nifly::NiShape* shape, nifly::NiShader* shader, std::string& search_prefix);
	bool enableParallaxOnShape(nifly::NifFile& nif, nifly::NiShape* shape, nifly::NiShader* shader, std::string& search_prefix);
};

#endif