# 1. 新建一个 Release build 目录
cd D:/WangWorkSpace/C++WithQt/Demo01
mkdir build_release
cd build_release

# 2. 配置 CMake，指定 Release
cmake -DCMAKE_BUILD_TYPE=Release ..

# 3. 编译
cmake --build . --config Release