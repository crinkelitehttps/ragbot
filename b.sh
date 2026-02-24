export CODE_DIR=$(pwd)
echo "running qmake"
qmake ../ragbot

echo "running make"
make -j$(nproc)

cp ../ragbot/conversations.db ./
cd $CODE_DIR
