cmake_minimum_required(VERSION 3.29)
project(envyd C)

set(CMAKE_C_STANDARD 17)

# 1. sudo dnf config-manager --add-repo https://developer.download.nvidia.com/compute/cuda/repos/fedora39/x86_64/cuda-fedora39.repo
# 2. sudo dnf clean all
# 3. sudo dnf module disable nvidia-driver
# 4. sudo dnf install -y cuda
#... afterwards, the following must match: sudo find /usr -type f -name nvml.h
# 5. sudo dnf install -y json-c\*

# for anyone who's going to look at this in the future,
#  the author is completely incompetent in setting up anything
#  that might remotely *resemble* an actual build, so please fix it if you may :)

include_directories("/usr/local/cuda-12.6/include")
include_directories("/usr/include/json-c")

find_package(JSON-C REQUIRED)

add_executable(envyd src/main.c
        src/helpers.h
        src/network.c
        src/network.h)

target_link_libraries(envyd
        PRIVATE "/usr/local/cuda-12.6/lib64/stubs/libnvidia-ml.so"
        PRIVATE "/usr/lib64/libjson-c.so")