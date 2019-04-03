

#include "datatypes/raster.h"
#include "util/gdal.h"
#include "util/log.h"
#include "util/CrsDirectory.h"

#include <mutex>

#include <gdal_alg.h>

#include <ogr_spatialref.h>


namespace GDAL {


static std::once_flag gdal_init_once;

static void MyGDALErrorHandler(CPLErr eErrClass, int err_no, const char *msg) {
	if (msg == nullptr || *msg == '\0')
		return;

	// Workaround: Reprojection of MSG data will trigger this message many many times.
	// Reduce its log level to reduce log clutter.
	if (eErrClass == CE_Failure && err_no == CPLE_AppDefined &&
		(strcmp("tolerance condition error", msg) == 0 ||
		 strncmp("Reprojection failed", msg, 19) == 0)) {
		eErrClass = CE_Debug;
	}


	if (eErrClass == CE_Warning) {
		Log::warn("GDAL Warning [%d] %s", err_no, msg);
	}
	else if (eErrClass == CE_Failure) {
		Log::error("GDAL Failure [%d] %s", err_no, msg);
	}
	else if (eErrClass == CE_Fatal) {
		Log::error("GDAL Fatal [%d] %s", err_no, msg);
		// GDAL says that the error handler should not return on a CE_Fatal error.
		exit(5);
	}
	else {
		// Make sure not to lose any messages, but put everything else under "debug".
		Log::debug("GDAL Debug [%d] %s", err_no, msg);
	}
}

void init() {
	std::call_once(gdal_init_once, []{
		GDALAllRegister();
		CPLSetErrorHandler(MyGDALErrorHandler);
		//GetGDALDriverManager()->AutoLoadDrivers();
	});
}


std::string WKTFromCrsId(const CrsId &crsId) {
	OGRSpatialReference ogrRef(nullptr);

	// try if gdal knows crs
	OGRErr error = ogrRef.SetFromUserInput(crsId.to_string().c_str());

	if(error == OGRERR_NONE) {
		char *pszResult = nullptr;
		ogrRef.exportToWkt(&pszResult);
		std::string wkt(pszResult);
		CPLFree(pszResult);
		return wkt;
	} else {
		// check mapping crs directory
        auto wkt = CrsDirectory::getWKTForCrsId(crsId);

		if(wkt.empty()) {
			throw ArgumentException("Unknown CrsId specified");
		}
		return wkt;
	}
}



CRSTransformer::CRSTransformer(CrsId in_crsId, CrsId out_crsId) : in_crsId(in_crsId), out_crsId(out_crsId), transformer(nullptr) {
	init();

	if (in_crsId == CrsId::unreferenced() || out_crsId == CrsId::unreferenced())
		throw GDALException("in- or out-crsId is UNKNOWN");
	if (in_crsId == out_crsId)
		throw GDALException("Cannot transform when in_crsId == out_crsId");

	transformer = GDALCreateReprojectionTransformer(WKTFromCrsId(in_crsId).c_str(), WKTFromCrsId(out_crsId).c_str());
	if (!transformer)
		throw GDALException("Could not initialize ReprojectionTransformer");
}

CRSTransformer::CRSTransformer(CRSTransformer &&other)
		: in_crsId(other.in_crsId), out_crsId(other.out_crsId), transformer(other.transformer)
{
	other.transformer = nullptr;
}

CRSTransformer::~CRSTransformer() {
	if (transformer) {
		GDALDestroyReprojectionTransformer(transformer);
		transformer = nullptr;
	}
}

bool CRSTransformer::transform(double &px, double &py, double &pz) const {
	int success;
	if (in_crsId != out_crsId) {
		if (!GDALReprojectionTransform(transformer, false, 1, &px, &py, &pz, &success) || !success)
			return false;
	}

	return true;
}


} // End namespace GDAL
