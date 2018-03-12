#ifndef SERVICES_HTTPPARSING_H
#define SERVICES_HTTPPARSING_H

#include "services/httpservice.h"
#include <fcgio.h>

void parseQuery(const std::string& query, Parameters &params);
void parseGetData(Parameters &params);
void parsePostData(Parameters &params, std::istream &in);

void parseGetData(Parameters &params, FCGX_Request &request);
void parsePostData(Parameters &params, std::istream &in, FCGX_Request &request);
void parseMultipartPostData(Parameters &params, std::istream &in);

#endif
