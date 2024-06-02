#include "ParallaxGenDirectory/ParallaxGenDirectory.hpp"

#include <spdlog/spdlog.h>
#include <DirectXTex.h>
#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/asio.hpp>
#include <boost/algorithm/string.hpp>
#include <thread>
#include <chrono>
#include <cmath>

#include "ParallaxGenUtil/ParallaxGenUtil.hpp"

using namespace std;
using namespace nifly;
namespace fs = filesystem;

ParallaxGenDirectory::ParallaxGenDirectory(BethesdaGame bg, const bool ignore_parallax, const bool ignore_complex_material) : BethesdaDirectory(bg, true) {
	this->ignore_parallax = ignore_parallax;
	this->ignore_complex_material = ignore_complex_material;
}

void ParallaxGenDirectory::buildFileVectors()
{
	// build file vectors
	findHeightMaps();
	findComplexMaterialMaps();
	findMeshes();
}

void ParallaxGenDirectory::patchMeshes()
{
	// patch meshes
	// loop through each mesh nif file
	size_t finished_task = 0;
	size_t num_meshes = meshes.size();
	for (fs::path mesh : meshes) {
		if (finished_task % 100 == 0) {
			spdlog::info("Completed tasks: {}/{}", finished_task, num_meshes);
		}

		processNIF(mesh);
		finished_task++;
	}
}

void ParallaxGenDirectory::findHeightMaps()
{
	// find height maps
	spdlog::info("Finding parallax height maps");
	heightMaps = BethesdaDirectory::findFilesBySuffix("_p.dds");
	spdlog::info("Found {} height maps", heightMaps.size());
}

void ParallaxGenDirectory::findComplexMaterialMaps()
{
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
			spdlog::warn("Failed to load DDS from memory: {} - skipping", env_map.string());
			continue;
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

void ParallaxGenDirectory::findMeshes()
{
	// find meshes
	spdlog::info("Finding meshes");
	meshes = BethesdaDirectory::findFilesBySuffix(".nif", mesh_blocklist);
	spdlog::info("Found {} meshes", meshes.size());
}

// shorten some enum names
typedef BSLightingShaderPropertyShaderType BSLSP;
typedef SkyrimShaderPropertyFlags1 SSPF1;
typedef SkyrimShaderPropertyFlags2 SSPF2;
void ParallaxGenDirectory::processNIF(fs::path nif_file)
{
	// process nif file
	vector<std::byte> nif_file_data = BethesdaDirectory::getFile(nif_file);

	boost::iostreams::array_source nif_array_source(reinterpret_cast<const char*>(nif_file_data.data()), nif_file_data.size());
	boost::iostreams::stream<boost::iostreams::array_source> nif_stream(nif_array_source);

	// load nif file
	NifFile nif(nif_stream);
	bool nif_modified = false;

	// ignore nif if has attached havok animations
	//todo: I don't think this is right
	for (NiNode* node : nif.GetNodes()) {
		string block_name = node->GetBlockName();
		if (block_name == "BSBehaviorGraphExtraData") {
			spdlog::debug("Rejecting NIF file {} due to attached havok animations", nif_file.string());
			return;
		}
	}

	// loop through each node in nif
	size_t shape_id = 0;
	for (NiShape* shape : nif.GetShapes()) {
		// exclusions
		// get shader type
		if (!shape->HasShaderProperty()) {
			//spdlog::debug("Rejecting shape {} in NIF file {}: No shader property", shape_id, nif_file.string());
			continue;
		}

		// only allow BSLightingShaderProperty blocks
		string shape_block_name = shape->GetBlockName();
		if (shape_block_name != "NiTriShape" && shape_block_name != "BSTriShape") {
			//spdlog::debug("Rejecting shape {} in NIF file {}: Incorrect shape block type", shape_id, nif_file.string());
			continue;
		}

		// ignore skinned meshes, these don't support parallax
		if (shape->HasSkinInstance() || shape->IsSkinned()) {
			//spdlog::debug("Rejecting shape {} in NIF file {}: Skinned mesh", shape_id, nif_file.string());
			continue;
		}

		// get shader from shape
		NiShader* shader = nif.GetShader(shape);

		string shader_block_name = shader->GetBlockName();
		if (shader_block_name != "BSLightingShaderProperty") {
			//spdlog::debug("Rejecting shape {} in NIF file {}: Incorrect shader block type", shape->GetBlockName(), nif_file.string());
			continue;
		}

		// Ignore if shader type is not 0 (nothing) or 1 (environemnt map) or 3 (parallax)
		BSLSP shader_type = static_cast<BSLSP>(shader->GetShaderType());
		if (shader_type != BSLSP::BSLSP_DEFAULT && shader_type != BSLSP::BSLSP_ENVMAP && shader_type != BSLSP::BSLSP_PARALLAX) {
			//spdlog::debug("Rejecting shape {} in NIF file {}: Incorrect shader type", shape->GetBlockName(), nif_file.string());
			continue;
		}

		// build search vector
		vector<string> search_prefixes;
		// diffuse map lookup first
		string diffuse_map;
		uint32_t diffuse_result = nif.GetTextureSlot(shape, diffuse_map, 0);
		if (diffuse_result == 0) {
			continue;
		}
		ParallaxGenUtil::addUniqueElement(search_prefixes, diffuse_map.substr(0, diffuse_map.find_last_of('.')));
		// normal map lookup
		string normal_map;
		uint32_t normal_result = nif.GetTextureSlot(shape, normal_map, 1);
		if (diffuse_result > 0) {
			ParallaxGenUtil::addUniqueElement(search_prefixes, normal_map.substr(0, normal_map.find_last_of('_')));
		}

		// check if complex parallax should be enabled
		for (string& search_prefix : search_prefixes) {
			// check if complex material file exists
			fs::path search_path;

			// processing for complex material
			if (!ignore_complex_material) {
				search_path = search_prefix + "_m.dds";
				if (find(complexMaterialMaps.begin(), complexMaterialMaps.end(), search_path) != complexMaterialMaps.end()) {
					// Enable complex parallax for this shape!
					nif_modified |= enableComplexMaterialOnShape(nif, shape, shader, search_prefix);
					break;
				}
			}

			// processing for parallax
			if (!ignore_parallax) {
				search_path = search_prefix + "_p.dds";
				if (find(heightMaps.begin(), heightMaps.end(), search_path) != heightMaps.end()) {
					// Enable regular parallax for this shape!
					nif_modified |= enableParallaxOnShape(nif, shape, shader, search_prefix);
					break;
				}
			}
		}

		shape_id++;
	}

	// save NIF if it was modified
	if (nif_modified) {
		spdlog::info("NIF Modified: {}", nif_file.string());
		if (nif.Save("ParallaxGen_Output" / nif_file, nif_save_options)) {
			spdlog::error("Unable to save NIF file: {}", nif_file.string());
		}
	}
}

bool ParallaxGenDirectory::enableComplexMaterialOnShape(NifFile& nif, NiShape* shape, NiShader* shader, const string& search_prefix)
{
	// enable complex material on shape
	bool changed = false;
	// 1. set shader type to env map
	if (shader->GetShaderType() != BSLSP::BSLSP_ENVMAP) {
		shader->SetShaderType(BSLSP::BSLSP_ENVMAP);
		changed = true;
	}
	// 2. set shader flags
	BSLightingShaderProperty* cur_bslsp = dynamic_cast<BSLightingShaderProperty*>(shader);
	if (!(cur_bslsp->shaderFlags1 & SSPF1::SLSF1_ENVIRONMENT_MAPPING)) {
		cur_bslsp->shaderFlags1 |= SSPF1::SLSF1_ENVIRONMENT_MAPPING;
		changed = true;
	}
	// 3. set vertex colors for shape
	if (!shape->HasVertexColors()) {
		shape->SetVertexColors(true);
		changed = true;
	}
	// 4. set vertex colors for shader
	if (!shader->HasVertexColors()) {
		shader->SetVertexColors(true);
		changed = true;
	}
	// 5. set complex material texture
	string env_map;
	uint32_t env_result = nif.GetTextureSlot(shape, env_map, 5);
	if (env_result == 0 || env_map.empty()) {
		// add height map
		string new_env_map = search_prefix + "_m.dds";
		nif.SetTextureSlot(shape, new_env_map, 5);
		changed = true;
	}
	return changed;
}

bool ParallaxGenDirectory::enableParallaxOnShape(NifFile& nif, NiShape* shape, NiShader* shader, const string& search_prefix)
{
	// enable parallax on shape
	bool changed = false;
	// 1. set shader type to parallax
	if (shader->GetShaderType() != BSLSP::BSLSP_PARALLAX) {
		shader->SetShaderType(BSLSP::BSLSP_PARALLAX);
		changed = true;
	}
	// 2. Set shader flags
	BSLightingShaderProperty* cur_bslsp = dynamic_cast<BSLightingShaderProperty*>(shader);
	if (!(cur_bslsp->shaderFlags1 & SSPF1::SLSF1_PARALLAX)) {
		cur_bslsp->shaderFlags1 |= SSPF1::SLSF1_PARALLAX;
		changed = true;
	}
	// 3. set vertex colors for shape
	if (!shape->HasVertexColors()) {
		shape->SetVertexColors(true);
		changed = true;
	}
	// 4. set vertex colors for shader
	if (!shader->HasVertexColors()) {
		shader->SetVertexColors(true);
		changed = true;
	}
	// 5. set parallax heightmap texture
	string height_map;
	uint32_t height_result = nif.GetTextureSlot(shape, height_map, 3);
	if (height_result == 0 || height_map.empty()) {
		// add height map
		string new_height_map = search_prefix + "_p.dds";
		nif.SetTextureSlot(shape, new_height_map, 3);
		changed = true;
	}

	return changed;
}
