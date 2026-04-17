export BUILD_DIR="../build-ragbot"
export CODE_DIR=$(pwd)
cp ./config.json $BUILD_DIR
cd $BUILD_DIR
gdb --args ./ragbot -c ./config.json
cd $CODE_DIR
