#include "ParallaxGenDirectory/ParallaxGenDirectory.hpp"

#include <spdlog/spdlog.h>
#include <DirectXTex.h>
#include <boost/algorithm/string.hpp>

#include "ParallaxGenUtil/ParallaxGenUtil.hpp"

using namespace std;
using namespace ParallaxGenUtil;

ParallaxGenDirectory::ParallaxGenDirectory(BethesdaGame bg) : BethesdaDirectory(bg, true) {}

void ParallaxGenDirectory::findHeightMaps()
{
	// find height maps
	spdlog::info("Finding parallax height maps");
	heightMaps = findFilesBySuffix("_p.dds", true);
	spdlog::info("Found {} height maps", heightMaps.size());
}

void ParallaxGenDirectory::findComplexMaterialMaps()
{
	// TODO GPU acceleration for this part

	spdlog::info("Finding complex material maps");

	// find complex material maps
	vector<filesystem::path> env_maps = findFilesBySuffix("_m.dds", true);

	// loop through env maps
	for (filesystem::path env_map : env_maps) {
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
}

void ParallaxGenDirectory::findMeshes()
{
	// find meshes
	spdlog::info("Finding meshes");
	meshes = findFilesBySuffix(".nif", true, mesh_blocklist);
	spdlog::info("Found {} meshes", meshes.size());
}

void ParallaxGenDirectory::addHeightMap(filesystem::path path)
{
	filesystem::path path_lower = getPathLower(path);

	// add to vector
	addUniqueElement(heightMaps, path_lower);
}

void ParallaxGenDirectory::addComplexMaterialMap(filesystem::path path)
{
	filesystem::path path_lower = getPathLower(path);

	// add to vector
	addUniqueElement(complexMaterialMaps, path_lower);
}

void ParallaxGenDirectory::addMesh(filesystem::path path)
{
	filesystem::path path_lower = getPathLower(path);

	// add to vector
	addUniqueElement(meshes, path_lower);
}

bool ParallaxGenDirectory::isHeightMap(filesystem::path path) const
{
	return find(heightMaps.begin(), heightMaps.end(), getPathLower(path)) != heightMaps.end();
}

bool ParallaxGenDirectory::isComplexMaterialMap(filesystem::path path) const
{
	return find(complexMaterialMaps.begin(), complexMaterialMaps.end(), getPathLower(path)) != complexMaterialMaps.end();
}

bool ParallaxGenDirectory::isMesh(filesystem::path path) const
{
	return find(meshes.begin(), meshes.end(), getPathLower(path)) != meshes.end();
}

bool ParallaxGenDirectory::defCubemapExists()
{
	return isFile(default_cubemap_path);
}

const vector<filesystem::path> ParallaxGenDirectory::getHeightMaps() const
{
	return heightMaps;
}

const vector<filesystem::path> ParallaxGenDirectory::getComplexMaterialMaps() const
{
	return complexMaterialMaps;
}

const vector<filesystem::path> ParallaxGenDirectory::getMeshes() const
{
	return meshes;
}

// Helpers
filesystem::path ParallaxGenDirectory::changeDDSSuffix(std::filesystem::path path, std::wstring suffix)
{
	wstring path_str = path.wstring();
	size_t pos = path_str.find_last_of('_');

    // If an underscore is found, resize the string
    if (pos != std::wstring::npos) {
        path_str = path_str.substr(0, pos);
    }

	return path_str + suffix + L".dds";
}