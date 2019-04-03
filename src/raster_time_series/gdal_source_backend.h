
#ifndef RASTER_TIME_SERIES_GDAL_SOURCE_H
#define RASTER_TIME_SERIES_GDAL_SOURCE_H

#include "time_step.h"
#include <gdal_priv.h>
#include <boost/date_time/posix_time/posix_time.hpp>

#include "source_backend.h"

namespace rts {

    /**
     * RasterTimeSeries source operator backend for loading loading raster data with the GDAL library.
     * The raster datasets used by this backend must be in regular time intervals.
     */
    class GDALSourceBackend : public SourceBackend {
    public:
        GDALSourceBackend(const QueryRectangle &qrect, Json::Value &params);

        void initialize() override;
        bool supportsOrder(ProcessingOrder o) const override;
        OptionalDescriptor createDescriptor(const SourceState &s, GenericOperator *op, const QueryTools &tools) override;
        double getCurrentTimeEnd(double currTime) const override;
        void increaseCurrentTime(double &currTime) override;
        void beforeTemporalIncrease() override;
        Origin getOrigin() const override;
    private:
        std::shared_ptr<GDALDataset> currDataset;
        GDALRasterBand *currRasterband; //this can stay a normal ptr, because it is handled by the dataset. The dataset now always has to live as long as the rasterband. maybe put them in one structure?
        double currDatasetTime;

        TimeStep timeStep;
        std::string timeFormat;
        std::string baseFileName;
        std::string path;
        int channel;
        Origin origin;
        std::map<std::string, std::shared_ptr<GDALDataset>> openDatasets;

        Json::Value loadDatasetJson(const std::string &name);
        double parseIsoTime(const std::string &str) const;
        void loadCurrentGdalDataset(double time, ProcessingOrder order);
        std::string timeToString(time_t time, const std::string &timeFormat) const;
    };

}

#endif //RASTER_TIME_SERIES_GDAL_SOURCE_H
