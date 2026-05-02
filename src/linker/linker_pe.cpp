// Windows / PE-COFF linker implementation.
// Compiled on Windows hosts only — see CMakeLists.txt.

#include "linker/linker.h"

#include <llvm/Support/FileSystem.h>
#include <llvm/Support/Program.h>
#include <llvm/Support/raw_ostream.h>

#include <array>
#include <cstdio>
#include <filesystem>

namespace fs = std::filesystem;

// ==========================
// Internal helpers
// ==========================

static std::string captureCommand(const std::string& cmd) {
	std::array<char, 512> buffer;
	std::string result;
	FILE* pipe = _popen(cmd.c_str(), "r");
	if (!pipe)
		return "";
	while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe)) {
		result += buffer.data();
	}
	_pclose(pipe);
	while (!result.empty() && (result.back() == '\n' || result.back() == '\r' || result.back() == ' '))
		result.pop_back();
	return result;
}

/// Return the Visual Studio installation root via vswhere.exe, or empty string.
static std::string findVSInstallPath() {
	static const std::string vswhere =
		"C:\\Program Files (x86)\\Microsoft Visual Studio\\Installer\\vswhere.exe";
	if (!llvm::sys::fs::exists(vswhere))
		return "";
	return captureCommand("\"" + vswhere + "\" -latest -property installationPath 2>nul");
}

/// Return the lexicographically greatest subdirectory name under `dir` whose
/// name starts with `requiredPrefix` (e.g. "10."). Pass an empty prefix to
/// match any subdirectory.
static std::string latestVersionIn(const std::string& dir, const std::string& requiredPrefix = "") {
	std::string latest;
	std::error_code ec;
	for (const auto& entry : fs::directory_iterator(dir, ec)) {
		if (!entry.is_directory())
			continue;
		std::string name = entry.path().filename().string();
		if (!requiredPrefix.empty() && name.rfind(requiredPrefix, 0) != 0)
			continue;
		if (name > latest)
			latest = name;
	}
	return latest;
}

// ==========================
// Toolchain discovery
// ==========================

std::string Linker::findWindowsSDKPath(const Config& config) {
	if (!config.windowsSdkPath.empty())
		return config.windowsSdkPath;

	// Prefer environment set by vcvarsall.bat / Developer Command Prompt.
	const char* sdkDir = std::getenv("WindowsSdkDir");
	if (sdkDir && *sdkDir && llvm::sys::fs::is_directory(sdkDir))
		return sdkDir;

	// Fall back to well-known installation paths.
	for (const auto* p : {
			 "C:\\Program Files (x86)\\Windows Kits\\10",
			 "C:\\Program Files\\Windows Kits\\10",
		 }) {
		if (llvm::sys::fs::is_directory(p))
			return p;
	}
	return "";
}

std::string Linker::findWindowsSDKVersion(const std::string& sdkBase) {
	// Honour the environment variable that vcvarsall sets.
	const char* sdkVer = std::getenv("WindowsSdkVersion");
	if (sdkVer && *sdkVer) {
		std::string ver = sdkVer;
		while (!ver.empty() && ver.back() == '\\')
			ver.pop_back();
		if (llvm::sys::fs::is_directory(sdkBase + "\\Lib\\" + ver))
			return ver;
	}

	return latestVersionIn(sdkBase + "\\Lib", "10.");
}

std::string Linker::findMSVCToolsPath(const Config& config) {
	if (!config.msvcToolsPath.empty())
		return config.msvcToolsPath;

	// Prefer environment set by vcvarsall.bat.
	const char* vcTools = std::getenv("VCToolsInstallDir");
	if (vcTools && *vcTools && llvm::sys::fs::is_directory(vcTools))
		return vcTools;

	// Helper: given a VS installation root, return the latest MSVC tools path.
	auto probeVSRoot = [](const std::string& vsPath) -> std::string {
		std::string msvcBase = vsPath + "\\VC\\Tools\\MSVC";
		if (!llvm::sys::fs::is_directory(msvcBase))
			return "";
		std::string latest = latestVersionIn(msvcBase);
		if (latest.empty())
			return "";
		return msvcBase + "\\" + latest;
	};

	// Try vswhere first (covers any VS edition/version).
	std::string vsPath = findVSInstallPath();
	if (!vsPath.empty()) {
		std::string result = probeVSRoot(vsPath);
		if (!result.empty())
			return result;
	}

	// Fallback: scan well-known installation directories for VS 2022 and 2019.
	static const char* knownVSRoots[] = {
		"C:\\Program Files\\Microsoft Visual Studio\\2022\\Community",
		"C:\\Program Files\\Microsoft Visual Studio\\2022\\Professional",
		"C:\\Program Files\\Microsoft Visual Studio\\2022\\Enterprise",
		"C:\\Program Files\\Microsoft Visual Studio\\2022\\BuildTools",
		"C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\Community",
		"C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\Professional",
		"C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\Enterprise",
		"C:\\Program Files (x86)\\Microsoft Visual Studio\\2019\\BuildTools",
	};
	for (const char* root : knownVSRoots) {
		std::string result = probeVSRoot(root);
		if (!result.empty())
			return result;
	}

	return "";
}

// ==========================
// Linker discovery
// ==========================

std::string Linker::findLinker() {
	// Prefer lld-link (LLVM's PE/COFF linker — cross-platform capable).
	auto lldLink = llvm::sys::findProgramByName("lld-link");
	if (lldLink)
		return lldLink.get();

	// Try MSVC link.exe via PATH, but reject the POSIX 'link' utility that
	// lives at /usr/bin/link in MSYS2 / Git Bash / Cygwin — it creates
	// hardlinks, not PE/COFF executables.
	auto linkExe = llvm::sys::findProgramByName("link.exe");
	if (linkExe) {
		const std::string& p = linkExe.get();
		bool looksUnix = p.rfind("/usr/", 0) == 0
					  || p.rfind("/bin/", 0) == 0
					  || p.rfind("/mingw", 0) == 0
					  || p.rfind("/opt/", 0) == 0;
		if (!looksUnix)
			return p;
	}

	// Last resort: look for link.exe directly inside the MSVC toolchain
	// directory (works even when the Developer Command Prompt is not active).
	Config empty;
	std::string msvcTools = findMSVCToolsPath(empty);
	if (!msvcTools.empty()) {
		std::string candidate = msvcTools + "\\bin\\Hostx64\\x64\\link.exe";
		if (llvm::sys::fs::exists(candidate))
			return candidate;
	}

	throw LinkerError(
		"No PE/COFF linker found. ByteFrost requires lld-link or MSVC link.exe.\n"
		"  Install LLVM:          winget install LLVM.LLVM\n"
		"  Or open a Visual Studio \"x64 Native Tools Command Prompt\".\n"
		"  Or install Visual Studio with the \"Desktop development with C++\" workload.\n"
		"See https://lld.llvm.org for more information.");
}

// ==========================
// Public interface
// ==========================

void Linker::link(const Config& config) {
	std::string linkerPath = findLinker();

	// Resolve output filename: default to "a.exe" on Windows.
	std::string outputFile = config.outputFile.empty() ? "a.exe" : config.outputFile;

	// Detect MSVC tools and Windows SDK paths.
	std::string msvcTools = findMSVCToolsPath(config);
	std::string sdkBase   = findWindowsSDKPath(config);
	std::string sdkVer    = sdkBase.empty() ? "" : findWindowsSDKVersion(sdkBase);

	// Warn when paths can't be auto-detected; lld-link may still succeed via
	// its own Windows-registry-based SDK lookup, but we surface the gap early.
	if (msvcTools.empty()) {
		llvm::errs()
			<< "ByteFrost warning: Could not locate MSVC toolchain directory.\n"
			<< "  Set VCToolsInstallDir, or run from a Visual Studio Developer Command Prompt.\n";
	}
	if (sdkBase.empty() || sdkVer.empty()) {
		llvm::errs()
			<< "ByteFrost warning: Could not locate Windows SDK.\n"
			<< "  Set WindowsSdkDir / WindowsSdkVersion, or install the Windows 10 SDK.\n";
	}

	// Determine architecture — x64 is the only supported target for now.
	const std::string arch = "x64";

	std::vector<std::string> args;
	args.push_back(linkerPath);

	// Output path.
	args.push_back("/OUT:" + outputFile);

	// Subsystem.
	args.push_back(config.windowsSubsystem == Config::WindowsSubsystem::Console
					   ? "/SUBSYSTEM:CONSOLE"
					   : "/SUBSYSTEM:WINDOWS");

	// Architecture.
	args.push_back("/MACHINE:X64");

	// Suppress the lld-link version banner for cleaner output.
	args.push_back("/NOLOGO");

	// Library search paths.
	if (!msvcTools.empty()) {
		args.push_back("/LIBPATH:" + msvcTools + "\\lib\\" + arch);
		// legacy_stdio_definitions.lib lives in the MSVC tools lib dir.
		// It provides direct (non-DLL-import) stubs for printf/scanf/etc.
		// LLVM generates direct calls to printf with the MSVC triple, so we
		// must include this lib or printf will be unresolved at link time.
		args.push_back("legacy_stdio_definitions.lib");
		// Also include legacy_stdio_wide_specifiers for wprintf / wscanf family.
		args.push_back("legacy_stdio_wide_specifiers.lib");
	}

	if (!sdkBase.empty() && !sdkVer.empty()) {
		args.push_back("/LIBPATH:" + sdkBase + "\\Lib\\" + sdkVer + "\\ucrt\\" + arch);
		args.push_back("/LIBPATH:" + sdkBase + "\\Lib\\" + sdkVer + "\\um\\"   + arch);
	}

	for (const auto& p : config.extraLibPaths)
		args.push_back("/LIBPATH:" + p);

	// Input object file(s).
	if (!config.objectFiles.empty()) {
		for (const auto& obj : config.objectFiles)
			args.push_back(obj);
	} else {
		args.push_back(config.objectFile);
	}

	// C runtime libraries.
	// Dynamic (/MD): msvcrt.lib + vcruntime.lib + ucrt.lib
	// Static  (/MT): libcmt.lib + libvcruntime.lib + libucrt.lib
	if (config.useStaticCRT) {
		args.push_back("libcmt.lib");
		args.push_back("libvcruntime.lib");
		args.push_back("libucrt.lib");
	} else {
		args.push_back("msvcrt.lib");
		args.push_back("vcruntime.lib");
		args.push_back("ucrt.lib");
	}

	// Core Windows import library (always required).
	args.push_back("kernel32.lib");

	// User-specified extra libs.
	for (const auto& lib : config.extraLibs)
		args.push_back(lib + ".lib");

	// Execute the linker.  On failure, include the full command line in the
	// error so it is easy to reproduce and debug outside of ByteFrost.
	std::vector<llvm::StringRef> argsRef;
	for (const auto& a : args)
		argsRef.push_back(a);

	// Optionally print the full command (set BF_LINKER_VERBOSE=1 to enable).
	if (const char* v = std::getenv("BF_LINKER_VERBOSE")) {
		if (v[0] == '1') {
			llvm::errs() << "ByteFrost linker command:\n";
			for (const auto& a : args)
				llvm::errs() << "  " << a << "\n";
		}
	}

	std::string errMsg;
	int rc = llvm::sys::ExecuteAndWait(linkerPath,
									   argsRef,
									   /*Env=*/std::nullopt,
									   /*Redirects=*/{},
									   /*SecondsToWait=*/60,
									   /*MemoryLimit=*/0,
									   &errMsg);

	if (rc != 0) {
		// Build full command string for the error message.
		std::string cmdLine;
		for (const auto& a : args) {
			if (!cmdLine.empty()) cmdLine += ' ';
			cmdLine += a;
		}
		std::string msg = "Linking failed (exit code " + std::to_string(rc) + ")";
		if (!errMsg.empty())
			msg += ": " + errMsg;
		msg += "\nLinker command: " + cmdLine;
		throw LinkerError(msg);
	}
}
