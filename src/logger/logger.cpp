#include "logger/logger.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include <cstdlib>

namespace {
constexpr const char* RESET  = "\033[0m";
constexpr const char* CYAN   = "\033[36m";  // DEBUG
constexpr const char* GREEN  = "\033[32m";  // INFO
constexpr const char* YELLOW = "\033[33m";  // WARNING
constexpr const char* RED    = "\033[31m";  // ERROR

bool streamSupportsColor(std::ostream& stream) {
	// Respect the NO_COLOR convention (https://no-color.org/)
	if (std::getenv("NO_COLOR") != nullptr) return false;

#ifdef _WIN32
	HANDLE h = INVALID_HANDLE_VALUE;
	if (stream.rdbuf() == std::cout.rdbuf())
		h = GetStdHandle(STD_OUTPUT_HANDLE);
	else if (stream.rdbuf() == std::cerr.rdbuf())
		h = GetStdHandle(STD_ERROR_HANDLE);
	if (h == INVALID_HANDLE_VALUE) return false;  // file stream or unknown
	DWORD mode = 0;
	if (!GetConsoleMode(h, &mode)) return false;  // redirected to file, pipe, etc.
	return SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0;
#else
	if (stream.rdbuf() == std::cout.rdbuf()) return isatty(STDOUT_FILENO) != 0;
	if (stream.rdbuf() == std::cerr.rdbuf()) return isatty(STDERR_FILENO) != 0;
	return false;  // file streams and other non-tty streams
#endif
}
}  // namespace

std::ostream* Logger::output = &std::cout;
std::mutex Logger::mutex;
bool Logger::colorsEnabled = streamSupportsColor(std::cout);

void Logger::init(std::ostream& stream) {
	std::lock_guard<std::mutex> lock(mutex);
	output = &stream;
	colorsEnabled = streamSupportsColor(stream);
}

void Logger::setColors(bool enabled) {
	std::lock_guard<std::mutex> lock(mutex);
	colorsEnabled = enabled;
}

void Logger::log(LogLevel level, const std::string& message) {
	const char* levelStr;
	const char* color;
	switch (level) {
		case LogLevel::DEBUG:
			levelStr = "DEBUG";
			color = CYAN;
			break;
		case LogLevel::INFO:
			levelStr = "INFO";
			color = GREEN;
			break;
		case LogLevel::WARNING:
			levelStr = "WARNING";
			color = YELLOW;
			break;
		case LogLevel::ERROR_LVL:
			levelStr = "ERROR";
			color = RED;
			break;
		default:
			levelStr = "UNKNOWN";
			color = RESET;
	}

	std::lock_guard<std::mutex> lock(mutex);
	if (colorsEnabled) {
		*output << color << "[" << levelStr << "] " << message << RESET << "\n";
	} else {
		*output << "[" << levelStr << "] " << message << "\n";
	}
}

void Logger::debug(const std::string& message) {
	log(LogLevel::DEBUG, message);
}

void Logger::info(const std::string& message) {
	log(LogLevel::INFO, message);
}

void Logger::warning(const std::string& message) {
	log(LogLevel::WARNING, message);
}

void Logger::error(const std::string& message) {
	log(LogLevel::ERROR_LVL, message);
}
