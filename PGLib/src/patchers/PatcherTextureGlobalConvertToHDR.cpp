#include "patchers/PatcherTextureGlobalConvertToHDR.hpp"
#include "ParallaxGenD3D.hpp"

#include <DirectXTex.h>

using namespace std;

auto PatcherTextureGlobalConvertToHDR::getFactory() -> PatcherTextureGlobal::PatcherGlobalFactory
{
    return [](const std::filesystem::path& ddsPath, DirectX::ScratchImage* dds) {
        return std::make_unique<PatcherTextureGlobalConvertToHDR>(ddsPath, dds);
    };
}

void PatcherTextureGlobalConvertToHDR::loadOptions(const unordered_map<string, string>& optionsStr)
{
    for (const auto& [option, value] : optionsStr) {
        if (option == "luminance_mult") {
            s_luminanceMult = stof(value);
        }

        if (option == "output_format") {
            s_outputFormat = ParallaxGenD3D::getDXGIFormatFromString(value);
        }
    }
}

PatcherTextureGlobalConvertToHDR::PatcherTextureGlobalConvertToHDR(
    std::filesystem::path ddsPath, DirectX::ScratchImage* dds)
    : PatcherTextureGlobal(std::move(ddsPath), dds, "ConvertToHDR")
{
}

void PatcherTextureGlobalConvertToHDR::applyPatch(bool& ddsModified)
{
    getPGD3D()->convertToHDR(getDDS(), ddsModified, s_luminanceMult, s_outputFormat);
}
