#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <fstream>
#include <istream>
#include <libdbc/dbc.hpp>
#include <libdbc/exceptions/error.hpp>
#include <libdbc/message.hpp>
#include <libdbc/signal.hpp>
#include <libdbc/utils/utils.hpp>
#include <regex>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace Libdbc {

#ifdef _WIN32
// std::ifstream(const char*) on Windows converts the path through the
// process's current ANSI code page, not UTF-8. file_name is expected to be
// UTF-8 encoded (as it is everywhere else this library runs), so a path
// containing e.g. Chinese characters would silently get mangled into a
// nonexistent path and fail to open. Converting to UTF-16 ourselves and
// opening with the (non-standard, but widely supported) wchar_t* overload
// avoids that.
static std::wstring utf8_path_to_wide(const std::string& utf8) {
	if (utf8.empty()) {
		return std::wstring();
	}

	int size = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), nullptr, 0);
	if (size <= 0) {
		return std::wstring();
	}

	std::wstring wide(static_cast<size_t>(size), L'\0');
	MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), &wide[0], size);
	return wide;
}
#endif

const auto floatPattern = "(-?\\d+\\.?(\\d+)?)"; // Can be negative

const auto signalIdentifierPattern = "(SG_)";
const auto namePattern = "(\\w+)";
const auto bitStartPattern = "(\\d+)"; // Cannot be negative
const auto lengthPattern = "(\\d+)"; // Cannot be negative
const auto byteOrderPattern = "([0-1])";
const auto signPattern = "(\\+|\\-)";
const auto scalePattern = "(\\d+\\.?(\\d+)?)"; // Non negative float
const auto offsetPattern = floatPattern;
// NOLINTNEXTLINE -- Disable warning for runtime initialization and can throw. Can't fix until newer c++ version with constexpr
const auto offsetScalePattern = std::string("\\(") + scalePattern + "\\," + offsetPattern + "\\)";
const auto minPattern = floatPattern;
const auto maxPattern = floatPattern;
// NOLINTNEXTLINE -- Disable warning for runtime initialization and can throw. Can't fix until newer c++ version with constexpr
const auto minMaxPattern = std::string("\\[") + minPattern + "\\|" + maxPattern + "\\]";
const auto unitPattern = "\"(.*)\""; // Random string
const auto receiverPattern = "([\\w\\,]+|Vector__XXX)*";
const auto multiplexPattern = "(?:\\s(m\\d+|M))?"; // Optional multiplexor (M) or multiplexed member (m<N>) marker; group 3 captures the marker
const auto whiteSpace = "\\s";

constexpr unsigned SIGNAL_NAME_GROUP = 2;
constexpr unsigned SIGNAL_MULTIPLEX_GROUP = 3;
constexpr unsigned SIGNAL_START_BIT_GROUP = 4;
constexpr unsigned SIGNAL_SIZE_GROUP = 5;
constexpr unsigned SIGNAL_ENDIAN_GROUP = 6;
constexpr unsigned SIGNAL_SIGNED_GROUP = 7;
constexpr unsigned SIGNAL_FACTOR_GROUP = 8;
constexpr unsigned SIGNAL_OFFSET_GROUP = 10;
constexpr unsigned SIGNAL_MIN_GROUP = 12;
constexpr unsigned SIGNAL_MAX_GROUP = 14;
constexpr unsigned SIGNAL_UNIT_GROUP = 16;
constexpr unsigned SIGNAL_RECEIVER_GROUP = 17;

// std::regex on libstdc++ backtracks recursively; an unmatched line with a long run of
// word characters can recurse deep enough to overflow the stack. No legitimate DBC line
// approaches this length, so lines beyond it are treated as unparseable without ever
// reaching the regex engine.
constexpr size_t MAX_MATCHABLE_LINE_LENGTH = 1000;

constexpr uint64_t MAX_MESSAGE_SIZE_BYTES = 255;
constexpr uint32_t MAX_SIGNAL_SIZE_BITS = 64;

constexpr unsigned MESSAGE_ID_GROUP = 2;
constexpr unsigned MESSAGE_NAME_GROUP = 3;
constexpr unsigned MESSAGE_SIZE_GROUP = 4;
constexpr unsigned MESSAGE_NODE_GROUP = 5;

namespace {
// Converts a numeric regex capture to uint64_t. std::stoul would throw
// std::out_of_range / std::invalid_argument, which are not part of this
// library's exception hierarchy, so any malformed number is reported as a
// DbcFileParseError instead.
uint64_t to_uint(const std::string& text, const std::string& line, const char* field) {
	try {
		size_t pos = 0;
		const uint64_t value = std::stoull(text, &pos);
		if (pos != text.size() || text.find('-') != std::string::npos) {
			throw std::invalid_argument(text);
		}
		return value;
	} catch (const std::exception&) {
		throw DbcFileParseError(line, std::string("Failed to parse ") + field + " value (\"" + text + "\").");
	}
}
}

DbcParser::DbcParser()
	: version_re("^(VERSION)\\s\"(.*)\"")
	, bit_timing_re("^(BS_)\\s*:")
	, name_space_re("^(NS_)\\s\\:")
	, node_re("^(BU_:)\\s?((?:[\\w]+?\\s?)*)?")
	, message_re("^(BO_)\\s(\\d+)\\s(\\w+)\\s*\\:\\s*(\\d+)\\s(\\w+|Vector__XXX)")
	, value_re("^(VAL_)\\s(\\d+)\\s(\\w+)((?:\\s(\\d+)\\s\"([^\"]*)\")+)\\s;$")
	, signal_re(std::string("^") + whiteSpace + signalIdentifierPattern + whiteSpace + namePattern + multiplexPattern + whiteSpace + "\\:" + whiteSpace
				+ bitStartPattern + "\\|" + lengthPattern + "\\@" + byteOrderPattern + signPattern + whiteSpace + offsetScalePattern + whiteSpace
				+ minMaxPattern + whiteSpace + unitPattern + whiteSpace + receiverPattern) {
}

void DbcParser::parse_file(std::istream& stream) {
	std::string line;
	std::vector<std::string> lines;

	messages.clear();
	message_id_to_index.clear();

	parse_dbc_header(stream);
	parse_dbc_nodes(stream);

	while (!stream.eof()) {
		Utils::StreamHandler::get_next_non_blank_line(stream, line);
		lines.push_back(line);
	}

	parse_dbc_messages(lines);
}

void DbcParser::parse_file(const std::string& file_name) {
	auto extension = get_extension(file_name);
	if (extension != ".dbc") {
		throw NonDbcFileFormatError(file_name, extension);
	}

#ifdef _WIN32
	std::ifstream stream(utf8_path_to_wide(file_name).c_str());
#else
	std::ifstream stream(file_name.c_str());
#endif
	if (!stream.is_open()) {
		throw DbcFileCouldNotBeOpened(file_name);
	}

	parse_file(stream);
}

std::string DbcParser::get_extension(const std::string& file_name) {
	std::size_t dot = file_name.find_last_of(".");
	if (dot != std::string::npos) {
		return file_name.substr(dot, file_name.size() - dot);
	}

	return "";
}

const std::string& DbcParser::get_version() const {
	return version;
}

const std::vector<std::string>& DbcParser::get_nodes() const {
	return nodes;
}

const std::vector<Libdbc::Message>& DbcParser::get_messages() const {
	return messages;
}

const Message* DbcParser::get_message_by_id(uint32_t message_id) const {
	auto iter = message_id_to_index.find(message_id);
	if (iter != message_id_to_index.end()) {
		return &messages[iter->second];
	}
	return nullptr;
}

Message::ParseSignalsStatus DbcParser::parse_message(const uint32_t message_id, const std::vector<uint8_t>& data, std::vector<double>& out_values) const {
	auto iter = message_id_to_index.find(message_id);
	if (iter != message_id_to_index.end()) {
		return messages[iter->second].parse_signals(data, out_values);
	}
	return Message::ParseSignalsStatus::ErrorUnknownID;
}

void DbcParser::parse_dbc_header(std::istream& file_stream) {
	std::string line;
	std::smatch match;

	// Some DBC exporters (e.g. Vector CANdb++ / Mentor Graphics) prepend a
	// "// ..." comment banner before the VERSION line. That's not part of
	// the DBC grammar, but skip it (and any blank lines) rather than
	// failing outright, since these files are otherwise well-formed.
	for (;;) {
		Utils::StreamHandler::get_line(file_stream, line);
		if (file_stream.eof() || (!line.empty() && Utils::String::trim(line).rfind("//", 0) != 0)) {
			break;
		}
	}

	std::regex_search(line, match, version_re);

	if (match.empty()) {
		throw DbcFileIsMissingVersion(line);
	}

	version = match.str(2);

	Utils::StreamHandler::get_next_non_blank_line(file_stream, line);
	Utils::StreamHandler::skip_to_next_blank_line(file_stream, line);
	Utils::StreamHandler::get_next_non_blank_line(file_stream, line);

	if (!std::regex_search(line, match, bit_timing_re)) {
		throw DbcFileIsMissingBitTiming(line);
	}
}

void DbcParser::parse_dbc_nodes(std::istream& file_stream) {
	std::string line;
	std::smatch match;

	Utils::StreamHandler::get_next_non_blank_line(file_stream, line);

	std::regex_search(line, match, node_re);

	if (match.length() > 2) {
		std::string node = match.str(2);
		Utils::String::split(node, nodes);
	}
}

void DbcParser::parse_message_line(const std::string& line, const std::smatch& match) {
	uint32_t message_id = static_cast<uint32_t>(to_uint(match.str(MESSAGE_ID_GROUP), line, "message id"));
	std::string name = match.str(MESSAGE_NAME_GROUP);
	const uint64_t message_size = to_uint(match.str(MESSAGE_SIZE_GROUP), line, "message size");
	if (message_size > MAX_MESSAGE_SIZE_BYTES) {
		throw DbcFileParseError(line, "Message \"" + name + "\" has an unsupported size (" + std::to_string(message_size) + "); expected at most 255 bytes.");
	}
	uint8_t size = static_cast<uint8_t>(message_size);
	std::string node = match.str(MESSAGE_NODE_GROUP);

	Message msg(message_id, name, size, node);

	message_id_to_index[message_id] = messages.size();
	messages.push_back(msg);
}

void DbcParser::parse_signal_line(const std::string& line, const std::smatch& match) {
	std::string name = match.str(SIGNAL_NAME_GROUP);

	// Multiplexing markers: "M" marks the multiplexor signal, "m<N>" marks
	// a member signal that is only active when the multiplexor value is N.
	bool is_multiplexed = false;
	bool is_multiplexor = false;
	int32_t multiplex_value = -1;
	const std::string multiplex_marker = match.str(SIGNAL_MULTIPLEX_GROUP);
	if (multiplex_marker == "M") {
		is_multiplexor = true;
	} else if (!multiplex_marker.empty()) {
		is_multiplexed = true;
		multiplex_value = static_cast<int32_t>(to_uint(multiplex_marker.substr(1), line, "multiplex value"));
	}

	uint32_t start_bit = static_cast<uint32_t>(to_uint(match.str(SIGNAL_START_BIT_GROUP), line, "signal start bit"));
	uint32_t size = static_cast<uint32_t>(to_uint(match.str(SIGNAL_SIZE_GROUP), line, "signal size"));
	if (size < 1 || size > MAX_SIGNAL_SIZE_BITS) {
		// Some vendor exports (e.g. raw BLE/UWB payload messages) use a
		// single signal spanning the whole message as an undissected
		// blob, well past the 64 bits this library can decode into a
		// double. Skip just that signal instead of failing the whole
		// file, consistent with the MAX_MATCHABLE_LINE_LENGTH skip above.
		missed_lines.push_back(line);
		return;
	}
	bool is_bigendian = (to_uint(match.str(SIGNAL_ENDIAN_GROUP), line, "signal byte order") == 0);
	bool is_signed = (match.str(SIGNAL_SIGNED_GROUP) == "-");

	double factor = 0;
	double offset = 0;
	double min = 0;
	double max = 0;
	if (!Utils::String::try_convert_to_double(match.str(SIGNAL_FACTOR_GROUP), factor)
		|| !Utils::String::try_convert_to_double(match.str(SIGNAL_OFFSET_GROUP), offset)
		|| !Utils::String::try_convert_to_double(match.str(SIGNAL_MIN_GROUP), min)
		|| !Utils::String::try_convert_to_double(match.str(SIGNAL_MAX_GROUP), max)) {
		throw DbcFileParseError(line, "Failed to parse a numeric field of signal \"" + name + "\".");
	}

	std::string unit = match.str(SIGNAL_UNIT_GROUP);

	std::vector<std::string> receivers;
	Utils::String::split(match.str(SIGNAL_RECEIVER_GROUP), receivers, ',');

	Signal sig(name, is_multiplexed, start_bit, size, is_bigendian, is_signed, factor, offset, min, max, unit, receivers);
	sig.is_multiplexor = is_multiplexor;
	sig.multiplex_value = multiplex_value;
	messages.back().append_signal(sig);
}

void DbcParser::parse_value_line(const std::string& line, const std::smatch& match, std::vector<Value>& signal_value) {
	uint32_t message_id = static_cast<uint32_t>(to_uint(match.str(2), line, "value description message id"));
	std::string signal_name = match.str(3);

	// Loop over the rest of the descriptions
	std::string rest_of_descriptions = match.str(4);
	std::regex description_re("\\s(\\d+)\\s\"([^\"]*)\"");

	std::sregex_iterator desc_iter(rest_of_descriptions.begin(), rest_of_descriptions.end(), description_re);
	std::sregex_iterator desc_end = std::sregex_iterator();

	std::vector<Signal::ValueDescription> values{};
	for (std::sregex_iterator i = desc_iter; i != desc_end; ++i) {
		std::smatch desc_match = *desc_iter;
		uint32_t number = static_cast<uint32_t>(to_uint(desc_match.str(1), line, "value description number"));
		std::string text = desc_match.str(2);

		values.push_back(Signal::ValueDescription{number, text});
		++desc_iter;
	}

	signal_value.push_back(Value{message_id, signal_name, values});
}

void DbcParser::parse_dbc_messages(const std::vector<std::string>& lines) {
	std::smatch match;

	std::vector<Value> signal_value;

	for (const auto& line : lines) {
		if (line.length() > MAX_MATCHABLE_LINE_LENGTH) {
			missed_lines.push_back(line);
			continue;
		}

		// message_re, signal_re, and value_re each require a distinct literal prefix
		// ("BO_", "<one whitespace char>SG_", "VAL_"), so at most one of the three can
		// ever match a given line. Checking the cheap prefix first avoids running the
		// other two (expensive, especially on libstdc++'s backtracking std::regex)
		// against lines that could never match them.
		const bool could_be_message = line.compare(0, 3, "BO_") == 0;
		const bool could_be_signal = line.size() >= 4 && std::isspace(static_cast<unsigned char>(line[0])) != 0 && line.compare(1, 3, "SG_") == 0;
		const bool could_be_value = line.compare(0, 4, "VAL_") == 0;

		if (could_be_message && std::regex_search(line, match, message_re)) {
			parse_message_line(line, match);
			continue;
		}

		if (could_be_signal && !messages.empty() && std::regex_search(line, match, signal_re)) {
			parse_signal_line(line, match);
			continue;
		}

		if (could_be_value && !messages.empty() && std::regex_search(line, match, value_re)) {
			parse_value_line(line, match, signal_value);
			continue;
		}

		if (line.length() > 0) {
			missed_lines.push_back(line);
		}
	}

	for (const auto& signal : signal_value) {
		for (auto& msg : messages) {
			if (msg.id() == signal.can_id) {
				msg.add_value_description(signal.signal_name, signal.value_descriptions);
				break;
			}
		}
	}
}

const std::vector<std::string>& DbcParser::unused_lines() const {
	return missed_lines;
}

}
