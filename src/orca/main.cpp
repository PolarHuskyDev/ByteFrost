/// orca — ByteFrost build orchestrator
///
/// Subcommands:
///   orca init [<name>]           Create a new project scaffold
///   orca build                   Build the project (default)
///   orca run [-- <args>]         Build then execute the project
///   orca clean [--all]           Remove build artifacts
///   orca check                   Parse-check without emitting files
///
/// Build options (orca build / orca run / orca):
///   --project <path>             Path to orca.toml (default: orca.toml)
///   -o <output>                  Output executable name (default: project name)
///   --release                    Build with release profile
///   --debug                      Build with debug profile [default]
///   --emit-ir                    Write LLVM IR files instead of linking
///   --emit-obj                   Write object files and stop before linking
///
/// General options:
///   -h, --help                   Show this help message

#include <codegen/codegen.h>
#include <linker/linker.h>
#include <module_resolver/module_resolver.h>
#include <parser/parser.h>
#include <tokenizer/lexer.h>
#include <tokenizer/tokens.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// ==========================
// Minimal TOML parser
// (supports [section], key = "value", and # comments)
// ==========================

struct OrcaConfig {
	std::string name;
	std::string version;
	std::string entry;		 // e.g. "src/main.bf"
	std::string debugOpt;	 // [profile.debug] opt  (empty = use buildOpt or "0")
	std::string releaseOpt;	 // [profile.release] opt (empty = use buildOpt or "2")
	std::string buildOpt;	 // [build] opt  base default (empty = not set)
	std::string output;		 // [build] output path override
};

static std::string trim(const std::string& s) {
	size_t start = s.find_first_not_of(" \t\r\n");
	if (start == std::string::npos)
		return "";
	size_t end = s.find_last_not_of(" \t\r\n");
	return s.substr(start, end - start + 1);
}

static OrcaConfig parseOrcaToml(const std::string& path) {
	std::ifstream f(path);
	if (!f.is_open()) {
		throw std::runtime_error("Cannot open orca.toml at: " + path);
	}

	OrcaConfig cfg;
	std::string section;
	std::string line;

	while (std::getline(f, line)) {
		// Strip comments
		auto hashPos = line.find('#');
		if (hashPos != std::string::npos)
			line = line.substr(0, hashPos);
		line = trim(line);
		if (line.empty())
			continue;

		if (line.front() == '[' && line.back() == ']') {
			section = trim(line.substr(1, line.size() - 2));
			continue;
		}

		auto eqPos = line.find('=');
		if (eqPos == std::string::npos)
			continue;
		std::string key = trim(line.substr(0, eqPos));
		std::string val = trim(line.substr(eqPos + 1));

		// Strip surrounding quotes from string values
		if (val.size() >= 2 && val.front() == '"' && val.back() == '"') {
			val = val.substr(1, val.size() - 2);
		}

		if (section == "project") {
			if (key == "name")
				cfg.name = val;
			if (key == "version")
				cfg.version = val;
			if (key == "entry")
				cfg.entry = val;
		}
		if (section == "profile.debug") {
			if (key == "opt")
				cfg.debugOpt = val;
		}
		if (section == "profile.release") {
			if (key == "opt")
				cfg.releaseOpt = val;
		}
		if (section == "build") {
			if (key == "opt")
				cfg.buildOpt = val;
			if (key == "output")
				cfg.output = val;
		}
	}

	if (cfg.entry.empty()) {
		throw std::runtime_error("orca.toml [project] must specify entry = \"src/main.bf\"");
	}
	return cfg;
}

// ==========================
// Opt level helper
// Mirrors Rust: opt-level = 0/1/2/3/"s"/"z"
// ==========================

static CodeGen::OptLevel parseOptLevel(const std::string& s) {
	if (s == "1")
		return CodeGen::OptLevel::O1;
	if (s == "2")
		return CodeGen::OptLevel::O2;
	if (s == "3")
		return CodeGen::OptLevel::O3;
	if (s == "s")
		return CodeGen::OptLevel::Os;
	if (s == "z")
		return CodeGen::OptLevel::Oz;
	return CodeGen::OptLevel::O0;  // "0" or anything unrecognised
}

// ==========================
// Import extraction
// (reads import paths from a .bf file without full compilation)
// ==========================

static std::vector<std::string> extractImports(const std::string& filePath) {
	std::ifstream f(filePath);
	if (!f.is_open())
		return {};
	std::string source((std::istreambuf_iterator<char>(f)), {});

	Lexer lexer(source);
	std::vector<Token> tokens = lexer.tokenize();

	std::vector<std::string> imports;
	for (size_t i = 0; i < tokens.size(); ++i) {
		if (tokens[i].type != TokenType::IMPORT_TOKEN)
			continue;
		++i;
		if (i >= tokens.size())
			break;

		// Collect identifiers and dots; stop at 'from', ';', or EOF.
		// For "import Foo from m.path;" the module path starts after 'from'.
		// For "import m.path;"          the module path starts right after 'import'.

		std::vector<std::string> moduleSegments;

		if (tokens[i].type == TokenType::IDENTIFIER_TOKEN) {
			// Could be the start of a module path, or the start of an import list.
			// Lookahead: if next is '.', 'from', ',', or ';' determines interpretation.
			// Collect potential module path first.
			moduleSegments.push_back(tokens[i].value);
			++i;
			while (i < tokens.size() && tokens[i].type == TokenType::DOT_TOKEN) {
				++i;
				if (i < tokens.size() && tokens[i].type == TokenType::IDENTIFIER_TOKEN) {
					moduleSegments.push_back(tokens[i].value);
					++i;
				}
			}
			// Now check: did we hit 'as' (alias)? If so, skip alias identifier and continue.
			// e.g. "import abs as myAbs from math.utils" — consume 'as myAbs', then look for 'from'.
			if (i < tokens.size() && tokens[i].type == TokenType::AS_TOKEN) {
				++i;  // skip 'as'
				if (i < tokens.size() && tokens[i].type == TokenType::IDENTIFIER_TOKEN) {
					++i;  // skip alias name
				}
			}
			// Now check: did we hit 'from'? If so, discard moduleSegments and read the path after 'from'.
			if (i < tokens.size() && tokens[i].type == TokenType::FROM_TOKEN) {
				// Skip past comma-separated items until we reach 'from'.
				// We already consumed the first item; skip any remaining ones.
				++i;  // skip 'from'
				moduleSegments.clear();
				while (i < tokens.size() && tokens[i].type == TokenType::IDENTIFIER_TOKEN) {
					moduleSegments.push_back(tokens[i].value);
					++i;
					if (i < tokens.size() && tokens[i].type == TokenType::DOT_TOKEN) {
						++i;
					} else {
						break;
					}
				}
			} else if (i < tokens.size() && tokens[i].type == TokenType::COMMA_TOKEN) {
				// "import Foo, Bar from m.path;" — skip identifiers until 'from'
				while (i < tokens.size() && tokens[i].type != TokenType::FROM_TOKEN
					   && tokens[i].type != TokenType::SEMICOLON_TOKEN) {
					++i;
				}
				if (i < tokens.size() && tokens[i].type == TokenType::FROM_TOKEN) {
					++i;
					moduleSegments.clear();
					while (i < tokens.size() && tokens[i].type == TokenType::IDENTIFIER_TOKEN) {
						moduleSegments.push_back(tokens[i].value);
						++i;
						if (i < tokens.size() && tokens[i].type == TokenType::DOT_TOKEN) {
							++i;
						} else {
							break;
						}
					}
				}
			}
			// else: bare "import m.path;" — moduleSegments is already the path
		}

		if (!moduleSegments.empty()) {
			std::string modPath;
			for (size_t j = 0; j < moduleSegments.size(); ++j) {
				if (j)
					modPath += ".";
				modPath += moduleSegments[j];
			}
			imports.push_back(modPath);
		}

		// Skip to semicolon to position for next statement
		while (i < tokens.size() && tokens[i].type != TokenType::SEMICOLON_TOKEN)
			++i;
	}
	return imports;
}

// ==========================
// Topological sort
// ==========================

struct TopoSorter {
	const std::unordered_map<std::string, std::vector<std::string>>& deps;
	std::unordered_set<std::string> visited;
	std::unordered_set<std::string> inStack;
	std::vector<std::string> order;

	void visit(const std::string& mod) {
		if (visited.count(mod))
			return;
		if (inStack.count(mod)) {
			throw std::runtime_error("Circular import detected involving module: " + mod);
		}
		inStack.insert(mod);
		auto it = deps.find(mod);
		if (it != deps.end()) {
			for (const auto& dep : it->second) {
				visit(dep);
			}
		}
		inStack.erase(mod);
		visited.insert(mod);
		order.push_back(mod);
	}
};

// ==========================
// Parse a .bf file → Program AST
// ==========================

static Program parseFile(const std::string& filePath) {
	std::ifstream f(filePath);
	if (!f.is_open()) {
		throw std::runtime_error("Cannot open source file: " + filePath);
	}
	std::string source((std::istreambuf_iterator<char>(f)), {});
	Lexer lexer(source);
	auto tokens = lexer.tokenize();
	Parser parser(tokens);
	return parser.parseProgram();
}

// ==========================
// Derive output .o path for a .bf source file
// ==========================

static std::string deriveObjPath(const std::string& srcPath, const std::string& buildDir) {
	std::string objName = srcPath;
	for (char& c : objName) {
		if (c == '/' || c == '\\')
			c = '_';
	}
	if (objName.size() > 3 && objName.substr(objName.size() - 3) == ".bf") {
		objName = objName.substr(0, objName.size() - 3);
	}
	return buildDir + "/" + objName + ".o";
}

// ==========================
// Compile one .bf file → .o
// (externs: exported FunctionDecls from imported modules to pre-declare)
// ==========================

static std::string compileFile(const std::string& srcPath,
							   const std::string& buildDir,
							   const Program& program,
							   const std::vector<const FunctionDecl*>& externs,
							   CodeGen::OptLevel optLevel = CodeGen::OptLevel::O0) {
	std::string objPath = deriveObjPath(srcPath, buildDir);

	CodeGen codegen;
	codegen.setOptLevel(optLevel);
	// Pre-declare exported functions from imported modules so cross-module
	// call sites resolve correctly at IR generation time.
	for (const auto* fn : externs) {
		codegen.declareExternFunction(*fn);
	}
	codegen.emitObjectFile(program, objPath);
	return objPath;
}

// ==========================
// Emit IR for one .bf file → .ll
// ==========================

static std::string emitIRFile(const std::string& srcPath,
							  const std::string& buildDir,
							  const Program& program,
							  const std::vector<const FunctionDecl*>& externs,
							  CodeGen::OptLevel optLevel = CodeGen::OptLevel::O0) {
	// Derive output .ll path from source path (same logic as compileFile).
	std::string irName = srcPath;
	for (char& c : irName) {
		if (c == '/' || c == '\\')
			c = '_';
	}
	if (irName.size() > 3 && irName.substr(irName.size() - 3) == ".bf") {
		irName = irName.substr(0, irName.size() - 3);
	}
	std::string irPath = buildDir + "/" + irName + ".ll";

	CodeGen codegen;
	codegen.setOptLevel(optLevel);
	for (const auto* fn : externs) {
		codegen.declareExternFunction(*fn);
	}
	std::string irStr = codegen.generate(program);

	std::ofstream out(irPath);
	if (!out.is_open())
		throw std::runtime_error("Cannot write IR file: " + irPath);
	out << irStr;
	return irPath;
}

// ==========================
// Incremental build cache
// Simple text format: srcPath=mtime_count  (one entry per line)
// ==========================

static std::string fileTimestamp(const std::string& path) {
	std::error_code ec;
	auto t = std::filesystem::last_write_time(path, ec);
	if (ec)
		return "";
	return std::to_string(t.time_since_epoch().count());
}

static std::unordered_map<std::string, std::string> readCache(const std::string& cachePath) {
	std::unordered_map<std::string, std::string> cache;
	std::ifstream f(cachePath);
	if (!f.is_open())
		return cache;
	std::string line;
	while (std::getline(f, line)) {
		auto eq = line.find('=');
		if (eq == std::string::npos)
			continue;
		cache[line.substr(0, eq)] = line.substr(eq + 1);
	}
	return cache;
}

static void writeCache(const std::string& cachePath, const std::unordered_map<std::string, std::string>& cache) {
	std::ofstream f(cachePath);
	for (const auto& [k, v] : cache) {
		f << k << "=" << v << "\n";
	}
}

// ==========================
// orca clean
// ==========================

static int cmdClean(const std::string& tomlPath, bool all) {
	OrcaConfig cfg;
	std::string projectRoot = ".";
	try {
		cfg = parseOrcaToml(tomlPath);
	} catch (...) {
		// If orca.toml is missing or unreadable, proceed with defaults.
	}
	{
		auto slashPos = tomlPath.rfind('/');
		if (slashPos != std::string::npos)
			projectRoot = tomlPath.substr(0, slashPos);
	}

	std::error_code ec;
	auto canonRoot = std::filesystem::canonical(projectRoot, ec);
	if (ec) {
		std::cerr << "[orca] Error: Cannot resolve project root '" << projectRoot << "': " << ec.message() << "\n";
		return 1;
	}

	// Path-traversal guard: verify p is inside the project root.
	auto isInsideRoot = [&](const std::filesystem::path& p) -> bool {
		std::error_code e2;
		auto canon = std::filesystem::weakly_canonical(p, e2);
		if (e2)
			return false;
		auto rel = std::filesystem::relative(canon, canonRoot, e2);
		if (e2)
			return false;
		auto s = rel.string();
		// ".." at the start means the path is outside the root.
		return s.substr(0, 2) != "..";
	};

	auto safeRemove = [&](const std::string& pathStr) {
		std::filesystem::path p(pathStr);
		if (!isInsideRoot(p)) {
			std::cerr << "[orca] Refusing to remove path outside project root: " << pathStr << "\n";
			return;
		}
		if (!std::filesystem::exists(p, ec))
			return;
		std::filesystem::remove_all(p, ec);
		if (ec)
			std::cerr << "[orca] Warning: Failed to remove " << pathStr << ": " << ec.message() << "\n";
		else
			std::cout << "[orca] Removed: " << pathStr << "\n";
	};

	if (all) {
		safeRemove(projectRoot + "/build");
	} else {
		safeRemove(projectRoot + "/build/orca");
	}

	// Remove the output executable.
	const std::string exeName = cfg.output.empty() ? (cfg.name.empty() ? "" : cfg.name) : cfg.output;
	if (!exeName.empty()) {
		safeRemove(exeName.front() == '/' ? exeName : projectRoot + "/" + exeName);
	}

	std::cout << "[orca] Clean complete.\n";
	return 0;
}

// ==========================
// orca check
// ==========================

static int cmdCheck(const std::string& tomlPath) {
	try {
		OrcaConfig cfg = parseOrcaToml(tomlPath);
		std::string projectRoot = ".";
		{
			auto slashPos = tomlPath.rfind('/');
			if (slashPos != std::string::npos)
				projectRoot = tomlPath.substr(0, slashPos);
		}

		std::string srcRoot = projectRoot + "/src";
		ModuleResolver resolver(srcRoot);

		std::unordered_map<std::string, std::string> modToFile;
		for (const auto& mod : resolver.allModulePaths()) {
			std::string file = resolver.resolve(mod);
			if (!file.empty())
				modToFile[mod] = file;
		}
		modToFile["__entry__"] = projectRoot + "/" + cfg.entry;

		int errorCount = 0;
		std::unordered_set<std::string> checkedFiles;
		for (const auto& [mod, file] : modToFile) {
			if (!checkedFiles.insert(file).second)
				continue;  // skip duplicate file paths
			try {
				parseFile(file);
			} catch (const ParseError& e) {
				std::cerr << file << ":" << e.line << ":" << e.column << ": error: " << e.what() << "\n";
				++errorCount;
			} catch (const std::exception& e) {
				std::cerr << file << ": error: " << e.what() << "\n";
				++errorCount;
			}
		}

		if (errorCount == 0) {
			std::cout << "[orca] check: " << modToFile.size() << " file(s) OK\n";
			return 0;
		}
		std::cerr << "[orca] check: " << errorCount << " error(s)\n";
		return 1;
	} catch (const std::exception& e) {
		std::cerr << "[orca] Error: " << e.what() << "\n";
		return 1;
	}
}

// ==========================
// orca init
// ==========================

static void printHelp() {
	std::cout << "orca — ByteFrost build orchestrator\n"
				 "\n"
				 "USAGE:\n"
				 "  orca [build] [OPTIONS]          Build the project in the current directory\n"
				 "  orca run [OPTIONS] [-- <args>]  Build then execute the project\n"
				 "  orca clean [--all]              Remove build artifacts\n"
				 "  orca check                      Parse-check without emitting files\n"
				 "  orca init [<name>]              Create a new project scaffold\n"
				 "  orca help                       Show this message\n"
				 "\n"
				 "BUILD / RUN OPTIONS:\n"
				 "  --project <path>           Path to orca.toml (default: orca.toml)\n"
				 "  -o <output>                Output executable name (default: project name)\n"
				 "  --release                  Build with release profile (default opt: 2)\n"
				 "  --debug                    Build with debug profile (default opt: 0) [default]\n"
				 "  --emit-ir                  Write LLVM IR and stop before linking\n"
				 "  --emit-obj                 Write object files and stop before linking\n"
				 "\n"
				 "CLEAN OPTIONS:\n"
				 "  --all                      Also remove the entire build/ directory\n"
				 "\n"
				 "INIT:\n"
				 "  orca init                  Scaffold a new project in the current directory\n"
				 "  orca init <name>           Create <name>/ and scaffold a project inside it\n"
				 "\n"
				 "GENERAL:\n"
				 "  -h, --help                 Show this message\n";
}

/// Write a file, creating any missing parent directories.
static void writeFile(const std::string& path, const std::string& content) {
	std::filesystem::path filePath(path);
	std::filesystem::path dir = filePath.parent_path();
	if (!dir.empty()) {
		std::error_code ec;
		std::filesystem::create_directories(dir, ec);
		if (ec) {
			throw std::runtime_error("Failed to create directory: " + dir.string());
		}
	}
	std::ofstream f(path);
	if (!f.is_open())
		throw std::runtime_error("Cannot write file: " + path);
	f << content;
}

static int cmdInit(const std::string& name) {
	// Root of the new project.
	std::string root = name.empty() ? "." : name;

	// Derive a project name: basename of the resolved root.
	std::string projectName = name.empty() ? "my_project" : name;
	// If name contains path separators, use only the last component.
	auto sep = projectName.rfind('/');
	if (sep != std::string::npos)
		projectName = projectName.substr(sep + 1);

	// Check the target doesn't already contain an orca.toml.
	{
		std::ifstream check(root + "/orca.toml");
		if (check.is_open()) {
			std::cerr << "[orca] Error: " << root << "/orca.toml already exists.\n";
			return 1;
		}
	}

	// Create directories.
	std::error_code mkdirEc;
	std::filesystem::create_directories(root + "/src", mkdirEc);
	if (mkdirEc) {
		std::cerr << "[orca] Error: Failed to create project directories: " << mkdirEc.message() << "\n";
		return 1;
	}

	// orca.toml
	std::string toml =
		"[project]\n"
		"name    = \"" + projectName + "\"\n"
		"version = \"0.1.0\"\n"
		"entry   = \"src/main.bf\"\n"
		"\n"
		"[profile.debug]\n"
		"opt = 0          # 0, 1, 2, 3, \"s\" (size), \"z\" (aggressive size)\n"
		"\n"
		"[profile.release]\n"
		"opt = 2\n";
	writeFile(root + "/orca.toml", toml);

	// src/main.bf — minimal hello-world entry point.
	std::string mainBf =
		"// " + projectName + " — entry point\n"
		"\n"
		"main(): int {\n"
		"    print(\"Hello from " + projectName + "!\");\n"
		"    return 0;\n"
		"}\n";
	writeFile(root + "/src/main.bf", mainBf);

	// .gitignore — ignore build artefacts and the compiled binary.
	std::string gitignore = "build/\n" + projectName + "\n";
	writeFile(root + "/.gitignore", gitignore);

	std::cout << "[orca] Created project '" << projectName << "'\n";
	if (!name.empty())
		std::cout << "[orca] Enter the project:  cd " << name << "\n";
	std::cout << "[orca] Build it:           orca build\n";
	std::cout << "[orca] Run it:             orca run\n";
	return 0;
}

// ==========================
// Entry point
// ==========================

int main(int argc, char* argv[]) {
	// Detect subcommand as the first non-flag argument.
	std::string subcommand = "build";  // default

	if (argc >= 2) {
		std::string first = argv[1];
		if (first == "init") {
			// orca init [<name>]
			std::string name;
			if (argc >= 3) {
				std::string second = argv[2];
				if (second == "-h" || second == "--help") {
					std::cout << "USAGE: orca init [<name>]\n"
								 "\n"
								 "  Creates a new ByteFrost project scaffold.\n"
								 "\n"
								 "  orca init           Scaffold in the current directory\n"
								 "  orca init <name>    Create <name>/ and scaffold inside it\n";
					return 0;
				}
				name = second;
			}
			return cmdInit(name);
		} else if (first == "build") {
			subcommand = "build";
			// Shift argv so the option parsing below starts at argv[2].
			argc -= 1;
			argv += 1;
		} else if (first == "run") {
			subcommand = "run";
			// Shift argv so the option parsing below starts at argv[2].
			argc -= 1;
			argv += 1;
		} else if (first == "clean") {
			// orca clean [--all] [--project <path>]
			bool cleanAll = false;
			std::string cleanToml = "orca.toml";
			for (int i = 2; i < argc; ++i) {
				if (std::strcmp(argv[i], "--all") == 0) {
					cleanAll = true;
				} else if (std::strcmp(argv[i], "--project") == 0 && i + 1 < argc) {
					cleanToml = argv[++i];
				} else if (std::strcmp(argv[i], "-h") == 0 || std::strcmp(argv[i], "--help") == 0) {
					std::cout << "USAGE: orca clean [--all] [--project <path>]\n\n"
								 "  --all   Also remove the entire build/ directory\n";
					return 0;
				} else {
					std::cerr << "[orca] Unknown option for clean: " << argv[i] << "\n";
					return 1;
				}
			}
			return cmdClean(cleanToml, cleanAll);
		} else if (first == "check") {
			// orca check [--project <path>]
			std::string checkToml = "orca.toml";
			for (int i = 2; i < argc; ++i) {
				if (std::strcmp(argv[i], "--project") == 0 && i + 1 < argc) {
					checkToml = argv[++i];
				} else if (std::strcmp(argv[i], "-h") == 0 || std::strcmp(argv[i], "--help") == 0) {
					std::cout << "USAGE: orca check [--project <path>]\n\n"
								 "  Parse-checks all source files without emitting files.\n";
					return 0;
				} else {
					std::cerr << "[orca] Unknown option for check: " << argv[i] << "\n";
					return 1;
				}
			}
			return cmdCheck(checkToml);
		} else if (first == "help" || first == "--help" || first == "-h") {
			printHelp();
			return 0;
		}
		// If first arg looks like a flag (starts with -), treat as build options.
	}

	// ---- build / run subcommand ----
	std::string tomlPath = "orca.toml";
	std::string outputFile;
	bool doRun = (subcommand == "run");
	std::vector<std::string> runArgs;
	bool emitIR = false;
	bool emitObj = false;
	bool releaseMode = false;

	for (int i = 1; i < argc; ++i) {
		if (doRun && std::strcmp(argv[i], "--") == 0) {
			// Everything after -- are arguments forwarded to the executed binary.
			for (int j = i + 1; j < argc; ++j)
				runArgs.push_back(argv[j]);
			break;
		}
		if (std::strcmp(argv[i], "--project") == 0 && i + 1 < argc) {
			tomlPath = argv[++i];
		} else if (std::strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
			outputFile = argv[++i];
		} else if (std::strcmp(argv[i], "--emit-ir") == 0) {
			emitIR = true;
		} else if (std::strcmp(argv[i], "--emit-obj") == 0) {
			emitObj = true;
		} else if (std::strcmp(argv[i], "--release") == 0) {
			releaseMode = true;
		} else if (std::strcmp(argv[i], "--debug") == 0) {
			releaseMode = false;
		} else if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
			printHelp();
			return 0;
		} else {
			std::cerr << "[orca] Unknown option: " << argv[i] << "\n";
			std::cerr << "Run 'orca --help' for usage.\n";
			return 1;
		}
	}

	try {
		// 1. Parse orca.toml
		OrcaConfig cfg = parseOrcaToml(tomlPath);
		if (outputFile.empty()) {
			// [build].output overrides the default (project name).
			if (!cfg.output.empty())
				outputFile = cfg.output;
			else
				outputFile = cfg.name.empty() ? "a.out" : cfg.name;
		}

		// Select optimization level: [build].opt is base default; profile overrides it.
		const std::string effectiveDebugOpt =
			cfg.debugOpt.empty() ? (cfg.buildOpt.empty() ? "0" : cfg.buildOpt) : cfg.debugOpt;
		const std::string effectiveReleaseOpt =
			cfg.releaseOpt.empty() ? (cfg.buildOpt.empty() ? "2" : cfg.buildOpt) : cfg.releaseOpt;
		const std::string& selectedOpt = releaseMode ? effectiveReleaseOpt : effectiveDebugOpt;
		CodeGen::OptLevel optLevel = parseOptLevel(selectedOpt);

		// Determine project root (directory containing orca.toml).
		std::string projectRoot = ".";
		auto slashPos = tomlPath.rfind('/');
		if (slashPos != std::string::npos) {
			projectRoot = tomlPath.substr(0, slashPos);
		}

		std::string srcRoot = projectRoot + "/src";
		std::string buildDir = projectRoot + "/build/orca";

		// Create build directory.
		{
			std::error_code ec;
			std::filesystem::create_directories(buildDir, ec);
			if (ec)
				throw std::runtime_error("Failed to create build directory: " + buildDir);
		}

		// 2. Scan for all .bf modules under src/
		ModuleResolver resolver(srcRoot);

		// 3. Build dependency graph via import extraction.
		std::unordered_map<std::string, std::string> modToFile;	 // module path → file path
		std::unordered_map<std::string, std::vector<std::string>> deps;

		for (const auto& mod : resolver.allModulePaths()) {
			std::string file = resolver.resolve(mod);
			if (file.empty())
				continue;
			modToFile[mod] = file;
			deps[mod] = extractImports(file);
		}

		// Also include the entry file, even if it doesn't appear as a named module.
		std::string entryFile = projectRoot + "/" + cfg.entry;
		const std::string entryMod = "__entry__";
		modToFile[entryMod] = entryFile;
		deps[entryMod] = extractImports(entryFile);

		// Validate that every imported module path resolves to a known file.
		// Fail fast with a clear error rather than silently producing missing symbols.
		for (const auto& [mod, imports] : deps) {
			const std::string& srcFile = modToFile.at(mod);
			for (const auto& imp : imports) {
				if (modToFile.find(imp) == modToFile.end()) {
					std::cerr << "[orca] Error: " << srcFile << ": cannot resolve import '" << imp << "'\n"
							  << "       No .bf file found for module path '" << imp << "' under " << srcRoot << "\n";
					return 1;
				}
			}
		}

		// 4. Topological sort: start from entry.
		TopoSorter sorter{deps, {}, {}, {}};
		sorter.visit(entryMod);

		// 5. Parse all modules upfront so we can collect exported symbols.
		//    This is needed to emit extern declarations in each translation unit.
		std::unordered_map<std::string, Program> parsedPrograms;
		for (const auto& mod : sorter.order) {
			auto it = modToFile.find(mod);
			if (it == modToFile.end())
				continue;
			try {
				parsedPrograms.emplace(mod, parseFile(it->second));
			} catch (const ParseError& e) {
				std::cerr << "Parse error in " << it->second << " at " << e.line << ":" << e.column << ": " << e.what()
						  << "\n";
				return 1;
			}
		}

		// 6. Compile each module in dependency order, injecting extern hints.
		// Incremental build: skip unchanged source files using an mtime cache.
		const std::string cacheFile = buildDir + "/.orca_cache";
		auto cache = readCache(cacheFile);
		bool cacheUpdated = false;
		std::vector<std::string> objectFiles;
		for (const auto& mod : sorter.order) {
			auto it = modToFile.find(mod);
			if (it == modToFile.end())
				continue;
			const std::string& file = it->second;

			// Collect exported functions from all direct dependencies.
			std::vector<const FunctionDecl*> externs;
			auto depsIt = deps.find(mod);
			if (depsIt != deps.end()) {
				for (const auto& depMod : depsIt->second) {
					auto progIt = parsedPrograms.find(depMod);
					if (progIt == parsedPrograms.end())
						continue;
					for (const auto& fn : progIt->second.functions) {
						if (fn->isExported)
							externs.push_back(fn.get());
					}
				}
			}

			// Incremental: if we're compiling (not emitting IR), check the cache.
			if (!emitIR) {
				std::string objPath = deriveObjPath(file, buildDir);
				std::string ts = fileTimestamp(file);
				auto cit = cache.find(file);
				if (!ts.empty() && cit != cache.end() && cit->second == ts && std::filesystem::exists(objPath)) {
					std::cout << "[orca] Skipping (up-to-date): " << file << "\n";
					objectFiles.push_back(objPath);
					continue;
				}
			}

			std::cout << "[orca] Compiling " << file << " (" << mod << ")\n";
			try {
				auto progIt = parsedPrograms.find(mod);
				if (progIt == parsedPrograms.end())
					continue;
				if (emitIR) {
					std::string irPath = emitIRFile(file, buildDir, progIt->second, externs, optLevel);
					std::cout << "[orca] IR written: " << irPath << "\n";
				} else {
					std::string objPath = compileFile(file, buildDir, progIt->second, externs, optLevel);
					objectFiles.push_back(objPath);
					// Update cache entry after a successful compile.
					std::string ts = fileTimestamp(file);
					if (!ts.empty()) {
						cache[file] = ts;
						cacheUpdated = true;
					}
				}
			} catch (const CodeGenError& e) {
				std::cerr << "Codegen error in " << file << ": " << e.what() << "\n";
				return 1;
			}
		}

		// Persist incremental build cache (only when object files were compiled).
		if (cacheUpdated)
			writeCache(cacheFile, cache);

		if (emitIR) {
			std::cout << "[orca] IR files written to " << buildDir << "\n";
			return 0;
		}

		if (emitObj) {
			std::cout << "[orca] Object files written to " << buildDir << "\n";
			return 0;
		}

		// 6. Link all object files.
		if (objectFiles.empty()) {
			std::cerr << "[orca] No object files produced — nothing to link.\n";
			return 1;
		}

		// For now, pass all objects as a combined archive via the first object
		// and extra -l flags.  A proper multi-object link requires slight changes
		// to Linker::Config — until then we link the entry object and pass the
		// rest as extra positional args by writing them into a thin archive or
		// using a response file.  For this initial implementation we compile
		// everything into one combined object using `ld -r` (partial link) and
		// then do the final link.

		std::cout << "[orca] Linking → " << outputFile << "\n";
		Linker::Config linkConfig;
		linkConfig.objectFiles = objectFiles;
		linkConfig.outputFile = outputFile;
		try {
			Linker::link(linkConfig);
		} catch (const LinkerError& e) {
			std::cerr << "[orca] Linker error: " << e.what() << "\n";
			return 1;
		}

		std::cout << "[orca] Build succeeded: " << outputFile << "\n";

		// orca run: replace this process with the built binary.
		if (doRun) {
			// execvp does not search CWD; prefix relative paths with "./".
			std::string execPath = outputFile;
			if (!execPath.empty() && execPath.front() != '/') {
				execPath = "./" + execPath;
			}
			std::vector<const char*> runArgv;
			runArgv.push_back(execPath.c_str());
			for (const auto& a : runArgs)
				runArgv.push_back(a.c_str());
			runArgv.push_back(nullptr);
			execvp(execPath.c_str(), const_cast<char* const*>(runArgv.data()));
			// execvp only returns on error.
			std::cerr << "[orca] Failed to execute '" << execPath << "': " << strerror(errno) << "\n";
			return 1;
		}

	} catch (const std::exception& e) {
		std::cerr << "[orca] Error: " << e.what() << "\n";
		return 1;
	}

	return 0;
}
