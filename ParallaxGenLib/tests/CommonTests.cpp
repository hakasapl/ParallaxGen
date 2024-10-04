#include "CommonTests.hpp"

using namespace std;

auto PGTesting::getExecutableDir() -> filesystem::path {
  char Buffer[MAX_PATH];                                   // NOLINT
  if (GetModuleFileName(nullptr, Buffer, MAX_PATH) == 0) { // NOLINT
    cerr << "Error getting executable path: " << GetLastError() << "\n";
    exit(1);
  }

  filesystem::path OutPath = filesystem::path(Buffer);

  if (filesystem::exists(OutPath)) {
    return OutPath.parent_path();
  }

  cerr << "Error getting executable path: path does not exist\n";
  exit(1);

  return {};
}
