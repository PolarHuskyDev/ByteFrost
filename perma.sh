#!/bin/bash

# Cross-platform build script for ByteFrost
# Supports Linux (Unix Makefiles) and Windows (Visual Studio + MSVC)

cmd=$1
preset=${2:-Release}

# Detect if colors are supported
if [ -t 1 ] && command -v tput &> /dev/null && [ "$(tput colors)" -ge 8 ]; then
	RED=$(tput setaf 1)
	GREEN=$(tput setaf 2)
	YELLOW=$(tput setaf 3)
	CYAN=$(tput setaf 6)
	BOLD=$(tput bold)
	NC=$(tput sgr0)
else
	RED=""
	GREEN=""
	YELLOW=""
	CYAN=""
	BOLD=""
	NC=""
fi

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

# Profile management
PROFILE_CONFIG=".conan/active-profile"

function detect_default_profile() {
	case "$OS" in
		windows) echo "windows-msvc" ;;
		linux)   echo "linux-gcc" ;;
		*)       echo "" ;;
	esac
}

function get_active_profile() {
	if [ -n "$BF_PROFILE" ]; then
		echo "$BF_PROFILE"
	elif [ -f "$PROFILE_CONFIG" ]; then
		cat "$PROFILE_CONFIG"
	else
		detect_default_profile
	fi
}

if [ "$cmd" == "prepare" ] || [ "$cmd" == "build" ]; then
	printf "${BOLD}${CYAN}===== ByteFrost Build Script =====${NC}\n"
	printf "  ${BOLD}Platform:${NC}      %s\n" "$OS"
	printf "  ${BOLD}Preset:${NC}        %s\n" "$preset"
	printf "  ${BOLD}BF_VERSION:${NC}    %s\n" "$BF_VERSION"
	printf "  ${BOLD}Conan profile:${NC} ${CYAN}%s${NC}\n" "$(get_active_profile)"
	printf "\n"

	if [ "$preset" != "Debug" ] && [ "$preset" != "Release" ]; then
		printf "${RED}Error:${NC} Invalid preset '%s'. Valid presets are: Debug, Release\n" "$preset"
		exit 1
	fi
fi


function help() {
	printf "Usage: ${CYAN}$0${NC} [command] [options]\n"
	printf "\n"
	printf "${BOLD}Commands:${NC}\n"
	printf "  ${CYAN}profile <name>${NC}    Set the active Conan profile (saved to $PROFILE_CONFIG)\n"
	printf "  ${CYAN}prepare [preset]${NC}  Install dependencies using the active profile\n"
	printf "  ${CYAN}build   [preset]${NC}  Configure and build the project\n"
	printf "  ${CYAN}clean${NC}             Remove build artifacts\n"
	printf "  ${CYAN}help${NC}              Show this help message\n"
	printf "\n"
	printf "${BOLD}Presets${NC} (default: Release):\n"
	printf "  ${GREEN}Release${NC}   Optimized build\n"
	printf "  ${YELLOW}Debug${NC}     Debug build with symbols\n"
	printf "\n"
	printf "${BOLD}Available profiles${NC} (in .conan/):\n"
	for f in .conan/*.profile; do
		printf "  ${CYAN}%s${NC}\n" "$(basename "$f" .profile)"
	done
	printf "\n"
	printf "${BOLD}Profile resolution order${NC} (highest priority first):\n"
	printf "  1. ${YELLOW}BF_PROFILE${NC} env var      BF_PROFILE=linux-gcc $0 prepare\n"
	printf "  2. ${YELLOW}%s${NC}   (set via: $0 profile <name>)\n" "$PROFILE_CONFIG"
	printf "  3. Auto-detect from OS\n"
	printf "\n"
	printf "Version is read from the nearest git tag (vX.Y.Z).\n"
	printf "Override with:  ${YELLOW}BF_VERSION=1.2.3${NC} $0 build\n"
}

function check_tools() {
	local missing=0
	
	for tool in git cmake conan; do
		if ! command -v "$tool" &> /dev/null; then
			printf "${RED}Error:${NC} '%s' not found in PATH\n" "$tool"
			missing=1
		fi
	done
	
	if [ $missing -eq 1 ]; then
		printf "\n${RED}Please install missing tools and try again.${NC}\n"
		exit 1
	fi
}

function set_profile() {
	local profile_name="$1"

	if [ -z "$profile_name" ]; then
		printf "${RED}Error:${NC} No profile name provided.\n"
		printf "Usage: ${CYAN}$0 profile <name>${NC}\n"
		printf "\n${BOLD}Available profiles:${NC}\n"
		for f in .conan/*.profile; do
			printf "  ${CYAN}%s${NC}\n" "$(basename "$f" .profile)"
		done
		exit 1
	fi

	local profile_file=".conan/${profile_name}.profile"
	if [ ! -f "$profile_file" ]; then
		printf "${RED}Error:${NC} Profile not found: %s\n" "$profile_file"
		printf "\n${BOLD}Available profiles:${NC}\n"
		for f in .conan/*.profile; do
			printf "  ${CYAN}%s${NC}\n" "$(basename "$f" .profile)"
		done
		exit 1
	fi

	echo "$profile_name" > "$PROFILE_CONFIG"
	printf "${GREEN}Active profile set to:${NC} ${CYAN}%s${NC}\n" "$profile_name"
}

function prepare() {
	check_tools

	local profile_name
	profile_name=$(get_active_profile)

	if [ -z "$profile_name" ]; then
		printf "${RED}Error:${NC} No active profile set and could not auto-detect one.\n"
		printf "Run: ${CYAN}$0 profile <name>${NC}\n"
		exit 1
	fi

	local profile_file=".conan/${profile_name}.profile"
	if [ ! -f "$profile_file" ]; then
		printf "${RED}Error:${NC} Profile file not found: %s\n" "$profile_file"
		printf "Run: ${CYAN}$0 profile <name>${NC}\n"
		exit 1
	fi

	# Persist the resolved profile so subsequent builds stay consistent
	echo "$profile_name" > "$PROFILE_CONFIG"

	printf "${YELLOW}>> Installing dependencies via Conan...${NC}\n"
	conan install . --build=missing --profile:all="$profile_file" -s:a build_type="$preset"

	if [ $? -ne 0 ]; then
		printf "${RED}Error:${NC} Conan install failed\n"
		exit 1
	fi
}

function build() {
	check_tools

	local profile_name
	profile_name=$(get_active_profile)

	if [ -z "$profile_name" ]; then
		printf "${RED}Error:${NC} No active profile. Run '${CYAN}$0 prepare${NC}' first.\n"
		exit 1
	fi

	# Determine build preset based on preset selection
	buildPreset="conan-release"
	if [ "$preset" == "Debug" ]; then
		buildPreset="conan-debug"
	fi

	# Multi-config generators (Visual Studio/Windows) configure once via
	# conan-default then select Release/Debug at build time.
	# Single-config generators (Unix Makefiles/Linux) bake the build type
	# into the configure preset, so conan-release/conan-debug serve both roles.
	if [ "$OS" == "windows" ]; then
		configPreset="conan-default"
	else
		configPreset="$buildPreset"
	fi

	printf "${YELLOW}>> Configuring with preset: %s${NC}\n" "$configPreset"
	cmake --preset="$configPreset" -DBF_VERSION="$BF_VERSION"
	if [ $? -ne 0 ]; then
		printf "${RED}Error:${NC} CMake configuration failed\n"
		exit 1
	fi
	
	printf "${YELLOW}>> Building with preset: %s${NC}\n" "$buildPreset"
	cmake --build --preset="$buildPreset"
	if [ $? -ne 0 ]; then
		printf "${RED}Error:${NC} Build failed\n"
		exit 1
	fi
	
	printf "${GREEN}Build complete!${NC}\n"
}

function clean() {
	printf "${YELLOW}>> Cleaning build artifacts...${NC}\n"
	rm -rf build CMakeUserPresets.json conaninfo.txt
	printf "${GREEN}Clean complete!${NC}\n"
}

case "$cmd" in
	profile)
		set_profile "$2"
		;;
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
		printf "${RED}Unknown command:${NC} %s\n\n" "$cmd"
		help
		exit 1
		;;
esac
