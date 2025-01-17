#include "ParallaxGenDirectory.hpp"

#include <DirectXTex.h>
#include <NifFile.hpp>
#include <Shaders.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <filesystem>
#include <mutex>
#include <shlwapi.h>
#include <spdlog/spdlog.h>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <winnt.h>

#include "BethesdaDirectory.hpp"
#include "ModManagerDirectory.hpp"
#include "NIFUtil.hpp"
#include "ParallaxGenRunner.hpp"
#include "ParallaxGenTask.hpp"
#include "ParallaxGenUtil.hpp"

using namespace std;
using namespace ParallaxGenUtil;

ParallaxGenDirectory::ParallaxGenDirectory(BethesdaGame* bg, filesystem::path outputPath, ModManagerDirectory* mmd)
    : BethesdaDirectory(bg, std::move(outputPath), mmd, true)
{
}

ParallaxGenDirectory::ParallaxGenDirectory(
    filesystem::path dataPath, filesystem::path outputPath, ModManagerDirectory* mmd)
    : BethesdaDirectory(std::move(dataPath), std::move(outputPath), mmd, true)
{
}

auto ParallaxGenDirectory::findFiles() -> void
{
    // Clear existing unconfirmedtextures
    m_unconfirmedTextures.clear();
    m_unconfirmedMeshes.clear();

    // Populate unconfirmed maps
    spdlog::info("Finding Relevant Files");
    const auto& fileMap = getFileMap();

    if (fileMap.empty()) {
        throw runtime_error("File map was not populated");
    }

    for (const auto& [path, file] : fileMap) {
        const auto& firstPath = path.begin()->wstring();
        if (boost::iequals(firstPath, "textures") && boost::iequals(path.extension().wstring(), L".dds")) {
            if (!isPathAscii(path)) {
                // Skip non-ascii paths
                spdlog::warn(
                    L"Texture {} contains non-ascii characters which are not allowed - skipping", path.wstring());
                continue;
            }

            // Found a DDS
            spdlog::trace(L"Finding Files | Found DDS | {}", path.wstring());
            m_unconfirmedTextures[path] = {};
        } else if (boost::iequals(firstPath, "meshes") && boost::iequals(path.extension().wstring(), L".nif")) {
            // Found a NIF
            spdlog::trace(L"Finding Files | Found NIF | {}", path.wstring());
            m_unconfirmedMeshes.insert(path);
        } else if (boost::iequals(path.extension().wstring(), L".json")) {
            // Found a JSON file
            if (boost::iequals(firstPath, L"pbrnifpatcher")) {
                // Found PBR JSON config
                spdlog::trace(L"Finding Files | Found PBR JSON | {}", path.wstring());
                m_pbrJSONs.push_back(path);
            }
        }
    }
    spdlog::info("Finding files done");
}

auto ParallaxGenDirectory::mapFiles(const vector<wstring>& nifBlocklist, const vector<wstring>& nifAllowlist,
    const vector<pair<wstring, NIFUtil::TextureType>>& manualTextureMaps, const vector<wstring>& parallaxBSAExcludes,
    const bool& mapFromMeshes, const bool& multithreading, const bool& cacheNIFs) -> void
{
    findFiles();

    // Helpers
    const unordered_map<wstring, NIFUtil::TextureType> manualTextureMapsMap(
        manualTextureMaps.begin(), manualTextureMaps.end());

    spdlog::info("Starting building texture map");

    // Create task tracker
    ParallaxGenTask taskTracker("Loading NIFs", m_unconfirmedMeshes.size(), MAPTEXTURE_PROGRESS_MODULO);

    // Create runner
    ParallaxGenRunner runner(multithreading);

    // Loop through each mesh to confirm textures
    for (const auto& mesh : m_unconfirmedMeshes) {
        if (!nifAllowlist.empty() && !checkGlobMatchInVector(mesh.wstring(), nifAllowlist)) {
            // Skip mesh because it is not on allowlist
            spdlog::trace(L"Loading NIFs | Skipping Mesh due to Allowlist | Mesh: {}", mesh.wstring());
            taskTracker.completeJob(ParallaxGenTask::PGResult::SUCCESS);
            continue;
        }

        if (!nifBlocklist.empty() && checkGlobMatchInVector(mesh.wstring(), nifBlocklist)) {
            // Skip mesh because it is on blocklist
            spdlog::trace(L"Loading NIFs | Skipping Mesh due to Blocklist | Mesh: {}", mesh.wstring());
            taskTracker.completeJob(ParallaxGenTask::PGResult::SUCCESS);
            continue;
        }

        if (!mapFromMeshes) {
            // Skip mapping textures from meshes
            m_meshes.insert(mesh);
            taskTracker.completeJob(ParallaxGenTask::PGResult::SUCCESS);
            continue;
        }

        runner.addTask(
            [this, &taskTracker, &mesh, &cacheNIFs] { taskTracker.completeJob(mapTexturesFromNIF(mesh, cacheNIFs)); });
    }

    // Blocks until all tasks are done
    runner.runTasks();

    // Loop through unconfirmed textures to confirm them
    for (const auto& [texture, property] : m_unconfirmedTextures) {
        bool foundInstance = false;

        // Find winning texture slot
        size_t maxVal = 0;
        NIFUtil::TextureSlots winningSlot = {};
        for (const auto& [slot, count] : property.slots) {
            foundInstance = true;
            if (count > maxVal) {
                maxVal = count;
                winningSlot = slot;
            }
        }

        // Find winning texture type
        maxVal = 0;
        NIFUtil::TextureType winningType = {};
        for (const auto& [type, count] : property.types) {
            foundInstance = true;
            if (count > maxVal) {
                maxVal = count;
                winningType = type;
            }
        }

        if (!foundInstance) {
            // Determine slot and type by suffix
            const auto defProperty = NIFUtil::getDefaultsFromSuffix(texture);
            winningSlot = get<0>(defProperty);
            winningType = get<1>(defProperty);
        }

        if (manualTextureMapsMap.find(texture.wstring()) != manualTextureMapsMap.end()) {
            // Manual texture map found, override
            winningType = manualTextureMapsMap.at(texture.wstring());
            winningSlot = NIFUtil::getSlotFromTexType(winningType);
        }

        if ((winningSlot == NIFUtil::TextureSlots::PARALLAX) && isFileInBSA(texture, parallaxBSAExcludes)) {
            spdlog::trace(L"Mapping Textures | Ignored vanilla parallax texture | Texture: {}", texture.wstring());
            continue;
        }

        // Log result
        spdlog::trace(L"Mapping Textures | Mapping Result | Texture: {} | Slot: {} | Type: {}", texture.wstring(),
            static_cast<size_t>(winningSlot), utf8toUTF16(NIFUtil::getStrFromTexType(winningType)));

        // Add to texture map
        if (winningSlot != NIFUtil::TextureSlots::UNKNOWN) {
            // Only add if no unknowns
            addToTextureMaps(texture, winningSlot, winningType, {});
        }
    }

    // cleanup
    m_unconfirmedTextures.clear();
    m_unconfirmedMeshes.clear();

    spdlog::info("Mapping textures done");
}

auto ParallaxGenDirectory::checkGlobMatchInVector(const wstring& check, const vector<std::wstring>& list) -> bool
{
    // convert wstring to LPCWSTR
    LPCWSTR checkCstr = check.c_str();

    // check if string matches any glob
    return std::ranges::any_of(list, [&](const wstring& glob) { return PathMatchSpecW(checkCstr, glob.c_str()); });
}

auto ParallaxGenDirectory::mapTexturesFromNIF(
    const filesystem::path& nifPath, const bool& cacheNIFs) -> ParallaxGenTask::PGResult
{
    auto result = ParallaxGenTask::PGResult::SUCCESS;

    // Load NIF
    vector<std::byte> nifBytes;
    try {
        nifBytes = getFile(nifPath, cacheNIFs);
    } catch (const exception& e) {
        spdlog::error(L"Error reading NIF File \"{}\" (skipping): {}", nifPath.wstring(), asciitoUTF16(e.what()));
        return ParallaxGenTask::PGResult::FAILURE;
    }

    NifFile nif;
    try {
        // Attempt to load NIF file
        nif = NIFUtil::loadNIFFromBytes(nifBytes);
    } catch (const exception& e) {
        // Unable to read NIF, delete from Meshes set
        spdlog::error(L"Error reading NIF File \"{}\" (skipping): {}", nifPath.wstring(), asciitoUTF16(e.what()));
        return ParallaxGenTask::PGResult::FAILURE;
    }

    // Loop through each shape
    bool hasAtLeastOneTextureSet = false;
    for (auto& shape : nif.GetShapes()) {
        if (!shape->HasShaderProperty()) {
            // No shader, skip
            continue;
        }

        // Get shader
        auto* const shader = nif.GetShader(shape);
        if (shader == nullptr) {
            // No shader, skip
            continue;
        }

        if (!shader->HasTextureSet()) {
            // No texture set, skip
            continue;
        }

        // We have a texture set
        hasAtLeastOneTextureSet = true;

        // Loop through each texture slot
        for (uint32_t slot = 0; slot < NUM_TEXTURE_SLOTS; slot++) {
            string texture;
            nif.GetTextureSlot(shape, texture, slot);

            if (!containsOnlyAscii(texture)) {
                spdlog::error(L"NIF {} has texture slot(s) with invalid non-ASCII chars (skipping)", nifPath.wstring());
                return ParallaxGenTask::PGResult::FAILURE;
            }

            if (texture.empty()) {
                // No texture in this slot
                continue;
            }

            boost::to_lower(texture); // Lowercase for comparison

            const auto shaderType = shader->GetShaderType();
            NIFUtil::TextureType textureType = {};

            // Check to make sure appropriate shaders are set for a given texture
            auto* const shaderBSSP = dynamic_cast<BSShaderProperty*>(shader);
            if (shaderBSSP == nullptr) {
                // Not a BSShaderProperty, skip
                continue;
            }

            switch (static_cast<NIFUtil::TextureSlots>(slot)) {
            case NIFUtil::TextureSlots::DIFFUSE:
                // Diffuse check
                textureType = NIFUtil::TextureType::DIFFUSE;
                break;
            case NIFUtil::TextureSlots::NORMAL:
                // Normal check
                if (shaderType == BSLSP_SKINTINT && NIFUtil::hasShaderFlag(shaderBSSP, SLSF1_FACEGEN_RGB_TINT)) {
                    // This is a skin tint map
                    textureType = NIFUtil::TextureType::MODELSPACENORMAL;
                    break;
                }

                textureType = NIFUtil::TextureType::NORMAL;
                break;
            case NIFUtil::TextureSlots::GLOW:
                // Glowmap check
                if ((shaderType == BSLSP_GLOWMAP && NIFUtil::hasShaderFlag(shaderBSSP, SLSF2_GLOW_MAP))
                    || (shaderType == BSLSP_DEFAULT && NIFUtil::hasShaderFlag(shaderBSSP, SLSF2_UNUSED01))) {
                    // This is an emmissive map (either vanilla glowmap shader or PBR)
                    textureType = NIFUtil::TextureType::EMISSIVE;
                    break;
                }

                if (shaderType == BSLSP_MULTILAYERPARALLAX
                    && NIFUtil::hasShaderFlag(shaderBSSP, SLSF2_MULTI_LAYER_PARALLAX)) {
                    // This is a subsurface map
                    textureType = NIFUtil::TextureType::SUBSURFACECOLOR;
                    break;
                }

                if (shaderType == BSLSP_SKINTINT && NIFUtil::hasShaderFlag(shaderBSSP, SLSF1_FACEGEN_RGB_TINT)) {
                    // This is a skin tint map
                    textureType = NIFUtil::TextureType::SKINTINT;
                    break;
                }

                continue;
            case NIFUtil::TextureSlots::PARALLAX:
                // Parallax check
                if ((shaderType == BSLSP_PARALLAX && NIFUtil::hasShaderFlag(shaderBSSP, SLSF1_PARALLAX))) {
                    // This is a height map
                    textureType = NIFUtil::TextureType::HEIGHT;
                    break;
                }

                if ((shaderType == BSLSP_DEFAULT && NIFUtil::hasShaderFlag(shaderBSSP, SLSF2_UNUSED01))) {
                    // This is a height map for PBR
                    textureType = NIFUtil::TextureType::HEIGHTPBR;
                    break;
                }

                continue;
            case NIFUtil::TextureSlots::CUBEMAP:
                // Cubemap check
                if (shaderType == BSLSP_ENVMAP && NIFUtil::hasShaderFlag(shaderBSSP, SLSF1_ENVIRONMENT_MAPPING)) {
                    textureType = NIFUtil::TextureType::CUBEMAP;
                    break;
                }

                continue;
            case NIFUtil::TextureSlots::ENVMASK:
                // Envmap check
                if (shaderType == BSLSP_ENVMAP && NIFUtil::hasShaderFlag(shaderBSSP, SLSF1_ENVIRONMENT_MAPPING)) {
                    textureType = NIFUtil::TextureType::ENVIRONMENTMASK;
                    break;
                }

                if (shaderType == BSLSP_DEFAULT && NIFUtil::hasShaderFlag(shaderBSSP, SLSF2_UNUSED01)) {
                    textureType = NIFUtil::TextureType::RMAOS;
                    break;
                }

                continue;
            case NIFUtil::TextureSlots::MULTILAYER:
                // Tint check
                if (shaderType == BSLSP_MULTILAYERPARALLAX
                    && NIFUtil::hasShaderFlag(shaderBSSP, SLSF2_MULTI_LAYER_PARALLAX)) {
                    if (NIFUtil::hasShaderFlag(shaderBSSP, SLSF2_UNUSED01)) {
                        // 2 layer PBR
                        textureType = NIFUtil::TextureType::COATNORMALROUGHNESS;
                    } else {
                        // normal multilayer
                        textureType = NIFUtil::TextureType::INNERLAYER;
                    }
                    break;
                }

                continue;
            case NIFUtil::TextureSlots::BACKLIGHT:
                // Backlight check
                if (shaderType == BSLSP_MULTILAYERPARALLAX && NIFUtil::hasShaderFlag(shaderBSSP, SLSF2_UNUSED01)) {
                    textureType = NIFUtil::TextureType::SUBSURFACEPBR;
                    break;
                }

                if (NIFUtil::hasShaderFlag(shaderBSSP, SLSF2_BACK_LIGHTING)) {
                    textureType = NIFUtil::TextureType::BACKLIGHT;
                    break;
                }

                if (shaderType == BSLSP_SKINTINT && NIFUtil::hasShaderFlag(shaderBSSP, SLSF1_FACEGEN_RGB_TINT)) {
                    textureType = NIFUtil::TextureType::SPECULAR;
                    break;
                }

                continue;
            default:
                textureType = NIFUtil::TextureType::UNKNOWN;
            }

            // Log finding
            spdlog::trace(L"Mapping Textures | Slot Found | NIF: {} | Texture: {} | Slot: {} | Type: {}",
                nifPath.wstring(), asciitoUTF16(texture), slot, utf8toUTF16(NIFUtil::getStrFromTexType(textureType)));

            // Update unconfirmed textures map
            updateUnconfirmedTexturesMap(texture, static_cast<NIFUtil::TextureSlots>(slot), textureType);
        }
    }

    if (hasAtLeastOneTextureSet) {
        // Add mesh to set
        addMesh(nifPath);
    }

    return result;
}

auto ParallaxGenDirectory::updateUnconfirmedTexturesMap(
    const filesystem::path& path, const NIFUtil::TextureSlots& slot, const NIFUtil::TextureType& type) -> void
{
    // Use mutex to make this thread safe
    lock_guard<mutex> lock(m_unconfirmedTexturesMutex);

    // Check if texture is already in map
    if (m_unconfirmedTextures.find(path) != m_unconfirmedTextures.end()) {
        // Texture is present
        m_unconfirmedTextures[path].slots[slot]++;
        m_unconfirmedTextures[path].types[type]++;
    }
}

auto ParallaxGenDirectory::addToTextureMaps(const filesystem::path& path, const NIFUtil::TextureSlots& slot,
    const NIFUtil::TextureType& type, const unordered_set<NIFUtil::TextureAttribute>& attributes) -> void
{
    // Get texture base
    const auto& base = NIFUtil::getTexBase(path);
    const auto& slotInt = static_cast<size_t>(slot);

    // Add to texture map
    NIFUtil::PGTexture newPGTexture = { path, type };
    {
        lock_guard<mutex> lock(m_textureMapsMutex);
        m_textureMaps.at(slotInt)[base].insert(newPGTexture);
    }

    {
        lock_guard<mutex> lock(m_textureTypesMutex);
        const TextureDetails details = { type, attributes };
        m_textureTypes[path] = details;
    }
}

auto ParallaxGenDirectory::addMesh(const filesystem::path& path) -> void
{
    // Use mutex to make this thread safe
    lock_guard<mutex> lock(m_meshesMutex);

    // Add mesh to set
    m_meshes.insert(path);
}

auto ParallaxGenDirectory::getTextureMap(
    const NIFUtil::TextureSlots& slot) -> map<wstring, unordered_set<NIFUtil::PGTexture, NIFUtil::PGTextureHasher>>&
{
    return m_textureMaps.at(static_cast<size_t>(slot));
}

auto ParallaxGenDirectory::getTextureMapConst(const NIFUtil::TextureSlots& slot) const
    -> const map<wstring, unordered_set<NIFUtil::PGTexture, NIFUtil::PGTextureHasher>>&
{
    return m_textureMaps.at(static_cast<size_t>(slot));
}

auto ParallaxGenDirectory::getMeshes() const -> const unordered_set<filesystem::path>& { return m_meshes; }

auto ParallaxGenDirectory::getPBRJSONs() const -> const vector<filesystem::path>& { return m_pbrJSONs; }

auto ParallaxGenDirectory::addTextureAttribute(
    const filesystem::path& path, const NIFUtil::TextureAttribute& attribute) -> bool
{
    lock_guard<mutex> lock(m_textureTypesMutex);

    if (m_textureTypes.find(path) != m_textureTypes.end()) {
        return m_textureTypes.at(path).attributes.insert(attribute).second;
    }

    return false;
}

auto ParallaxGenDirectory::removeTextureAttribute(
    const filesystem::path& path, const NIFUtil::TextureAttribute& attribute) -> bool
{
    lock_guard<mutex> lock(m_textureTypesMutex);

    if (m_textureTypes.find(path) != m_textureTypes.end()) {
        return m_textureTypes.at(path).attributes.erase(attribute) > 0;
    }

    return false;
}

auto ParallaxGenDirectory::hasTextureAttribute(
    const filesystem::path& path, const NIFUtil::TextureAttribute& attribute) -> bool
{
    lock_guard<mutex> lock(m_textureTypesMutex);

    if (m_textureTypes.find(path) != m_textureTypes.end()) {
        return m_textureTypes.at(path).attributes.find(attribute) != m_textureTypes.at(path).attributes.end();
    }

    return false;
}

auto ParallaxGenDirectory::getTextureAttributes(
    const filesystem::path& path) -> unordered_set<NIFUtil::TextureAttribute>
{
    lock_guard<mutex> lock(m_textureTypesMutex);

    if (m_textureTypes.find(path) != m_textureTypes.end()) {
        return m_textureTypes.at(path).attributes;
    }

    return {};
}

void ParallaxGenDirectory::setTextureType(const filesystem::path& path, const NIFUtil::TextureType& type)
{
    lock_guard<mutex> lock(m_textureTypesMutex);

    m_textureTypes[path].type = type;
}

auto ParallaxGenDirectory::getTextureType(const filesystem::path& path) -> NIFUtil::TextureType
{
    lock_guard<mutex> lock(m_textureTypesMutex);

    if (m_textureTypes.find(path) != m_textureTypes.end()) {
        return m_textureTypes.at(path).type;
    }

    return NIFUtil::TextureType::UNKNOWN;
}
