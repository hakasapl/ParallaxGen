#include "ParallaxGenConfig.hpp"

#include "BethesdaGame.hpp"
#include "Logger.hpp"
#include "ModManagerDirectory.hpp"
#include "NIFUtil.hpp"
#include "ParallaxGenUtil.hpp"

#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/split.hpp>
#include <mutex>
#include <spdlog/spdlog.h>

#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <string>

using namespace std;
using namespace ParallaxGenUtil;

// Statics
filesystem::path ParallaxGenConfig::s_exePath;

void ParallaxGenConfig::loadStatics(const filesystem::path& exePath)
{
    // Set ExePath
    ParallaxGenConfig::s_exePath = exePath;
}

auto ParallaxGenConfig::getUserConfigFile() -> filesystem::path
{
    if (s_exePath.empty()) {
        throw runtime_error("ExePath not set");
    }

    // Get user config file
    static const filesystem::path userConfigFile = s_exePath / "cfg" / "user.json";
    return userConfigFile;
}

auto ParallaxGenConfig::getDefaultParams() -> PGParams
{
    PGParams outParams;

    // Game
    outParams.Game.dir = BethesdaGame::findGamePathFromSteam(BethesdaGame::GameType::SKYRIM_SE);

    // Output
    if (s_exePath.empty()) {
        throw runtime_error("ExePath not set");
    }
    outParams.Output.dir = s_exePath / "ParallaxGen_Output";

    // Mesh Rules
    static const vector<wstring> defaultMeshBlocklist = { L"*\\cameras\\*", L"*\\dyndolod\\*", L"*\\lod\\*",
        L"*\\magic\\*", L"*\\markers\\*", L"*\\mps\\*", L"*\\sky\\*" };
    outParams.MeshRules.blockList = defaultMeshBlocklist;

    // Texture Rules
    static const vector<wstring> defaultVanillaBSAList = { L"Skyrim - Textures0.bsa", L"Skyrim - Textures1.bsa",
        L"Skyrim - Textures2.bsa", L"Skyrim - Textures3.bsa", L"Skyrim - Textures4.bsa", L"Skyrim - Textures5.bsa",
        L"Skyrim - Textures6.bsa", L"Skyrim - Textures7.bsa", L"Skyrim - Textures8.bsa",
        L"Project Clarity AIO Half Res Packed.bsa", L"Project Clarity AIO Half Res Packed - Textures.bsa",
        L"Project Clarity AIO Half Res Packed0 - Textures.bsa", L"Project Clarity AIO Half Res Packed1 - Textures.bsa",
        L"Project Clarity AIO Half Res Packed2 - Textures.bsa", L"Project Clarity AIO Half Res Packed3 - Textures.bsa",
        L"Project Clarity AIO Half Res Packed4 - Textures.bsa", L"Project Clarity AIO Half Res Packed5 - Textures.bsa",
        L"Project Clarity AIO Half Res Packed6 - Textures.bsa" };
    outParams.TextureRules.vanillaBSAList = defaultVanillaBSAList;

    return outParams;
}

void ParallaxGenConfig::loadConfig()
{
    Logger::Prefix prefix("PGC/loadConfig");

    bool loadedConfig = false;
    if (filesystem::exists(getUserConfigFile())) {
        // don't load a config that doesn't exist
        Logger::debug(L"Loading ParallaxGen Config: {}", getUserConfigFile().wstring());

        nlohmann::json j;
        if (parseJSON(getFileBytes(getUserConfigFile()), j)) {
            if (!j.empty()) {
                replaceForwardSlashes(j);
                addConfigJSON(j);
            }

            m_userConfig = j;
            loadedConfig = true;
            Logger::info("Loaded user config successfully");
        } else {
            Logger::error("Failed to parse ParallaxGen config file");
        }
    }

    if (!loadedConfig) {
        Logger::warn("No user config found, using defaults");
        m_params = getDefaultParams();
    }
}

auto ParallaxGenConfig::addConfigJSON(const nlohmann::json& j) -> void
{
    // "mod_order" field
    if (j.contains("mod_order")) {
        setModOrder(utf8VectorToUTF16(j["mod_order"].get<vector<string>>()));
    }

    // "params" field
    if (j.contains("params")) {
        const auto& paramJ = j["params"];

        // "game"
        if (paramJ.contains("game") && paramJ["game"].contains("dir")) {
            m_params.Game.dir = UTF8toUTF16(paramJ["game"]["dir"].get<string>());
        }
        if (paramJ.contains("game") && paramJ["game"].contains("type")) {
            paramJ["game"]["type"].get_to<BethesdaGame::GameType>(m_params.Game.type);
        }

        // "modmanager"
        if (paramJ.contains("modmanager") && paramJ["modmanager"].contains("type")) {
            paramJ["modmanager"]["type"].get_to<ModManagerDirectory::ModManagerType>(m_params.ModManager.type);
        }
        if (paramJ.contains("modmanager") && paramJ["modmanager"].contains("mo2instancedir")) {
            paramJ["modmanager"]["mo2instancedir"].get_to<filesystem::path>(m_params.ModManager.mo2InstanceDir);
        }
        if (paramJ.contains("modmanager") && paramJ["modmanager"].contains("mo2profile")) {
            m_params.ModManager.mo2Profile = UTF8toUTF16(paramJ["modmanager"]["mo2profile"].get<string>());
        }
        if (paramJ.contains("modmanager") && paramJ["modmanager"].contains("mo2useorder")) {
            paramJ["modmanager"]["mo2useorder"].get_to<bool>(m_params.ModManager.mo2UseOrder);
        }

        // "output"
        if (paramJ.contains("output") && paramJ["output"].contains("dir")) {
            m_params.Output.dir = UTF8toUTF16(paramJ["output"]["dir"].get<string>());
        }
        if (paramJ.contains("output") && paramJ["output"].contains("zip")) {
            paramJ["output"]["zip"].get_to<bool>(m_params.Output.zip);
        }

        // "advanced"
        if (paramJ.contains("advanced")) {
            paramJ["advanced"].get_to<bool>(m_params.advanced);
        }

        // "processing"
        if (paramJ.contains("processing") && paramJ["processing"].contains("multithread")) {
            paramJ["processing"]["multithread"].get_to<bool>(m_params.Processing.multithread);
        }
        if (paramJ.contains("processing") && paramJ["processing"].contains("highmem")) {
            paramJ["processing"]["highmem"].get_to<bool>(m_params.Processing.highMem);
        }
        if (paramJ.contains("processing") && paramJ["processing"].contains("gpuacceleration")) {
            paramJ["processing"]["gpuacceleration"].get_to<bool>(m_params.Processing.gpuAcceleration);
        }
        if (paramJ.contains("processing") && paramJ["processing"].contains("bsa")) {
            paramJ["processing"]["bsa"].get_to<bool>(m_params.Processing.bsa);
        }
        if (paramJ.contains("processing") && paramJ["processing"].contains("pluginpatching")) {
            paramJ["processing"]["pluginpatching"].get_to<bool>(m_params.Processing.pluginPatching);
        }
        if (paramJ.contains("processing") && paramJ["processing"].contains("pluginesmify")) {
            paramJ["processing"]["pluginesmify"].get_to<bool>(m_params.Processing.pluginESMify);
        }
        if (paramJ.contains("processing") && paramJ["processing"].contains("mapfrommeshes")) {
            paramJ["processing"]["mapfrommeshes"].get_to<bool>(m_params.Processing.mapFromMeshes);
        }

        // "prepatcher"
        if (paramJ.contains("prepatcher") && paramJ["prepatcher"].contains("disablemlp")) {
            paramJ["prepatcher"]["disablemlp"].get_to<bool>(m_params.PrePatcher.disableMLP);
        }

        // "shaderpatcher"
        if (paramJ.contains("shaderpatcher") && paramJ["shaderpatcher"].contains("parallax")) {
            paramJ["shaderpatcher"]["parallax"].get_to<bool>(m_params.ShaderPatcher.parallax);
        }
        if (paramJ.contains("shaderpatcher") && paramJ["shaderpatcher"].contains("complexmaterial")) {
            paramJ["shaderpatcher"]["complexmaterial"].get_to<bool>(m_params.ShaderPatcher.complexMaterial);
        }
        if (paramJ.contains("shaderpatcher")
            && paramJ["shaderpatcher"].contains("complexmaterialdyncubemapblocklist")) {
            for (const auto& item : paramJ["shaderpatcher"]["complexmaterialdyncubemapblocklist"]) {
                m_params.ShaderPatcher.ShaderPatcherComplexMaterial.listsDyncubemapBlocklist.push_back(
                    UTF8toUTF16(item.get<string>()));
            }
        }
        if (paramJ.contains("shaderpatcher") && paramJ["shaderpatcher"].contains("truepbr")) {
            paramJ["shaderpatcher"]["truepbr"].get_to<bool>(m_params.ShaderPatcher.truePBR);
        }

        // "shadertransforms"
        if (paramJ.contains("shadertransforms") && paramJ["shadertransforms"].contains("parallaxtocm")) {
            paramJ["shadertransforms"]["parallaxtocm"].get_to<bool>(m_params.ShaderTransforms.parallaxToCM);
        }

        // "postpatcher"
        if (paramJ.contains("postpatcher") && paramJ["postpatcher"].contains("optimizemeshes")) {
            paramJ["postpatcher"]["optimizemeshes"].get_to<bool>(m_params.PostPatcher.optimizeMeshes);
        }

        // "meshrules"
        if (paramJ.contains("meshrules") && paramJ["meshrules"].contains("allowlist")) {
            for (const auto& item : paramJ["meshrules"]["allowlist"]) {
                m_params.MeshRules.allowList.push_back(UTF8toUTF16(item.get<string>()));
            }
        }

        if (paramJ.contains("meshrules") && paramJ["meshrules"].contains("blocklist")) {
            for (const auto& item : paramJ["meshrules"]["blocklist"]) {
                m_params.MeshRules.blockList.push_back(UTF8toUTF16(item.get<string>()));
            }
        }

        // "texturerules"
        if (paramJ.contains("texturerules") && paramJ["texturerules"].contains("texturemaps")) {
            for (const auto& item : paramJ["texturerules"]["texturemaps"].items()) {
                m_params.TextureRules.textureMaps.emplace_back(
                    UTF8toUTF16(item.key()), NIFUtil::getTexTypeFromStr(item.value().get<string>()));
            }
        }

        if (paramJ.contains("texturerules") && paramJ["texturerules"].contains("vanillabsalist")) {
            for (const auto& item : paramJ["texturerules"]["vanillabsalist"]) {
                m_params.TextureRules.vanillaBSAList.push_back(UTF8toUTF16(item.get<string>()));
            }
        }
    }
}

auto ParallaxGenConfig::parseJSON(const vector<std::byte>& bytes, nlohmann::json& j) -> bool
{
    // Parse JSON
    try {
        j = nlohmann::json::parse(bytes);
    } catch (...) {
        j = {};
        return false;
    }

    return true;
}

void ParallaxGenConfig::replaceForwardSlashes(nlohmann::json& json)
{
    if (json.is_string()) {
        auto& str = json.get_ref<string&>();
        for (auto& ch : str) {
            if (ch == '/') {
                ch = '\\';
            }
        }
    } else if (json.is_object()) {
        for (const auto& item : json.items()) {
            replaceForwardSlashes(item.value());
        }
    } else if (json.is_array()) {
        for (auto& element : json) {
            replaceForwardSlashes(element);
        }
    }
}

auto ParallaxGenConfig::getModOrder() -> vector<wstring>
{
    lock_guard<mutex> lock(m_modOrderMutex);
    return m_modOrder;
}

auto ParallaxGenConfig::getModPriority(const wstring& mod) -> int
{
    lock_guard<mutex> lock(m_modOrderMutex);
    if (m_modPriority.contains(mod)) {
        return m_modPriority[mod];
    }

    return -1;
}

auto ParallaxGenConfig::getModPriorityMap() -> unordered_map<wstring, int>
{
    lock_guard<mutex> lock(m_modOrderMutex);
    return m_modPriority;
}

void ParallaxGenConfig::setModOrder(const vector<wstring>& modOrder)
{
    lock_guard<mutex> lock(m_modOrderMutex);
    this->m_modOrder = modOrder;

    m_modPriority.clear();
    for (size_t i = 0; i < modOrder.size(); i++) {
        m_modPriority[modOrder[i]] = static_cast<int>(i);
    }
}

auto ParallaxGenConfig::getParams() const -> PGParams { return m_params; }

void ParallaxGenConfig::setParams(const PGParams& params) { this->m_params = params; }

auto ParallaxGenConfig::validateParams(const PGParams& params, vector<string>& errors) -> bool
{
    // Helpers
    unordered_set<wstring> checkSet;

    // Game
    if (params.Game.dir.empty()) {
        errors.emplace_back("Game Location is required.");
    }

    if (!BethesdaGame::isGamePathValid(params.Game.dir, params.Game.type)) {
        errors.emplace_back("Game Location is not valid.");
    }

    // Mod Manager
    if (params.ModManager.type == ModManagerDirectory::ModManagerType::ModOrganizer2) {
        if (params.ModManager.mo2InstanceDir.empty()) {
            errors.emplace_back("MO2 Instance Location is required");
        }

        if (!filesystem::exists(params.ModManager.mo2InstanceDir)) {
            errors.emplace_back("MO2 Instance Location does not exist");
        }

        if (params.ModManager.mo2Profile.empty()) {
            errors.emplace_back("MO2 Profile is required");
        }

        // Check if the MO2 profile exists
        const auto profiles = ModManagerDirectory::getMO2ProfilesFromInstanceDir(params.ModManager.mo2InstanceDir);
        if (find(profiles.begin(), profiles.end(), params.ModManager.mo2Profile) == profiles.end()) {
            errors.emplace_back("MO2 Profile does not exist");
        }
    }

    // Output
    if (params.Output.dir.empty()) {
        errors.emplace_back("Output Location is required");
    }

    // Processing

    // Pre-Patchers

    // Shader Patchers

    checkSet.clear();
    for (const auto& item : params.ShaderPatcher.ShaderPatcherComplexMaterial.listsDyncubemapBlocklist) {
        if (!checkSet.insert(item).second) {
            errors.emplace_back("Duplicate entry in Complex Material Dyncubemap Block List: " + UTF16toUTF8(item));
        }
    }

    // Shader Transforms
    if (params.ShaderTransforms.parallaxToCM
        && (!params.ShaderPatcher.parallax || !params.ShaderPatcher.complexMaterial)) {
        errors.emplace_back(
            "Upgrade Parallax to Complex Material requires both the Complex Material and Parallax shader patchers");
    }

    if (params.ShaderTransforms.parallaxToCM && !params.Processing.gpuAcceleration) {
        errors.emplace_back("Upgrade Parallax to Complex Material requires GPU acceleration to be enabled");
    }

    // Post-Patchers

    // Mesh Rules
    checkSet.clear();
    for (const auto& item : params.MeshRules.allowList) {
        if (item.empty()) {
            errors.emplace_back("Empty entry in Mesh Allow List");
        }

        if (!checkSet.insert(item).second) {
            errors.emplace_back("Duplicate entry in Mesh Allow List: " + UTF16toUTF8(item));
        }
    }

    checkSet.clear();
    for (const auto& item : params.MeshRules.blockList) {
        if (item.empty()) {
            errors.emplace_back("Empty entry in Mesh Block List");
        }

        if (!checkSet.insert(item).second) {
            errors.emplace_back("Duplicate entry in Mesh Block List: " + UTF16toUTF8(item));
        }
    }

    // Texture Rules

    checkSet.clear();
    for (const auto& [key, value] : params.TextureRules.textureMaps) {
        if (key.empty()) {
            errors.emplace_back("Empty key in Texture Rules");
        }

        if (!checkSet.insert(key).second) {
            errors.emplace_back("Duplicate entry in Texture Rules: " + UTF16toUTF8(key));
        }
    }

    checkSet.clear();
    for (const auto& item : params.TextureRules.vanillaBSAList) {
        if (item.empty()) {
            errors.emplace_back("Empty entry in Vanilla BSA List");
        }

        if (!checkSet.insert(item).second) {
            errors.emplace_back("Duplicate entry in Vanilla BSA List: " + UTF16toUTF8(item));
        }
    }

    return errors.empty();
}

void ParallaxGenConfig::saveUserConfig()
{
    // build output json
    nlohmann::json j = m_userConfig;

    // Mod Order
    j["mod_order"] = utf16VectorToUTF8(m_modOrder);

    // Params

    // "game"
    j["params"]["game"]["dir"] = UTF16toUTF8(m_params.Game.dir.wstring());
    j["params"]["game"]["type"] = m_params.Game.type;

    // "modmanager"
    j["params"]["modmanager"]["type"] = m_params.ModManager.type;
    j["params"]["modmanager"]["mo2instancedir"] = UTF16toUTF8(m_params.ModManager.mo2InstanceDir.wstring());
    j["params"]["modmanager"]["mo2profile"] = UTF16toUTF8(m_params.ModManager.mo2Profile);
    j["params"]["modmanager"]["mo2useorder"] = m_params.ModManager.mo2UseOrder;

    // "output"
    j["params"]["output"]["dir"] = UTF16toUTF8(m_params.Output.dir.wstring());
    j["params"]["output"]["zip"] = m_params.Output.zip;

    // "advanced"
    j["params"]["advanced"] = m_params.advanced;

    // "processing"
    j["params"]["processing"]["multithread"] = m_params.Processing.multithread;
    j["params"]["processing"]["highmem"] = m_params.Processing.highMem;
    j["params"]["processing"]["gpuacceleration"] = m_params.Processing.gpuAcceleration;
    j["params"]["processing"]["bsa"] = m_params.Processing.bsa;
    j["params"]["processing"]["pluginpatching"] = m_params.Processing.pluginPatching;
    j["params"]["processing"]["pluginesmify"] = m_params.Processing.pluginESMify;
    j["params"]["processing"]["mapfrommeshes"] = m_params.Processing.mapFromMeshes;

    // "prepatcher"
    j["params"]["prepatcher"]["disablemlp"] = m_params.PrePatcher.disableMLP;

    // "shaderpatcher"
    j["params"]["shaderpatcher"]["parallax"] = m_params.ShaderPatcher.parallax;
    j["params"]["shaderpatcher"]["complexmaterial"] = m_params.ShaderPatcher.complexMaterial;
    j["params"]["shaderpatcher"]["complexmaterialdyncubemapblocklist"]
        = utf16VectorToUTF8(m_params.ShaderPatcher.ShaderPatcherComplexMaterial.listsDyncubemapBlocklist);
    j["params"]["shaderpatcher"]["truepbr"] = m_params.ShaderPatcher.truePBR;

    // "shadertransforms"
    j["params"]["shadertransforms"]["parallaxtocm"] = m_params.ShaderTransforms.parallaxToCM;

    // "postpatcher"
    j["params"]["postpatcher"]["optimizemeshes"] = m_params.PostPatcher.optimizeMeshes;

    // "meshrules"
    j["params"]["meshrules"]["allowlist"] = utf16VectorToUTF8(m_params.MeshRules.allowList);
    j["params"]["meshrules"]["blocklist"] = utf16VectorToUTF8(m_params.MeshRules.blockList);

    // "texturerules"
    j["params"]["texturerules"]["texturemaps"] = nlohmann::json::object();
    for (const auto& [Key, Value] : m_params.TextureRules.textureMaps) {
        j["params"]["texturerules"]["texturemaps"][UTF16toUTF8(Key)] = NIFUtil::getStrFromTexType(Value);
    }
    j["params"]["texturerules"]["vanillabsalist"] = utf16VectorToUTF8(m_params.TextureRules.vanillaBSAList);

    // Update UserConfig var
    m_userConfig = j;

    // write to file
    try {
        ofstream outFile(getUserConfigFile());
        outFile << j.dump(2) << endl;
        outFile.close();
    } catch (const exception& e) {
        spdlog::error("Failed to save user config: {}", e.what());
    }
}

auto ParallaxGenConfig::PGParams::getString() const -> wstring
{
    wstring outStr;
    outStr += L"GameDir: " + Game.dir.wstring() + L"\n";
    outStr += L"GameType: " + UTF8toUTF16(BethesdaGame::getStrFromGameType(Game.type)) + L"\n";
    outStr += L"ModManagerType: " + UTF8toUTF16(ModManagerDirectory::getStrFromModManagerType(ModManager.type)) + L"\n";
    outStr += L"MO2InstanceDir: " + ModManager.mo2InstanceDir.wstring() + L"\n";
    outStr += L"MO2Profile: " + ModManager.mo2Profile + L"\n";
    outStr += L"OutputDir: " + Output.dir.wstring() + L"\n";
    outStr += L"ZipOutput: " + to_wstring(static_cast<int>(Output.zip)) + L"\n";
    outStr += L"Multithread: " + to_wstring(static_cast<int>(Processing.multithread)) + L"\n";
    outStr += L"HighMem: " + to_wstring(static_cast<int>(Processing.highMem)) + L"\n";
    outStr += L"GPUAcceleration: " + to_wstring(static_cast<int>(Processing.gpuAcceleration)) + L"\n";
    outStr += L"BSA: " + to_wstring(static_cast<int>(Processing.bsa)) + L"\n";
    outStr += L"PluginPatching: " + to_wstring(static_cast<int>(Processing.pluginPatching)) + L"\n";
    outStr += L"MapFromMeshes: " + to_wstring(static_cast<int>(Processing.mapFromMeshes)) + L"\n";
    outStr += L"DisableMLP: " + to_wstring(static_cast<int>(PrePatcher.disableMLP)) + L"\n";
    outStr += L"Parallax: " + to_wstring(static_cast<int>(ShaderPatcher.parallax)) + L"\n";
    outStr += L"ComplexMaterial: " + to_wstring(static_cast<int>(ShaderPatcher.complexMaterial)) + L"\n";
    outStr += L"TruePBR: " + to_wstring(static_cast<int>(ShaderPatcher.truePBR)) + L"\n";
    outStr += L"ParallaxToCM: " + to_wstring(static_cast<int>(ShaderTransforms.parallaxToCM)) + L"\n";
    outStr += L"OptimizeMeshes: " + to_wstring(static_cast<int>(PostPatcher.optimizeMeshes));

    return outStr;
}
