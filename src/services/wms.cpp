
#include "services/ogcservice.h"
#include "processing/queryprocessor.h"
#include "datatypes/raster.h"
#include "datatypes/raster/raster_priv.h"
#include "datatypes/plot.h"
#include "datatypes/colorizer.h"
#include "util/configuration.h"
#include "util/log.h"


/**
 * Implementation of the OGC WMS standard http://www.opengeospatial.org/standards/wms
 * It currently only supports our specific use cases
 */
class WMSService : public OGCService {
	public:
		using OGCService::OGCService;
		virtual ~WMSService() = default;
		virtual void run();

        std::unique_ptr<Colorizer> createColorizer(GenericRaster &raster, std::string colors);
};
REGISTER_HTTP_SERVICE(WMSService, "WMS");


void WMSService::run() {
	auto session = UserDB::loadSession(params.get("sessiontoken"));
	auto user = session->getUser();

	bool debug = params.getBool("debug", Configuration::get<bool>("global.debug", false));
	auto query_crsId = parseCrsId(params, "crs");
	TemporalReference tref = parseTime(params);

	std::string request = params.get("request");
	// GetCapabilities
	if (request == "GetCapabilities") {
		// TODO!
	}
	// GetMap
	else if (request == "GetMap") {
		if (params.get("version") != "1.3.0")
			response.send500("Invalid version");

		int output_width = params.getInt("width");
		int output_height = params.getInt("height");
		if (output_width <= 0 || output_height <= 0) {
			response.send500("output_width not valid");
		}

		try {
			// Wir ignorieren:
			// format
			// transparent

			// Unbekannt:
			// &STYLES=dem

			//if (params["tiled"] != "true")
			//	send403("only tiled for now");

			SpatialReference sref = parseBBOX(params.get("bbox"), query_crsId, false);
			auto colors = params.get("colors", "");
			auto format = params.get("format", "image/png");

			bool flipx, flipy;
			QueryRectangle qrect(
				sref,
				tref,
				QueryResolution::pixels(output_width, output_height)
			);


			Query query(params.get("layers"), Query::ResultType::RASTER, qrect);
			auto result_raster = processQuery(query, session)
				->getRaster(GenericOperator::RasterQM::EXACT);

			double bbox[4] = {sref.x1, sref.y1, sref.x2, sref.y2};
			flipx = (bbox[2] > bbox[0]) != (result_raster->pixel_scale_x > 0);
			flipy = (bbox[3] > bbox[1]) == (result_raster->pixel_scale_y > 0);

			std::unique_ptr<Raster2D<uint8_t>> overlay;
			if (debug) {
				Unit u = Unit::unknown();
				u.setMinMax(0, 1);
				DataDescription dd_overlay(GDT_Byte, u);
				overlay.reset( (Raster2D<uint8_t> *) GenericRaster::create(dd_overlay, SpatioTemporalReference::unreferenced(), output_width, output_height).release() );
				overlay->clear(0);

				// Write debug info
				std::ostringstream msg_tl;
				msg_tl.precision(2);
				msg_tl << std::fixed << bbox[0] << ", " << bbox[1] << " [" << result_raster->stref.x1 << ", " << result_raster->stref.y1 << "]";
				overlay->print(4, 4, 1, msg_tl.str().c_str());

				std::ostringstream msg_br;
				msg_br.precision(2);
				msg_br << std::fixed << bbox[2] << ", " << bbox[3] << " [" << result_raster->stref.x2 << ", " << result_raster->stref.y2 << "]";;
				std::string msg_brs = msg_br.str();
				overlay->print(overlay->width-4-8*msg_brs.length(), overlay->height-12, 1, msg_brs.c_str());

				if (result_raster->height >= 512) {
					const auto &messages = Log::getMemoryMessages();
					int ypos = 46;
					for (const auto &msg : messages) {
						overlay->print(4, ypos, 1, msg.c_str());
						ypos += 10;
					}
					ypos += 20;
					overlay->print(4, ypos, 1, "Attributes:");
					ypos += 10;
					for (auto val : result_raster->global_attributes.numeric()) {
						std::ostringstream msg;
						msg << "attribute " << val.first << "=" << val.second;
						overlay->print(4, ypos, 1, msg.str().c_str());
						ypos += 10;
					}

				}
			}

			outputImage(*result_raster, flipx, flipy, *createColorizer(*result_raster, colors), overlay.get());
		}  catch (const MappingException &e) {
			// Alright, something went wrong.
			// We're still in a WMS request though, so do our best to output an image with a clear error message.

			Unit u("errormessage", "errormessage");
			DataDescription dd(GDT_Byte, u, true, 0);
			auto errorraster = GenericRaster::create(dd, SpatioTemporalReference::unreferenced(), output_width, output_height);
			errorraster->clear(0);

			Json::Value exceptionJson;
			readNestedException(exceptionJson, e);
			std::string msg(exceptionJson.toStyledString());

			// determine exception type from error message. This is an ugly hack because the actual exception class gets lost in the QueryResult. Maybe needs refactoring.
			if (msg.find("NoRasterForGivenTimeException") != std::string::npos) {
				// no raster for given time: it's application dependent whether this is an error
				// dpeneding on config, output the error or just output a blank tile
				if (Configuration::get<bool>("wms.norasterforgiventimeexception", true)) {
					errorraster->printCentered(1, msg.c_str());
				}
			} else {
				// gneral exceptions: print error
				errorraster->printCentered(1, msg.c_str());
			}

			outputImage(*errorraster, false, false, Colorizer::error());
		}
		// cut into pieces



		/*
	 	 	 ?SERVICE=WMS
	 	 	 &VERSION=1.3.0
	 	 	 &REQUEST=GetMap
	 	 	 &FORMAT=image%2Fpng8
	 	 	 &TRANSPARENT=true
	 	 	 &LAYERS=elevation%3Asrtm_41_90m
	 	 	 &TILED=true
	 	 	 &STYLES=dem
	 	 	 &WIDTH=256
	 	 	 &HEIGHT=256
	 	 	 &CRS=EPSG%3A3857
	 	 	 &BBOX=0%2C0%2C10018754.171394622%2C10018754.171394622
	 	 	*/
		/*
		addLayer(LayerType.WMS, {
			layer : {
				url : 'http://dbsvm.mathematik.uni-marburg.de:9833/geoserver/elevation/wms',
				params : {
					'LAYERS' : 'elevation:srtm_41_90m',
					'TILED' : true,
					'FORMAT' : 'image/png8',
					'STYLES' : 'dem'
				},
				serverType : 'geoserver'
			},
			title : "SRTM"
		});
		 */
	}
	else if (request == "GetColorizer") {
		// TODO: remove this request and use DescribeLayer to check for validity of raster result
		if (params.get("version") != "1.3.0")
			response.send500("Invalid version");

		bool flipx, flipy;
		QueryRectangle qrect(
			SpatialReference::extent(query_crsId),
			tref,
			QueryResolution::pixels(1, 1)
		);

		Query query(params.get("layers"), Query::ResultType::RASTER, qrect);
		auto result_raster = processQuery(query, session)
			->getRaster(GenericOperator::RasterQM::LOOSE);

		response.sendContentType("application/json");
		response.finishHeaders();
		// for now reply with a mock response as long as front end uses this method to check raster result validity
		response << "{\"interpolation\":\"linear\", \"breakpoints\": [0,\"#000000\"]}";
	}
	// GetFeatureInfo (optional)
	else if (request == "GetFeatureInfo") {
		response.send500("WMS::GetFeatureInfo not implemented");

	}
	else
		response.send500("unknown request");

}

std::unique_ptr<Colorizer> WMSService::createColorizer(GenericRaster& raster, std::string colors) {
    std::unique_ptr<Colorizer> colorizer;
    if(colors.empty()) {
        if (raster.dd.unit.hasMinMax()) {
            colorizer = Colorizer::greyscale(raster.dd.unit.getMin(), raster.dd.unit.getMax());
        } else {
            throw ArgumentException("OGCService: no colorizer given and min/max values missing for default colorizer");
        }
    } else {
        Json::Reader reader(Json::Features::strictMode());
        Json::Value json;
        if (!reader.parse(colors, json))
            throw std::runtime_error(concat("OGCService: could not parse colors"));

        colorizer = Colorizer::fromJson(json);
    }
    return colorizer;
}
