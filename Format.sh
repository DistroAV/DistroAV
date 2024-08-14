set -e
echo "Formatting..."
./build-aux/run-cmake-format
./build-aux/run-clang-format
