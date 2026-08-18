#include "testing_utils/common.hpp"
#include "testing_utils/defines.hpp"
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <libdbc/dbc.hpp>
#include <sstream>
#include <string>

// These tests guard against pathological slowdowns (e.g. regex backtracking blowups or
// accidental O(n^2) behavior) rather than exact timing. Bounds are set generously above
// what normal hardware needs, so a failure here indicates a real performance regression.

namespace {

std::string build_large_dbc(int message_count, int signals_per_message) {
	std::ostringstream out;
	out << "VERSION \"1.0.0\"\n\nNS_ :\n\nBS_:\n\nBU_: DBG DRIVER IO MOTOR SENSOR\n\n";

	for (int m = 0; m < message_count; ++m) {
		out << "BO_ " << (100 + m) << " MSG_" << m << ": 8 Vector__XXX\n";
		for (int s = 0; s < signals_per_message; ++s) {
			int start_bit = (s * 8) % 64;
			out << " SG_ Sig_" << m << "_" << s << " : " << start_bit << "|8@1+ (1,0) [0|255] \"unit\" DBG,DRIVER,IO,MOTOR,SENSOR\n";
		}
	}

	for (int m = 0; m < message_count; ++m) {
		out << "VAL_ " << (100 + m) << " Sig_" << m << "_0 1 \"one\" 2 \"two\" 3 \"three\" ;\n";
	}

	return out.str();
}

}

TEST_CASE("Parsing a large dbc file completes quickly", "[performance]") {
	const std::string dbc_contents = build_large_dbc(2000, 5);
	const auto filename = create_temporary_dbc_with(dbc_contents.c_str());

	Libdbc::DbcParser parser;

	auto start = std::chrono::steady_clock::now();
	REQUIRE_NOTHROW(parser.parse_file(filename.c_str()));
	auto elapsed = std::chrono::steady_clock::now() - start;

	REQUIRE(parser.get_messages().size() == 2000);
	REQUIRE(std::chrono::duration_cast<std::chrono::seconds>(elapsed).count() < 10);
}

TEST_CASE("Repeated parse_message calls complete quickly", "[performance]") {
	const std::string dbc_contents = build_large_dbc(50, 8);
	const auto filename = create_temporary_dbc_with(dbc_contents.c_str());

	Libdbc::DbcParser parser;
	parser.parse_file(filename.c_str());

	std::vector<uint8_t> data{1, 2, 3, 4, 5, 6, 7, 8};
	std::vector<double> out_values;

	auto start = std::chrono::steady_clock::now();
	for (int i = 0; i < 100000; ++i) {
		auto status = parser.parse_message(100 + (i % 50), data, out_values);
		REQUIRE(status == Libdbc::Message::ParseSignalsStatus::Success);
	}
	auto elapsed = std::chrono::steady_clock::now() - start;

	REQUIRE(std::chrono::duration_cast<std::chrono::seconds>(elapsed).count() < 10);
}

TEST_CASE("A dbc file with many malformed lines is parsed quickly instead of hanging", "[performance]") {
	// Lines that fail every regex still have to be attempted against each pattern; this
	// guards against catastrophic regex backtracking on non-matching input.
	std::ostringstream out;
	out << PRIMITIVE_DBC;
	out << "BO_ 234 MSG1: 8 Vector__XXX\n";

	for (int i = 0; i < 5000; ++i) {
		// Well formed prefix followed by garbage that never completes a valid match,
		// forcing each regex to search to the end of the line before giving up.
		out << " SG_ Broken" << i << " : not_a_valid_signal_line_" << std::string(40, 'A') << "!!!\n";
	}

	const auto filename = create_temporary_dbc_with(out.str().c_str());
	Libdbc::DbcParser parser;

	auto start = std::chrono::steady_clock::now();
	REQUIRE_NOTHROW(parser.parse_file(filename.c_str()));
	auto elapsed = std::chrono::steady_clock::now() - start;

	REQUIRE(parser.unused_lines().size() == 5000);
	REQUIRE(std::chrono::duration_cast<std::chrono::seconds>(elapsed).count() < 10);
}

TEST_CASE("A single very long malformed line does not crash or hang the parser", "[performance]") {
	// GCC libstdc++'s std::regex backtracks recursively; a long run of word characters
	// with no terminating structure used to stack-overflow the process well before the
	// 20000 characters used here (the crash threshold measured around ~5000 chars).
	std::ostringstream out;
	out << PRIMITIVE_DBC;
	out << "BO_ 234 MSG1: 8 Vector__XXX\n";
	out << " SG_ " << std::string(20000, 'A') << "\n";

	const auto filename = create_temporary_dbc_with(out.str().c_str());
	Libdbc::DbcParser parser;

	auto start = std::chrono::steady_clock::now();
	REQUIRE_NOTHROW(parser.parse_file(filename.c_str()));
	auto elapsed = std::chrono::steady_clock::now() - start;

	REQUIRE(std::chrono::duration_cast<std::chrono::seconds>(elapsed).count() < 10);
}

TEST_CASE("An extremely long blank-ish line does not crash or hang the parser", "[performance]") {
	// Utils::StreamHandler used to test for a blank line with a regex on the raw line;
	// that alone could stack-overflow before any DBC-specific parsing was even attempted.
	std::ostringstream out;
	out << PRIMITIVE_DBC;
	out << std::string(200000, ' ') << "\n";
	out << "BO_ 234 MSG1: 8 Vector__XXX\n";
	out << " SG_ Sig1 : 0|8@1+ (1,0) [0|0] \"\" Vector__XXX\n";

	const auto filename = create_temporary_dbc_with(out.str().c_str());
	Libdbc::DbcParser parser;

	auto start = std::chrono::steady_clock::now();
	REQUIRE_NOTHROW(parser.parse_file(filename.c_str()));
	auto elapsed = std::chrono::steady_clock::now() - start;

	REQUIRE(parser.get_messages().size() == 1);
	REQUIRE(std::chrono::duration_cast<std::chrono::seconds>(elapsed).count() < 10);
}

TEST_CASE("Repeated parse_message calls on maximum size CAN FD frames complete quickly", "[performance][canfd]") {
	// CAN FD frames can be up to 64 bytes (vs. classic CAN's 8), so pack one message with
	// as many signals as will fit to make sure the wider payload doesn't blow up runtime.
	std::ostringstream out;
	out << PRIMITIVE_DBC;
	out << "BO_ 1 MSG_FD: 64 Vector__XXX\n";
	for (int byte = 0; byte < 64; ++byte) {
		out << " SG_ Sig_" << byte << " : " << (byte * 8) << "|8@1+ (1,0) [0|255] \"\" Vector__XXX\n";
	}

	const auto filename = create_temporary_dbc_with(out.str().c_str());
	Libdbc::DbcParser parser;
	parser.parse_file(filename.c_str());

	std::vector<uint8_t> data(64, 0x42);
	std::vector<double> out_values;

	auto start = std::chrono::steady_clock::now();
	for (int i = 0; i < 20000; ++i) {
		auto status = parser.parse_message(1, data, out_values);
		REQUIRE(status == Libdbc::Message::ParseSignalsStatus::Success);
	}
	auto elapsed = std::chrono::steady_clock::now() - start;

	REQUIRE(out_values.size() == 64);
	REQUIRE(std::chrono::duration_cast<std::chrono::seconds>(elapsed).count() < 10);
}

TEST_CASE("A signal line with a realistically large receiver list still parses correctly", "[performance]") {
	// Guards against the stack-overflow line-length cap in DbcParser being set so low
	// that it starts rejecting legitimate (if unusually large) real world dbc content.
	std::vector<std::string> receiver_names;
	for (int i = 0; i < 40; ++i) {
		receiver_names.push_back("NODE_" + std::to_string(i));
	}
	std::string receivers;
	for (size_t i = 0; i < receiver_names.size(); ++i) {
		if (i > 0) {
			receivers += ",";
		}
		receivers += receiver_names.at(i);
	}

	std::string dbc_contents = PRIMITIVE_DBC + "BO_ 234 MSG1: 8 Vector__XXX\n SG_ Sig1 : 0|8@1+ (1,0) [0|0] \"\" " + receivers;
	const auto filename = create_temporary_dbc_with(dbc_contents.c_str());

	Libdbc::DbcParser parser;
	REQUIRE_NOTHROW(parser.parse_file(filename.c_str()));

	REQUIRE(parser.get_messages().size() == 1);
	REQUIRE(parser.get_messages().at(0).get_signals().size() == 1);
	REQUIRE(parser.get_messages().at(0).get_signals().at(0).receivers.size() == 40);
	REQUIRE(parser.unused_lines().empty());
}
