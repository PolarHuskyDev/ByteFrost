#include "bytefrost/diagnostics/diagnostic_engine.h"

#include <iostream>
#include <ostream>
#include <sstream>
#include <string>

namespace bytefrost {

// ANSI escape codes used when color output is enabled.
namespace ansi {
static const char* Reset   = "\033[0m";
static const char* Bold    = "\033[1m";
static const char* Red     = "\033[1;31m";
static const char* Yellow  = "\033[1;33m";
static const char* Cyan    = "\033[1;36m";
static const char* Magenta = "\033[1;35m";
static const char* Gray    = "\033[0;37m";
}  // namespace ansi

// -----------------------------------------------------------------------
// Public interface
// -----------------------------------------------------------------------

Diagnostic& DiagnosticEngine::error(SourceLocation loc, std::string msg) {
	++errorCount_;
	diagnostics_.emplace_back(DiagnosticSeverity::Error, std::move(loc), std::move(msg));
	return diagnostics_.back();
}

Diagnostic& DiagnosticEngine::warning(SourceLocation loc, std::string msg) {
	++warningCount_;
	diagnostics_.emplace_back(DiagnosticSeverity::Warning, std::move(loc), std::move(msg));
	return diagnostics_.back();
}

Diagnostic& DiagnosticEngine::note(SourceLocation loc, std::string msg) {
	diagnostics_.emplace_back(DiagnosticSeverity::Note, std::move(loc), std::move(msg));
	return diagnostics_.back();
}

Diagnostic& DiagnosticEngine::error(std::string msg) {
	return error(SourceLocation{}, std::move(msg));
}

Diagnostic& DiagnosticEngine::warning(std::string msg) {
	return warning(SourceLocation{}, std::move(msg));
}

void DiagnosticEngine::clear() {
	diagnostics_.clear();
	errorCount_   = 0;
	warningCount_ = 0;
}

// -----------------------------------------------------------------------
// Formatting
// -----------------------------------------------------------------------

std::string DiagnosticEngine::severityLabel(DiagnosticSeverity sev, bool useColor) {
	switch (sev) {
		case DiagnosticSeverity::Error:
			return useColor ? std::string(ansi::Red) + "error" + ansi::Reset
			                : "error";
		case DiagnosticSeverity::Warning:
			return useColor ? std::string(ansi::Yellow) + "warning" + ansi::Reset
			                : "warning";
		case DiagnosticSeverity::Note:
			return useColor ? std::string(ansi::Cyan) + "note" + ansi::Reset
			                : "note";
		case DiagnosticSeverity::Hint:
			return useColor ? std::string(ansi::Magenta) + "hint" + ansi::Reset
			                : "hint";
	}
	return "diagnostic";
}

void DiagnosticEngine::formatDiagnostic(std::ostream& os, const Diagnostic& d,
                                        bool useColor, int indent) {
	std::string pad(static_cast<size_t>(indent * 2), ' ');

	// Location prefix (if valid).
	if (d.location.isValid()) {
		if (useColor) os << ansi::Bold;
		os << pad << d.location.file
		   << ':' << d.location.line
		   << ':' << d.location.column
		   << ": ";
		if (useColor) os << ansi::Reset;
	} else if (!pad.empty()) {
		os << pad;
	}

	os << severityLabel(d.severity, useColor) << ": ";
	if (useColor) os << ansi::Bold;
	os << d.message;
	if (useColor) os << ansi::Reset;
	os << '\n';

	// Attached notes/hints.
	for (const auto& sub : d.notes) {
		formatDiagnostic(os, sub, useColor, indent + 1);
	}
}

void DiagnosticEngine::emit(std::ostream& os, bool useColor) const {
	for (const auto& d : diagnostics_) {
		formatDiagnostic(os, d, useColor, 0);
	}

	// Summary line if there are errors or warnings.
	if (errorCount_ > 0 || warningCount_ > 0) {
		if (useColor) os << ansi::Bold;
		if (errorCount_ > 0) {
			os << "aborting due to " << errorCount_
			   << (errorCount_ == 1 ? " previous error" : " previous errors");
			if (warningCount_ > 0) {
				os << "; " << warningCount_
				   << (warningCount_ == 1 ? " warning" : " warnings") << " emitted";
			}
		} else {
			os << warningCount_
			   << (warningCount_ == 1 ? " warning" : " warnings") << " generated";
		}
		if (useColor) os << ansi::Reset;
		os << '\n';
	}
}

void DiagnosticEngine::emitToStderr(bool useColor) const {
	emit(std::cerr, useColor);
}

}  // namespace bytefrost
