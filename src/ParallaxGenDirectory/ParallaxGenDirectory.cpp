#include "ParallaxGenDirectory/ParallaxGenDirectory.hpp"

using namespace std;
namespace fs = filesystem;

ParallaxGenDirectory::ParallaxGenDirectory(BethesdaGame bg) : BethesdaDirectory(bg, true) {
	// contructor

}

vector<fs::path> ParallaxGenDirectory::findHeightMaps() const {
	// find height maps
	return vector<fs::path>();
}

vector<fs::path> ParallaxGenDirectory::findComplexMaterialMaps() const {
	// find complex material maps
	return vector<fs::path>();
}

vector<fs::path> ParallaxGenDirectory::findMeshes() const {
	// find meshes
	return vector<fs::path>();
}
