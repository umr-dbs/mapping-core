#include <gtest/gtest.h>
#include "util/uriloader.h"
#include "util/exceptions.h"

static std::string getAsString(std::istream& stream) {
	std::istreambuf_iterator<char> eos;
	std::string s(std::istreambuf_iterator<char>(stream), eos);

	return s;
}

TEST(URILoader, dataUriSimple) {
	auto ss = URILoader::loadFromURI("data:text/plain,test");

	EXPECT_EQ("test", getAsString(*ss));
}

TEST(URILoader, dataUriSimple2) {
	auto ss = URILoader::loadFromURI("data:text/plain,test;foo,bar");

	EXPECT_EQ("test;foo,bar", getAsString(*ss));
}

TEST(URILoader, dataUriCharset) {
	auto ss = URILoader::loadFromURI("data:text/plain;charset=ASCII,test");

	EXPECT_EQ("test", getAsString(*ss));
}

TEST(URILoader, dataUriCharset2) {
	auto ss = URILoader::loadFromURI("data:text/plain;charset=ASCII,test;foo,bar");

	EXPECT_EQ("test;foo,bar", getAsString(*ss));
}

TEST(URILoader, dataUriBase64) {
	// TODO adjust once Base64 is supported
	EXPECT_THROW(URILoader::loadFromURI("data:text/plain;base64,test"), ArgumentException);
}

TEST(URILoader, dataUriCarchsetBase64) {
	// TODO adjust once Base64 is supported
	EXPECT_THROW(URILoader::loadFromURI("data:text/plain;charset=ASCII;base64,test"), ArgumentException);
}

TEST(URILoader, dataUriinUri) {
	auto ss = URILoader::loadFromURI("data:text/plain,url: http://example.org");

	EXPECT_EQ("url: http://example.org", getAsString(*ss));
}
