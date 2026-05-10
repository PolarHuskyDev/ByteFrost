#pragma once

#include <string>
#include <vector>

namespace bytefrost {

enum class DiagnosticSeverity {
	Error,
	Warning,
	Note,
	Hint,
};

struct SourceLocation {
	std::string file;
	int line = 0;
	int column = 0;

	bool isValid() const { return line > 0; }
};

struct Diagnostic {
	DiagnosticSeverity severity;
	SourceLocation location;
	std::string message;
	/// Sub-diagnostics (notes, hints) attached to this diagnostic.
	std::vector<Diagnostic> notes;

	Diagnostic(DiagnosticSeverity sev, SourceLocation loc, std::string msg)
		: severity(sev), location(std::move(loc)), message(std::move(msg)) {}

	Diagnostic& addNote(SourceLocation loc, std::string msg) {
		notes.emplace_back(DiagnosticSeverity::Note, std::move(loc), std::move(msg));
		return *this;
	}

	Diagnostic& addHint(SourceLocation loc, std::string msg) {
		notes.emplace_back(DiagnosticSeverity::Hint, std::move(loc), std::move(msg));
		return *this;
	}
};

}  // namespace bytefrost
