# MAPPING Core Module

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
 * Optional
   * OpenCL

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
 * Enable/Disabled OpenCL
   * `-DUSE_OPENCL=<ON/OFF>`
 * Module
   * `-DMAPPING_MODULE_PATH=<path>`
   * `-DMAPPING_MODULES=<module1,module2,...>`


### Example
```
cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DUSE_OPENCL=ON \
  -DMAPPING_MODULES="mapping-gfbio;mapping-r" \ 
  .
  
make -j20
```