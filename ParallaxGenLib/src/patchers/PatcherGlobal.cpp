#include "patchers/PatcherGlobal.hpp"

using namespace std;

PatcherGlobal::PatcherGlobal(std::filesystem::path NIFPath, nifly::NifFile *NIF, std::string PatcherName)
    : Patcher(std::move(NIFPath), NIF, std::move(PatcherName)) {}
