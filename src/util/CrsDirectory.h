#ifndef MAPPING_CORE_CRSDIRECTORY_H
#define MAPPING_CORE_CRSDIRECTORY_H


#include <string>
#include <datatypes/spatiotemporal.h>

/**
 * Management of supported Crs/Projections
 */
class CrsDirectory {
public:
    /**
    * get the WKT definition of the requested Crs. Return empty string CrsId is unknown
    */
    static std::string getWKTForCrsId(const CrsId &crsId);
};


#endif //MAPPING_CORE_CRSDIRECTORY_H
