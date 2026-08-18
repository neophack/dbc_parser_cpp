#include "testing_utils/defines.hpp"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <fstream>
#include <libdbc/utils/utils.hpp>
#include <sstream>

namespace Utils {

TEST_CASE("Basic file input with safe get_line that is non line ending specific", "") {
	SECTION("Verify various line ending input files") {
		std::ifstream TextFile;
		std::string test;

		TextFile.open(TEXT_FILE, std::ios::in);
		CHECK(TextFile.is_open());

		if (TextFile.is_open()) {
			StreamHandler::get_line(TextFile, test);
			REQUIRE(test == "This is a non dbc formatted file.");
			StreamHandler::get_line(TextFile, test);
			REQUIRE(test == "");
			StreamHandler::get_line(TextFile, test);
			REQUIRE(test == "Make sure things pass with this");
			StreamHandler::get_line(TextFile, test);
			REQUIRE(test == "Who knows 	what might happen.");

			TextFile.close();
		}
	}
}

TEST_CASE("Test line finding utility functions", "") {
	std::string line;
	std::string test_string =
		"hello\n\
	\n\
		 \n\
  \n\
this is not blank\n\
maybe not this one either\n\
\n\
Someone wrote something....\n\
   b\n\
end";

	std::istringstream stream(test_string);

	SECTION("Test skipping empty lines") {
		StreamHandler::get_line(stream, line);

		CHECK(line == "hello");

		StreamHandler::get_next_non_blank_line(stream, line);
		REQUIRE(line == "this is not blank");

		StreamHandler::skip_to_next_blank_line(stream, line);
		REQUIRE(line == "");

		StreamHandler::get_next_non_blank_line(stream, line);
		REQUIRE(line == "Someone wrote something....");

		StreamHandler::get_next_non_blank_line(stream, line);
		REQUIRE(line == "   b");

		StreamHandler::get_next_non_blank_line(stream, line);
		REQUIRE(line == "end");

		SECTION("Test end of the files", "[edge case]") {
			StreamHandler::get_next_non_blank_line(stream, line);
			REQUIRE(line == "");

			StreamHandler::skip_to_next_blank_line(stream, line);
			REQUIRE(line == "");
		}
	}
}

TEST_CASE("Test the string trim feature", "[string]") {
	std::string s = "    	there might be some white space....   ";

	REQUIRE(String::trim(s) == "there might be some white space....");
}

TEST_CASE("Test string split feature", "[string]") {
	std::string s = "name1 name2 name3 name4 name5 ";
	std::vector<std::string> vs = {"name1", "name2", "name3", "name4", "name5"};

	std::vector<std::string> v;

	String::split(s, v);

	REQUIRE(v == vs);
}

TEST_CASE("String trim of an all whitespace string returns empty", "[string]") {
	REQUIRE(String::trim("   \t\r\n  ") == "");
}

TEST_CASE("String trim of an empty string returns empty", "[string]") {
	REQUIRE(String::trim("") == "");
}

TEST_CASE("String trim with no surrounding whitespace is unchanged", "[string]") {
	REQUIRE(String::trim("abc") == "abc");
}

TEST_CASE("String split with a custom delimiter", "[string]") {
	std::string s = "a,b,c";
	std::vector<std::string> expected = {"a", "b", "c"};
	std::vector<std::string> v;

	String::split(s, v, ',');

	REQUIRE(v == expected);
}

TEST_CASE("String split of an empty string produces no tokens", "[string]") {
	std::string s;
	std::vector<std::string> v;

	String::split(s, v);

	REQUIRE(v.empty());
}

TEST_CASE("convert_to_double parses well formed numbers", "[string][conversion]") {
	REQUIRE(String::convert_to_double("-12.5") == Catch::Approx(-12.5));
	REQUIRE(String::convert_to_double("0.5") == Catch::Approx(0.5));
	REQUIRE(String::convert_to_double(".5") == Catch::Approx(0.5));
	REQUIRE(String::convert_to_double("5.") == Catch::Approx(5.0));
	REQUIRE(String::convert_to_double("100") == Catch::Approx(100.0));
}

TEST_CASE("convert_to_double falls back to the default value on invalid input", "[string][conversion]") {
	REQUIRE(String::convert_to_double("") == Catch::Approx(0.0));
	REQUIRE(String::convert_to_double("", 5.0) == Catch::Approx(5.0));
	REQUIRE(String::convert_to_double("not_a_number", 7.0) == Catch::Approx(7.0));
}

TEST_CASE("convert_to_double parses the leading number of a partially valid string", "[string][conversion]") {
	// fast_float parses as much of a valid prefix as it can, mirroring std::from_chars.
	REQUIRE(String::convert_to_double("12abc", 9.0) == Catch::Approx(12.0));
}

} // Utils
