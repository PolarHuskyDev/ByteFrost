#pragma once

#include <iostream>
#include <mutex>
#include <ostream>
#include <string>

enum class LogLevel { DEBUG, INFO, WARNING, ERROR_LVL };

class Logger {
   public:
	Logger() = delete;

	// Set the output stream (defaults to std::cout).
	// Color support is re-detected automatically for the new stream.
	static void init(std::ostream& stream);

	// Override the auto-detected color setting.
	// Useful when piping to a file or forcing colors in a CI environment.
	static void setColors(bool enabled);

	static void debug(const std::string& message);
	static void info(const std::string& message);
	static void warning(const std::string& message);
	static void error(const std::string& message);

   private:
	static void log(LogLevel level, const std::string& message);

   private:
	static std::ostream* output;
	static std::mutex mutex;
	static bool colorsEnabled;
};
