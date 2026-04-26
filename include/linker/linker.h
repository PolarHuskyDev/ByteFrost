#pragma once

#include <stdexcept>
#include <string>
#include <vector>

class LinkerError : public std::runtime_error {
   public:
	LinkerError(const std::string& msg) : std::runtime_error(msg) {}
};

class Linker {
   public:
	struct Config {
		std::string objectFile;
		std::string outputFile = "a.out";
		std::vector<std::string> extraLibs;      // additional -l flags
		std::vector<std::string> extraLibPaths;  // additional -L flags
		// Target-specific settings; leave empty for auto-detection
		std::string targetTriple;      // e.g. "x86_64-linux-gnu"
		std::string linkerEmulation;   // e.g. "elf_x86_64"; auto-detected if empty
		std::string dynamicLinker;     // path to ld-linux-*.so; auto-detected if empty
		std::vector<std::string> crtSearchPaths;  // extra dirs to search for CRT objects
	};

	/// Link an object file into an executable. Throws LinkerError on failure.
	static void link(const Config& config);

   private:
	static std::string findLinker();
	static std::string findCRTObject(const std::string& name,
	                                  const std::vector<std::string>& extraSearchPaths);
	static std::string findDynamicLinker(const std::string& linkerEmulation);
	static std::string detectLinkerEmulation();
	static std::vector<std::string> detectGCCLibPaths();
	static std::vector<std::string> detectSystemLibPaths(const std::string& linkerEmulation);
};
