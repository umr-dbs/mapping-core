#!/usr/bin/env bash

set -e # Exit on error
set -x # Output command

apt-get install --yes \
    tar \
    wget \
    lsb-core

wget --no-verbose \
    http://registrationcenter-download.intel.com/akdlm/irc_nas/9019/opencl_runtime_16.1.1_x64_ubuntu_6.4.0.25.tgz
tar -xzf opencl_runtime_16.1.1_x64_ubuntu_6.4.0.25.tgz
cd opencl_runtime_16.1.1_x64_ubuntu_6.4.0.25

./install.sh --silent /app/mapping-core/docker-files/opencl-silent.cfg

apt-get install --yes \
    ocl-icd-libopencl1 \
    opencl-headers \
    clinfo

wget --no-verbose \
    --output-document=/usr/include/CL/cl.hpp \
    https://www.khronos.org/registry/OpenCL/api/2.1/cl.hpp

clinfo
