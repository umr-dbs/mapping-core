# GDALSource
The GDALSource allows incorporating arbitrary data sets that can be read by GDAL. Adding a data set requires defining a .json file in the `gdalsource.datasets.path`directory that specifies how the rasters (time series) are read from the data set. The name of the .json file gives the name of the data set.

## Example
```
{
   "channels" : [
      {
         "datatype" : "Float32",
         "nodata" : -3.4e+38 ,
         "unit" : {
            "interpolation" : "unknown",
            "max" : 23.9866,
            "measurement" : "temperature",
            "min" : -56.5146
         }
      }
   ],
   "coords" : {
      "epsg" : "4326",
      "origin" : [ -180, 90 ],
      "scale" : [ 0.008333333333333,-0.008333333333333 ],
      "size" : [ 43200, 21600 ]
   },
   "dataset_name" : "TestDataset",
   "file_name" : "TestDataset_%%%TIME_STRING%%%.tif",
   "path" : "gdal_source/data/test",
   "provenance" : {
      "citation" : "citation dup dup",
      "license" : "some license",
      "uri" : "uri uri slash dot uri"
   },
   "time_interval" : {
         "unit" : "Month",
         "value" : 1
   },
   "time_format" : "%m",
   "time_start" : "2000-01-01T00:00:00Z",
   "time_end" : "2010-01-01T00:00:00Z"
}
```

## Time Series
Raster time series assume that there are multiple raster files for individual time periods. The `file_name` thus supports the specification of a placeholder where the time string is defined: `%%%TIME_STRING%%%`.
For a given point in time in a query rectangle, the GDALSource finds the corresponding raster file using the `time_start` and `interval`parameters. The latter one secifies how long each raster file is valid.
The query time is snapped to the beginning of the validity interval and inserted into the `file_name` as specified in the `time_format`.

## Channels
The channel definition under `channels` allows for overriding the specification of the time parameters and for setting the id of the target channel in the target file as `channel`.
