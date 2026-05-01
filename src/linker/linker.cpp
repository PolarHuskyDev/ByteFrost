#include "linker/linker.h"

#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Program.h>
#include <llvm/Support/raw_ostream.h>

#include <array>
#include <cstdio>
#include <sstream>

// ==========================
// Public interface
// ==========================

void Linker::link(const Config& config) {
	std::string linkerPath = findLinker();

	// Resolve emulation string: use config if provided, else auto-detect.
	std::string emulation = config.linkerEmulation.empty() ? detectLinkerEmulation() : config.linkerEmulation;

	// Resolve dynamic linker path: use config if provided, else detect from emulation.
	std::string dynLinker = config.dynamicLinker.empty() ? findDynamicLinker(emulation) : config.dynamicLinker;

	std::vector<std::string> gccLibPaths = detectGCCLibPaths();
	std::vector<std::string> sysLibPaths = detectSystemLibPaths(emulation);

	// Locate CRT objects.
	std::string scrt1 = findCRTObject("Scrt1.o", config.crtSearchPaths);
	std::string crti = findCRTObject("crti.o", config.crtSearchPaths);
	std::string crtn = findCRTObject("crtn.o", config.crtSearchPaths);
	std::string crtbegin = findCRTObject("crtbeginS.o", config.crtSearchPaths);
	std::string crtend = findCRTObject("crtendS.o", config.crtSearchPaths);

	// Build the linker command line, mirroring what clang would produce.
	std::vector<std::string> args;
	args.push_back(linkerPath);

	// ELF options.
	args.push_back("-z");
	args.push_back("relro");
	args.push_back("--hash-style=gnu");
	args.push_back("--build-id");
	args.push_back("--eh-frame-hdr");
	args.push_back("-m");
	args.push_back(emulation);
	args.push_back("-pie");
	args.push_back("-dynamic-linker");
	args.push_back(dynLinker);

	// Output.
	args.push_back("-o");
	args.push_back(config.outputFile);

	// CRT prologue.
	args.push_back(scrt1);
	args.push_back(crti);
	if (!crtbegin.empty())
		args.push_back(crtbegin);

	// Library search paths.
	for (const auto& p : gccLibPaths) {
		args.push_back("-L" + p);
	}
	for (const auto& p : sysLibPaths) {
		args.push_back("-L" + p);
	}
	for (const auto& p : config.extraLibPaths) {
		args.push_back("-L" + p);
	}

	// Input object file(s).
	if (!config.objectFiles.empty()) {
		for (const auto& obj : config.objectFiles) {
			args.push_back(obj);
		}
	} else {
		args.push_back(config.objectFile);
	}

	// Libraries (user-specified + standard).
	for (const auto& lib : config.extraLibs) {
		args.push_back("-l" + lib);
	}
	args.push_back("-lm");
	args.push_back("-lgcc");
	args.push_back("--as-needed");
	args.push_back("-lgcc_s");
	args.push_back("--no-as-needed");
	args.push_back("-lc");
	args.push_back("-lgcc");
	args.push_back("--as-needed");
	args.push_back("-lgcc_s");
	args.push_back("--no-as-needed");

	// CRT epilogue.
	if (!crtend.empty())
		args.push_back(crtend);
	args.push_back(crtn);

	// Convert to SmallVector<StringRef> for LLVM API.
	std::vector<llvm::StringRef> argsRef;
	for (const auto& a : args) {
		argsRef.push_back(a);
	}

	// Execute the linker.
	std::string errMsg;
	int rc = llvm::sys::ExecuteAndWait(linkerPath,
									   argsRef,
									   /*Env=*/std::nullopt,
									   /*Redirects=*/{},
									   /*SecondsToWait=*/60,
									   /*MemoryLimit=*/0,
									   &errMsg);

	if (rc != 0) {
		std::string msg = "Linking failed (exit code " + std::to_string(rc) + ")";
		if (!errMsg.empty())
			msg += ": " + errMsg;
		throw LinkerError(msg);
	}
}

// ==========================
// Linker discovery
// ==========================

std::string Linker::findLinker() {
	// Prefer ld.lld, fall back to system ld.
	auto lld = llvm::sys::findProgramByName("ld.lld");
	if (lld)
		return lld.get();

	auto ld = llvm::sys::findProgramByName("ld");
	if (ld)
		return ld.get();

	throw LinkerError("Could not find ld.lld or ld in PATH");
}

// ==========================
// CRT object discovery
// ==========================

/// Run a command and capture its stdout (trimmed).
static std::string captureCommand(const std::string& cmd) {
	std::array<char, 512> buffer;
	std::string result;
	FILE* pipe = popen(cmd.c_str(), "r");
	if (!pipe)
		return "";
	while (fgets(buffer.data(), buffer.size(), pipe)) {
		result += buffer.data();
	}
	pclose(pipe);
	// Trim trailing whitespace.
	while (!result.empty() && (result.back() == '\n' || result.back() == '\r' || result.back() == ' ')) {
		result.pop_back();
	}
	return result;
}

std::string Linker::findCRTObject(const std::string& name, const std::vector<std::string>& extraSearchPaths) {
	// Strategy 1: Ask GCC.
	std::string path = captureCommand("gcc -print-file-name=" + name + " 2>/dev/null");
	if (!path.empty() && path != name && llvm::sys::fs::exists(path)) {
		return path;
	}

	// Strategy 2: Caller-supplied paths first.
	for (const auto& dir : extraSearchPaths) {
		std::string candidate = dir + "/" + name;
		if (llvm::sys::fs::exists(candidate))
			return candidate;
	}

	// Strategy 3: Well-known paths.
	static const std::vector<std::string> searchPaths = {
		"/usr/lib/x86_64-linux-gnu",
		"/lib/x86_64-linux-gnu",
		"/usr/lib/aarch64-linux-gnu",
		"/lib/aarch64-linux-gnu",
		"/usr/lib64",
		"/lib64",
		"/usr/lib",
	};

	// For GCC-specific objects (crtbeginS.o, crtendS.o), also search GCC dirs.
	if (name.find("crtbegin") != std::string::npos || name.find("crtend") != std::string::npos) {
		auto gccPaths = detectGCCLibPaths();
		for (const auto& dir : gccPaths) {
			std::string candidate = dir + "/" + name;
			if (llvm::sys::fs::exists(candidate))
				return candidate;
		}
	}

	for (const auto& dir : searchPaths) {
		std::string candidate = dir + "/" + name;
		if (llvm::sys::fs::exists(candidate))
			return candidate;
	}

	// crtbegin/crtend are optional (not strictly required for simple programs).
	if (name.find("crtbegin") != std::string::npos || name.find("crtend") != std::string::npos) {
		return "";
	}

	throw LinkerError("Could not find CRT object: " + name);
}

// Map linker emulation string to the dynamic linker paths for that ABI.
std::string Linker::findDynamicLinker(const std::string& linkerEmulation) {
	// Build a list of candidate paths based on the emulation string.
	std::vector<std::string> candidates;

	if (linkerEmulation == "elf_x86_64") {
		candidates = {
			"/lib64/ld-linux-x86-64.so.2",
			"/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2",
		};
	} else if (linkerEmulation == "elf_i386") {
		candidates = {
			"/lib/ld-linux.so.2",
			"/lib/i386-linux-gnu/ld-linux.so.2",
		};
	} else if (linkerEmulation == "aarch64linux") {
		candidates = {
			"/lib/ld-linux-aarch64.so.1",
			"/lib/aarch64-linux-gnu/ld-linux-aarch64.so.1",
		};
	} else if (linkerEmulation == "armelf_linux_eabi") {
		candidates = {
			"/lib/ld-linux-armhf.so.3",
			"/lib/arm-linux-gnueabihf/ld-linux-armhf.so.3",
			"/lib/ld-linux.so.3",
		};
	} else {
		// Unknown emulation: try to find via gcc -dumpmachine
		std::string triple = captureCommand("gcc -dumpmachine 2>/dev/null");
		if (!triple.empty()) {
			candidates.push_back("/lib/" + triple + "/ld.so.1");
			candidates.push_back("/lib/ld.so.1");
		}
	}

	for (const auto& p : candidates) {
		if (llvm::sys::fs::exists(p))
			return p;
	}

	throw LinkerError("Could not find dynamic linker for emulation: " + linkerEmulation);
}

// Auto-detect the linker emulation string for the current host.
std::string Linker::detectLinkerEmulation() {
	std::string triple = captureCommand("gcc -dumpmachine 2>/dev/null");
	if (triple.empty()) {
		// Last-resort guess based on sizeof(void*).
		return (sizeof(void*) == 8) ? "elf_x86_64" : "elf_i386";
	}

	if (triple.find("x86_64") != std::string::npos)
		return "elf_x86_64";
	if (triple.find("i386") != std::string::npos || triple.find("i686") != std::string::npos)
		return "elf_i386";
	if (triple.find("aarch64") != std::string::npos)
		return "aarch64linux";
	if (triple.find("arm") != std::string::npos)
		return "armelf_linux_eabi";

	// Unknown — return empty; caller will throw or handle gracefully.
	throw LinkerError("Could not determine linker emulation for target triple: " + triple);
}

std::vector<std::string> Linker::detectGCCLibPaths() {
	std::string output = captureCommand("gcc -print-search-dirs 2>/dev/null");
	std::vector<std::string> paths;

	// Parse the "libraries: =..." line.
	auto pos = output.find("libraries: =");
	if (pos == std::string::npos)
		return paths;

	std::string libs = output.substr(pos + 12);	 // skip "libraries: ="
	// Split on ':'
	std::istringstream ss(libs);
	std::string dir;
	while (std::getline(ss, dir, ':')) {
		if (!dir.empty() && llvm::sys::fs::is_directory(dir)) {
			paths.push_back(dir);
		}
	}
	return paths;
}

std::vector<std::string> Linker::detectSystemLibPaths(const std::string& linkerEmulation) {
	std::vector<std::string> paths;

	std::vector<std::string> candidates;
	if (linkerEmulation == "elf_x86_64") {
		candidates = {
			"/lib/x86_64-linux-gnu",
			"/lib64",
			"/usr/lib/x86_64-linux-gnu",
			"/usr/lib64",
			"/lib",
			"/usr/lib",
		};
	} else if (linkerEmulation == "elf_i386") {
		candidates = {
			"/lib/i386-linux-gnu",
			"/lib",
			"/usr/lib/i386-linux-gnu",
			"/usr/lib",
		};
	} else if (linkerEmulation == "aarch64linux") {
		candidates = {
			"/lib/aarch64-linux-gnu",
			"/usr/lib/aarch64-linux-gnu",
			"/lib",
			"/usr/lib",
		};
	} else {
		candidates = {"/lib", "/usr/lib"};
	}

	for (const auto& p : candidates) {
		if (llvm::sys::fs::is_directory(p)) {
			paths.push_back(p);
		}
	}
	return paths;
}
