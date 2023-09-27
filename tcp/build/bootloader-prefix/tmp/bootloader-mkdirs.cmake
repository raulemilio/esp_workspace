# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/rome/eclipse/cpp-2023-06/esp-idf-v_5-1-1/esp-idf-v5.1.1/components/bootloader/subproject"
  "/home/rome/esp/esp_workspace/tcp/build/bootloader"
  "/home/rome/esp/esp_workspace/tcp/build/bootloader-prefix"
  "/home/rome/esp/esp_workspace/tcp/build/bootloader-prefix/tmp"
  "/home/rome/esp/esp_workspace/tcp/build/bootloader-prefix/src/bootloader-stamp"
  "/home/rome/esp/esp_workspace/tcp/build/bootloader-prefix/src"
  "/home/rome/esp/esp_workspace/tcp/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/rome/esp/esp_workspace/tcp/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/rome/esp/esp_workspace/tcp/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
