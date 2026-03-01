export BUILD_DIR="../build-ragbot"
export CODE_DIR=$(pwd)
cd $BUILD_DIR
gdb --args ./ragbot -c ./config.json -d "/home/joe/source/Cataclysm-DDA/data/json"
cd $CODE_DIR
