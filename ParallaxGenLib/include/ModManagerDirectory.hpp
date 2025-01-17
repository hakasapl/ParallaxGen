#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <unordered_set>

class ModManagerDirectory {

public:
    enum class ModManagerType { None, Vortex, ModOrganizer2 };

private:
    std::unordered_map<std::filesystem::path, std::wstring> m_modFileMap;
    std::unordered_set<std::wstring> m_allMods;
    std::vector<std::wstring> m_inferredOrder;

    ModManagerType m_mmType;

    static constexpr const char* MO2INI_PROFILESDIR_KEY = "profiles_directory=";
    static constexpr const char* MO2INI_MODDIR_KEY = "mod_directory=";
    static constexpr const char* MO2INI_BASEDIR_KEY = "base_directory=";

public:
    ModManagerDirectory(const ModManagerType& mmType);

    [[nodiscard]] auto getModFileMap() const -> const std::unordered_map<std::filesystem::path, std::wstring>&;
    [[nodiscard]] auto getMod(const std::filesystem::path& relPath) const -> std::wstring;
    [[nodiscard]] auto getInferredOrder() const -> const std::vector<std::wstring>&;

    static auto getMO2ProfilesFromInstanceDir(const std::filesystem::path& instanceDir) -> std::vector<std::wstring>;

    void populateModFileMapMO2(
        const std::filesystem::path& instanceDir, const std::wstring& profile, const std::filesystem::path& outputDir);
    void populateModFileMapVortex(const std::filesystem::path& deploymentDir);

    // Helpers
    [[nodiscard]] static auto getModManagerTypes() -> std::vector<ModManagerType>;
    [[nodiscard]] static auto getStrFromModManagerType(const ModManagerType& type) -> std::string;
    [[nodiscard]] static auto getModManagerTypeFromStr(const std::string& type) -> ModManagerType;
};
