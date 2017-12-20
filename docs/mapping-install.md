# MAPPING Installation

## OpenCL

### Ubuntu 14 LTS

Download newest OpenCL runtime from <https://software.intel.com/en-us/articles/opencl-drivers>.

#### CPU-only
```
wget http://registrationcenter-download.intel.com/akdlm/irc_nas/9019/opencl_runtime_16.1.1_x64_ubuntu_6.4.0.25.tgz
tar -xzf opencl_runtime_16.1.1_x64_ubuntu_6.4.0.25.tgz
cd opencl_runtime_16.1.1_x64_ubuntu_6.4.0.25

sudo apt-get install lsb-core

sudo ./install.sh
```

Install headers
```
sudo apt-get install ocl-icd-opencl-dev
```

Verify that the installation was successful.
```
sudo apt-get install clinfo
clinfo
```

#### GPU
**TODO**


### Ubuntu 16 LTS / Ubuntu 17.04

#### Open CL

##### Intel CPU only
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
clinfo
```


## MAPPING Dependencies
For Ubuntu <= 16:
Repository for GDAL:
```
sudo add-apt-repository ppa:ubuntugis/ppa
sudo apt-get update
```

For Ubuntu <= 16 also do
```
sudo add-apt-repository ppa:adrozdoff/cmake
sudo apt-get update
```


Install Packages:
```
MAPPING_LIBS=""

MAPPING_LIBS+=" clang"
MAPPING_LIBS+=" g++"
MAPPING_LIBS+=" make"
MAPPING_LIBS+=" cmake"

MAPPING_LIBS+=" libpng-dev"
MAPPING_LIBS+=" libjpeg-dev"
MAPPING_LIBS+=" libturbojpeg0-dev"
MAPPING_LIBS+=" libgeos-dev"
MAPPING_LIBS+=" libgeos++-dev"
MAPPING_LIBS+=" libbz2-dev"
MAPPING_LIBS+=" libcurl3-dev"
MAPPING_LIBS+=" libboost-all-dev"
MAPPING_LIBS+=" libsqlite3-dev"
MAPPING_LIBS+=" liburiparser-dev"
MAPPING_LIBS+=" libgtest-dev"
MAPPING_LIBS+=" libfcgi-dev"
MAPPING_LIBS+=" libxerces-c-dev"
MAPPING_LIBS+=" libarchive-dev"

MAPPING_LIBS+=" libproj-dev"
MAPPING_LIBS+=" git"
MAPPING_LIBS+=" valgrind"

MAPPING_LIBS+=" r-cran-***REMOVED***"
MAPPING_LIBS+=" libpqxx-dev"

MAPPING_LIBS+=" libgdal-dev"

sudo apt-get install $MAPPING_LIBS
```

## Checkout MAPPING
Generate SSH key and add it to Phabricator. Use HTTP alternatively.
```
ssh-keygen -t rsa
```

Checkout Mapping
```
git clone ssh://phabricator-vcs@dbs-projects.mathematik.uni-marburg.de:91/diffusion/MAPPINGCORE/mapping-core.git
cd mapping-core
vim Makefile.local
```

Add the following lines to add the necessary modules:
```
MODULES_PATH=..
MODULES_LIST+=mapping-gfbio mapping-distributed mapping-r
```
This requires the modules to be checked out into the same folder as mapping-core.


Build the project:
```
make -j8
```

#Tests

## Unit tests
Run via `make unittest`

## Systemtests
Run via `make systemtest`

Configure llvm-symbolizer path in Makefile.local it or create link at /usr/lib/llvm-symbolizer

e.g.
```
LLVM_SYMBOLIZER=/usr/lib/llvm-3.8/bin/llvm-symbolizer
```

 
