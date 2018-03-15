#include "services/httpparsing.h"

#include <gtest/gtest.h>
#include <stdlib.h>
#include <sstream>
#include <util/exceptions.h>
#include <algorithm>
#include <Poco/Exception.h>

void parseCGIEnvironment(Parameters &params, std::string method,
		const std::string &url, const std::string &query_string,
		const std::string &postmethod = "", const std::string &postdata = "") {

	std::stringstream input;

	EXPECT_EQ(setenv("REQUEST_METHOD", method.c_str(), true), 0);
	EXPECT_EQ(setenv("QUERY_STRING", query_string.c_str(), true), 0);

	// force method upper-case to allow case-sensitivity checks in the parse*Data methods.
	std::transform(method.begin(), method.end(), method.begin(), ::toupper);

	std::string request_uri = url;
	if (query_string != "")
		request_uri += "?" + query_string;
	EXPECT_EQ(setenv("REQUEST_URI", request_uri.c_str(), true), 0);
	if (method == "GET") {
		EXPECT_EQ(unsetenv("CONTENT_TYPE"), 0);
		EXPECT_EQ(unsetenv("CONTENT_LENGTH"), 0);
	} else if (method == "POST") {
		EXPECT_EQ(setenv("CONTENT_TYPE", postmethod.c_str(), true), 0);
		EXPECT_EQ(
				setenv("CONTENT_LENGTH",
						std::to_string(postdata.length()).c_str(), true), 0);

		input << postdata;
	} else
		FAIL();

	parseGetData(params);
	parsePostData(params, input);
}

// Test a parameter that appears multiple times, names are not case sensitive.
TEST(HTTPParsing, getrepeated) {
	Parameters params;
	parseCGIEnvironment(params, "GET", "/cgi-bin/bla",
			"PARAM=one&param=two&pArAm=%C3%A4%C3%B6%C3%BC%C3%9F");

	EXPECT_EQ(params.get("param", ""), "one");
    EXPECT_EQ(params.getLast("param", ""), "äöüß");
    EXPECT_EQ(params.getAll("param").size(), 3);
}

// Test a parameter that has no value.
TEST(HTTPParsing, getnovalue) {
	Parameters params;
	parseCGIEnvironment(params, "GET", "/cgi-bin/bla",
			"flag1&flag2&flag3=&value1=3&value2=4");

	EXPECT_EQ(params.get("flag1", "-"), "");
	EXPECT_EQ(params.get("flag2", "-"), "");
	EXPECT_EQ(params.get("flag3", "-"), "");
	EXPECT_EQ(params.get("value1", ""), "3");
	EXPECT_EQ(params.get("value2", ""), "4");
}

// + and %20 should be decoded to spaces.
TEST(HTTPParsing, spaces){
    Parameters params;
    parseCGIEnvironment(params, "GET", "/cgi-bin/bla",
            "value=what+the%20spaces?");
    EXPECT_EQ(params.get("value", ""), "what the spaces?");
}


// Test an empty query string
TEST(HTTPParsing, emptyget) {
	Parameters params;
	parseCGIEnvironment(params, "GET", "/cgi-bin/bla", "");

	EXPECT_EQ(params.size(), 0);
}

// Test urlencoded postdata
TEST(HTTPParsing, posturlencoded) {
	Parameters params;
	parseCGIEnvironment(params, "POST", "/cgi-bin/bla", "",
			"application/x-www-form-urlencoded",
			"flag1&flag2=&pArAm=%C3%A4%C3%B6%C3%BC%C3%9F");

	EXPECT_EQ(params.get("flag1", "-"), "");
    EXPECT_EQ(params.get("flag2", "-"), "");
	EXPECT_EQ(params.get("param", ""), "äöüß");
}

// Test weird query string formats
TEST(HTTPParsing, testquerystringspecialchars) {
	Parameters params;
	parseCGIEnvironment(params, "GET", "/cgi-bin/bla",
			"p1&p2=1=2=%C3%A4%C3%B6%C3%BC%C3%9F&p3=&p4&&p5&?????&&&====&=&???&&p6==?");

	EXPECT_EQ(params.get("p1", "-"), "");
	EXPECT_EQ(params.get("p2", ""), "1=2=äöüß");
	EXPECT_EQ(params.get("p3", "-"), "");
	EXPECT_EQ(params.get("p4", "-"), "");
	EXPECT_EQ(params.get("p5", "-"), "");
	EXPECT_EQ(params.get("p6", ""), "=?");
	//EXPECT_EQ(params.size(), 8); // Also interprets '?????' and '???' as keys.
}

// Test illegal percent encoding
TEST(HTTPParsing, illegalpercentencoding) {
	Parameters params;
    EXPECT_THROW(parseCGIEnvironment(params, "GET", "/cgi-bin/bla", "p1=%22%ZZ%5F"), Poco::SyntaxException);
}

// This is a multipart message that with content-disposition NOT set as "form-data". Thus, no parameter is parsed here and the body is ignored as always.
const std::string multipart_message =
					"\r\n"
					"--frontier\r\n"
					"Content-Type: text/plain\r\n"
					"\r\n"
					"This is the body of the message.\r\n"
                    "\r\n"
					"--frontier\r\n"
					"Content-Type: application/octet-stream\r\n"
					"Content-Transfer-Encoding: base64\r\n"
					"\r\n"
					"PGh0bWw+CiAgPGhlYWQ+CiAgPC9oZWFkPgogIDxib2R5PgogICAgPHA+VGhpcyBpcyB0aGUg\r\n"
					"Ym9keSBvZiB0aGUgbWVzc2FnZS48L3A+CiAgPC9ib2R5Pgo8L2h0bWw+Cg==\r\n"
					"\r\n"
                    "--frontier--"
					"--frontier--\r\n";

// This is a multipart message that with content-disposition set as "form-data". A 'name' key is provided.
const std::string multipart_message2 =
		"\r\n"
		"----myboundary\r\n"
		"Content-Disposition: form-data; name=\"text\"\r\n"
		"\r\n"
		"text default\r\n"
		"----myboundary\r\n"
		"Content-Disposition: form-data; name=\"file1\"; filename=\"a.txt\"\r\n"
		"Content-Type: text/plain\r\n"
		"\r\n"
		"Content of a.txt.\r\n"
        "\r\n"
		"----myboundary\r\n"
		"Content-Disposition: form-data; name=\"file2\"; filename=\"a.html\"\r\n"
		"Content-Type: text/html\r\n"
		"\n\n"
		"----myboundary--"
		;

// This is a multipart message that with content-disposition set as "form-data". A 'name' key is NOT provided (which is not allowed).
const std::string multipart_message3 =
		"--xyz\r\n"
                "\r\n"
		"Content-Disposition: form-data;\r\n"
		"xyz content\r\n"
                "\r\n"
		"--xyz--\r\n"
		;

// This is a multipart message with a corrupt boundary
const std::string multipart_message4 =
		"--xyz\r\n"
                "\r\n"
		"Content-Disposition: form-data;\r\n"
		"xyz content\r\n"
            "\r\n"
		;

// Parsing multipart data is not supported right now, so ArgumentException is expected.
TEST(HTTPParsing, multipart) {
	Parameters params;
	EXPECT_THROW(parseCGIEnvironment(params, "POST", "/cgi-bin/bla", "","multipart/mixed; boundary=frontier", multipart_message), ArgumentException);
}
