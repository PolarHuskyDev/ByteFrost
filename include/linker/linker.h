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
		std::vector<std::string> extraLibs;     // additional -l flags
		std::vector<std::string> extraLibPaths;  // additional -L flags
	};

	/// Link an object file into an executable. Throws LinkerError on failure.
	static void link(const Config& config);

   private:
	static std::string findLinker();
	static std::string findCRTObject(const std::string& name);
	static std::string findDynamicLinker();
	static std::vector<std::string> detectGCCLibPaths();
	static std::vector<std::string> detectSystemLibPaths();
};
