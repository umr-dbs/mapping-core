
#include "services/httpservice.h"
#include "userdb/userdb.h"
#include "util/configuration.h"

#include <jwt/jwt.hpp>

/**
 * This class provides login using jwt
 *
 * Operations:
 * - request = login: login with credential, returns sessiontoken
 *   - parameters:
 *     - token
 *  - request = clientToken: get the client token for provider
 */
class JWTService : public HTTPService {
	public:
		using HTTPService::HTTPService;
		virtual ~JWTService() = default;
		virtual void run();

    private:
        static constexpr const char* EXTERNAL_ID_PREFIX = "JWT:";
};
REGISTER_HTTP_SERVICE(JWTService, "jwt");



void JWTService::run() {
	try {
        if (params.get("request") == "login") {
            auto dec_obj = jwt::decode(params.get("token"),
                                       jwt::params::algorithms({Configuration::get<std::string>("jwt.algorithm")}),
                                       jwt::params::secret(Configuration::get<std::string>("jwt.provider_key")),
                                       jwt::params::leeway(Configuration::get<uint32_t>("jwt.allowed_clock_skew_seconds")),
                                       jwt::params::verify(true));

            // example payload {"aud":"http://localhost:8000/","exp":1550594460,"nbf":1550590860,"sub":"example.user"}
            jwt::jwt_payload &payload = dec_obj.payload();

            // TODO: check "aud" parameter for matching VAT instance

            std::string externalId = EXTERNAL_ID_PREFIX + payload.get_claim_value<std::string>("sub");

            std::shared_ptr<UserDB::Session> session;
            try {
                // create session for user if he already exists
                session = UserDB::createSessionForExternalUser(externalId, 8 * 3600);
            } catch (const UserDB::authentication_error &e) {
                // TODO: get user details
                try {
                    auto user = UserDB::createExternalUser(payload.get_claim_value<std::string>("sub"),
                                                           "JWT User",
                                                           payload.get_claim_value<std::string>("sub"), externalId);

                    session = UserDB::createSessionForExternalUser(externalId, 8 * 3600);
                } catch (const std::exception &) {
                    response.sendFailureJSON(concat("could not create new user "));
                    return;
                }
            }

            response.sendSuccessJSON("session", session->getSessiontoken());
            return;
        }

        if (params.get("request") == "clientToken") {
            std::string clientToken = Configuration::get<std::string>("jwt.redirect_token", "");
            if (clientToken != "") {
                response.sendSuccessJSON("clientToken", clientToken);
            } else {
                jwt::jwt_object obj{jwt::params::algorithm({Configuration::get<std::string>("jwt.algorithm")}),
                                    jwt::params::secret(Configuration::get<std::string>("jwt.client_key")),
                                    jwt::params::payload({{"redirect", Configuration::get<std::string>("jwt.redirect_url")}})};
                response.sendSuccessJSON("clientToken", obj.signature());
            }
            return;
        }
	}
	catch (const std::exception &e) {
		response.sendFailureJSON(e.what());
	}
}
