#include <gtest/gtest.h>
#include <boost/filesystem.hpp>
#include <boost/filesystem/operations.hpp>
#include "uploader/uploader.h"

//copied from userdb unittest for in memory userdb.
class UserDBTestClock_Uploader : public UserDB::Clock {
public:
    UserDBTestClock_Uploader(time_t *now) : now(now) {}
    virtual ~UserDBTestClock_Uploader() {};
    virtual time_t time() {
            return *now;
    };
    time_t *now;
};


std::string temp_dir_name = "this_should_not_be_the_real_uploader_directory_name";

class UploaderTest : public ::testing::Test {
protected:
    virtual void SetUp();
    virtual void TearDown();
    std::string sessiontoken;
    std::string upl_dir;
    std::string userID;
};


// SetUp & TearDown create a temp directory for the upload, after the test it is deleted.
// these functions are called for each test individually.
void UploaderTest::SetUp() {
    Configuration::loadFromDefaultPaths();

    time_t now = time(nullptr);
    UserDB::init("sqlite", ":memory:", make_unique<UserDBTestClock_Uploader>(&now), 0);

    //change upload dir parameter to a test directory
    Configuration::loadFromString("[uploader]\ndirectory=\""+ temp_dir_name + "\"");
    upl_dir = Configuration::get<std::string>("uploader.directory");

    const std::string username = "dummy";
    const std::string password = "12345";
    const std::string userpermission = "upload";

    // Create a user and session
    auto user = UserDB::createUser(username, "realname", "email", password);
    user->addPermission(userpermission);
    auto session = UserDB::createSession(username, password);
    sessiontoken = session->getSessiontoken();
    userID = user->getUserIDString();

    //set env variables for the http request
    setenv("REQUEST_METHOD", "POST", true);
    setenv("CONTENT_TYPE", "multipart/mixed", true);
}

void UploaderTest::TearDown() {
    //delete test upload dir and just in case change back the parameter.
    boost::filesystem::path test_path(upl_dir);
    boost::filesystem::remove_all(test_path);
    UserDB::shutdown();
}

std::string two_file_request_p1 =
        "--frontier\n"
        "Content-Type: application/x-www-form-urlencoded\n"
        "\n"
        "upload_name=test_upload&append_upload=false&sessiontoken=";
std::string two_file_request_p2 =
        "\n"
        "--frontier\n"
        "Content-Disposition: form-data; filename=\"hello.csv\"\n"
        "Content-Type: application/octet-stream\n"
        "Content-Transfer-Encoding: base64\n"
        "\n"
        "d2t0O29zbV9pZDtuYW1lO2lzX2luO3RpbWVfc3RhcnQ7dGltZV9lbmQ7cG9wdWxhdGlvbg0KUE9JTlQoLTAuMTI3NjQ3NCA1MS41MDczMjE5KTsxMDc3NzU7TG9uZG9uO0VuZ2xhbmQsIFVuaXRlZCBLaW5nZG9tLCBVSywgR3JlYXQgQnJpdGFpbiwgRXVyb3BlOzIwMTUtMDktMTUgMTI6NDg6MTIrMDI7MjAxNS0xMC0wNCAxOTo0ODoxMiswMjs4NDE2NTM1DQpQT0lOVCgtMS41NDM3OTQxIDUzLjc5NzQxODUpOzM1ODMwOTtMZWVkcztVSyxVbml0ZWQgS2luZ2RvbSwgWW9ya3NoaXJlLFdlc3QgWW9ya3NoaXJlLCBBaXJlZGFsZTsyMDE1LTA4LTEzIDAyOjIxOjEyKzAyOzIwMTUtMDktMTEgMTI6MjE6MTIrMDI7NzcwODAwDQpQT0lOVCgyMy43Mjc5ODQzIDM3Ljk4NDE0OTMpOzQ0MTE4MzvOkc64zq7Ovc6xO0F0aGluYSBtdW5pY2lwYWxpdHksQXR0aWtpLEdyZWVjZSxFVTsyMDE1LTA5LTE1IDE0OjU1OjI4KzAyOzIwMTUtMTAtMjEgMTQ6NTU6MjgrMDI7MTk3ODg0Nw0KUE9JTlQoLTc2LjYxMDgwNzMgMzkuMjkwODYwOCk7NjcxMTEzO0JhbHRpbW9yZTs7MjAxNS0wNy0yMyAxNTozNTowMiswMjsyMDE1LTEwLTIzIDE1OjM1OjAyKzAyOzYyMTM0Mg0KUE9JTlQoMTQuNTA2ODkyMSA0Ni4wNDk4NjUpOzY5Njg4Mjc7TGp1YmxqYW5hO1Nsb3ZlbmlhLCBFdXJvcGU7MjAxNS0wOC0xMyAwMjoyMToxMyswMjsyMDE1LTA5LTIzIDAyOjIxOjEzKzAyOzI3MjAwMA0K\n"
        "--frontier\n"
        "Content-Disposition: form-data; filename=\"hello.csvt\"\n"
        "Content-Type: text/plain\n"
        "\n"
        "\"WKT\",\"Integer\",\"String\",\"String\",\"DateTime\",\"DateTime\",\"DateTime\",\"Integer\"\n"
        "--frontier--\n";
/*
 * Runnig twice the same upload (with append_upload=false), first successfull, second throwing an exception.
 */
TEST_F(UploaderTest, same_upload_twice){
    {
        //test upload. sessiontoken is inserted into test strings.
        std::stringstream fake_in;
        fake_in << two_file_request_p1;
        fake_in << sessiontoken;
        fake_in << two_file_request_p2;
        std::stringstream fake_out;

        UploadService service(fake_in.rdbuf(), fake_out.rdbuf(), std::cerr.rdbuf());
        service.run();

        //read the header from the response, until the response json starts.
        while (fake_out.good() && fake_out.peek() != '{') {
            fake_out.get();
        }
        if (!fake_out.good()) {
            EXPECT_TRUE(false);
            return;
        }

        Json::Value result_json;
        fake_out >> result_json;

        EXPECT_TRUE(result_json["result"].asBool());
        EXPECT_EQ(result_json["upload_name"], "test_upload");

        boost::filesystem::path path_test(temp_dir_name);
        path_test /= userID;
        path_test /= "test_upload";
        path_test /= "hello.csv";

        EXPECT_TRUE(boost::filesystem::exists(path_test));
        EXPECT_TRUE(boost::filesystem::is_regular_file(path_test));
    }

    {
        //second test with same files and they already exist, should throw exception, that's returned as 500 response at the moment.
        std::stringstream fake_in;
        fake_in << two_file_request_p1;
        fake_in << sessiontoken;
        fake_in << two_file_request_p2;
        std::stringstream fake_out;

        UploadService service(fake_in.rdbuf(), fake_out.rdbuf(), std::cerr.rdbuf());
        service.run();
        EXPECT_EQ(fake_out.str(), "Status: 500 Internal Server Error\r\n"
                "Content-type: text/plain\r\n"
                "\r\nInvalid upload: UploaderException: Upload with same name already exists");
    }
}

std::string two_file_request_crash_p2 =
                "\n"
                "--frontier\n"
                "Content-Disposition: form-data; filename=\"hello.csv\"\n"
                "Content-Type: application/octet-stream\n"
                "Content-Transfer-Encoding: base64\n"
                "\n"
                "d2t0O29zbV9pZDtuYW1lO2lzX2luO3RpbWVfc3RhcnQ7dGltZV9lbmQ7cG9wdWxhdGlvbg0KUE9JTlQoLTAuMTI3NjQ3NCA1MS41MDczMjE5KTsxMDc3NzU7TG9uZG9uO0VuZ2xhbmQsIFVuaXRlZCBLaW5nZG9tLCBVSywgR3JlYXQgQnJpdGFpbiwgRXVyb3BlOzIwMTUtMDktMTUgMTI6NDg6MTIrMDI7MjAxNS0xMC0wNCAxOTo0ODoxMiswMjs4NDE2NTM1DQpQT0lOVCgtMS41NDM3OTQxIDUzLjc5NzQxODUpOzM1ODMwOTtMZWVkcztVSyxVbml0ZWQgS2luZ2RvbSwgWW9ya3NoaXJlLFdlc3QgWW9ya3NoaXJlLCBBaXJlZGFsZTsyMDE1LTA4LTEzIDAyOjIxOjEyKzAyOzIwMTUtMDktMTEgMTI6MjE6MTIrMDI7NzcwODAwDQpQT0lOVCgyMy43Mjc5ODQzIDM3Ljk4NDE0OTMpOzQ0MTE4MzvOkc64zq7Ovc6xO0F0aGluYSBtdW5pY2lwYWxpdHksQXR0aWtpLEdyZWVjZSxFVTsyMDE1LTA5LTE1IDE0OjU1OjI4KzAyOzIwMTUtMTAtMjEgMTQ6NTU6MjgrMDI7MTk3ODg0Nw0KUE9JTlQoLTc2LjYxMDgwNzMgMzkuMjkwODYwOCk7NjcxMTEzO0JhbHRpbW9yZTs7MjAxNS0wNy0yMyAxNTozNTowMiswMjsyMDE1LTEwLTIzIDE1OjM1OjAyKzAyOzYyMTM0Mg0KUE9JTlQoMTQuNTA2ODkyMSA0Ni4wNDk4NjUpOzY5Njg4Mjc7TGp1YmxqYW5hO1Nsb3ZlbmlhLCBFdXJvcGU7MjAxNS0wOC0xMyAwMjoyMToxMyswMjsyMDE1LTA5LTIzIDAyOjIxOjEzKzAyOzI3MjAwMA0K\n"
                "--frontier\n"
                "Content-Disposition: form-data; filename=\"hello.csvt\"\n"
                "Content-Type: application/octet-stream\n"
                "Content-Transfer-Encoding: proencodingyeah\n"
                "\n"
                "\"WKT\",\"Integer\",\"String\",\"String\",\"DateTime\",\"DateTime\",\"DateTime\",\"Integer\"\n"
                "--frontier--\n";

std::string single_file_p2 =
                "\n"
                "--frontier\n"
                "Content-Disposition: form-data; filename=\"iwillsurvi.ve\"\n"
                "Content-Type: text/plain\n"
                "\n"
                "\"WKT\",\"Integer\",\"String\",\"String\",\"DateTime\",\"DateTime\",\"DateTime\",\"Integer\"\n"
                "--frontier--\n";

std::string two_file_request_append_upload_p1 =
                "--frontier\n"
                "Content-Type: application/x-www-form-urlencoded\n"
                "\n"
                "upload_name=test_upload&append_upload=true&sessiontoken=";

/*
 * Two uploads. The second appends the first, but fails with an exception (because of wrong encoding).
 * In the end the file from first upload should exist but the files from second upload should be deleted.
 */
TEST_F(UploaderTest, upload_failed) {

    {
        //Upload a file successfully, it should not be deleted on second upload
        std::stringstream fake_in;
        fake_in << two_file_request_p1;
        fake_in << sessiontoken;
        fake_in << single_file_p2;
        std::stringstream fake_out;

        UploadService service(fake_in.rdbuf(), fake_out.rdbuf(), std::cerr.rdbuf());
        service.run();
    }

    {
        //this upload is supposed to fail
        std::stringstream fake_in;
        fake_in << two_file_request_append_upload_p1;
        fake_in << sessiontoken;
        fake_in << two_file_request_crash_p2;
        std::stringstream fake_out;

        UploadService service(fake_in.rdbuf(), fake_out.rdbuf(), std::cerr.rdbuf());
        service.run();

        //check that an error was returned
        std::string line;
        std::getline(fake_out, line);
        EXPECT_EQ("Status: 500 Internal Server Error\r", line);
    }


    //check that both files from the upload do not exist, but the file from the first upload is still there
    boost::filesystem::path base_path(temp_dir_name);
    base_path /= userID;
    base_path /= "test_upload";
    boost::filesystem::path csv_path = base_path;
    csv_path /= "hello.csv";
    boost::filesystem::path csvt_path = base_path;
    csvt_path /= "hello.csvt";
    boost::filesystem::path survive_path = base_path;
    survive_path /= "iwillsurvi.ve";

    EXPECT_FALSE(boost::filesystem::exists(csv_path));
    EXPECT_FALSE(boost::filesystem::exists(csvt_path));
    EXPECT_TRUE (boost::filesystem::exists(survive_path));
}
