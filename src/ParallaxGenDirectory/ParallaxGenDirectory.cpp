#include "ParallaxGenDirectory/ParallaxGenDirectory.hpp"

#include <spdlog/spdlog.h>
#include <DirectXTex.h>
#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/asio.hpp>
#include <thread>
#include <chrono>
#include <cmath>

#include "ParallaxGenUtil/ParallaxGenUtil.hpp"

using namespace std;
using namespace nifly;
namespace fs = filesystem;

ParallaxGenDirectory::ParallaxGenDirectory(BethesdaGame bg) : BethesdaDirectory(bg, true)
{
	// contructor
	findHeightMaps();
	//findComplexMaterialMaps();
	findMeshes();

	mapTexturesToMeshes(true);
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

void ParallaxGenDirectory::findMeshes()
{
	// find meshes
	spdlog::info("Finding meshes");
	meshes = BethesdaDirectory::findFilesBySuffix(".nif", mesh_blocklist);
	spdlog::info("Found {} meshes", meshes.size());
}

void ParallaxGenDirectory::monitorThread(size_t num_workers)
{
	// monitor thread
	size_t i = 0;
	while (completed_tasks < num_workers) {
		if (i % 100 == 0) {
			spdlog::info("Completed tasks: {}/{}", completed_tasks, meshes.size());
		}

		this_thread::sleep_for(chrono::seconds(1));
	}
}

void ParallaxGenDirectory::mapTexturesToMeshes(bool threaded = false)
{
	// randomize input
	ParallaxGenUtil::shuffleVector(meshes);

	//loop through each mesh nif file
	unsigned int max_threads = thread::hardware_concurrency();
	if (max_threads < 1) {
		max_threads = 1;
	}
	max_threads = 1;  // DEBUG TEMP

	boost::asio::thread_pool pool(max_threads);

	// setup monitor thread
	completed_tasks = 0;
	size_t max_tasks = meshes.size();
	thread monitorThread([this, max_tasks]() { monitorThread(max_tasks); });

	// get thread intervals
	size_t nif_batch_size = ceil(max_tasks / max_threads);

	for (size_t i = 0; i < max_tasks; i += nif_batch_size) {
		size_t b = i + nif_batch_size - 1;
		if (b >= max_tasks) {
			b = max_tasks - 1;
		}

		vector<fs::path> subvec = ParallaxGenUtil::getSubVector(meshes, i, b);

		boost::asio::post(pool, [this, subvec]() {
			processNIFBatch(subvec);
		});
	}

	pool.join();
	monitorThread.join();
}

void ParallaxGenDirectory::processNIFBatch(vector<fs::path> nif_files)
{
	// process nif files
	for (fs::path nif_file : nif_files) {
		processNIF(nif_file);
		completed_tasks++;
	}
}

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

	// loop through each node in nif
	for (NiShape* shape : nif.GetShapes()) {
		// exclusions
		// get shader type
		if (!shape->HasShaderProperty()) {
			continue;
		}

		NiShader* shader = nif.GetShader(shape);

		// Ignore if shader type is not 0 (nothing) or 1 (environemnt map) or 3 (parallax)
		BSLSP shader_type = static_cast<BSLSP>(shader->GetShaderType());
		if (shader_type != BSLSP::BSLSP_DEFAULT && shader_type != BSLSP::BSLSP_ENVMAP && shader_type != BSLSP::BSLSP_PARALLAX) {
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
			search_path = search_prefix + "_m.dds";
			if (find(complexMaterialMaps.begin(), complexMaterialMaps.end(), search_path) != complexMaterialMaps.end()) {
				// Enable complex parallax for this shape!
				nif_modified |= enableComplexMaterialOnShape(nif, shape, shader, search_prefix);
				break;
			}

			search_path = search_prefix + "_p.dds";
			if (find(heightMaps.begin(), heightMaps.end(), search_path) != heightMaps.end()) {
				// Enable regular parallax for this shape!
				nif_modified |= enableParallaxOnShape(nif, shape, shader, search_prefix);
				break;
			}
		}
	}

	if (nif_modified) {
		spdlog::info("NIF Modified: {}", nif_file.string());
		if (nif.Save(data_dir / nif_file, nif_save_options) != 0) {
			spdlog::error("Unable to save NIF file: {}", nif_file.string());
		}
	}
}

bool ParallaxGenDirectory::enableComplexMaterialOnShape(NifFile& nif, NiShape* shape, NiShader* shader, string& search_prefix)
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

bool ParallaxGenDirectory::enableParallaxOnShape(NifFile& nif, NiShape* shape, NiShader* shader, string& search_prefix)
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