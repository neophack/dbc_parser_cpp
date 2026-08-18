#include <cstddef>
#include <fast_float/fast_float.h>
#include <system_error>
#include <istream>
#include <libdbc/utils/utils.hpp>
#include <string>

namespace Utils {

namespace {
// A regex such as "\s*(.*)" was previously used to test for blank-ness, but std::regex's
// recursive backtracking can stack-overflow on very long lines. A plain scan has no such
// limit and is what the regex was actually checking for.
bool has_non_whitespace(const std::string& line) {
	return line.find_first_not_of(" \t\n\v\f\r") != std::string::npos;
}
}

std::istream& StreamHandler::get_line(std::istream& stream, std::string& line) {
	std::string newline;

	std::getline(stream, newline);

	// Windows CRLF (\r\n)
	if (!newline.empty() && newline[newline.size() - 1] == '\r') {
		line = newline.substr(0, newline.size() - 1);
	} else {
		line = newline;
	}

	return stream;
}

std::istream& StreamHandler::get_next_non_blank_line(std::istream& stream, std::string& line) {
	bool is_blank = true;

	while (is_blank) {
		Utils::StreamHandler::get_line(stream, line);

		if (has_non_whitespace(line) || stream.eof()) {
			is_blank = false;
		}
	}

	return stream;
}

std::istream& StreamHandler::skip_to_next_blank_line(std::istream& stream, std::string& line) {
	bool line_is_empty = false;

	while (!line_is_empty) {
		Utils::StreamHandler::get_line(stream, line);

		if (!has_non_whitespace(line) || stream.eof()) {
			line_is_empty = true;
		}
	}

	return stream;
}

std::string String::trim(const std::string& line) {
	const char* WhiteSpace = " \t\v\r\n";
	std::size_t start = line.find_first_not_of(WhiteSpace);
	if (start == std::string::npos) {
		return std::string();
	}
	std::size_t end = line.find_last_not_of(WhiteSpace);
	return line.substr(start, end - start + 1);
}

double String::convert_to_double(const std::string& value, double default_value) {
	double converted_value = default_value;
	// NOLINTNEXTLINE -- Trying to iterators on the value causes the test to infinitly hang on windows builds
	fast_float::from_chars(value.data(), value.data() + value.size(), converted_value);
	return converted_value;
}

bool String::try_convert_to_double(const std::string& value, double& result) {
	double converted_value = 0;
	const auto answer = fast_float::from_chars(value.data(), value.data() + value.size(), converted_value);
	if (answer.ec != std::errc() || answer.ptr != value.data() + value.size()) {
		return false;
	}
	result = converted_value;
	return true;
}

} // Namespace Utils
