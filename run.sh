export BUILD_DIR="../build-ragbot"
export CODE_DIR=$(pwd)
cp ./config.json $BUILD_DIR
cd $BUILD_DIR
gdb -ex "set debuginfod enabled on" -ex "set print thread-events off" --args ./ragbot -c ./config.json
cd $CODE_DIR
