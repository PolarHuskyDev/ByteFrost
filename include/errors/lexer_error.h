#pragma once


#include <string>

struct LexerError {
	std::string message;
	int line;
	int column;

	LexerError(const std::string& msg, int ln, int col) : message(msg), line(ln), column(col) {
	}
};
