export CODE_DIR=$(pwd)
cd ../build-ragbot/
echo "running make clean"
make clean

echo "running qmake"
qmake ../ragbot

echo "running make"

bear -- make -j$(nproc)
cd $CODE_DIR
