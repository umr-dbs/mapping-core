
#include "util/configuration.h"
#include "util/log.h"
#include "uploader.h"
#include <iostream>
#include <userdb/userdb.h>

int main() {
    Configuration::loadFromDefaultPaths();
    Log::off();
    UserDB::initFromConfiguration();

    UploadService service(std::cin.rdbuf(), std::cout.rdbuf(), std::cerr.rdbuf());
    service.run();
}
