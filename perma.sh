#!/bin/bash

# Cross-platform build script for ByteFrost
# Supports Linux (Unix Makefiles) and Windows (Visual Studio + MSVC)

cmd=$1
preset=${2:-Release}

# Detect OS
OS_TYPE="$(uname -s)"
case "$OS_TYPE" in
	Linux*) OS="linux" ;;
	MINGW*|MSYS*|CYGWIN*) OS="windows" ;;
	Darwin*) OS="macos" ;;
	*) OS="unknown" ;;
esac

# Version: honour BF_VERSION env var, else detect from git tag, else fall back.
if [ -z "$BF_VERSION" ]; then
	BF_VERSION=$(git describe --tags --abbrev=0 2>/dev/null | sed 's/^v//')
	BF_VERSION=${BF_VERSION:-0.0.0-dev}
fi

if [ "$cmd" == "prepare" ] || [ "$cmd" == "build" ]; then
	echo "Platform:      $OS"
	echo "Using preset:  $preset"
	echo "BF_VERSION:    $BF_VERSION"

	if [ "$preset" != "Debug" ] && [ "$preset" != "Release" ]; then
		echo "Invalid preset: $preset"
		echo "Valid presets are: Debug, Release"
		exit 1
	fi
fi


function help() {
	echo "Usage: $0 [command] [preset]"
	echo ""
	echo "Commands:"
	echo "  prepare   Prepare the build (install dependencies)"
	echo "  build     Configure and build the project"
	echo "  clean     Remove build artifacts"
	echo "  help      Show this help message"
	echo ""
	echo "Presets (default: Release):"
	echo "  Release   Optimized build"
	echo "  Debug     Debug build with symbols"
	echo ""
	echo "Version is read from the nearest git tag (vX.Y.Z)."
	echo "Override with:  BF_VERSION=1.2.3 $0 build"
	echo ""
	echo "Platform Support:"
	echo "  - Linux (GCC/Clang + Unix Makefiles)"
	echo "  - Windows (MSVC + Visual Studio)"
	echo "  - macOS (Clang + Unix Makefiles)"
}

function check_tools() {
	local missing=0
	
	for tool in git cmake conan; do
		if ! command -v "$tool" &> /dev/null; then
			echo "Error: '$tool' not found in PATH"
			missing=1
		fi
	done
	
	if [ $missing -eq 1 ]; then
		echo ""
		echo "Please install missing tools and try again."
		exit 1
	fi
}

function prepare() {
	check_tools
	
	echo "Installing dependencies via Conan..."
	
	if [ "$OS" == "windows" ]; then
		# Windows: Use Visual Studio generator
		conan install . --build=missing -s build_type="$preset" -s compiler.version=193 -s compiler="msvc"
	else
		# Linux/macOS: Use Unix Makefiles
		conan install . --build=missing -s build_type="$preset"
	fi
	
	if [ $? -ne 0 ]; then
		echo "Error: Conan install failed"
		exit 1
	fi
}

function build() {
	check_tools
	
	# Determine build preset based on preset selection
	buildPreset="conan-release"
	if [ "$preset" == "Debug" ]; then
		buildPreset="conan-debug"
	fi
	
	echo "Configuring with preset: conan-default"
	cmake --preset="conan-default" -DBF_VERSION="$BF_VERSION"
	if [ $? -ne 0 ]; then
		echo "Error: CMake configuration failed"
		exit 1
	fi
	
	echo "Building with preset: $buildPreset"
	cmake --build --preset="$buildPreset"
	if [ $? -ne 0 ]; then
		echo "Error: Build failed"
		exit 1
	fi
	
	echo "Build complete!"
}

function clean() {
	echo "Cleaning build artifacts..."
	rm -rf build CMakeUserPresets.json conaninfo.txt
	echo "Clean complete!"
}

case "$cmd" in
	prepare)
		prepare
		;;
	build)
		build
		;;
	clean)
		clean
		;;
	help|"")
		help
		;;
	*)
		echo "Unknown command: $cmd"
		echo ""
		help
		exit 1
		;;
esac
