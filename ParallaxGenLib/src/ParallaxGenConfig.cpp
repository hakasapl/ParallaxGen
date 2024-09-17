#include "ParallaxGenConfig.hpp"

#include <filesystem>
#include <nlohmann/json-schema.hpp>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <string>

#include "ParallaxGenUtil.hpp"

using namespace std;
using namespace ParallaxGenUtil;

ParallaxGenConfig::ParallaxGenConfig(ParallaxGenDirectory *PGD, std::filesystem::path ExePath)
    : PGD(PGD), ExePath(std::move(ExePath)) {}

auto ParallaxGenConfig::getConfigValidation() -> nlohmann::json {
  static nlohmann::json PGConfigSchema = R"(
  {
    "$schema": "http://json-schema.org/draft-07/schema#",
    "title": "parallaxgen-cfg",
    "type": "object",
    "properties": {
      "parallax_lookup": {
        "type": "object",
        "properties": {
          "allowlist": {
            "type": "array",
            "items": {
              "type": "string",
              "pattern": ".*\\.dds$"
            }
          },
          "blocklist": {
            "type": "array",
            "items": {
              "type": "string"
            }
          },
          "archive_blocklist": {
            "type": "array",
            "items": {
              "type": "string",
              "pattern": "^[^\\/\\\\]*$"
            }
          }
        },
        "required": [
          "allowlist",
          "blocklist",
          "archive_blocklist"
        ]
      },
      "parallax_processing": {
        "type": "object",
        "properties": {
          "lookup_order": {
            "type": "array",
            "items": {
              "type": "integer",
              "minimum": 0,
              "maximum": 8
            }
          }
        },
        "required": [
          "lookup_order"
        ]
      },
      "complexmaterial_lookup": {
        "type": "object",
        "properties": {
          "allowlist": {
            "type": "array",
            "items": {
              "type": "string",
              "pattern": ".*\\.dds$"
            }
          },
          "blocklist": {
            "type": "array",
            "items": {
              "type": "string"
            }
          },
          "archive_blocklist": {
            "type": "array",
            "items": {
              "type": "string",
              "pattern": "^[^\\/\\\\]*$"
            }
          }
        },
        "required": [
          "allowlist",
          "blocklist",
          "archive_blocklist"
        ]
      },
      "complexmaterial_processing": {
        "type": "object",
        "properties": {
          "lookup_order": {
            "type": "array",
            "items": {
              "type": "integer",
              "minimum": 0,
              "maximum": 8
            }
          },
          "dyncubemap_blocklist": {
            "type": "array",
            "items": {
              "type": "string"
            }
          }
        },
        "required": [
          "lookup_order",
          "dyncubemap_blocklist"
        ]
      },
      "truepbr_cfg_lookup": {
        "type": "object",
        "properties": {
          "allowlist": {
            "type": "array",
            "items": {
              "type": "string",
              "pattern": ".*\\.json$"
            }
          },
          "blocklist": {
            "type": "array",
            "items": {
              "type": "string"
            }
          },
          "archive_blocklist": {
            "type": "array",
            "items": {
              "type": "string",
              "pattern": "^[^\\/\\\\]*$"
            }
          }
        },
        "required": [
          "allowlist",
          "blocklist",
          "archive_blocklist"
        ]
      },
      "truepbr_cfg_processing": {
        "type": "object",
        "properties": {
          "lookup_order": {
            "type": "array",
            "items": {
              "type": "integer",
              "minimum": 0,
              "maximum": 8
            }
          }
        },
        "required": [
          "lookup_order"
        ]
      },
      "nif_lookup": {
        "type": "object",
        "properties": {
          "allowlist": {
            "type": "array",
            "items": {
              "type": "string",
              "pattern": ".*\\.nif$"
            }
          },
          "blocklist": {
            "type": "array",
            "items": {
              "type": "string"
            }
          },
          "archive_blocklist": {
            "type": "array",
            "items": {
              "type": "string",
              "pattern": "^[^\\/\\\\]*$"
            }
          }
        },
        "required": [
          "allowlist",
          "blocklist",
          "archive_blocklist"
        ]
      },
      "suffixes": {
        "type": "array",
        "items": {
          "type": "array",
          "items": {
            "type": "string"
          }
        }
      }
    },
    "required": [
      "parallax_lookup",
      "parallax_processing",
      "complexmaterial_lookup",
      "complexmaterial_processing",
      "truepbr_cfg_lookup",
      "truepbr_cfg_processing",
      "nif_lookup",
      "suffixes"
    ]
  }
  )"_json;

  return PGConfigSchema;
}

void ParallaxGenConfig::loadConfig(const bool &LoadNative) {
  size_t NumConfigs = 0;

  if (LoadNative) {
    filesystem::path DefConfPath = ExePath / "cfg";
    // Loop through all files in DefConfPath recursively directoryiterator
    for (const auto &Entry : filesystem::recursive_directory_iterator(DefConfPath)) {
      if (filesystem::is_regular_file(Entry) && Entry.path().filename().extension() == ".json") {
        spdlog::debug(L"Loading ParallaxGen Config: {}", Entry.path().wstring());
        try {
          const auto J = nlohmann::json::parse(getFileBytes(Entry.path()));
          mergeJSONSmart(PGConfig, J);
          NumConfigs++;
        } catch (const nlohmann::json::parse_error &E) {
          spdlog::error(L"Error parsing ParallaxGen Config File {} (Skipping "
                        L"this Config): {}",
                        Entry.path().wstring(), stringToWstring(E.what()));
        }
      }
    }
  }

  // loop through PGConfigPaths (in load order)
  for (const auto &PGConfigFile : PGD->getPGJSONs()) {
    spdlog::debug(L"Loading ParallaxGen Config: {}", PGConfigFile.wstring());
    try {
      const auto J = nlohmann::json::parse(PGD->getFile(PGConfigFile));
      mergeJSONSmart(PGConfig, J);
      NumConfigs++;
    } catch (const nlohmann::json::parse_error &E) {
      spdlog::error(L"Error parsing ParallaxGen Config File {} (Skipping this "
                    L"Config): {}",
                    PGConfigFile.wstring(), stringToWstring(E.what()));
    }
  }

  // Replace any forward slashes with backslashes
  replaceForwardSlashes(PGConfig);

  // Validate the config
  if (!validateConfig()) {
    exit(1);
  }

  // Initialize Member Variables
  for (const auto &JSON : PGConfig["suffixes"].items()) {
    CfgSuffixes.push_back(JSON.value().get<vector<string>>());
  }

  // Print number of configs loaded
  spdlog::info("Loaded {} ParallaxGen configs successfully", NumConfigs);
}

auto ParallaxGenConfig::getConfig() const -> const nlohmann::json & { return PGConfig; }

auto ParallaxGenConfig::getSuffix(const unsigned int &Slot) const -> vector<string> {
  if (Slot >= CfgSuffixes.size()) {
    return {};
  }

  return CfgSuffixes[Slot];
}

auto ParallaxGenConfig::validateConfig() -> bool {
  // Validate PGConfig against schema
  static nlohmann::json_schema::json_validator Validator;
  try {
    Validator.set_root_schema(getConfigValidation());
  } catch (const std::exception &E) {
    spdlog::critical("Unable to validate JSON validation: {}", E.what());
    return false;
  }

  // Validate
  try {
    Validator.validate(PGConfig);
  } catch (const std::exception &E) {
    spdlog::critical("Error validating ParallaxGen config: {}", E.what());
    return false;
  }

  return true;
}

void ParallaxGenConfig::mergeJSONSmart(nlohmann::json &Target, const nlohmann::json &Source) {
  // recursively merge json objects while preseving lists
  for (const auto &[Key, Value] : Source.items()) {
    if (Value.is_object()) {
      // This is a JSON object
      if (!Target.contains(Key)) {
        // Create an object if it doesn't exist in the Target
        Target[Key] = nlohmann::json::object();
      }

      mergeJSONSmart(Target[Key], Value);
    } else if (Value.is_array()) {
      // This is a list

      if (!Target.contains(Key)) {
        // Create an array if it doesn't exist in the Target
        Target[Key] = nlohmann::json::array();
      }

      // Loop through each item in array and add only if it doesn't already
      // exist in the list
      for (const auto &Item : Value) {
        if (std::find(Target[Key].begin(), Target[Key].end(), Item) == Target[Key].end()) {
          // Add item to Target
          Target[Key].push_back(Item);
        }
      }
    } else {
      // This is a single object
      // Add item to Target
      Target[Key] = Value;
    }
  }
}

void ParallaxGenConfig::replaceForwardSlashes(nlohmann::json &JSON) {
  if (JSON.is_string()) {
    auto &Str = JSON.get_ref<string &>();
    for (auto &Ch : Str) {
      if (Ch == '/') {
        Ch = '\\';
      }
    }
  } else if (JSON.is_object()) {
    for (const auto &Item : JSON.items()) {
      replaceForwardSlashes(Item.value());
    }
  } else if (JSON.is_array()) {
    for (auto &Element : JSON) {
      replaceForwardSlashes(Element);
    }
  }
}
