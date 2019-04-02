
#include "source_backend.h"
#include "gdal_source_backend.h"
#include "util/configuration.h"

rts::SourceBackend::SourceBackend(const QueryRectangle &qrect, Json::Value &params) : qrect(qrect), params(params) {
    Json::Value tileResJson = params["tile_resolution"];
    //if tileRes parameter is not provided, use standard value from configuration.
    if(tileResJson.isNull())  {
        tileRes.resX = Configuration::get<int>("rastertimeseries.source.tile_resolution_x", 1000);
        tileRes.resY = Configuration::get<int>("rastertimeseries.source.tile_resolution_y", 1000);
    } else {
        tileRes.resX = tileResJson[0].asInt();
        tileRes.resY = tileResJson[1].asInt();
    }

}

void rts::SourceBackend::beforeTemporalIncrease() {

}

void rts::SourceBackend::beforeSpatialIncrease() {

}

std::unordered_map<std::string, rts::BackendConstructor> rts::SourceBackendRegistration::registered_constructors;

std::unique_ptr<rts::SourceBackend> rts::SourceBackend::create(const std::string &backendName, const QueryRectangle &qrect, Json::Value &params) {
    auto constructor = rts::SourceBackendRegistration::registered_constructors[backendName];
    return constructor(qrect, params);
}

rts::SourceBackendRegistration::SourceBackendRegistration(const char *name, BackendConstructor constructor) {
    rts::SourceBackendRegistration::registered_constructors[std::string(name)] = constructor;
}