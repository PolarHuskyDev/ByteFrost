#include <iostream>
#include "version.h"

#include "bytefrost/tokenizer/tokens.h"

// #include <llvm/IR/BasicBlock.h>

int main() {
	std::cout << "Hello, ByteFrost v" << BF_VERSION << "!" << std::endl;
	return 0;
}