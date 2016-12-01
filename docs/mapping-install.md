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


### Ubuntu 16 LTS

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
```


## MAPPING Dependencies
Repository for GDAL:
```
sudo add-apt-repository ppa:ubuntugis/ppa
sudo apt-get update
```

Install Packages:
```
MAPPING_LIBS=""

MAPPING_LIBS+=" clang"
MAPPING_LIBS+=" g++"
MAPPING_LIBS+=" make"

MAPPING_LIBS+=" libpng-dev"
MAPPING_LIBS+=" libjpeg-dev"
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
git clone ssh://phabricator-vcs@dbs.mathematik.uni-marburg.de:91/diffusion/VAT/geo.git
cd geo/mapping
vim Makefile.local
```

Add the following lines to add the necessary modules:
```
MODULES_LIST+=mapping-gfbio mapping-distributed mapping-r
```

Build the project:
```
make -j8
```

## Configure MAPPING
```
vim mapping.conf
```

Put in the following content. Modify this if necessary.

### Standalone
**TODO**

#### Index Node
**TODO**

#### Cluster Node
```
log.level=INFO

################################
#
# CGI/NODE CONFIG
#
################################

global.debug=0
global.opencl.forcecpu=1

operators.r.socket=/tmp/rserver_socket2
operators.r.host=127.0.0.1
operators.r.port=12349

rserver.port=12349
rserver.loglevel=debug
rserver.packages=raster,caret,randomForest

rasterdb.tileserver.port=12345
rasterdb.backend=local
rasterdb.local.location=/home/rastersources/

################################
#
# NODE CONFIG
#
################################

nodeserver.port=12348
nodeserver.threads=16
nodeserver.cache.manager=remote
nodeserver.cache.raster.size=524288000
nodeserver.cache.points.size=524288000
nodeserver.cache.lines.size=524288000
nodeserver.cache.polygons.size=524288000
nodeserver.cache.plots.size=524288000
nodeserver.cache.strategy=uncached

################################
#
# INDEX CONFIG
#
################################

indexserver.port=12346
indexserver.host=***REMOVED***

###############################
#
# Operator Config
#
###############################

operators.gfbiosource.dbcredentials=user = 'gfbio' host = '***REMOVED***' password = '***REMOVED***' dbname = 'gfbio'
gfbio.abcd.datapath=/home/gfbio/abcd/

```

### Get Data
**TODO**

### Firewall
Open up your firewall for the remote ports.

### Start Node
```
./cache_node
```