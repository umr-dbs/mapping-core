# MAPPING Installation

## Supported Operating Systems
 * Ubuntu 16.04 LTS
 * Ubuntu 17.04
 * Ubuntu 17.10

## OpenCL

### Intel CPU only

#### Ubuntu 16.04 LTS
```
apt-get install --yes \
    tar \
    wget \
    lsb-core

wget --no-verbose http://registrationcenter-download.intel.com/akdlm/irc_nas/9019/opencl_runtime_16.1.1_x64_ubuntu_6.4.0.25.tgz
tar -xzf opencl_runtime_16.1.1_x64_ubuntu_6.4.0.25.tgz
cd opencl_runtime_16.1.1_x64_ubuntu_6.4.0.25

./install.sh --silent ../scripts/opencl-silent.cfg

apt-get install --yes \
    ocl-icd-opencl-dev \
    opencl-headers \
    clinfo

wget --no-verbose \
    --output-document=/usr/include/CL/cl.hpp \
    https://www.khronos.org/registry/OpenCL/api/2.1/cl.hpp

clinfo
```

#### Others
download intel driver:
https://software.intel.com/en-us/articles/opencl-drivers#latest_linux_driver
```
sudo usermod -a -G video USERNAME
sudo apt-get install xz-utils
mkdir intel-opencl
tar -C intel-opencl -Jxf intel-opencl-r3.0-BUILD_ID.x86_64.tar.xz
tar -C intel-opencl -Jxf intel-opencl-devel-r3.0-BUILD_ID.x86_64.tar.xz
tar -C intel-opencl -Jxf intel-opencl-cpu-r3.0-BUILD_ID.x86_64.tar.xz
sudo cp -R intel-opencl/* /
sudo ldconfig

sudo apt install clinfo
sudo apt install opencl-headers
sudo apt install ocl-icd-opencl-dev

clinfo
```


## MAPPING Dependencies
### For Ubuntu ≤ 16
Add repository for GDAL:
```
sudo add-apt-repository ppa:ubuntugis/ppa
sudo apt-get update
```

Add repository for CMAKE
```
sudo add-apt-repository ppa:adrozdoff/cmake
sudo apt-get update
```

### For Ubuntu ≤ 17.10
```
sudo add-apt-repository ppa:gezakovacs/poco
sudo apt-get update
```

### Install Packages:
```
# Programs
apt-get install --yes \
    clang \
    cmake \
    curl \
    git \
    make \
    sqlite3 \
    xxdiff

# Dependencies
apt-get install --yes \
    libpng-dev \
    libjpeg-dev \
    libgeos-dev \
    libgeos++-dev \
    libbz2-dev \
    libcurl3-dev \
    libboost-all-dev \
    libsqlite3-dev \
    liburiparser-dev \
    libgtest-dev \
    libfcgi-dev \
    libxerces-c-dev \
    libarchive-dev \
    libproj-dev \
    valgrind \
    r-cran-rcpp \
    libpqxx-dev \
    libgdal-devs \
    libpoco-dev
```

## Checkout MAPPING
Generate SSH key and add it to GitHub. Use HTTPS alternatively.
```
ssh-keygen -t rsa
```

Checkout Mapping
```
# Mandatory
git clone git@github.com:umr-dbs/mapping-core.git
# Optional Modules
git clone git@github.com:umr-dbs/mapping-gfbio.git
git clone git@github.com:umr-dbs/mapping-r.git
```

Build the project:
```
cmake -DCMAKE_BUILD_TYPE=Release .
# add -DMAPPING_MODULES="mapping-r" for adding module R

make -j$(cat /proc/cpuinfo | grep processor | wc -l)
```

## Run Tests
```
make test
```
