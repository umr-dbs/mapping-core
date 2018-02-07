#ifndef SERVICES_OGCSERVICE_H
#define SERVICES_OGCSERVICE_H

#include "services/httpservice.h"
#include "datatypes/spatiotemporal.h"
#include "datatypes/simplefeaturecollection.h"
#include "datatypes/raster/raster_priv.h"
#include "operators/provenance.h"

/*
 * This is an abstract helper class to implement services of the OGC.
 * It contains functionality common to multiple OGC protocols.
 */
class OGCService : public HTTPService {
	protected:
		using HTTPService::HTTPService;

		CrsId parseCrsId(const Parameters &params, const std::string &key, CrsId def = CrsId::from_epsg_code(3857));
		TemporalReference parseTime(const Parameters &params) const;
		SpatialReference parseBBOX(const std::string bbox_str, CrsId crsId = CrsId::from_epsg_code(3857), bool allow_infinite = false);

		void outputImage(GenericRaster *raster, bool flipx = false, bool flipy = false, const std::string &colors = "", Raster2D<uint8_t> *overlay = nullptr);
		void outputSimpleFeatureCollectionGeoJSON(SimpleFeatureCollection *collection, bool displayMetadata = false);
		void outputSimpleFeatureCollectionCSV(SimpleFeatureCollection *collection);
		void outputSimpleFeatureCollectionARFF(SimpleFeatureCollection* collection);

		void exportZip(const std::string &operatorGraph, const char* data, size_t dataLength, const std::string &format, ProvenanceCollection &provenance);

		static constexpr const char* EXPORT_MIME_PREFIX = "application/x-export;";
};


#endif
