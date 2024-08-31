cmake_minimum_required(VERSION 3.29)
project(envyd VERSION 0.1 LANGUAGES C)

set(CMAKE_C_STANDARD 17)

# 1. sudo dnf config-manager --add-repo https://developer.download.nvidia.com/compute/cuda/repos/fedora39/x86_64/cuda-fedora39.repo
# 2. sudo dnf clean all
# 3. sudo dnf module disable nvidia-driver
# 4. sudo dnf install -y cuda
#... afterwards, the following must match: sudo find /usr -type f -name nvml.h
# 5. sudo dnf install -y json-c\*
# 6. nvdialog (https://github.com/tseli0s/nvdialog):
#    - download from releases
#    - sudo mkdir /usr/local/include/nvdialog/
#    - sudo mv include/ /usr/local/include/nvdialog/
#    - sudo mv libnvdialog.so /usr/local/lib/

# for anyone who's going to look at this in the future,
#  the author is completely incompetent in setting up anything
#  that might remotely *resemble* an actual build, so please fix it if you may :)

option(INSECURE "Disable authorization for setters" ON)

include_directories("/usr/local/cuda-12.6/include")
include_directories("/usr/include/json-c")
include_directories("/usr/local/include/nvdialog")

find_package(JSON-C REQUIRED)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
if(INSECURE)
    add_definitions(-DINSECURE)
endif()

add_executable(
        ${PROJECT_NAME}
        src/main.c
        src/helpers.h
        src/network.c
        src/network.h
)

target_link_libraries(
        ${PROJECT_NAME}
        PRIVATE "/usr/local/cuda-12.6/lib64/stubs/libnvidia-ml.so"
        PRIVATE "/usr/lib64/libjson-c.so"
        PRIVATE "/usr/local/lib/libnvdialog.so.2"
)
