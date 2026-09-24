#!/usr/bin/env sh
# One-command build without CMake: compiles the CLI and the tests with g++,
# runs the tests, then regenerates the playground traces if node is present.
#   ./build.sh
set -e
cd "$(dirname "$0")"
mkdir -p build
SRC="Column.cpp CsvReader.cpp DataFrame.cpp DataFrameView.cpp GroupedDataFrame.cpp"
FLAGS="-std=c++20 -O2 -Wall -Wextra -I."
# MinGW: link the runtime statically so a foreign libstdc++-6.dll on PATH can't break exceptions.
case "$(uname -s)" in MINGW*|MSYS*|CYGWIN*) FLAGS="$FLAGS -static" ;; esac
EXE=""; [ -n "$WINDIR" ] && EXE=".exe"
${CXX:-g++} $FLAGS $SRC main.cpp -o build/DataFrame$EXE
${CXX:-g++} $FLAGS $SRC tests/test_dataframe.cpp -o build/dataframe_tests$EXE
./build/dataframe_tests$EXE web/data
if command -v node >/dev/null 2>&1; then
  node tools/gen-traces.mjs
  node tools/verify-port.mjs
fi
