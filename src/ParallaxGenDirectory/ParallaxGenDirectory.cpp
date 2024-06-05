#include "ParallaxGenDirectory/ParallaxGenDirectory.hpp"

#include <spdlog/spdlog.h>
#include <DirectXTex.h>
#include <boost/algorithm/string.hpp>

#include "ParallaxGenUtil/ParallaxGenUtil.hpp"

using namespace std;
namespace fs = filesystem;

ParallaxGenDirectory::ParallaxGenDirectory(BethesdaGame bg) : BethesdaDirectory(bg, true) {}

vector<fs::path> ParallaxGenDirectory::findHeightMaps() const
{
	// find height maps
	spdlog::info("Finding parallax height maps");
	vector<fs::path> heightMaps = findFilesBySuffix("_p.dds", true);
	spdlog::info("Found {} height maps", heightMaps.size());
	return heightMaps;
}

vector<fs::path> ParallaxGenDirectory::findComplexMaterialMaps() const
{
	spdlog::info("Finding complex material maps");

	// initialize output vector
	vector<fs::path> complexMaterialMaps;

	// find complex material maps
	vector<fs::path> env_maps = findFilesBySuffix("_m.dds", true);

	// loop through env maps
	for (fs::path env_map : env_maps) {
		// check if env map is actually a complex material map
		const vector<std::byte> env_map_data = getFile(env_map);

		// load image into directxtex
		DirectX::ScratchImage image;
		HRESULT hr = DirectX::LoadFromDDSMemory(env_map_data.data(), env_map_data.size(), DirectX::DDS_FLAGS_NONE, nullptr, image);
		if (FAILED(hr)) {
			spdlog::warn(L"Failed to load DDS from memory: {} - skipping", env_map.wstring());
			continue;
		}

		// check if image is a complex material map
		if (!image.IsAlphaAllOpaque()) {
			// if alpha channel is used, there is parallax data
			// this won't work on complex matterial maps that don't make use of complex parallax
			// I'm not sure there's a way to check for those other cases
			spdlog::trace(L"Adding {} as a complex material map", env_map.wstring());
			complexMaterialMaps.push_back(env_map);
		}
	}

	spdlog::info("Found {} complex material maps", complexMaterialMaps.size());

	return complexMaterialMaps;
}

vector<fs::path> ParallaxGenDirectory::findMeshes() const
{
	// find meshes
	spdlog::info("Finding meshes");
	vector<fs::path> meshes = findFilesBySuffix(".nif", true, mesh_blocklist);
	spdlog::info("Found {} meshes", meshes.size());
	return meshes;
}
