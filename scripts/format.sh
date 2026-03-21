#!/bin/bash

path_to_project=$(pwd)

# format source code
find "$path_to_project/src" -name '*.cpp' -exec clang-format -i {} \;
find "$path_to_project/src" -name '*.h' -exec clang-format -i {} \;

# format include code
find "$path_to_project/include" -name '*.cpp' -exec clang-format -i {} \;
find "$path_to_project/include" -name '*.h' -exec clang-format -i {} \;

# format test code
find "$path_to_project/tests" -name '*.cpp' -exec clang-format -i {} \;
find "$path_to_project/tests" -name '*.h' -exec clang-format -i {} \;
