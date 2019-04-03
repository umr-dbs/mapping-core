
#ifndef MAPPING_CORE_RTS_BACKEND_H
#define MAPPING_CORE_RTS_BACKEND_H

#include <json/json.h>
#include <unordered_map>
#include "datatypes/descriptor.h"
#include "datatypes/spatiotemporal.h"
#include "operators/queryrectangle.h"

namespace rts {

    class SourceState;

    /**
     * Abstract base class for backends for the RasterTimeSeriesSource operator.
     */
    class SourceBackend {
    public:
        /**
         * Instantiate a source backend by its name through backend registration.
         */
        static std::unique_ptr<SourceBackend> create(const std::string &backendName, const QueryRectangle &qrect, Json::Value &params);

        SourceBackend(const QueryRectangle &qrect, Json::Value &params);

        virtual ~SourceBackend() = default;

        virtual void initialize() = 0;

        virtual bool supportsOrder(ProcessingOrder o) const = 0;

        /**
         * Method for returning a descriptor for a specific time and space. Will be used by nextDescriptor()
         * as well as getDescriptor().
         * @param time The time identifying the raster to be used.
         * @param pixelStartX pixel positions from where to return the tile.
         * @param pixelStartY pixel positions from where to return the tile.
         * @return Descriptor for the tile.
         */
        virtual OptionalDescriptor createDescriptor(const SourceState &s, GenericOperator *op, const QueryTools &tools) = 0;

        /**
         * Backend can react to the source operator increasing the spatial state. Standard implementation does nothing.
         */
        virtual void beforeSpatialIncrease();

        /**
         * Backend can react to the source operator increasing the temporal state. Standard implementation does nothing.
         */
        virtual void beforeTemporalIncrease();

        /**
         * Increase the currTime variable to the time the next raster starts.
         */
        virtual void increaseCurrentTime(double &currTime) = 0;

        /**
         * Get the end time of the validity of the current raster.
         * @return The exclusive end time of the raster's temporal validity.
         */
        virtual double getCurrentTimeEnd(double currTime) const = 0;

        /**
         * The origin of the raster time series must be provided by the backend.
         */
        virtual Origin getOrigin() const = 0;

        /**
         * The start time of the dataset used by the operator.
         */
        double datasetStartTime;
        /**
         * The end time of the dataset used by the operator.
         */
        double datasetEndTime;

        /**
         * The resolution of each created tile.
         */
        Resolution tileRes;

    protected:
        Json::Value &params;
        QueryRectangle qrect;
    };

    //TODO:
    class SourceBackendListing {
    public:
        //static void add();
    };

    //Backend Registration: same as for operators, but with 'using' syntax and static class variable for the map.
    using BackendConstructor = std::unique_ptr<rts::SourceBackend> (*)(const QueryRectangle &qrect, Json::Value &params);

    class SourceBackendRegistration {
    public:
        SourceBackendRegistration(const char *name, std::unique_ptr<SourceBackend> (*constructor)(const QueryRectangle &qrect, Json::Value &params));
        static std::unordered_map<std::string, BackendConstructor> registered_constructors;
    };

#define REGISTER_RTS_SOURCE_BACKEND(classname, name) static std::unique_ptr<rts::SourceBackend> create##classname(const QueryRectangle &qrect, Json::Value &params) { return std::make_unique<classname>(qrect, params); } static SourceBackendRegistration register_##classname(name, create##classname)



}

#endif //MAPPING_CORE_RTS_BACKEND_H
