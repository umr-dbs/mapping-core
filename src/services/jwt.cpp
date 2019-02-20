
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
 *
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
        auto dec_obj = jwt::decode(params.get("token"),
                jwt::params::algorithms({Configuration::get<std::string>("jwt.algorithm")}),
                jwt::params::secret(Configuration::get<std::string>("jwt.provider_key")),
                jwt::params::verify(false));

        // example payload {"aud":"http://localhost:8000/","exp":1550594460,"nbf":1550590860,"sub":"example.user"}
        jwt::jwt_payload &payload = dec_obj.payload();

        // TODO: check "aud" parameter for matching VAT instance

        // check if claim is still valid
        std::chrono::system_clock::time_point tp = std::chrono::system_clock::now();
        std::chrono::system_clock::duration dtn = tp.time_since_epoch();
        long timestamp = std::chrono::duration_cast<std::chrono::seconds>(dtn).count();

        if (timestamp < payload.get_claim_value<size_t>("nbf") || timestamp > payload.get_claim_value<size_t>("exp")) {
            response.sendFailureJSON("payload expired");
            return;
        }

        std::string externalId = EXTERNAL_ID_PREFIX + payload.get_claim_value<std::string>("sub");

        std::shared_ptr<UserDB::Session> session;
        try {
            // create session for user if he already exists
            session = UserDB::createSessionForExternalUser(externalId, 8 * 3600);
        } catch (const UserDB::authentication_error& e) {
            // TODO: get user details
            try {
                auto user = UserDB::createExternalUser(payload.get_claim_value<std::string>("sub"),
                                                       "JWT User",
                                                       payload.get_claim_value<std::string>("sub"), externalId);

                session = UserDB::createSessionForExternalUser(externalId, 8 * 3600);
            } catch (const std::exception&) {
                response.sendFailureJSON(concat("could not create new user "));
                return;
            }
        }

        response.sendSuccessJSON("session", session->getSessiontoken());
        return;
	}
	catch (const std::exception &e) {
		response.sendFailureJSON(e.what());
	}
}
