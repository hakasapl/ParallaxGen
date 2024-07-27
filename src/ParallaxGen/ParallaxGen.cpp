#include "ParallaxGen/ParallaxGen.hpp"

#include <spdlog/spdlog.h>
#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/algorithm/string.hpp>
#include <vector>
#include <fstream>
#include <DirectXTex.h>

#include "ParallaxGenUtil/ParallaxGenUtil.hpp"

using namespace std;
using namespace ParallaxGenUtil;
using namespace nifly;

ParallaxGen::ParallaxGen(const filesystem::path output_dir, ParallaxGenDirectory* pgd, ParallaxGenD3D* pgd3d, bool optimize_meshes, bool ignore_parallax, bool ignore_complex_material)
{
    // constructor

	// set output directory
	this->output_dir = output_dir;
    this->pgd = pgd;
	this->pgd3d = pgd3d;

	// set optimize meshes flag
	nif_save_options.optimize = optimize_meshes;

	// set ignore flags
	this->ignore_parallax = ignore_parallax;
	this->ignore_complex_material = ignore_complex_material;
}

void ParallaxGen::upgradeShaders()
{
	// Get height maps (vanilla _p.dds files)
	auto heightMaps = pgd->getHeightMaps();

	// Define task parameters
	ParallaxGenTask task_tracker("Shader Upgrades", heightMaps.size());

	for (filesystem::path height_map : heightMaps) {
		task_tracker.completeJob(convertHeightMapToComplexMaterial(height_map));
	}
}

void ParallaxGen::patchMeshes()
{
	auto meshes = pgd->getMeshes();

	// Create task tracker
	ParallaxGenTask task_tracker("Mesh Patcher", meshes.size());

	for (filesystem::path mesh : meshes) {
		task_tracker.completeJob(processNIF(mesh));
	}
}

ParallaxGenTask::PGResult ParallaxGen::convertHeightMapToComplexMaterial(const filesystem::path& height_map)
{
	spdlog::trace(L"Upgrading height map: {}", height_map.wstring());

	auto result = ParallaxGenTask::PGResult::SUCCESS;

	// Replace "_p" with "_m" in the stem
	filesystem::path env_mask = replaceLastOf(height_map, L"_p.dds", L"_m.dds");
	filesystem::path complex_map = env_mask;

	if (pgd->isComplexMaterialMap(env_mask)) {
		// load order already has a complex material, skip this one
		return result;
	}

	if (!pgd->isFile(env_mask)) {
		// no env map
		env_mask = filesystem::path();
	}

	// upgrade to complex material
	DirectX::ScratchImage new_ComplexMap = pgd3d->upgradeToComplexMaterial(height_map, env_mask);

	// save to file
	if (new_ComplexMap.GetImageCount() > 0) {
		filesystem::path output_path = output_dir / complex_map;
		filesystem::create_directories(output_path.parent_path());

		HRESULT hr = DirectX::SaveToDDSFile(new_ComplexMap.GetImages(), new_ComplexMap.GetImageCount(), new_ComplexMap.GetMetadata(), DirectX::DDS_FLAGS_NONE, output_path.c_str());
		if (FAILED(hr)) {
			spdlog::error(L"Unable to save complex material {}: {}", output_path.wstring(), convertToWstring(ParallaxGenD3D::getHRESULTErrorMessage(hr)));
			result = ParallaxGenTask::PGResult::FAILURE;
			return result;
		}

		// add newly created file to complexMaterialMaps for later processing
		pgd->addComplexMaterialMap(complex_map);

		spdlog::debug(L"Generated complex material map: {}", complex_map.wstring());
	} else {
		result = ParallaxGenTask::PGResult::FAILURE;
	}

	return result;
}

void ParallaxGen::zipMeshes() {
	// zip meshes
	spdlog::info("Zipping meshes...");
	zipDirectory(output_dir, output_dir / "ParallaxGen_Output.zip");
}

void ParallaxGen::deleteMeshes() {
	// delete meshes
	spdlog::info("Cleaning up meshes generated by ParallaxGen...");
	// Iterate through the folder
	for (const auto& entry : filesystem::directory_iterator(output_dir)) {
		if (filesystem::is_directory(entry.path())) {
			// Remove the directory and all its contents
			try {
				filesystem::remove_all(entry.path());
				spdlog::trace(L"Deleted directory {}", entry.path().wstring());
			}
			catch (const exception& e) {
				spdlog::error(L"Error deleting directory {}: {}", entry.path().wstring(), ParallaxGenUtil::convertToWstring(e.what()));
			}
		}

		// remove state file
		if (entry.path().filename().wstring() == L"PARALLAXGEN_DONTDELETE") {
			try {
				filesystem::remove(entry.path());
			}
			catch (const exception& e) {
				spdlog::error(L"Error deleting state file {}: {}", entry.path().wstring(), ParallaxGenUtil::convertToWstring(e.what()));
			}
		}
	}
}

void ParallaxGen::deleteOutputDir() {
	// delete output directory
	if (filesystem::exists(output_dir) && filesystem::is_directory(output_dir)) {
		spdlog::info("Deleting existing ParallaxGen output...");

		try {
			for (const auto& entry : filesystem::directory_iterator(output_dir)) {
                filesystem::remove_all(entry.path());
            }
		}
		catch (const exception& e) {
			spdlog::critical(L"Error deleting output directory {}: {}", output_dir.wstring(), ParallaxGenUtil::convertToWstring(e.what()));
			ParallaxGenUtil::exitWithUserInput(1);
		}
	}
}

void ParallaxGen::initOutputDir() {
	// create state file
	ofstream state_file(output_dir / parallax_state_file);
	state_file.close();
}

// shorten some enum names
typedef BSLightingShaderPropertyShaderType BSLSP;
typedef SkyrimShaderPropertyFlags1 SSPF1;
typedef SkyrimShaderPropertyFlags2 SSPF2;
ParallaxGenTask::PGResult ParallaxGen::processNIF(const filesystem::path& nif_file)
{
	auto result = ParallaxGenTask::PGResult::SUCCESS;

	// Determine output path for patched NIF
	const filesystem::path output_file = output_dir / nif_file;
	if (filesystem::exists(output_file)) {
		spdlog::error(L"Unable to process NIF file, file already exists: {}", nif_file.wstring());
		result = ParallaxGenTask::PGResult::FAILURE;
		return result;
	}

	// Get NIF Bytes
	vector<std::byte> nif_file_data = pgd->getFile(nif_file);
	if (nif_file_data.empty()) {
		spdlog::error(L"Unable to read NIF file: {}", nif_file.wstring());
		result = ParallaxGenTask::PGResult::FAILURE;
		return result;
	}

	// Convert Byte Vector to Stream
	boost::iostreams::array_source nif_array_source(reinterpret_cast<const char*>(nif_file_data.data()), nif_file_data.size());
	boost::iostreams::stream<boost::iostreams::array_source> nif_stream(nif_array_source);

	// NIF file object
	NifFile nif;

	try {
		// try block for loading nif
		// TODO if NIF is a loose file nifly should load it directly
		nif.Load(nif_stream);
	}
	catch (const exception& e) {
		spdlog::error(L"Error reading NIF file: {}, {}", nif_file.wstring(), ParallaxGenUtil::convertToWstring(e.what()));
		result = ParallaxGenTask::PGResult::FAILURE;
		return result;
	}

	if (!nif.IsValid()) {
		spdlog::error(L"Invalid NIF file (ignoring): {}", nif_file.wstring());
		result = ParallaxGenTask::PGResult::FAILURE;
		return result;
	}

	// Stores whether the NIF has been modified throughout the patching process
	bool nif_modified = false;

	//
	// GLOBAL CHECKS IN NIF
	//

	// Determine if NIF has attached havok animations
	bool has_attached_havok = false;
	vector<NiObject*> block_tree;
	nif.GetTree(block_tree);

	for (NiObject* block : block_tree) {
		if (block->GetBlockName() == "BSBehaviorGraphExtraData") {
			has_attached_havok = true;
		}
	}

	// Patch each shape in NIF
	size_t num_shapes = 0;
	bool one_shape_success = false;
	for (NiShape* shape : nif.GetShapes()) {
		num_shapes++;
		// Get shape's block ID in NIF (used for logging)
		const auto shape_block_id = nif.GetBlockID(shape);

		ParallaxGenTask::updatePGResult(result, processShape(nif_file, nif, shape_block_id, shape, nif_modified, has_attached_havok), ParallaxGenTask::PGResult::SUCCESS_WITH_WARNINGS);
		if (result == ParallaxGenTask::PGResult::SUCCESS) {
			one_shape_success = true;
		}
	}

	if (!one_shape_success && num_shapes > 0) {
		// No shapes were successfully processed
		result = ParallaxGenTask::PGResult::FAILURE;
		return result;
	}

	// Save patched NIF if it was modified
	if (nif_modified) {
		spdlog::debug(L"NIF Patched: {}", nif_file.wstring());

		// create directories if required
		filesystem::create_directories(output_file.parent_path());

		if (nif.Save(output_file, nif_save_options)) {
			spdlog::error(L"Unable to save NIF file: {}", nif_file.wstring());
			result = ParallaxGenTask::PGResult::FAILURE;
			return result;
		}
	}

	return result;
}

ParallaxGenTask::PGResult ParallaxGen::processShape(const filesystem::path& nif_file, NifFile& nif, const uint32_t shape_block_id, NiShape* shape, bool& nif_modified, const bool has_attached_havok)
{
	auto result = ParallaxGenTask::PGResult::SUCCESS;

	// Check for exclusions
	// get shader type
	if (!shape->HasShaderProperty()) {
		spdlog::trace(L"Rejecting shape {}: No shader property", shape_block_id);
		return result;
	}

	// only allow BSLightingShaderProperty blocks
	string shape_block_name = shape->GetBlockName();
	if (shape_block_name != "NiTriShape" && shape_block_name != "BSTriShape") {
		spdlog::trace(L"Rejecting shape {}: Incorrect shape block type", shape_block_id);
		return result;
	}

	// get shader from shape
	NiShader* shader = nif.GetShader(shape);
	if (shader == nullptr) {
		// skip if no shader
		spdlog::trace(L"Rejecting shape {}: No shader", shape_block_id);
		return result;
	}

	// check that shader has a texture set
	if (!shader->HasTextureSet()) {
		spdlog::trace(L"Rejecting shape {}: No texture set", shape_block_id);
		return result;
	}

	// check that shader is a BSLightingShaderProperty
	string shader_block_name = shader->GetBlockName();
	if (shader_block_name != "BSLightingShaderProperty") {
		spdlog::trace(L"Rejecting shape {}: Incorrect shader block type", shape_block_id);
		return result;
	}

	auto search_prefixes = getSearchPrefixes(nif, shape);
	if (search_prefixes.empty()) {
		spdlog::trace(L"Rejecting shape {}: No search prefixes", shape_block_id);
		return result;
	}

	// check if meshes should be changed
	for (string& search_prefix : search_prefixes) {
		// COMPLEX MATERIAL
		bool enable_cm = false;
		ParallaxGenTask::updatePGResult(result, shouldEnableComplexMaterial(nif_file, nif, shape_block_id, shape, shader, search_prefix, enable_cm), ParallaxGenTask::PGResult::SUCCESS_WITH_WARNINGS);
		if (enable_cm) {
			// Determine if dynamic cubemaps should be set
			bool dynCubemaps = !pgd->checkIfAnyComponentIs(nif_file, dynCubemap_ignore_list) && !pgd->checkIfAnyComponentIs(search_prefix, dynCubemap_ignore_list);

			// Enable complex material on shape
			ParallaxGenTask::updatePGResult(result, enableComplexMaterialOnShape(nif, shape, shader, search_prefix, dynCubemaps, nif_modified));
			return result;
		}

		// VANILLA PARALLAX
		bool enable_parallax = false;
		ParallaxGenTask::updatePGResult(result, shouldEnableParallax(nif_file, nif, shape_block_id, shape, shader, search_prefix, has_attached_havok, enable_parallax), ParallaxGenTask::PGResult::SUCCESS_WITH_WARNINGS);
		if (enable_parallax) {
			// Enable parallax on shape
			ParallaxGenTask::updatePGResult(result, enableParallaxOnShape(nif, shape, shader, search_prefix, nif_modified));
			return result;
		}
	}

	return result;
}

ParallaxGenTask::PGResult ParallaxGen::shouldEnableComplexMaterial(const filesystem::path& nif_file, NifFile& nif, const uint32_t shape_block_id, NiShape* shape, NiShader* shader, const string& search_prefix, bool& enable_result)
{
	auto result = ParallaxGenTask::PGResult::SUCCESS;
	enable_result = true;  // Start with default true

	// Check if complex material file exists
	filesystem::path cm_map = search_prefix + "_m.dds";
	if (ignore_complex_material || !pgd->isComplexMaterialMap(cm_map)) {
		enable_result = false;
		return result;
	}

	// Get shader type
	BSLSP shader_type = static_cast<BSLSP>(shader->GetShaderType());
	if (shader_type != BSLSP::BSLSP_DEFAULT && shader_type != BSLSP::BSLSP_ENVMAP && shader_type != BSLSP::BSLSP_PARALLAX) {
		spdlog::trace(L"Rejecting shape {}: Incorrect shader type", shape_block_id);
		enable_result = false;
		return result;
	}

	// verify that maps match each other
	string diffuse_map;
	uint32_t diffuse_result = nif.GetTextureSlot(shape, diffuse_map, 0);
	if (!diffuse_map.empty() && !pgd->isFile(diffuse_map)) {
		// no diffuse map
		spdlog::trace(L"Rejecting shape {}: Diffuse map missing: {}", shape_block_id, convertToWstring(diffuse_map));
		enable_result = false;
		return result;
	}

	// TODO check that diffuse map actually exists

	bool same_aspect = false;
	ParallaxGenTask::updatePGResult(result, pgd3d->checkIfAspectRatioMatches(diffuse_map, cm_map, same_aspect), ParallaxGenTask::PGResult::SUCCESS_WITH_WARNINGS);
	if (!same_aspect) {
		spdlog::trace(L"Rejecting shape {} in NIF file {}: Complex material map does not match diffuse map", shape_block_id, nif_file.wstring());
		enable_result = false;
		return result;
	}

	return result;
}

ParallaxGenTask::PGResult ParallaxGen::shouldEnableParallax(const filesystem::path& nif_file, NifFile& nif, const uint32_t shape_block_id, NiShape* shape, NiShader* shader, const string& search_prefix, const bool has_attached_havok, bool& enable_result)
{
	auto result = ParallaxGenTask::PGResult::SUCCESS;
	enable_result = true;  // Start with default true

	// processing for parallax
	filesystem::path height_map = search_prefix + "_p.dds";
	if (ignore_parallax || !pgd->isHeightMap(height_map)) {
		enable_result = false;
		return result;
	}

	// Check if nif has attached havok (results in crashes for vanilla parallax)
	if (has_attached_havok) {
		spdlog::trace(L"Rejecting NIF file {} due to attached havok animations", nif_file.wstring());
		enable_result = false;
		return result;
	}

	// ignore skinned meshes, these don't support parallax
	if (shape->HasSkinInstance() || shape->IsSkinned()) {
		spdlog::trace(L"Rejecting shape {}: Skinned mesh", shape_block_id, nif_file.wstring());
		enable_result = false;
		return result;
	}

	// Enable regular parallax for this shape!
	BSLSP shader_type = static_cast<BSLSP>(shader->GetShaderType());
	if (shader_type != BSLSP::BSLSP_DEFAULT && shader_type != BSLSP::BSLSP_PARALLAX) {
		// don't overwrite existing shaders
		spdlog::trace(L"Rejecting shape {} in NIF file {}: Incorrect shader type", shape_block_id, nif_file.wstring());
		enable_result = false;
		return result;
	}

	// decals don't work with regular parallax
	BSLightingShaderProperty* cur_bslsp = dynamic_cast<BSLightingShaderProperty*>(shader);
	if (cur_bslsp->shaderFlags1 & SSPF1::SLSF1_DECAL || cur_bslsp->shaderFlags1 & SSPF1::SLSF1_DYNAMIC_DECAL) {
		spdlog::trace(L"Rejecting shape {} in NIF file {}: Decal shape", shape_block_id, nif_file.wstring());
		enable_result = false;
		return result;
	}

	// Mesh lighting doesn't work with regular parallax
	if (cur_bslsp->shaderFlags2 & SSPF2::SLSF2_SOFT_LIGHTING || cur_bslsp->shaderFlags2 & SSPF2::SLSF2_RIM_LIGHTING || cur_bslsp->shaderFlags2 & SSPF2::SLSF2_BACK_LIGHTING) {
		spdlog::trace(L"Rejecting shape {} in NIF file {}: Lighting on shape", shape_block_id, nif_file.wstring());
		enable_result = false;
		return result;
	}

	// verify that maps match each other (this is somewhat expense so it happens last)
	string diffuse_map;
	uint32_t diffuse_result = nif.GetTextureSlot(shape, diffuse_map, 0);
	if (!diffuse_map.empty() && !pgd->isFile(diffuse_map)) {
		// no diffuse map
		spdlog::trace(L"Rejecting shape {}: Diffuse map missing: {}", shape_block_id, convertToWstring(diffuse_map));
		enable_result = false;
		return result;
	}

	bool same_aspect = false;
	ParallaxGenTask::updatePGResult(result, pgd3d->checkIfAspectRatioMatches(diffuse_map, height_map, same_aspect), ParallaxGenTask::PGResult::SUCCESS_WITH_WARNINGS);
	if (!same_aspect) {
		spdlog::trace(L"Rejecting shape {} in NIF file {}: Height map does not match diffuse map", shape_block_id, nif_file.wstring());
		enable_result = false;
		return result;
	}

	// All checks passed
	return result;
}

ParallaxGenTask::PGResult ParallaxGen::enableComplexMaterialOnShape(NifFile& nif, NiShape* shape, NiShader* shader, const string& search_prefix, bool dynCubemaps, bool& nif_modified)
{
	// enable complex material on shape
	auto result = ParallaxGenTask::PGResult::SUCCESS;

	// 1. set shader type to env map
	if (shader->GetShaderType() != BSLSP::BSLSP_ENVMAP) {
		shader->SetShaderType(BSLSP::BSLSP_ENVMAP);
		nif_modified = true;
	}
	// 2. set shader flags
	BSLightingShaderProperty* cur_bslsp = dynamic_cast<BSLightingShaderProperty*>(shader);
	if(cur_bslsp->shaderFlags1 & SSPF1::SLSF1_PARALLAX) {
		// Complex material cannot have parallax shader flag
		cur_bslsp->shaderFlags1 &= ~SSPF1::SLSF1_PARALLAX;
	}

	if (!(cur_bslsp->shaderFlags1 & SSPF1::SLSF1_ENVIRONMENT_MAPPING)) {
		cur_bslsp->shaderFlags1 |= SSPF1::SLSF1_ENVIRONMENT_MAPPING;
		nif_modified = true;
	}

	// 5. set complex material texture
	string height_map;
	uint32_t height_result = nif.GetTextureSlot(shape, height_map, 3);
	if (height_result != 0 || !height_map.empty()) {
		// remove height map
		string new_height_map = "";
		nif.SetTextureSlot(shape, new_height_map, 3);
		nif_modified = true;
	}

	string env_map;
	uint32_t env_result = nif.GetTextureSlot(shape, env_map, 5);
	string new_env_map = search_prefix + "_m.dds";
	if (!boost::iequals(env_map, new_env_map)) {
		// add height map
		nif.SetTextureSlot(shape, new_env_map, 5);
		nif_modified = true;
	}

	// Dynamic cubemaps (if enabled)
	if (dynCubemaps) {
		// add cubemap to slot
		string cubemap;
		uint32_t cubemap_result = nif.GetTextureSlot(shape, cubemap, 4);
		string new_cubemap = ParallaxGenDirectory::default_cubemap_path.string();

		if (!boost::iequals(cubemap, new_cubemap)) {
			// only fill if dyn cubemap not already there
			nif.SetTextureSlot(shape, new_cubemap, 4);
			nif_modified = true;
		}
	}

	return result;
}

ParallaxGenTask::PGResult ParallaxGen::enableParallaxOnShape(NifFile& nif, NiShape* shape, NiShader* shader, const string& search_prefix, bool& nif_modified)
{
	// enable parallax on shape
	auto result = ParallaxGenTask::PGResult::SUCCESS;

	// 1. set shader type to parallax
	if (shader->GetShaderType() != BSLSP::BSLSP_PARALLAX) {
		shader->SetShaderType(BSLSP::BSLSP_PARALLAX);
		nif_modified = true;
	}
	// 2. Set shader flags
	BSLightingShaderProperty* cur_bslsp = dynamic_cast<BSLightingShaderProperty*>(shader);
	if(cur_bslsp->shaderFlags1 & SSPF1::SLSF1_ENVIRONMENT_MAPPING) {
		// Vanilla parallax cannot have environment mapping flag
		cur_bslsp->shaderFlags1 &= ~SSPF1::SLSF1_ENVIRONMENT_MAPPING;
	}

	if (!(cur_bslsp->shaderFlags1 & SSPF1::SLSF1_PARALLAX)) {
		cur_bslsp->shaderFlags1 |= SSPF1::SLSF1_PARALLAX;
		nif_modified = true;
	}
	// 3. set vertex colors for shape
	if (!shape->HasVertexColors()) {
		shape->SetVertexColors(true);
		nif_modified = true;
	}
	// 4. set vertex colors for shader
	if (!shader->HasVertexColors()) {
		shader->SetVertexColors(true);
		nif_modified = true;
	}
	// 5. set parallax heightmap texture
	string height_map;
	uint32_t height_result = nif.GetTextureSlot(shape, height_map, 3);
	string new_height_map = search_prefix + "_p.dds";

	if (!boost::iequals(height_map, new_height_map)) {
		// add height map
		nif.SetTextureSlot(shape, new_height_map, 3);
		nif_modified = true;
	}

	return result;
}

vector<string> ParallaxGen::getSearchPrefixes(NifFile& nif, nifly::NiShape* shape)
{
	// TODO there's probably a smarter way to do this
	// build search vector
	vector<string> search_prefixes;

	// diffuse map lookup first
	string diffuse_map;
	uint32_t diffuse_result = nif.GetTextureSlot(shape, diffuse_map, 0);
	if (diffuse_result == 0) {
		return vector<string>();
	}
	ParallaxGenUtil::addUniqueElement(search_prefixes, diffuse_map.substr(0, diffuse_map.find_last_of('.')));

	// normal map lookup
	string normal_map;
	uint32_t normal_result = nif.GetTextureSlot(shape, normal_map, 1);
	if (diffuse_result > 0) {
		ParallaxGenUtil::addUniqueElement(search_prefixes, normal_map.substr(0, normal_map.find_last_of('_')));
	}

	return search_prefixes;
}

void ParallaxGen::addFileToZip(mz_zip_archive& zip, const filesystem::path& filePath, const filesystem::path& zipPath)
{
	// ignore zip file itself
	if (filePath == zipPath) {
		return;
	}

	// open file stream
	vector<std::byte> buffer = ParallaxGenUtil::getFileBytes(filePath);

	// get relative path
	filesystem::path zip_relative_path = filePath.lexically_relative(output_dir);

	string zip_file_path = ParallaxGenUtil::wstring_to_utf8(zip_relative_path.wstring());

	// add file to zip
    if (!mz_zip_writer_add_mem(&zip, zip_file_path.c_str(), buffer.data(), buffer.size(), MZ_NO_COMPRESSION)) {
		spdlog::error(L"Error adding file to zip: {}", filePath.wstring());
		ParallaxGenUtil::exitWithUserInput(1);
    }
}

void ParallaxGen::zipDirectory(const filesystem::path& dirPath, const filesystem::path& zipPath)
{
	mz_zip_archive zip;

	// init to 0
    memset(&zip, 0, sizeof(zip));

	// check if directory exists
	if (!filesystem::exists(dirPath)) {
		spdlog::info("No outputs were created");
		ParallaxGenUtil::exitWithUserInput(0);
	}

	// Check if file already exists and delete
	if (filesystem::exists(zipPath)) {
		spdlog::info(L"Deleting existing output zip file: {}", zipPath.wstring());
		filesystem::remove(zipPath);
	}

	// initialize file
	string zip_path_string = ParallaxGenUtil::wstring_to_utf8(zipPath);
    if (!mz_zip_writer_init_file(&zip, zip_path_string.c_str(), 0)) {
		spdlog::critical(L"Error creating zip file: {}", zipPath.wstring());
		ParallaxGenUtil::exitWithUserInput(1);
    }

	// add each file in directory to zip
    for (const auto &entry : filesystem::recursive_directory_iterator(dirPath)) {
        if (filesystem::is_regular_file(entry.path())) {
            addFileToZip(zip, entry.path(), zipPath);
        }
    }

	// finalize zip
    if (!mz_zip_writer_finalize_archive(&zip)) {
		spdlog::critical(L"Error finalizing zip archive: {}", zipPath.wstring());
		ParallaxGenUtil::exitWithUserInput(1);
    }

    mz_zip_writer_end(&zip);

	spdlog::info(L"Please import this file into your mod manager: {}", zipPath.wstring());
}
