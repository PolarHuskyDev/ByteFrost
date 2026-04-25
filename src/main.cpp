#include <cstring>
#include <fstream>
#include <iostream>

#include "codegen/codegen.h"
#include "linker/linker.h"
#include "parser/parser.h"
#include "tokenizer/lexer.h"
#include "tokenizer/tokens.h"

int main(int argc, char* argv[]) {
	const char* filename = "tests/fib.bf";
	bool emitIR = false;
	bool emitObj = false;
	std::string outputFile;

	for (int i = 1; i < argc; i++) {
		if (std::strcmp(argv[i], "--emit-ir") == 0) {
			emitIR = true;
		} else if (std::strcmp(argv[i], "--emit-obj") == 0) {
			emitObj = true;
		} else if (std::strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
			outputFile = argv[++i];
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
			// Code generation
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
			std::string objPath = outputFile.empty() ? "output.o" : outputFile;
			codegen.emitObjectFile(program, objPath);
		} else {
			// Default: compile + link → executable.
			CodeGen codegen;
			std::string tmpObj = outputFile + ".tmp.o";
			if (outputFile.empty()) {
				outputFile = "a.out";
				tmpObj = "a.out.tmp.o";
			}
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
