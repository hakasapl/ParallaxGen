#include "ParallaxGen.hpp"

#include <DirectXTex.h>
#include <Geometry.hpp>
#include <NifFile.hpp>
#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/asio.hpp>
#include <boost/crc.hpp>
#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/thread.hpp>
#include <filesystem>
#include <fstream>
#include <memory>
#include <miniz.h>
#include <mutex>
#include <ranges>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include <d3d11.h>

#include "Logger.hpp"
#include "NIFUtil.hpp"
#include "ParallaxGenDirectory.hpp"
#include "ParallaxGenPlugin.hpp"
#include "ParallaxGenRunner.hpp"
#include "ParallaxGenTask.hpp"
#include "ParallaxGenUtil.hpp"
#include "ParallaxGenWarnings.hpp"
#include "patchers/PatcherShader.hpp"
#include "patchers/PatcherUtil.hpp"

using namespace std;
using namespace ParallaxGenUtil;
using namespace nifly;

ParallaxGen::ParallaxGen(
    filesystem::path outputDir, ParallaxGenDirectory* pgd, ParallaxGenD3D* pgd3D, const bool& optimizeMeshes)
    : m_outputDir(std::move(outputDir))
    , m_pgd(pgd)
    , m_pgd3D(pgd3D)
    , m_modPriority(nullptr)
{
    // constructor

    // set optimize meshes flag
    m_nifSaveOptions.optimize = optimizeMeshes;
}

void ParallaxGen::loadPatchers(
    const PatcherUtil::PatcherMeshSet& meshPatchers, const PatcherUtil::PatcherTextureSet& texPatchers)
{
    this->m_meshPatchers = meshPatchers;
    this->m_texPatchers = texPatchers;
}

void ParallaxGen::loadModPriorityMap(unordered_map<wstring, int>* modPriority)
{
    this->m_modPriority = modPriority;
    ParallaxGenPlugin::loadModPriorityMap(modPriority);
}

void ParallaxGen::patch(const bool& multiThread, const bool& patchPlugin)
{
    auto meshes = m_pgd->getMeshes();

    // Define diff JSON
    nlohmann::json diffJSON = nlohmann::json::object();

    // Create task tracker
    ParallaxGenTask taskTracker("Mesh Patcher", meshes.size());

    // Create runner
    ParallaxGenRunner meshRunner(multiThread);

    // Add tasks
    for (const auto& mesh : meshes) {
        meshRunner.addTask([this, &taskTracker, &diffJSON, &mesh, &patchPlugin] {
            taskTracker.completeJob(processNIF(mesh, &diffJSON, patchPlugin));
        });
    }

    // Blocks until all tasks are done
    meshRunner.runTasks();

    // Print any resulting warning
    ParallaxGenWarnings::printWarnings();

    if (!m_texPatchers.globalPatchers.empty()) {
        // texture runner
        auto textures = m_pgd->getTextures();

        // Create task tracker
        ParallaxGenTask textureTaskTracker("Texture Patcher", textures.size());

        // Create runner
        ParallaxGenRunner textureRunner(multiThread);

        // Add tasks
        for (const auto& texture : textures) {
            textureRunner.addTask(
                [this, &textureTaskTracker, &texture] { textureTaskTracker.completeJob(processDDS(texture)); });
        }

        // Blocks until all tasks are done
        textureRunner.runTasks();
    }

    // Write diffJSON file
    spdlog::info("Saving diff JSON file...");
    const filesystem::path diffJSONPath = m_outputDir / getDiffJSONName();
    ofstream diffJSONFile(diffJSONPath);
    diffJSONFile << diffJSON << "\n";
    diffJSONFile.close();
}

auto ParallaxGen::findModConflicts(const bool& multiThread, const bool& patchPlugin)
    -> unordered_map<wstring, tuple<set<NIFUtil::ShapeShader>, unordered_set<wstring>>>
{
    auto meshes = m_pgd->getMeshes();

    // Create task tracker
    ParallaxGenTask taskTracker("Finding Mod Conflicts", meshes.size(), PROGRESS_INTERVAL_CONFLICTS);

    // Define conflicts
    PatcherUtil::ConflictModResults conflictMods;

    // Create runner
    ParallaxGenRunner runner(multiThread);

    // Add tasks
    for (const auto& mesh : meshes) {
        runner.addTask([this, &taskTracker, &mesh, &patchPlugin, &conflictMods] {
            taskTracker.completeJob(processNIF(mesh, nullptr, patchPlugin, &conflictMods));
        });
    }

    // Blocks until all tasks are done
    runner.runTasks();

    return conflictMods.mods;
}

void ParallaxGen::zipMeshes() const
{
    // Zip meshes
    spdlog::info("Zipping meshes...");
    zipDirectory(m_outputDir, m_outputDir / getOutputZipName());
}

void ParallaxGen::deleteOutputDir(const bool& preOutput) const
{
    static const unordered_set<filesystem::path> foldersToDelete
        = { "meshes", "textures", "LightPlacer", "PBRTextureSets" };
    static const unordered_set<filesystem::path> filesToDelete = { "ParallaxGen.esp", getDiffJSONName() };
    static const unordered_set<filesystem::path> filesToIgnore = { "meta.ini" };
    static const unordered_set<filesystem::path> filesToDeletePreOutput = { getOutputZipName() };

    if (!filesystem::exists(m_outputDir) || !filesystem::is_directory(m_outputDir)) {
        return;
    }

    for (const auto& entry : filesystem::directory_iterator(m_outputDir)) {
        if (entry.is_regular_file()
            && (filesToDelete.contains(entry.path().filename()) || filesToIgnore.contains(entry.path().filename())
                || filesToDeletePreOutput.contains(entry.path().filename()))) {
            continue;
        }

        if (entry.is_directory() && foldersToDelete.contains(entry.path().filename())) {
            continue;
        }

        spdlog::critical("Output directory has non-ParallaxGen related files. The output directory should only contain "
                         "files generated by ParallaxGen or empty. Exiting.");
        exit(1);
    }

    spdlog::info("Deleting old output files from output directory...");

    // Delete old output
    for (const auto& fileToDelete : filesToDelete) {
        const auto file = m_outputDir / fileToDelete;
        if (filesystem::exists(file)) {
            filesystem::remove(file);
        }
    }

    for (const auto& folderToDelete : foldersToDelete) {
        const auto folder = m_outputDir / folderToDelete;
        if (filesystem::exists(folder)) {
            filesystem::remove_all(folder);
        }
    }

    if (preOutput) {
        for (const auto& fileToDelete : filesToDeletePreOutput) {
            const auto file = m_outputDir / fileToDelete;
            if (filesystem::exists(file)) {
                filesystem::remove(file);
            }
        }
    }
}

auto ParallaxGen::getOutputZipName() -> filesystem::path { return "ParallaxGen_Output.zip"; }

auto ParallaxGen::getDiffJSONName() -> filesystem::path { return "ParallaxGen_Diff.json"; }

auto ParallaxGen::processNIF(const filesystem::path& nifFile, nlohmann::json* diffJSON, const bool& patchPlugin,
    PatcherUtil::ConflictModResults* conflictMods) -> ParallaxGenTask::PGResult
{
    auto result = ParallaxGenTask::PGResult::SUCCESS;

    const Logger::Prefix prefixNIF(nifFile.wstring());
    Logger::trace(L"Starting processing");

    // Determine output path for patched NIF
    const filesystem::path outputFile = m_outputDir / nifFile;
    if (filesystem::exists(outputFile)) {
        Logger::error(L"NIF Rejected: File already exists");
        result = ParallaxGenTask::PGResult::FAILURE;
        return result;
    }

    // Load NIF file
    vector<std::byte> nifFileData;
    try {
        nifFileData = m_pgd->getFile(nifFile);
    } catch (const exception& e) {
        Logger::error(L"NIF Rejected: Unable to load NIF: {}", utf8toUTF16(e.what()));
        result = ParallaxGenTask::PGResult::FAILURE;
        return result;
    }

    // Process NIF
    bool nifModified = false;
    vector<pair<filesystem::path, nifly::NifFile>> dupNIFs;
    auto nif = processNIF(nifFile, nifFileData, nifModified, nullptr, &dupNIFs, patchPlugin, conflictMods);

    // Save patched NIF if it was modified
    if (nifModified && conflictMods == nullptr && nif.IsValid()) {
        // Calculate CRC32 hash before
        boost::crc_32_type crcBeforeResult {};
        crcBeforeResult.process_bytes(nifFileData.data(), nifFileData.size());
        const auto crcBefore = crcBeforeResult.checksum();

        // create directories if required
        filesystem::create_directories(outputFile.parent_path());

        if (nif.Save(outputFile, m_nifSaveOptions) != 0) {
            Logger::error(L"Unable to save NIF file");
            result = ParallaxGenTask::PGResult::FAILURE;
            return result;
        }

        Logger::debug(L"Saving patched NIF to output");

        // Clear NIF from memory (no longer needed)
        nif.Clear();

        // Calculate CRC32 hash after
        const auto outputFileBytes = getFileBytes(outputFile);
        boost::crc_32_type crcResultAfter {};
        crcResultAfter.process_bytes(outputFileBytes.data(), outputFileBytes.size());
        const auto crcAfter = crcResultAfter.checksum();

        // Add to diff JSON
        if (diffJSON != nullptr) {
            auto jsonKey = utf16toUTF8(nifFile.wstring());
            threadSafeJSONUpdate(
                [&](nlohmann::json& json) {
                    json[jsonKey]["crc32original"] = crcBefore;
                    json[jsonKey]["crc32patched"] = crcAfter;
                },
                *diffJSON);
        }
    }

    // Save any duplicate NIFs
    for (auto& [dupNIFFile, dupNIF] : dupNIFs) {
        const auto dupNIFPath = m_outputDir / dupNIFFile;
        filesystem::create_directories(dupNIFPath.parent_path());
        // TODO do we need to add info about this to diff json?
        Logger::debug(L"Saving duplicate NIF to output: {}", dupNIFPath.wstring());
        if (dupNIF.Save(dupNIFPath, m_nifSaveOptions) != 0) {
            Logger::error(L"Unable to save duplicate NIF file {}", dupNIFFile.wstring());
            result = ParallaxGenTask::PGResult::FAILURE;
            return result;
        }
    }

    return result;
}

// shorten some enum names
auto ParallaxGen::processNIF(const std::filesystem::path& nifFile, const vector<std::byte>& nifBytes, bool& nifModified,
    const vector<NIFUtil::ShapeShader>* forceShaders, vector<pair<filesystem::path, nifly::NifFile>>* dupNIFs,
    const bool& patchPlugin, PatcherUtil::ConflictModResults* conflictMods) -> nifly::NifFile
{
    if (patchPlugin && dupNIFs == nullptr) {
        throw runtime_error("DupNIFs must be set if patchPlugin is true");
    }

    NifFile nif;
    try {
        nif = NIFUtil::loadNIFFromBytes(nifBytes);
    } catch (const exception& e) {
        Logger::error(L"NIF Rejected: Unable to load NIF: {}", utf8toUTF16(e.what()));
        return {};
    }

    nifModified = false;

    // Create patcher objects
    auto patcherObjects = PatcherUtil::PatcherMeshObjectSet();
    for (const auto& [shader, factory] : m_meshPatchers.shaderPatchers) {
        auto patcher = factory(nifFile, &nif);
        patcherObjects.shaderPatchers.emplace(shader, std::move(patcher));
    }
    for (const auto& [shader, factory] : m_meshPatchers.shaderTransformPatchers) {
        for (const auto& [transformShader, transformFactory] : factory) {
            auto transform = transformFactory(nifFile, &nif);
            patcherObjects.shaderTransformPatchers[shader].emplace(transformShader, std::move(transform));
        }
    }
    for (const auto& factory : m_meshPatchers.globalPatchers) {
        auto patcher = factory(nifFile, &nif);
        patcherObjects.globalPatchers.emplace_back(std::move(patcher));
    }

    // Get shapes
    auto shapes = nif.GetShapes();

    // Store plugin records
    vector<NIFUtil::ShapeShader> shadersAppliedMesh(shapes.size(), NIFUtil::ShapeShader::UNKNOWN);
    unordered_map<int, pair<vector<NIFUtil::ShapeShader>, vector<ParallaxGenPlugin::TXSTResult>>> recordHandleTracker;

    // Patch each shape in NIF
    int oldShapeIndex = 0;
    vector<tuple<NiShape*, int, int>> shapeTracker;
    for (NiShape* nifShape : shapes) {
        if (nifShape == nullptr) {
            spdlog::error(L"NIF {} has a null shape (skipping)", nifFile.wstring());
            nifModified = false;
            return {};
        }

        // Check for any non-ascii chars
        for (uint32_t slot = 0; slot < NUM_TEXTURE_SLOTS; slot++) {
            string texture;
            nif.GetTextureSlot(nifShape, texture, slot);

            if (!containsOnlyAscii(texture)) {
                spdlog::error(L"NIF {} has texture slot(s) with invalid non-ASCII chars (skipping)", nifFile.wstring());
                nifModified = false;
                return {};
            }
        }

        // get shape name and blockid
        const auto shapeBlockID = nif.GetBlockID(nifShape);
        const auto shapeName = utf8toUTF16(nifShape->name.get());
        const auto shapeIDStr = to_wstring(shapeBlockID) + L" / " + shapeName;
        const Logger::Prefix prefixShape(shapeIDStr);

        bool shapeModified = false;
        NIFUtil::ShapeShader shaderApplied = NIFUtil::ShapeShader::UNKNOWN;

        ParallaxGenTask::PGResult shapeResult = ParallaxGenTask::PGResult::SUCCESS;
        if (forceShaders != nullptr && oldShapeIndex < forceShaders->size()) {
            // Force shaders
            auto shaderForce = (*forceShaders)[oldShapeIndex];
            shapeResult = processShape(nifFile, nif, nifShape, oldShapeIndex, patcherObjects, shapeModified,
                shaderApplied, conflictMods, &shaderForce);
        } else {
            shapeResult = processShape(
                nifFile, nif, nifShape, oldShapeIndex, patcherObjects, shapeModified, shaderApplied, conflictMods);
        }

        if (shapeResult != ParallaxGenTask::PGResult::SUCCESS) {
            // Fail on process shape fail
            nifModified = false;
            return {};
        }

        nifModified |= shapeModified;
        shadersAppliedMesh[oldShapeIndex] = shaderApplied;

        // Update nifModified if shape was modified
        if (shaderApplied != NIFUtil::ShapeShader::UNKNOWN && patchPlugin && forceShaders == nullptr) {
            // Get all plugin results
            vector<ParallaxGenPlugin::TXSTResult> results;
            ParallaxGenPlugin::processShape(
                nifFile.wstring(), nifShape, shapeName, oldShapeIndex, patcherObjects, results, conflictMods);

            // Loop through results
            for (const auto& result : results) {
                if (recordHandleTracker.find(result.modelRecHandle) == recordHandleTracker.end()) {
                    recordHandleTracker[result.modelRecHandle]
                        = { vector<NIFUtil::ShapeShader>(shapes.size(), NIFUtil::ShapeShader::UNKNOWN), {} };
                }

                recordHandleTracker[result.modelRecHandle].first[oldShapeIndex] = result.shader;
                recordHandleTracker[result.modelRecHandle].second.push_back(result);
            }
        }

        shapeTracker.emplace_back(nifShape, oldShapeIndex, -1);
        oldShapeIndex++;
    }

    if (conflictMods != nullptr) {
        // no need to continue if just getting mod conflicts
        nifModified = false;
        return {};
    }

    if (patchPlugin && forceShaders == nullptr) {
        // Loop through plugin results and fix unknowns to match mesh
        for (auto& [modelRecHandle, results] : recordHandleTracker) {
            for (size_t i = 0; i < shapes.size(); i++) {
                if (results.first[i] == NIFUtil::ShapeShader::UNKNOWN) {
                    if (shadersAppliedMesh[i] == NIFUtil::ShapeShader::UNKNOWN) {
                        // No shader applied to mesh, use default
                        results.first[i] = NIFUtil::ShapeShader::UNKNOWN;
                    } else {
                        // Use mesh shader
                        results.first[i] = shadersAppliedMesh[i];
                    }
                }
            }
        }

        unordered_map<vector<NIFUtil::ShapeShader>, wstring, VectorHash> meshTracker;

        // Apply plugin results
        size_t numMesh = 0;
        for (const auto& [modelRecHandle, results] : recordHandleTracker) {
            wstring newNIFName;
            if (meshTracker.contains(results.first)) {
                // Mesh already exists, we only need to assign mesh
                newNIFName = meshTracker[results.first];
            } else {
                const auto curShaders = results.first;
                if (curShaders == shadersAppliedMesh) {
                    newNIFName = nifFile.wstring();
                } else {
                    // Different from mesh which means duplicate is needed
                    newNIFName
                        = (nifFile.parent_path() / nifFile.stem()).wstring() + L"_pg" + to_wstring(++numMesh) + L".nif";
                    // Create duplicate NIF object from original bytes
                    bool dupnifModified = false;
                    auto dupNIF = processNIF(newNIFName, nifBytes, dupnifModified, &curShaders);
                    dupNIFs->emplace_back(newNIFName, dupNIF);
                }
            }

            // assign mesh in plugin
            ParallaxGenPlugin::assignMesh(newNIFName, results.second);

            // Add to mesh tracker
            meshTracker[results.first] = newNIFName;
        }
    }

    // Run global patchers
    for (const auto& globalPatcher : patcherObjects.globalPatchers) {
        const Logger::Prefix prefixPatches(utf8toUTF16(globalPatcher->getPatcherName()));
        globalPatcher->applyPatch(nifModified);
    }

    if (!nifModified) {
        // No changes were made
        return {};
    }

    // Delete unreferenced blocks
    nif.DeleteUnreferencedBlocks();

    // Sort blocks and set plugin indices
    nif.PrettySortBlocks();

    if (patchPlugin && forceShaders == nullptr) {
        for (auto& shape : shapeTracker) {
            // Find new block id
            const auto newBlockID = nif.GetBlockID(get<0>(shape));
            get<2>(shape) = static_cast<int>(newBlockID);
        }

        // Sort ShapeTracker by new block id
        std::ranges::sort(shapeTracker, [](const auto& a, const auto& b) { return get<2>(a) < get<2>(b); });

        // Find new 3d index for each shape
        for (int i = 0; i < shapeTracker.size(); i++) {
            // get new plugin index
            get<2>(shapeTracker[i]) = i;
        }

        ParallaxGenPlugin::set3DIndices(nifFile.wstring(), shapeTracker);
    }

    return nif;
}

auto ParallaxGen::processShape(const filesystem::path& nifPath, NifFile& nif, NiShape* nifShape, const int& shapeIndex,
    PatcherUtil::PatcherMeshObjectSet& patchers, bool& shapeModified, NIFUtil::ShapeShader& shaderApplied,
    PatcherUtil::ConflictModResults* conflictMods, const NIFUtil::ShapeShader* forceShader) -> ParallaxGenTask::PGResult
{

    auto result = ParallaxGenTask::PGResult::SUCCESS;

    // Prep
    Logger::trace(L"Starting Processing");

    // Check for exclusions
    // only allow BSLightingShaderProperty blocks
    const string nifShapeName = nifShape->GetBlockName();
    if (nifShapeName != "NiTriShape" && nifShapeName != "BSTriShape" && nifShapeName != "BSLODTriShape"
        && nifShapeName != "BSMeshLODTriShape") {
        Logger::trace(L"Rejecting: Incorrect shape block type");
        return result;
    }

    // get NIFShader type
    if (!nifShape->HasShaderProperty()) {
        Logger::trace(L"Rejecting: No NIFShader property");
        return result;
    }

    // get NIFShader from shape
    NiShader* nifShader = nif.GetShader(nifShape);
    if (nifShader == nullptr) {
        // skip if no NIFShader
        Logger::trace(L"Rejecting: No NIFShader property");
        return result;
    }

    // check that NIFShader is a BSLightingShaderProperty
    const string nifShaderName = nifShader->GetBlockName();
    if (nifShaderName != "BSLightingShaderProperty") {
        Logger::trace(L"Rejecting: Incorrect NIFShader block type");
        return result;
    }

    shaderApplied = NIFUtil::ShapeShader::NONE;

    if (forceShader != nullptr) {
        // Force shader means we can just apply the shader and return
        shaderApplied = *forceShader;
        // Find correct patcher
        const auto& patcher = patchers.shaderPatchers.at(*forceShader);
        patcher->applyShader(*nifShape, shapeModified);

        return result;
    }

    // check that NIFShader has a texture set
    if (!nifShader->HasTextureSet()) {
        Logger::trace(L"Rejecting: No texture set");
        return result;
    }

    // Create cache key
    bool cacheExists = false;
    ParallaxGen::ShapeKey cacheKey;
    cacheKey.nifPath = nifPath;
    cacheKey.shapeIndex = shapeIndex;

    // Allowed shaders from result of patchers
    vector<PatcherUtil::ShaderPatcherMatch> matches;

    // Restore cache if exists
    {
        const lock_guard<mutex> lock(m_allowedShadersCacheMutex);

        // Check if shape has already been processed
        if (m_allowedShadersCache.find(cacheKey) != m_allowedShadersCache.end()) {
            cacheExists = true;
            matches = m_allowedShadersCache.at(cacheKey);
        }
    }

    // Loop through each shader
    unordered_set<wstring> modSet;
    if (!cacheExists) {
        for (const auto& [shader, patcher] : patchers.shaderPatchers) {
            // note: name is defined in source code in UTF8-encoded files
            const Logger::Prefix prefixPatches(utf8toUTF16(patcher->getPatcherName()));

            // Check if shader should be applied
            vector<PatcherShader::PatcherMatch> curMatches;
            if (!patcher->shouldApply(*nifShape, curMatches)) {
                Logger::trace(L"Rejecting: Shader not applicable");
                continue;
            }

            for (const auto& match : curMatches) {
                PatcherUtil::ShaderPatcherMatch curMatch;
                curMatch.mod = m_pgd->getMod(match.matchedPath);
                curMatch.shader = shader;
                curMatch.match = match;
                curMatch.shaderTransformTo = NIFUtil::ShapeShader::NONE;

                // See if transform is possible
                if (patchers.shaderTransformPatchers.contains(shader)) {
                    const auto& availableTransforms = patchers.shaderTransformPatchers.at(shader);
                    // loop from highest element of map to 0
                    for (const auto& availableTransform : ranges::reverse_view(availableTransforms)) {
                        if (patchers.shaderPatchers.at(availableTransform.first)->canApply(*nifShape)) {
                            // Found a transform that can apply, set the transform in the match
                            curMatch.shaderTransformTo = availableTransform.first;
                            break;
                        }
                    }
                }

                // Add to matches if shader can apply (or if transform shader exists and can apply)
                if (patcher->canApply(*nifShape) || curMatch.shaderTransformTo != NIFUtil::ShapeShader::NONE) {
                    matches.push_back(curMatch);
                    modSet.insert(curMatch.mod);
                }
            }
        }

        {
            // write to cache
            const lock_guard<mutex> lock(m_allowedShadersCacheMutex);
            m_allowedShadersCache[cacheKey] = matches;
        }
    }

    if (matches.empty()) {
        // no shaders to apply
        Logger::trace(L"Rejecting: No shaders to apply");
        return result;
    }

    // Populate conflict mods if set
    if (conflictMods != nullptr) {
        if (modSet.size() > 1) {
            const lock_guard<mutex> lock(conflictMods->mutex);

            // add mods to conflict set
            for (const auto& match : matches) {
                if (conflictMods->mods.find(match.mod) == conflictMods->mods.end()) {
                    conflictMods->mods.insert({ match.mod, { set<NIFUtil::ShapeShader>(), unordered_set<wstring>() } });
                }

                get<0>(conflictMods->mods[match.mod]).insert(match.shader);
                get<1>(conflictMods->mods[match.mod]).insert(modSet.begin(), modSet.end());
            }
        }

        return result;
    }

    // Get winning match
    auto winningShaderMatch = PatcherUtil::getWinningMatch(matches, m_modPriority);

    // Apply transforms
    winningShaderMatch = PatcherUtil::applyTransformIfNeeded(winningShaderMatch, patchers);
    shaderApplied = winningShaderMatch.shader;

    if (shaderApplied == NIFUtil::ShapeShader::NONE || shaderApplied == NIFUtil::ShapeShader::UNKNOWN) {
        // no shader to apply
        Logger::trace(L"Rejecting: No shaders to apply");
        return result;
    }

    // Fix num texture slots
    auto* txstRec = nif.GetHeader().GetBlock(nifShader->TextureSetRef());
    if (txstRec->textures.size() < NUM_TEXTURE_SLOTS) {
        txstRec->textures.resize(NUM_TEXTURE_SLOTS);
        shapeModified = true;
    }

    // loop through patchers
    array<wstring, NUM_TEXTURE_SLOTS> newSlots = patchers.shaderPatchers.at(winningShaderMatch.shader)
                                                     ->applyPatch(*nifShape, winningShaderMatch.match, shapeModified);

    // Post warnings if any
    for (const auto& curMatchedFrom : winningShaderMatch.match.matchedFrom) {
        ParallaxGenWarnings::mismatchWarn(
            winningShaderMatch.match.matchedPath, newSlots.at(static_cast<int>(curMatchedFrom)));
    }

    ParallaxGenWarnings::meshWarn(winningShaderMatch.match.matchedPath, nifPath.wstring());

    return result;
}

auto ParallaxGen::processDDS(const filesystem::path& ddsFile) -> ParallaxGenTask::PGResult
{
    auto result = ParallaxGenTask::PGResult::SUCCESS;

    // Prep
    Logger::trace(L"Starting Processing");

    // only allow DDS files
    const string ddsFileExt = ddsFile.extension().string();
    if (ddsFileExt != ".dds") {
        throw runtime_error("File is not a DDS file");
    }

    DirectX::ScratchImage ddsImage;
    if (m_pgd3D->getDDS(ddsFile, ddsImage) != ParallaxGenTask::PGResult::SUCCESS) {
        Logger::error(L"Unable to load DDS file: {}", ddsFile.wstring());
        return ParallaxGenTask::PGResult::FAILURE;
    }

    bool ddsModified = false;

    for (const auto& factory : m_texPatchers.globalPatchers) {
        auto patcher = factory(ddsFile, &ddsImage);
        patcher->applyPatch(ddsModified);
    }

    if (ddsModified) {
        // save to output
        const filesystem::path outputFile = m_outputDir / ddsFile;
        filesystem::create_directories(outputFile.parent_path());

        const HRESULT hr = DirectX::SaveToDDSFile(ddsImage.GetImages(), ddsImage.GetImageCount(),
            ddsImage.GetMetadata(), DirectX::DDS_FLAGS_NONE, outputFile.c_str());
        if (FAILED(hr)) {
            Logger::error(L"Unable to save DDS {}: {}", outputFile.wstring(),
                ParallaxGenUtil::utf8toUTF16(ParallaxGenD3D::getHRESULTErrorMessage(hr)));
            return ParallaxGenTask::PGResult::FAILURE;
        }

        // Update file map with generated file
        m_pgd->addGeneratedFile(ddsFile, m_pgd->getMod(ddsFile));
    }

    return result;
}

void ParallaxGen::threadSafeJSONUpdate(const std::function<void(nlohmann::json&)>& operation, nlohmann::json& diffJSON)
{
    const std::lock_guard<std::mutex> lock(m_jsonUpdateMutex);
    operation(diffJSON);
}

void ParallaxGen::addFileToZip(
    mz_zip_archive& zip, const filesystem::path& filePath, const filesystem::path& zipPath) const
{
    // ignore Zip file itself
    if (filePath == zipPath) {
        return;
    }

    vector<std::byte> buffer = getFileBytes(filePath);

    const filesystem::path relativePath = filePath.lexically_relative(m_outputDir);
    const string relativeFilePathAscii = utf16toASCII(relativePath.wstring());

    // add file to Zip
    if (mz_zip_writer_add_mem(&zip, relativeFilePathAscii.c_str(), buffer.data(), buffer.size(), MZ_NO_COMPRESSION)
        == 0) {
        spdlog::error(L"Error adding file to zip: {}", filePath.wstring());
        exit(1);
    }
}

void ParallaxGen::zipDirectory(const filesystem::path& dirPath, const filesystem::path& zipPath) const
{
    mz_zip_archive zip;

    // init to 0
    memset(&zip, 0, sizeof(zip));

    // Check if file already exists and delete
    if (filesystem::exists(zipPath)) {
        spdlog::info(L"Deleting existing output Zip file: {}", zipPath.wstring());
        filesystem::remove(zipPath);
    }

    // initialize file
    const string zipPathString = utf16toUTF8(zipPath);
    if (mz_zip_writer_init_file(&zip, zipPathString.c_str(), 0) == 0) {
        spdlog::critical(L"Error creating Zip file: {}", zipPath.wstring());
        exit(1);
    }

    // add each file in directory to Zip
    for (const auto& entry : filesystem::recursive_directory_iterator(dirPath)) {
        if (filesystem::is_regular_file(entry.path())) {
            addFileToZip(zip, entry.path(), zipPath);
        }
    }

    // finalize Zip
    if (mz_zip_writer_finalize_archive(&zip) == 0) {
        spdlog::critical(L"Error finalizing Zip archive: {}", zipPath.wstring());
        exit(1);
    }

    mz_zip_writer_end(&zip);

    spdlog::info(L"Please import this file into your mod manager: {}", zipPath.wstring());
}
