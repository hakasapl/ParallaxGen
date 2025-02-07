#include "CommonTests.hpp"
#include <array>

using namespace std;

auto PGTesting::getExecutableDir() -> filesystem::path
{
    array<wchar_t, MAX_PATH> buffer = {};
    if (GetModuleFileNameW(nullptr, buffer.data(), MAX_PATH) == 0) {
        cerr << "Error getting executable path: " << GetLastError() << "\n";
        exit(1);
    }

    const filesystem::path outPath = filesystem::path(buffer.data());

    if (filesystem::exists(outPath)) {
        return outPath.parent_path();
    }

    cerr << "Error getting executable path: path does not exist\n";
    exit(1);

    return {};
}
