#!/bin/bash
bin_path=$1
rm -f test/systemtests/data/ndvi.dat test/systemtests/data/ndvi.db
export MAPPING_CONFIGURATION=test/systemtests/mapping_test.conf ; bash test/systemtests/data/ndvi/import.sh $bin_path