export BUILD_DIR="../build-ragbot"
export CODE_DIR=$(pwd)
cd $BUILD_DIR
gdb --args ./ragbot -l --embed -m nomic-embed-v1.5 -d "/home/joe/source/Cataclysm-DDA/data/json"
cd $CODE_DIR

