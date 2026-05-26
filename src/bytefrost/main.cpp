#include <llvm/IR/BasicBlock.h>

#include <fstream>
#include <iostream>
#include <sstream>

#include "bytefrost/tokenizer/tokenizer.h"
#include "bytefrost/tokenizer/tokens.h"
#include "logger/logger.h"
#include "version.h"

int main(int argc, char* argv[]) {
	Logger::info("ByteFrost Compiler - Version " + std::string(BF_VERSION));

	if (argc < 2) {
		Logger::error("Usage: bytefrost <source-file>");
		return 1;
	}

	std::ifstream file(argv[1]);
	if (!file.is_open()) {
		Logger::error("Cannot open file: " + std::string(argv[1]));
		return 1;
	}

	std::ostringstream buf;
	buf << file.rdbuf();

	Tokenizer tokenizer(buf.str());

	std::vector<Token> tokens = tokenizer.tokenize();
	for (const auto& token : tokens) {
		std::cout << token.toString() << std::endl;
	}

	return 0;
}