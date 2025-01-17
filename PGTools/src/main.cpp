#include <CLI/CLI.hpp>

#include <spdlog/spdlog.h>

#include <windows.h>

#include <cpptrace/from_current.hpp>

#include <string>
#include <unordered_set>

#include "ParallaxGen.hpp"
#include "ParallaxGenD3D.hpp"
#include "ParallaxGenDirectory.hpp"
#include "ParallaxGenRunner.hpp"
#include "ParallaxGenWarnings.hpp"

#include "patchers/Patcher.hpp"
#include "patchers/PatcherComplexMaterial.hpp"
#include "patchers/PatcherParticleLightsToLP.hpp"
#include "patchers/PatcherTruePBR.hpp"
#include "patchers/PatcherUpgradeParallaxToCM.hpp"
#include "patchers/PatcherVanillaParallax.hpp"

using namespace std;

namespace {
auto getExecutablePath() -> filesystem::path
{
    array<wchar_t, MAX_PATH> buffer {};
    if (GetModuleFileNameW(nullptr, buffer.data(), MAX_PATH) == 0) {
        cerr << "Error getting executable path: " << GetLastError() << "\n";
        exit(1);
    }

    filesystem::path outPath = filesystem::path(buffer.data());

    if (filesystem::exists(outPath)) {
        return outPath;
    }

    cerr << "Error getting executable path: path does not exist\n";
    exit(1);

    return {};
}

struct PGToolsCLIArgs {
    int verbosity = 0;
    bool multithreading = true;
    bool gpuAcceleration = true;

    struct Patch {
        CLI::App* subCommand = nullptr;
        unordered_set<string> patchers;
        filesystem::path source = ".";
        filesystem::path output = "ParallaxGen_Output";
        bool mapTexturesFromMeshes = false;
        bool highMem = false;
    } Patch;
};

void mainRunner(PGToolsCLIArgs& args)
{
    // Welcome Message
    spdlog::info("Welcome to PGTools version {}!", PARALLAXGEN_VERSION);

    // Get EXE path
    const auto exePath = getExecutablePath().parent_path();

    // Test message if required
    if (PARALLAXGEN_TEST_VERSION > 0) {
        spdlog::warn("This is an EXPERIMENTAL development build of ParallaxGen: {} Test Build {}", PARALLAXGEN_VERSION,
            PARALLAXGEN_TEST_VERSION);
    }

    // Check if patch subcommand was used
    if (args.Patch.subCommand->parsed()) {
        // Get current time to compare later
        auto startTime = chrono::high_resolution_clock::now();
        long long timeTaken = 0;

        args.Patch.source = filesystem::absolute(args.Patch.source);
        args.Patch.output = filesystem::absolute(args.Patch.output);

        auto pgd = ParallaxGenDirectory(args.Patch.source, args.Patch.output, nullptr);
        auto pgd3D = ParallaxGenD3D(&pgd, args.Patch.output, exePath, args.gpuAcceleration);
        auto pg = ParallaxGen(args.Patch.output, &pgd, &pgd3D, args.Patch.patchers.contains("optimize"));

        Patcher::loadStatics(pgd, pgd3D);
        ParallaxGenWarnings::init(&pgd, {});

        // Check if GPU needs to be initialized
        if (args.gpuAcceleration) {
            pgd3D.initGPU();
        }

        // Create output directory
        try {
            filesystem::create_directories(args.Patch.output);
        } catch (const filesystem::filesystem_error& e) {
            spdlog::error("Failed to create output directory: {}", e.what());
            exit(1);
        }

        // If output dir is the same as data dir meshes might get overwritten
        if (filesystem::equivalent(args.Patch.output, pgd.getDataPath())) {
            spdlog::critical("Output directory cannot be the same directory as your data folder. "
                             "Exiting.");
            exit(1);
        }

        // delete existing output
        pg.deleteOutputDir();

        // Init file map
        pgd.populateFileMap(false);

        // Map files
        pgd.mapFiles({}, {}, {}, {}, args.Patch.mapTexturesFromMeshes, args.multithreading, args.Patch.highMem);

        // Split patchers into names and options
        unordered_map<string, unordered_set<string>> patcherDefs;
        for (const auto& patcher : args.Patch.patchers) {
            auto openBracket = patcher.find('[');
            auto closeBracket = patcher.find(']');
            if (openBracket == string::npos || closeBracket == string::npos) {
                patcherDefs[patcher] = {};
                continue;
            }

            // Get substring between brackets
            auto options = patcher.substr(openBracket + 1, closeBracket - openBracket - 1);
            // Split options by | into unordered set
            unordered_set<string> optionSet;
            for (const auto& option : options | views::split('|')) {
                optionSet.insert(string(option.begin(), option.end()));
            }

            // Add to set
            patcherDefs[patcher.substr(0, openBracket)] = optionSet;
        }

        if (patcherDefs.contains("complexmaterial")) {
            // Find CM maps
            spdlog::info("Finding complex material env maps");
            pgd3D.findCMMaps({});
            spdlog::info("Done finding complex material env maps");
        }

        // Create patcher factory
        PatcherUtil::PatcherSet patchers;
        if (patcherDefs.contains("parallax")) {
            patchers.shaderPatchers.emplace(
                PatcherVanillaParallax::getShaderType(), PatcherVanillaParallax::getFactory());
        }
        if (patcherDefs.contains("complexmaterial")) {
            patchers.shaderPatchers.emplace(
                PatcherComplexMaterial::getShaderType(), PatcherComplexMaterial::getFactory());
            PatcherComplexMaterial::loadStatics(args.Patch.patchers.contains("disablemlp"), {});
        }
        if (patcherDefs.contains("truepbr")) {
            patchers.shaderPatchers.emplace(PatcherTruePBR::getShaderType(), PatcherTruePBR::getFactory());
            PatcherTruePBR::loadStatics(pgd.getPBRJSONs());
            PatcherTruePBR::loadOptions(patcherDefs["truepbr"]);
        }
        if (patcherDefs.contains("parallaxtocm")) {
            patchers.shaderTransformPatchers[PatcherUpgradeParallaxToCM::getFromShader()].emplace(
                PatcherUpgradeParallaxToCM::getToShader(), PatcherUpgradeParallaxToCM::getFactory());
        }
        if (patcherDefs.contains("particlelightstolp")) {
            patchers.globalPatchers.emplace_back(PatcherParticleLightsToLP::getFactory());
        }

        pg.loadPatchers(patchers);
        pg.patchMeshes(args.multithreading, false);

        // Finalize step
        if (patcherDefs.contains("particlelightstolp")) {
            PatcherParticleLightsToLP::finalize();
        }

        // Release cached files, if any
        pgd.clearCache();

        // Check if dynamic cubemap file is needed
        if (args.Patch.patchers.contains("complexmaterial")) {
            // Install default cubemap file if needed
            static const filesystem::path dynCubeMapPath = "textures/cubemaps/dynamic1pxcubemap_black.dds";

            spdlog::info("Installing default dynamic cubemap file");

            // Create Directory
            const filesystem::path outputCubemapPath = args.Patch.output / dynCubeMapPath.parent_path();
            filesystem::create_directories(outputCubemapPath);

            const filesystem::path assetPath = filesystem::path(exePath) / "assets/dynamic1pxcubemap_black_ENB.dds";
            const filesystem::path outputPath = filesystem::path(args.Patch.output) / dynCubeMapPath;

            // Move File
            filesystem::copy_file(assetPath, outputPath, filesystem::copy_options::overwrite_existing);
        }

        const auto endTime = chrono::high_resolution_clock::now();
        timeTaken += chrono::duration_cast<chrono::seconds>(endTime - startTime).count();

        spdlog::info("ParallaxGen took {} seconds to complete", timeTaken);
    }
}

void addArguments(CLI::App& app, PGToolsCLIArgs& args)
{
    // Logging
    app.add_flag("-v", args.verbosity,
        "Verbosity level -v for DEBUG data or -vv for TRACE data "
        "(warning: TRACE data is very verbose)");
    app.add_flag("--no-multithreading", args.multithreading, "Disable multithreading");
    app.add_flag("--no-gpu-acceleration", args.gpuAcceleration, "Disable GPU acceleration");

    args.Patch.subCommand = app.add_subcommand("patch", "Patch meshes");
    args.Patch.subCommand->add_option("patcher", args.Patch.patchers, "List of patchers to use")
        ->required()
        ->delimiter(',');
    args.Patch.subCommand->add_option("source", args.Patch.source, "Source directory")->default_str("");
    args.Patch.subCommand->add_option("output", args.Patch.output, "Output directory")
        ->default_str("ParallaxGen_Output");
    args.Patch.subCommand->add_flag(
        "--map-textures-from-meshes", args.Patch.mapTexturesFromMeshes, "Map textures from meshes (default: false)");
    args.Patch.subCommand->add_flag("--high-mem", args.Patch.highMem, "High memory usage mode (default: false)");
}
}

auto main(int argC, char** argV) -> int
{
// Block until enter only in debug mode
#ifdef _DEBUG
    cout << "Press ENTER to start (DEBUG mode)...";
    cin.get();
#endif

    SetConsoleOutputCP(CP_UTF8);

    // CLI Arguments
    PGToolsCLIArgs args;
    CLI::App app { "PGTools: A collection of tools for ParallaxGen" };
    addArguments(app, args);

    // Parse CLI Arguments (this is what exits on any validation issues)
    CLI11_PARSE(app, argC, argV);

    // Initialize Logger
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");

    // Set logging mode
    if (args.verbosity >= 1) {
        spdlog::set_level(spdlog::level::debug);
        spdlog::debug("DEBUG logging enabled");
    }

    if (args.verbosity >= 2) {
        spdlog::set_level(spdlog::level::trace);
        spdlog::trace("TRACE logging enabled");
    }

    // Main Runner (Catches all exceptions)
    CPPTRACE_TRY { mainRunner(args); }
    CPPTRACE_CATCH(const exception& e)
    {
        ParallaxGenRunner::processException(e, cpptrace::from_current_exception().to_string());

        cout << "Press ENTER to abort...";
        cin.get();
        abort();
    }
}
