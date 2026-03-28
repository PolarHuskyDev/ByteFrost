#!/bin/bash

cmd=$1
preset=${2:-Release}

if [ $cmd == "prepare" ] || [ $cmd == "build" ]; then
	echo "Using preset: $preset"
	
	if [ "$preset" != "Debug" ] && [ "$preset" != "Release" ]; then
		echo "Invalid preset: $preset"
		echo "Valid presets are: Debug, Release"
		exit 1
	fi
fi


function help() {
	echo "Usage: $0 [command] [preset]"
	echo "Commands:"
	echo "  prepare   Prepare the package"
	echo "  build     Build the package"
	echo "  help      Show this help message"
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

		cmake --preset=$buildPreset
		cmake --build --preset=$buildPreset
		;;
	help|*)
		help
		;;
esac
