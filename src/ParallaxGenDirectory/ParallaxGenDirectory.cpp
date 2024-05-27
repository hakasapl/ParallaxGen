#include "ParallaxGenDirectory/ParallaxGenDirectory.hpp"

#include <spdlog/spdlog.h>
#include <DirectXTex.h>
#include <NifFile.hpp>

using namespace std;
namespace fs = filesystem;

ParallaxGenDirectory::ParallaxGenDirectory(BethesdaGame bg) : BethesdaDirectory(bg, true) {
	// contructor
	findHeightMaps();
	findComplexMaterialMaps();
	findMeshes();
}

void ParallaxGenDirectory::findHeightMaps() {
	// find height maps
	spdlog::info("Finding parallax height maps");
	heightMaps = BethesdaDirectory::findFilesBySuffix("_p.dds");
	spdlog::info("Found {} height maps", heightMaps.size());
}

void ParallaxGenDirectory::findComplexMaterialMaps() {
	spdlog::info("Finding complex material maps");

	// find complex material maps
	vector<fs::path> env_maps = BethesdaDirectory::findFilesBySuffix("_m.dds");

	// loop through env maps
	for (fs::path env_map : env_maps) {
		spdlog::debug("Checking {}", env_map.string());

		// check if env map is actually a complex material map
		const vector<std::byte> env_map_data = BethesdaDirectory::getFile(env_map);

		// load image into directxtex
		DirectX::ScratchImage image;
		HRESULT hr = DirectX::LoadFromDDSMemory(env_map_data.data(), env_map_data.size(), DirectX::DDS_FLAGS_NONE, nullptr, image);
		if (FAILED(hr)) {
			throw runtime_error("Failed to load DDS from memory");
		}

		// check if image is a complex material map
		if (!image.IsAlphaAllOpaque()) {
			// if alpha channel is used, there is parallax data
			// this won't work on complex matterial maps that don't make use of complex parallax
			// I'm not sure there's a way to check for those other cases
			complexMaterialMaps.push_back(env_map);
		}
	}

	spdlog::info("Found {} complex material maps", complexMaterialMaps.size());
}

void ParallaxGenDirectory::findMeshes() {
	// find meshes
	spdlog::info("Finding meshes");
	meshes = BethesdaDirectory::findFilesBySuffix(".nif");
	spdlog::info("Found {} meshes", meshes.size());
}
