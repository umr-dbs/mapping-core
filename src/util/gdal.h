#ifndef UTIL_GDAL_H
#define UTIL_GDAL_H

#include <string>
#include <stdint.h>
#include "datatypes/spatiotemporal.h"

namespace GDAL {
	void init();
	std::string WKTFromCrsId(const CrsId &crsId);

	/**
	 * This class allows the transformation of coordinates between two projections
	 */
	class CRSTransformer {
		public:
			CRSTransformer(CrsId in_crsId, CrsId out_crsId);
			~CRSTransformer();

			CRSTransformer(const CRSTransformer &copy) = delete;
			CRSTransformer &operator=(const CRSTransformer &copy) = delete;

			bool transform(double &px, double &py, double &pz) const;
			bool transform(double &px, double &py) const { double pz = 0.0; return transform(px, py, pz); }
			const CrsId in_crsId;
            const CrsId out_crsId;

			static void msg_pixcoord2geocoord(int column, int row, double *longitude, double *latitude);
			static void msg_geocoord2pixcoord(double longitude, double latitude, int *column, int *row);
		private:
			void *transformer;
	};

}

#endif
