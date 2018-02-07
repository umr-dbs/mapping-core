#!/bin/bash
bin_path=$1
rm -f systemtests/data/ndvi.dat systemtests/data/ndvi.db
export MAPPING_CONFIGURATION=systemtests/mapping_test.conf ; bash systemtests/data/ndvi/import.sh $bin_path