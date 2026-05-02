#pragma once

#include <string>
#include <unordered_map>
#include <vector>

/// ModuleResolver maps dotted ByteFrost module paths (e.g. "linker.linker")
/// to filesystem paths relative to a given source root (e.g. "src/").
///
/// Module path rules:
///   - The source root directory is treated as the root namespace.
///   - Each directory separator in the filesystem becomes a '.' in the module path.
///   - The file extension (.bf) is stripped from the leaf name.
///   - Example: src/net/http/client.bf  →  "net.http.client"
class ModuleResolver {
   public:
	/// Construct a resolver rooted at srcRoot (e.g. "/path/to/project/src").
	explicit ModuleResolver(const std::string& srcRoot);

	/// (Re)scan the source root, rebuilding the path → file mapping.
	/// Called automatically by the constructor; call again after new files are created.
	void scan();

	/// Look up the filesystem path for a dotted module path.
	/// Returns empty string if not found.
	std::string resolve(const std::string& modulePath) const;

	/// Return all known module paths in discovery order.
	const std::vector<std::string>& allModulePaths() const;

	const std::string& srcRoot() const {
		return srcRoot_;
	}

   private:
	/// Recursively walk dir, building entries under the given module prefix.
	void walk(const std::string& dir, const std::string& prefix);

	std::string srcRoot_;
	std::unordered_map<std::string, std::string> moduleToFile_;
	std::vector<std::string> orderedPaths_;
};
