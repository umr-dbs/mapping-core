

#include "datatypes/raster.h"
#include "util/gdal.h"
#include "util/log.h"

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


std::string SRSFromCrsId(const CrsId &crsId) {
		OGRSpatialReferenceH hSRS;
		char *pszResult = nullptr;

		CPLErrorReset();

		hSRS = OSRNewSpatialReference(nullptr);
		bool success = false;

		if(crsId == CrsId::from_srs_string("SR-ORG:81")) {
			//MSG handling
			success = (OSRSetGEOS(hSRS, 0, 35785831, 0, 0) == OGRERR_NONE); //this is valid for meteosat: lon, height, easting, northing (gdal notation)!
			success = (OSRSetWellKnownGeogCS(hSRS, "WGS84" ) == OGRERR_NONE);
			/*
			 * GDAL also uses the following lines:
			 *     oSRS.SetGeogCS( NULL, NULL, NULL, 6378169, 295.488065897, NULL, 0, NULL, 0 );
    		 *     oSRS.SetGeogCS( "unnamed ellipse", "unknown", "unnamed", 6378169, 295.488065897, "Greenwich", 0.0);
			 */
		}
		else {
			//others
			success = (OSRSetFromUserInput( hSRS, crsId.to_string().c_str() ) == OGRERR_NONE );
		}

		if(success)
			//check for success and throw an exception if something went wrong
			OSRExportToWkt( hSRS, &pszResult );
		else {
			throw GDALException(concat("SRS could not be created for crsId ", crsId.to_string()));
			/*
				CPLError( CE_Failure, CPLE_AppDefined,
									"Translating source or target SRS failed:\n%s",
									pszUserInput );
				*/
		}

		OSRDestroySpatialReference(hSRS);

		std::string result(pszResult);
		CPLFree(pszResult);
		return result;
}



CRSTransformer::CRSTransformer(CrsId in_crsId, CrsId out_crsId) : in_crsId(in_crsId), out_crsId(out_crsId), transformer(nullptr) {
	init();

	if (in_crsId == CrsId::unreferenced() || out_crsId == CrsId::unreferenced())
		throw GDALException("in- or out-crsId is UNKNOWN");
	if (in_crsId == out_crsId)
		throw GDALException("Cannot transform when in_crsId == out_crsId");

	transformer = GDALCreateReprojectionTransformer(SRSFromCrsId(in_crsId).c_str(), SRSFromCrsId(out_crsId).c_str());
	if (!transformer)
		throw GDALException("Could not initialize ReprojectionTransformer");
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
