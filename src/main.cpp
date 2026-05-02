#include <cstring>
#include <fstream>
#include <iostream>

#include "codegen/codegen.h"
#include "linker/linker.h"
#include "parser/parser.h"
#include "tokenizer/lexer.h"
#include "tokenizer/tokens.h"
#include "version.h"

int main(int argc, char* argv[]) {
	const char* filename = "tests/fib.bf";
	bool emitIR = false;
	bool emitObj = false;
	std::string outputFile;
	CodeGen::OptLevel optLevel = CodeGen::OptLevel::O0;

	for (int i = 1; i < argc; i++) {
		if (std::strcmp(argv[i], "--version") == 0 || std::strcmp(argv[i], "-V") == 0) {
			std::cout << "byte_frost " << BF_VERSION << "\n";
			return 0;
		} else if (std::strcmp(argv[i], "--emit-ir") == 0) {
			emitIR = true;
		} else if (std::strcmp(argv[i], "--emit-obj") == 0) {
			emitObj = true;
		} else if (std::strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
			outputFile = argv[++i];
		} else if (std::strncmp(argv[i], "-O", 2) == 0) {
			std::string_view lvl = argv[i] + 2;
			if (lvl == "0")
				optLevel = CodeGen::OptLevel::O0;
			else if (lvl == "1")
				optLevel = CodeGen::OptLevel::O1;
			else if (lvl == "2")
				optLevel = CodeGen::OptLevel::O2;
			else if (lvl == "3")
				optLevel = CodeGen::OptLevel::O3;
			else if (lvl == "s")
				optLevel = CodeGen::OptLevel::Os;
			else if (lvl == "z")
				optLevel = CodeGen::OptLevel::Oz;
			else {
				std::cerr << "Unknown optimization level: " << argv[i] << "  (valid: -O0 -O1 -O2 -O3 -Os -Oz)\n";
				return 1;
			}
		} else {
			filename = argv[i];
		}
	}

	std::ifstream file(filename);

	if (!file.is_open()) {
		std::cerr << "Failed to open file: " << filename << std::endl;
		return 1;
	}

	std::string source((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

	// Lexing
	Lexer lexer(source);
	auto tokens = lexer.tokenize();

	// Parsing
	try {
		Parser parser(tokens);
		Program program = parser.parseProgram();

		if (emitIR) {
			// Code generation (IR output ignores opt level — IR is pre-optimization)
			CodeGen codegen;
			std::string ir = codegen.generate(program);

			if (!outputFile.empty()) {
				std::ofstream out(outputFile);
				if (!out.is_open()) {
					std::cerr << "Failed to open output file: " << outputFile << std::endl;
					return 1;
				}
				out << ir;
			} else {
				std::cout << ir;
			}
		} else if (emitObj) {
			// Emit native object file
			CodeGen codegen;
			codegen.setOptLevel(optLevel);
#ifdef _WIN32
			std::string objPath = outputFile.empty() ? "output.obj" : outputFile;
#else
			std::string objPath = outputFile.empty() ? "output.o" : outputFile;
#endif
			codegen.emitObjectFile(program, objPath);
		} else {
			// Default: compile + link → executable.
			CodeGen codegen;
			codegen.setOptLevel(optLevel);
#ifdef _WIN32
			// Windows: produce a .exe and use .obj for the intermediate object.
			if (outputFile.empty())
				outputFile = "a.exe";
			std::string tmpObj = outputFile + ".tmp.obj";
#else
			if (outputFile.empty())
				outputFile = "a.out";
			std::string tmpObj = outputFile + ".tmp.o";
#endif
			codegen.emitObjectFile(program, tmpObj);

			try {
				Linker::Config linkConfig;
				linkConfig.objectFile = tmpObj;
				linkConfig.outputFile = outputFile;
				Linker::link(linkConfig);
				std::remove(tmpObj.c_str());
			} catch (...) {
				std::remove(tmpObj.c_str());
				throw;
			}
		}
	} catch (const ParseError& e) {
		std::cerr << "Parse error at " << e.line << ":" << e.column << ": " << e.what() << std::endl;
		return 1;
	} catch (const CodeGenError& e) {
		std::cerr << "Codegen error: " << e.what() << std::endl;
		return 1;
	} catch (const LinkerError& e) {
		std::cerr << "Linker error: " << e.what() << std::endl;
		return 1;
	}

	return 0;
}
