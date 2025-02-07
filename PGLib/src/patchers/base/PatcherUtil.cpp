#include "patchers/base/PatcherUtil.hpp"

#include "Logger.hpp"
#include "NIFUtil.hpp"

using namespace std;

// TODO these methods should probably move into shader and transform classes respectively
auto PatcherUtil::getWinningMatch(
    const vector<ShaderPatcherMatch>& matches, const unordered_map<wstring, int>* modPriority) -> ShaderPatcherMatch
{
    // Find winning mod
    int maxPriority = -1;
    auto winningShaderMatch = PatcherUtil::ShaderPatcherMatch();

    for (const auto& match : matches) {
        const Logger::Prefix prefixMod(match.mod);
        Logger::trace(L"Checking mod");

        int curPriority = -1;
        if (modPriority != nullptr && modPriority->find(match.mod) != modPriority->end()) {
            curPriority = modPriority->at(match.mod);
        }

        if (curPriority < maxPriority) {
            // skip mods with lower priority than current winner
            Logger::trace(L"Rejecting: Mod has lower priority than current winner");
            continue;
        }

        Logger::trace(L"Mod accepted");
        maxPriority = curPriority;
        winningShaderMatch = match;
    }

    Logger::trace(L"Winning mod: {}", winningShaderMatch.mod);
    return winningShaderMatch;
}

auto PatcherUtil::applyTransformIfNeeded(ShaderPatcherMatch& match, const PatcherMeshObjectSet& patchers) -> bool
{
    // Transform if required
    if (match.shaderTransformTo != NIFUtil::ShapeShader::UNKNOWN) {
        // Find transform object
        auto* const transform = patchers.shaderTransformPatchers.at(match.shader).at(match.shaderTransformTo).get();

        // Transform Shader
        if (transform->transform(match.match, match.match)) {
            // Reset Transform
            match.shader = match.shaderTransformTo;
        }

        match.shaderTransformTo = NIFUtil::ShapeShader::UNKNOWN;

        return true;
    }

    return false;
}
