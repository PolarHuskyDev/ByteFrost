#!/bin/bash

path_to_project=$(pwd)

if ! command -v clang-format &> /dev/null
then
	echo "Error: clang-format is not installed. Please install it to format the code."
	exit 0
fi

# format source code
find "$path_to_project/src" -name '*.cpp' -exec clang-format -i {} \;
find "$path_to_project/src" -name '*.h' -exec clang-format -i {} \;

# format include code
find "$path_to_project/include" -name '*.cpp' -exec clang-format -i {} \;
find "$path_to_project/include" -name '*.h' -exec clang-format -i {} \;

# format test code
find "$path_to_project/tests" -name '*.cpp' -exec clang-format -i {} \;
find "$path_to_project/tests" -name '*.h' -exec clang-format -i {} \;
