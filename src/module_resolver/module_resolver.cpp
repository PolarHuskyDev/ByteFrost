#include "module_resolver/module_resolver.h"

#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Path.h>

#include <algorithm>
#include <system_error>

ModuleResolver::ModuleResolver(const std::string& srcRoot) : srcRoot_(srcRoot) {
	scan();
}

void ModuleResolver::scan() {
	moduleToFile_.clear();
	orderedPaths_.clear();
	walk(srcRoot_, "");
}

std::string ModuleResolver::resolve(const std::string& modulePath) const {
	auto it = moduleToFile_.find(modulePath);
	if (it == moduleToFile_.end())
		return "";
	return it->second;
}

const std::vector<std::string>& ModuleResolver::allModulePaths() const {
	return orderedPaths_;
}

void ModuleResolver::walk(const std::string& dir, const std::string& prefix) {
	std::error_code ec;
	for (llvm::sys::fs::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec)) {
		const std::string& path = it->path();

		llvm::sys::fs::file_status st;
		if (llvm::sys::fs::status(path, st))
			continue;  // skip on error

		// Extract the base name of this entry using portable path utilities.
		std::string name = llvm::sys::path::filename(path).str();

		if (llvm::sys::fs::is_directory(st)) {
			std::string childPrefix = prefix.empty() ? name : (prefix + "." + name);
			walk(path, childPrefix);
		} else {
			// Only process .bf files.
			if (name.size() < 3 || name.substr(name.size() - 3) != ".bf")
				continue;
			std::string stem = name.substr(0, name.size() - 3);
			std::string modulePath = prefix.empty() ? stem : (prefix + "." + stem);
			moduleToFile_.emplace(modulePath, path);
			orderedPaths_.push_back(modulePath);
		}
	}

	// Sort for deterministic ordering within each directory level.
	std::sort(orderedPaths_.begin(), orderedPaths_.end());
}
