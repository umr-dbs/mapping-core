#!/usr/bin/env bash

## GDAL
add-apt-repository ppa:ubuntugis/ppa
## Poco
add-apt-repository ppa:gezakovacs/poco
## CMake >= 3.7
# add-apt-repository ppa:adrozdoff/cmake
add-apt-repository ppa:nschloe/cmake-nightly
## CLang 5.0
wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | apt-key add -
apt-add-repository "deb http://apt.llvm.org/xenial/ llvm-toolchain-xenial-5.0 main"

apt-get update
