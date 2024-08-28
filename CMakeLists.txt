cmake_minimum_required(VERSION 3.29)
project(envyd C)

set(CMAKE_C_STANDARD 17)

# 1. sudo dnf config-manager --add-repo https://developer.download.nvidia.com/compute/cuda/repos/fedora39/x86_64/cuda-fedora39.repo
# 2. sudo dnf clean all
# 3. sudo dnf module disable nvidia-driver
# 4. sudo dnf -y install cuda
#... afterwards, the following must match: sudo find /usr -type f -name nvml.h

include_directories("/usr/local/cuda-12.5/include")

add_executable(envyd src/main.c
        src/nvml/actions.c
        src/nvml/actions.h
        src/helpers.h
        src/network/network.c
        src/network/network.h)

target_link_libraries(envyd PRIVATE "/usr/local/cuda-12.5/lib64/stubs/libnvidia-ml.so")