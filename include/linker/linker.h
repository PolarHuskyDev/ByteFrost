#pragma once

#include <stdexcept>
#include <string>
#include <vector>

class LinkerError : public std::runtime_error {
   public:
	LinkerError(const std::string& msg) : std::runtime_error(msg) {
	}
};

class Linker {
   public:
	struct Config {
		// ----------------------------------------------------------------
		// Shared fields
		// ----------------------------------------------------------------
		std::string objectFile;				   // single object (kept for compatibility)
		std::vector<std::string> objectFiles;  // all object files to link (preferred when non-empty)
		std::string outputFile;				   // defaults are platform-specific; see Linker::link()
		std::vector<std::string> extraLibs;		 // additional lib flags
		std::vector<std::string> extraLibPaths;	 // additional lib search paths

		// ----------------------------------------------------------------
		// Target platform selection
		// ----------------------------------------------------------------
		enum class Platform {
			Auto,	 // detect from host at runtime
			Linux,	 // ELF / GNU toolchain
			Windows, // PE/COFF via lld-link or MSVC link.exe
			macOS,	 // Mach-O via ld (reserved for future use)
			WASM,	 // WebAssembly via wasm-ld (reserved for future use)
		};
		Platform platform = Platform::Auto;

		// ----------------------------------------------------------------
		// Linux / ELF-specific  (ignored on other platforms)
		// ----------------------------------------------------------------
		std::string targetTriple;				  // e.g. "x86_64-linux-gnu"
		std::string linkerEmulation;			  // e.g. "elf_x86_64"; auto-detected if empty
		std::string dynamicLinker;				  // path to ld-linux-*.so; auto-detected if empty
		std::vector<std::string> crtSearchPaths;  // extra dirs to search for CRT objects

		// ----------------------------------------------------------------
		// Windows / PE-specific  (ignored on other platforms)
		// ----------------------------------------------------------------
		enum class WindowsSubsystem { Console, Windows };
		WindowsSubsystem windowsSubsystem = WindowsSubsystem::Console;

		std::string windowsSdkPath;	 // base path of Windows Kit; auto-detected if empty
		std::string msvcToolsPath;	 // MSVC tools dir (…/VC/Tools/MSVC/x.y.z); auto-detected if empty
		bool useStaticCRT = false;	 // true → link /MT (libcmt); false → link /MD (msvcrt)
	};

	/// Link object file(s) into an executable. Throws LinkerError on failure.
	static void link(const Config& config);

   private:
	// findLinker() is implemented per-platform in linker_elf.cpp / linker_pe.cpp.
	static std::string findLinker();

#ifndef _WIN32
	// ---- Linux / ELF ---------------------------------------------------
	static std::string findCRTObject(const std::string& name, const std::vector<std::string>& extraSearchPaths);
	static std::string findDynamicLinker(const std::string& linkerEmulation);
	static std::string detectLinkerEmulation();
	static std::vector<std::string> detectGCCLibPaths();
	static std::vector<std::string> detectSystemLibPaths(const std::string& linkerEmulation);
#else
	// ---- Windows / PE --------------------------------------------------
	static std::string findWindowsSDKPath(const Config& config);
	static std::string findWindowsSDKVersion(const std::string& sdkBase);
	static std::string findMSVCToolsPath(const Config& config);
#endif
};
