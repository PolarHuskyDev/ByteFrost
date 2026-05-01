#!/bin/bash

cmd=$1
preset=${2:-Release}
# Version: honour BF_VERSION env var, else detect from git tag, else fall back.
if [ -z "$BF_VERSION" ]; then
	BF_VERSION=$(git describe --tags --abbrev=0 2>/dev/null | sed 's/^v//')
	BF_VERSION=${BF_VERSION:-0.0.0-dev}
fi

if [ "$cmd" == "prepare" ] || [ "$cmd" == "build" ]; then
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
	echo "Commands:"
	echo "  prepare   Prepare the package (conan install)"
	echo "  build     Configure and build the package"
	echo "  help      Show this help message"
	echo ""
	echo "Version is read from the nearest git tag (vX.Y.Z)."
	echo "Override with:  BF_VERSION=1.2.3 $0 build"
}

case "$cmd" in
	prepare)
		conan install . --build=missing -s build_type=$preset
		;;
	build)
		buildPreset="conan-release"
		if [ "$preset" == "Debug" ]; then
			buildPreset="conan-debug"
		fi

		cmake --preset=$buildPreset -DBF_VERSION="$BF_VERSION"
		cmake --build --preset=$buildPreset
		;;
	help|*)
		help
		;;
esac
