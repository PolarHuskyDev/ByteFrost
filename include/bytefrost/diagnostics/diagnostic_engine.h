#pragma once

#include <iosfwd>
#include <string>
#include <vector>

#include "bytefrost/diagnostics/diagnostic.h"

namespace bytefrost {

/// Central diagnostics collector for one compilation unit.
///
/// Usage:
///   DiagnosticEngine diag;
///   diag.error({file, line, col}, "undefined variable 'x'");
///   if (diag.hasErrors()) { return; }
///   diag.emitToStderr();
class DiagnosticEngine {
public:
	DiagnosticEngine() = default;

	/// Emit an error diagnostic. Increments the error count.
	Diagnostic& error(SourceLocation loc, std::string msg);

	/// Emit a warning diagnostic.
	Diagnostic& warning(SourceLocation loc, std::string msg);

	/// Emit a standalone note (not attached to an existing diagnostic).
	Diagnostic& note(SourceLocation loc, std::string msg);

	/// Convenience: error with no source location.
	Diagnostic& error(std::string msg);
	Diagnostic& warning(std::string msg);

	bool hasErrors()   const { return errorCount_ > 0; }
	bool hasWarnings() const { return warningCount_ > 0; }
	size_t errorCount()   const { return errorCount_; }
	size_t warningCount() const { return warningCount_; }

	const std::vector<Diagnostic>& all() const { return diagnostics_; }

	/// Format and write all diagnostics to the given stream.
	/// Pass useColor = false for plain text (e.g. CI logs, tests).
	void emit(std::ostream& os, bool useColor = true) const;

	/// Shorthand: emit to stderr.
	void emitToStderr(bool useColor = true) const;

	/// Clear all accumulated diagnostics and reset counts.
	void clear();

private:
	std::vector<Diagnostic> diagnostics_;
	size_t errorCount_   = 0;
	size_t warningCount_ = 0;

	static std::string severityLabel(DiagnosticSeverity sev, bool useColor);
	static void formatDiagnostic(std::ostream& os, const Diagnostic& d, bool useColor,
	                             int indent = 0);
};

}  // namespace bytefrost
