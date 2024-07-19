#include "BethesdaDirectory/BethesdaDirectory.hpp"

#include <spdlog/spdlog.h>
#include <iostream>
#include <fstream>
#include <boost/algorithm/string.hpp>
#include <binary_io/binary_io.hpp>
#include <boost/iostreams/device/mapped_file.hpp>
#include <boost/filesystem.hpp>

// BSA Includes
#include <cstdio>

#include "ParallaxGenUtil/ParallaxGenUtil.hpp"

using namespace std;
using namespace ParallaxGenUtil;
namespace fs = filesystem;

BethesdaDirectory::BethesdaDirectory(BethesdaGame& bg, const bool logging) : bg(bg)
{
	this->logging = logging;
	this->bg = bg;

	// Assign instance vars
	data_dir = fs::path(this->bg.getGameDataPath());

	if(this->logging) {
		// Log starting message
		spdlog::info(L"Opening Data Folder \"{}\"", data_dir.wstring());
	}
}

void BethesdaDirectory::populateFileMap()
{
	// clear map before populating
	fileMap.clear();

	// add BSA files to file map
	addBSAFilesToMap();

	// add loose files to file map
	addLooseFilesToMap();
}

map<fs::path, BethesdaDirectory::BethesdaFile> BethesdaDirectory::getFileMap() const
{
	return fileMap;
}

vector<std::byte> BethesdaDirectory::getFile(const fs::path rel_path) const
{
	// find bsa/loose file to open
	BethesdaFile file = getFileFromMap(rel_path);
	if (file.path.empty()) {
		if (logging) {
			spdlog::error(L"File not found in file map: {}", rel_path.wstring());
		}
		else {
			throw runtime_error("File not found in file map");
		}
	}

	shared_ptr<BSAFile> bsa_struct = file.bsa_file;
	if (bsa_struct == nullptr) {
		if (logging) {
			spdlog::trace(L"Reading loose file from BethesdaDirectory: {}", rel_path.wstring());
		}

		fs::path file_path = data_dir / rel_path;

		return getFileBytes(file_path);
	}
	else {
		fs::path bsa_path = bsa_struct->path;

		if (logging) {
			spdlog::trace(L"Reading BSA file from {}: {}", bsa_path.wstring(), rel_path.wstring());
		}

		// this is a bsa archive file
		bsa::tes4::version bsa_version = bsa_struct->version;
		bsa::tes4::archive bsa_obj = bsa_struct->archive;

		string parent_path = wstring_to_utf8(rel_path.parent_path().wstring());
		string filename = wstring_to_utf8(rel_path.filename().wstring());

		const auto file = bsa_obj[parent_path][filename];
		if (file) {
			binary_io::any_ostream aos{ std::in_place_type<binary_io::memory_ostream> };
			// read file from output stream
			try {
				file->write(aos, bsa_version);
			}
			catch (const std::exception& e) {
				if (logging) {
					spdlog::error(L"Failed to read file {}: {}", rel_path.wstring(), convertToWstring(e.what()));
				}
			}

			auto& s = aos.get<binary_io::memory_ostream>();
			return s.rdbuf();
		}
		else {
			if (logging) {
				spdlog::error(L"File not found in BSA archive: {}", rel_path.wstring());
			}
			else {
				throw runtime_error("File not found in BSA archive");
			}
		}
	}

	return vector<std::byte>();
}

bool BethesdaDirectory::isLooseFile(const fs::path rel_path) const
{
	BethesdaFile file = getFileFromMap(rel_path);
	return !file.path.empty() && file.bsa_file == nullptr;
}

bool BethesdaDirectory::isBSAFile(const fs::path rel_path) const
{
	BethesdaFile file = getFileFromMap(rel_path);
	return !file.path.empty() && file.bsa_file != nullptr;
}

bool BethesdaDirectory::isFile(const fs::path rel_path) const
{
	BethesdaFile file = getFileFromMap(rel_path);
	return !file.path.empty();
}

fs::path BethesdaDirectory::getFullPath(const fs::path rel_path) const
{
	return data_dir / rel_path;
}

fs::path BethesdaDirectory::getDataPath() const
{
	return data_dir;
}

vector<fs::path> BethesdaDirectory::findFilesBySuffix(const string_view suffix, const bool lower, const vector<wstring>& path_blocklist) const
{
	// find all keys in fileMap that match pattern
	vector<fs::path> found_files;

	// loop through filemap and match keys
	for (const auto& [key, value] : fileMap) {
		fs::path cur_file_path = value.path;

		if (logging) {
			spdlog::trace(L"Checking file for suffix {} match: {}", convertToWstring(string(suffix)), cur_file_path.wstring());
		}

        if (boost::algorithm::ends_with(key.wstring(), suffix)) {
			// check if any component of the path is in the blocklist
			bool block = false;
			for (wstring block_path : path_blocklist) {
				for (const auto& component : key) {
					if (block_path == component.wstring()) {
						block = true;
						break;
					}
				}
				
				if (block) {
					break;
				}
			}

			if (block) {
				continue;
			}

			if (logging) {
				spdlog::trace(L"Matched file by suffix: {}", cur_file_path.wstring());
			}

			if (lower) {
				found_files.push_back(key);
			}
			else {
				found_files.push_back(cur_file_path);
			}
		}
	}

	return found_files;
}

void BethesdaDirectory::addBSAFilesToMap()
{
	if(logging) {
		spdlog::info("Adding BSA files to file map.");
	}

	// Get list of BSA files
	vector<wstring> bsa_files = getBSAPriorityList();

	// Loop through each BSA file
	for (wstring bsa_name : bsa_files) {
		// add bsa to file map
		addBSAToFileMap(bsa_name);
	}
}

void BethesdaDirectory::addLooseFilesToMap()
{
	if(logging) {
		spdlog::info("Adding loose files to file map.");
	}

	for (const auto& entry : fs::recursive_directory_iterator(data_dir)) {
		if (entry.is_regular_file()) {
			const fs::path file_path = entry.path();
			fs::path relative_path = file_path.lexically_relative(data_dir);

			// check type of file, skip BSAs and ESPs
			if (!file_allowed(file_path)) {
				continue;
			}

			if (logging) {
				spdlog::trace(L"Adding loose file to map: {}", relative_path.wstring());
			}

			updateFileMap(relative_path, nullptr);
		}
	}
}

void BethesdaDirectory::addBSAToFileMap(const wstring& bsa_name)
{
	if(logging) {
		// log message
		spdlog::debug(L"Adding files from {} to file map.", bsa_name);
	}

	bsa::tes4::archive bsa_obj;
	fs::path bsa_path = data_dir / bsa_name;

	// skip BSA if it doesn't exist (can happen if it's in the ini but not in the data folder)
	if (!fs::exists(bsa_path)) {
		if(logging) {
			spdlog::warn(L"Skipping BSA {} because it doesn't exist", bsa_path.wstring());
		}
		return;
	}

	bsa::tes4::version bsa_version = bsa_obj.read(bsa_path);
	BSAFile bsa_struct = { bsa_path, bsa_version, bsa_obj };

	shared_ptr<BSAFile> bsa_shared_ptr = make_shared<BSAFile>(bsa_struct);

	// create itereator for bsa
	auto bsa_iter = bsa_obj.begin();
	// loop iterator
	for (auto bsa_iter = bsa_obj.begin(); bsa_iter != bsa_obj.end(); ++bsa_iter) {
		// get file entry from pointer
		try {
			const auto& file_entry = *bsa_iter;

			// get folder name within the BSA vfs
			const fs::path folder_name = file_entry.first.name();

			// .second stores the files in the folder
			const auto file_name = file_entry.second;

			// loop through files in folder
			for (const auto& entry : file_name) {
				// get name of file
				const string_view cur_entry = entry.first.name();
				fs::path cur_path = folder_name / cur_entry;

				// chekc if we should ignore this file
				if (!file_allowed(cur_path)) {
					continue;
				}

				if (logging) {
					spdlog::trace(L"Adding file from BSA {} to file map: {}", bsa_name, cur_path.wstring());
				}

				// add to filemap
				updateFileMap(cur_path, bsa_shared_ptr);
			}
		} catch (const std::exception& e) {
			if (logging) {
				spdlog::warn(L"Failed to get file pointer from BSA, skipping {}: {}", bsa_name, convertToWstring(e.what()));
			}
			continue;
		}
	}
}

vector<wstring> BethesdaDirectory::getBSAPriorityList() const
{
	// get bsa files not loaded from esp (also initializes output vector)
	vector<wstring> out_bsa_order = getBSAFilesFromINIs();

	// get esp priority list
	const vector<wstring> load_order = getPluginLoadOrder(true);

	// list BSA files in data directory
	const vector<wstring> all_bsa_files = getBSAFilesInDirectory();

	//loop through each esp in the priority list
	for (wstring plugin : load_order) {
		// add any BSAs to list
		vector<wstring> cur_found_bsas = findBSAFilesFromPluginName(all_bsa_files, plugin);
		concatenateVectorsWithoutDuplicates(out_bsa_order, cur_found_bsas);
	}

	if(logging) {
		// log output
		wstring bsa_list_str = boost::algorithm::join(out_bsa_order, ",");
		spdlog::trace(L"BSA Load Order: {}", bsa_list_str);

		for (wstring bsa : all_bsa_files) {
			if (find(out_bsa_order.begin(), out_bsa_order.end(), bsa) == out_bsa_order.end()) {
				spdlog::warn(L"BSA file {} not loaded by any plugin or INI.", bsa);
			}
		}
	}

	return out_bsa_order;
}

vector<wstring> BethesdaDirectory::getPluginLoadOrder(const bool trim_extension) const
{
	if (logging) {
		spdlog::debug("Reading plugin load order from loadorder.txt.");
	}

	// initialize output vector. Stores
	vector<wstring> output_lo;

	// set path to loadorder.txt
	fs::path lo_file = bg.getLoadOrderFile();

	/*if (!fs::exists(lo_file)) {
		if (logging) {
			spdlog::critical("loadorder.txt not found. If you are using mod organizer be sure to launch this program through MO2.");
			exitWithUserInput(1);
		}
		else {
			throw runtime_error("loadorder.txt not found");
		}
	}*/

	// open file
	wifstream f(lo_file, true);
	if (!f.is_open()) {
		if (logging) {
			spdlog::critical("Unable to open loadorder.txt");
			exitWithUserInput(1);
		}
		else {
			throw runtime_error("Unable to open loadorder.txt");
		}
	}

	// loop through each line of loadorder.txt
	wstring line;
	while (getline(f, line)) {
		// Ignore lines that start with '#', which are comment lines
		if (line.empty() || line[0] == '#') {
			continue;
		}

		// Remove extension from line
		if (trim_extension) {
			line = line.substr(0, line.find_last_of('.'));
		}

		// Add to output list
		output_lo.push_back(line);
	}

	// close file handle
	f.close();

	if(logging) {
		wstring load_order_str = boost::algorithm::join(output_lo, ",");
		spdlog::debug(L"Plugin Load Order: {}", load_order_str);
	}

	// return output
	return output_lo;
}

fs::path BethesdaDirectory::getPathLower(const fs::path path)
{
	return fs::path(boost::to_lower_copy(path.wstring()));
}

bool BethesdaDirectory::pathEqualityIgnoreCase(const fs::path path1, const fs::path path2)
{
	return getPathLower(path1) == getPathLower(path2);
}

vector<wstring> BethesdaDirectory::getBSAFilesFromINIs() const
{
	// output vector
	vector<wstring> bsa_files;

	if (logging) {
		spdlog::debug("Reading manually loaded BSAs from INI files.");
	}

	// find ini paths
	BethesdaGame::ININame ini_locs = bg.getINIPaths();

	vector<fs::path> ini_file_order = { ini_locs.ini, ini_locs.ini_custom };

	// loop through each field
	bool first_ini_read = true;
	for (string field : ini_bsa_fields) {
		// loop through each ini file
		wstring ini_val;
		for (fs::path ini_path : ini_file_order) {
			wstring cur_val = readINIValue(ini_path, L"Archive", convertToWstring(field), logging, first_ini_read);

			if (logging) {
				spdlog::trace(L"Found ini key pair from INI {}: {}: {}", ini_path.wstring(), convertToWstring(field), cur_val);
			}

			if (cur_val.empty()) {
				continue;
			}

			ini_val = cur_val;
		}

		first_ini_read = false;

		if (ini_val.empty()) {
			continue;
		}

		if (logging) {
			spdlog::trace(L"Found BSA files from INI field {}: {}", convertToWstring(field), ini_val);
		}

		// split into components
		vector<wstring> ini_components;
		boost::split(ini_components, ini_val, boost::is_any_of(","));
		for (wstring bsa : ini_components) {
			// remove leading/trailing whitespace
			boost::trim(bsa);

			// add to output
			addUniqueElement(bsa_files, bsa);
		}
	}

	return bsa_files;
}

vector<wstring> BethesdaDirectory::getBSAFilesInDirectory() const
{
	if (logging) {
		spdlog::debug("Finding existing BSA files in data directory.");
	}

	vector<wstring> bsa_files;

	for (const auto& entry : fs::directory_iterator(this->data_dir)) {
		if (entry.is_regular_file()) {
			const string file_extension = entry.path().extension().string();
			// only interested in BSA files
			if (!boost::iequals(file_extension, ".bsa")) {
				continue;
			}

			// add to output
			bsa_files.push_back(entry.path().filename().wstring());
		}
	}

	return bsa_files;
}

vector<wstring> BethesdaDirectory::findBSAFilesFromPluginName(const vector<wstring>& bsa_file_list, const wstring& plugin_prefix) const
{
	if (logging) {
		spdlog::trace(L"Finding BSA files that correspond to plugin {}", plugin_prefix);
	}

	vector<wstring> bsa_files_found;
	wstring plugin_prefix_lower = boost::to_lower_copy(plugin_prefix);

	for (wstring bsa : bsa_file_list) {
		wstring bsa_lower = boost::to_lower_copy(bsa);
		if (bsa_lower.starts_with(plugin_prefix_lower)) {
			if (bsa_lower == plugin_prefix_lower + L".bsa") {
				// load bsa with the plugin name before any others
				bsa_files_found.insert(bsa_files_found.begin(), bsa);
				continue;
			}

			// skip any BSAs that may start with the prefix but belong to a different plugin
			wstring after_prefix = bsa.substr(plugin_prefix.length());

			// todo: Is this actually how the game handles BSA files? Example: 3DNPC0.bsa, 3DNPC1.bsa, 3DNPC2.bsa are loaded,
			// todo: but 3DNPC - Textures.bsa is also loaded, whats the logic there?
			if (after_prefix.starts_with(L" ") && !after_prefix.starts_with(L" -")) {
				continue;
			}

			if (!after_prefix.starts_with(L" ") && !isdigit(after_prefix[0])) {
				continue;
			}

			if (logging) {
				spdlog::trace(L"Found BSA file that corresponds to plugin {}: {}", plugin_prefix, bsa);
			}

			bsa_files_found.push_back(bsa);
		}
	}

	return bsa_files_found;
}

bool BethesdaDirectory::file_allowed(const fs::path file_path) const
{
	string file_extension = file_path.extension().string();
	boost::algorithm::to_lower(file_extension);
	if (std::find(extension_blocklist.begin(), extension_blocklist.end(), file_extension) != extension_blocklist.end()) {
		//elem exists in the vector
		return false;
	}

	return true;
}

// helpers
BethesdaDirectory::BethesdaFile BethesdaDirectory::getFileFromMap(const fs::path& file_path) const
{
	fs::path lower_path = getPathLower(file_path);

	if (fileMap.find(lower_path) == fileMap.end()) {
		return BethesdaFile { fs::path(), nullptr };
	}

	return fileMap.at(lower_path);
}

void BethesdaDirectory::updateFileMap(const fs::path& file_path, shared_ptr<BethesdaDirectory::BSAFile> bsa_file)
{
	fs::path lower_path = getPathLower(file_path);

	BethesdaFile new_bfile = { file_path, bsa_file };

	fileMap[lower_path] = new_bfile;
}
