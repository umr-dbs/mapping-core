#ifndef UTIL_URILOADER_H_
#define UTIL_URILOADER_H_

#include <istream>
#include <memory>

/**
 * This class loads data from a given URI
 */
class URILoader {
public:
	URILoader() = delete;

	static std::unique_ptr<std::istream> loadFromURI(const std::string &uri);
};

#endif /* UTIL_URILOADER_H_ */
