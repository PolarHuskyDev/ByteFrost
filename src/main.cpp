#include <cstring>
#include <fstream>
#include <iostream>

#include "codegen/codegen.h"
#include "parser/parser.h"
#include "tokenizer/lexer.h"
#include "tokenizer/tokens.h"

int main(int argc, char* argv[]) {
	const char* filename = "tests/fib.bf";
	bool emitIR = false;
	std::string outputFile;

	for (int i = 1; i < argc; i++) {
		if (std::strcmp(argv[i], "--emit-ir") == 0) {
			emitIR = true;
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
		} else {
			// Default: print tokens and AST summary.
			std::printf("=== Tokens ===\n");
			for (const auto& token : tokens) {
				std::printf("%s\n", token.toString().c_str());
			}

			std::printf("\n=== AST Summary ===\n");
			std::printf("Functions: %zu\n", program.functions.size());
			for (const auto& fn : program.functions) {
				std::printf("  fn %s(%zu params) -> %s [%zu statements]\n",
							fn->name.c_str(),
							fn->params.size(),
							fn->returnType->name.c_str(),
							fn->body.statements.size());
			}
			std::printf("Structs: %zu\n", program.structs.size());
			for (const auto& s : program.structs) {
				std::printf("  struct %s [%zu members]\n",
							s->name.c_str(),
							s->members.size());
			}
		}
	} catch (const ParseError& e) {
		std::cerr << "Parse error at " << e.line << ":" << e.column << ": " << e.what() << std::endl;
		return 1;
	} catch (const CodeGenError& e) {
		std::cerr << "Codegen error: " << e.what() << std::endl;
		return 1;
	}

	return 0;
}
