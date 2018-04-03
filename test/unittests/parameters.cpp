#include "util/parameters.h"

#include <gtest/gtest.h>

TEST(Parameters, getInt) {
	Parameters params;
	params.insert(std::make_pair("42", "42"));
	// TODO: these three tests tell us the behaviour of stoi(). Not sure if that's the behaviour we want.
	params.insert(std::make_pair("43", " 43"));
	params.insert(std::make_pair("44", "44 "));
	params.insert(std::make_pair("45", "45b"));

	EXPECT_EQ(params.getInt("42"), 42);
	EXPECT_EQ(params.getInt("43"), 43);
	EXPECT_EQ(params.getInt("44"), 44);
	EXPECT_EQ(params.getInt("45"), 45);
}

TEST(Parameters, getBool) {
	Parameters params;
	params.insert(std::make_pair("yes", "yEs"));
	params.insert(std::make_pair("true", "trUe"));
	params.insert(std::make_pair("1", "1"));
	params.insert(std::make_pair("no", "No"));
	params.insert(std::make_pair("false", "faLSe"));
	params.insert(std::make_pair("0", "0"));

	EXPECT_EQ(params.getBool("yes"), true);
	EXPECT_EQ(params.getBool("true"), true);
	EXPECT_EQ(params.getBool("1"), true);
	EXPECT_EQ(params.getBool("no"), false);
	EXPECT_EQ(params.getBool("false"), false);
	EXPECT_EQ(params.getBool("0"), false);
}

TEST(Parameters, getPrefixedParameters) {
	Parameters params;
	params.insert(std::make_pair("test.a", "a"));
	params.insert(std::make_pair("test.b", "b"));
	params.insert(std::make_pair("test.c", "c"));
	params.insert(std::make_pair("test.", "should be ignored"));
	params.insert(std::make_pair("other.a", "o.a"));
	params.insert(std::make_pair("other.b", "o.b"));
	params.insert(std::make_pair("other.c", "o.c"));
	params.insert(std::make_pair("other.d", "o.d"));
	params.insert(std::make_pair("a", "not a"));

	auto prefixed = params.getPrefixedParameters("test.");
	EXPECT_EQ(prefixed.size(), 3);
	EXPECT_EQ(prefixed.get("a"), "a");
	EXPECT_EQ(prefixed.get("b"), "b");
	EXPECT_EQ(prefixed.get("c"), "c");
}

