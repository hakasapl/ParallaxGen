#include <CLI/CLI.hpp>

#include <spdlog/spdlog.h>

#include <windows.h>

#include <string>
#include <vector>

using namespace std;

struct PGToolsCLIArgs {
  int Verbosity = 0;

  struct Patch {
    bool Enabled = false;
    vector<string> Patchers;
  } Patch;
};

void mainRunner(PGToolsCLIArgs &Args) {
  // Welcome Message
  spdlog::info("Welcome to PGTools version {}!", PARALLAXGEN_VERSION);

  // Test message if required
  if (PARALLAXGEN_TEST_VERSION > 0) {
    spdlog::warn("This is an EXPERIMENTAL development build of ParallaxGen: {} Test Build {}", PARALLAXGEN_VERSION,
                 PARALLAXGEN_TEST_VERSION);
  }
}

void addArguments(CLI::App &App, PGToolsCLIArgs &Args) {
  // Logging
  App.add_flag("-v", Args.Verbosity,
               "Verbosity level -v for DEBUG data or -vv for TRACE data "
               "(warning: TRACE data is very verbose)");

  auto *SubCliPatch = App.add_subcommand("patch", "Patch meshes");
  SubCliPatch->add_option("patcher", Args.Patch.Patchers, "List of patchers to use")->required()->delimiter(',');
}

auto main(int ArgC, char **ArgV) -> int {
// Block until enter only in debug mode
#ifdef _DEBUG
  cout << "Press ENTER to start (DEBUG mode)...";
  cin.get();
#endif

  SetConsoleOutputCP(CP_UTF8);

  // CLI Arguments
  PGToolsCLIArgs Args;
  CLI::App App{"PGTools: A collection of tools for ParallaxGen"};
  addArguments(App, Args);

  // Parse CLI Arguments (this is what exits on any validation issues)
  CLI11_PARSE(App, ArgC, ArgV);

  // Initialize Logger
  spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] %v");

  // Set logging mode
  if (Args.Verbosity >= 1) {
    spdlog::set_level(spdlog::level::debug);
    spdlog::debug("DEBUG logging enabled");
  }

  if (Args.Verbosity >= 2) {
    spdlog::set_level(spdlog::level::trace);
    spdlog::trace("TRACE logging enabled");
  }

  // Main Runner (Catches all exceptions)
  try {
    mainRunner(Args);
  } catch (const exception &E) {
    spdlog::critical("An unhandled exception occurred (Please provide this entire message "
                     "in your bug report).\n\nException type: {}\nMessage: {}",
                     typeid(E).name(), E.what());
    abort();
  }
}
