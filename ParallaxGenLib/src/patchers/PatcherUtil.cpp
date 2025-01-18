#include "patchers/PatcherUtil.hpp"

#include "Logger.hpp"

using namespace std;

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

auto PatcherUtil::applyTransformIfNeeded(const ShaderPatcherMatch& match, const PatcherObjectSet& patchers)
    -> ShaderPatcherMatch
{
    auto transformedMatch = match;

    // Transform if required
    if (match.shaderTransformTo != NIFUtil::ShapeShader::NONE) {
        // Find transform object
        auto* const transform = patchers.shaderTransformPatchers.at(match.shader).at(match.shaderTransformTo).get();

        // Transform Shader
        if (transform->transform(match.match, transformedMatch.match)) {
            // Reset Transform
            transformedMatch.shader = match.shaderTransformTo;
        }

        transformedMatch.shaderTransformTo = NIFUtil::ShapeShader::NONE;
    }

    return transformedMatch;
}
