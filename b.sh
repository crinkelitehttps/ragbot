SOURCE_DIR=$(pwd)
BUILD_DIR="$SOURCE_DIR/../build-ragbot"

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

echo "running qmake"
qmake "$SOURCE_DIR"

echo "running make"
make -j$(nproc)

cp "$SOURCE_DIR/conversations.db" ./ 2>/dev/null || true
cd "$SOURCE_DIR"
