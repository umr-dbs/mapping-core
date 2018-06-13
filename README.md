[![Build Status](https://travis-ci.org/umr-dbs/mapping-core.svg?branch=master)](https://travis-ci.org/umr-dbs/mapping-core)

# MAPPING Core Module
This module is part of MAPPING - Marburg's Analysis, Processing and Provenance of Information for Networked Geographics.

## Requirements
 * BZip2
 * JPEGTURBO
 * GEOS
 * Boost: date_time
 * GDAL
 * PNG
 * ZLIB
 * PQXX
 * CURL
 * SQLite3
 * LibArchive
 * Fcgi
 * Fcgi++
 * cmake >= 3.7
 * OpenCL

## Install
For information how to install MAPPING see [mapping-install.md](docs/mapping-install.md).

## Configuration
For information how to configure MAPPING see [mapping-configuration.md](docs/mapping-configuration.md).
If you want to run MAPPING as FCGI, see [Fast CGI configuration.md](docs/Fast%20CGI%20configuration.md).

## Building
```
cmake .
make
```

### Options
 * Debug Build
   * `-DCMAKE_BUILD_TYPE=Debug`
 * Release Build
   * `-DCMAKE_BUILD_TYPE=Release`
 * Module
   * `-DMAPPING_MODULE_PATH=<path>`
   * `-DMAPPING_MODULES="<module1;module2;...>"`


### Example
```
cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DMAPPING_MODULES="mapping-gfbio;mapping-r" \
  .
  
make -j4
```
