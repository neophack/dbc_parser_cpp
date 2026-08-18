#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>

#include <libdbc/dbc.hpp>

#include "testing_utils/common.hpp"
#include "testing_utils/defines.hpp"

// Testing of parsing messages

TEST_CASE("Parse Message Unknown ID") {
	std::string dbc_contents = PRIMITIVE_DBC + R"(BO_ 234 MSG1: 8 Vector__XXX
 SG_ Msg1Sig1 : 0|8@0+ (1,0) [-3276.8|-3276.7] "C" Vector__XXX
 SG_ MsgSig2 : 8|8@0+ (1,0) [-3276.8|-3276.7] "C" Vector__XXX
BO_ 123 MSG2: 8 Vector__XXX
 SG_ Msg2Sig1 : 0|8@0+ (1,0) [-3276.8|-3276.7] "C" Vector__XXX
 SG_ Msg2Sig1 : 8|8@0+ (1,0) [-3276.8|-3276.7] "C" Vector__XXX
)";
	const auto filename = create_temporary_dbc_with(dbc_contents.c_str());

	Libdbc::DbcParser parser;
	parser.parse_file(filename.c_str());

	SECTION("Evaluating unknown` message id") {
		std::vector<double> out_values;
		CHECK(parser.parse_message(578, std::vector<uint8_t>({0xFF, 0xA2}), out_values) == Libdbc::Message::ParseSignalsStatus::ErrorUnknownID);
	}
}

TEST_CASE("Parse Message Big Number not aligned little endian") {
	std::string dbc_contents = PRIMITIVE_DBC + R"(BO_ 337 STATUS: 8 Vector__XXX
 SG_ Value6 : 27|3@1+ (1,0) [0|7] ""  Vector__XXX
 SG_ Value5 : 16|11@1+ (0.1,-102) [-102|102] "%"  Vector__XXX
 SG_ Value2 : 8|2@1+ (1,0) [0|2] ""  Vector__XXX
 SG_ Value3 : 10|1@1+ (1,0) [0|1] ""  Vector__XXX
 SG_ Value7 : 30|2@1+ (1,0) [0|3] ""  Vector__XXX
 SG_ Value4 : 11|4@1+ (1,0) [0|3] ""  Vector__XXX
 SG_ Value1 : 0|8@1+ (1,0) [0|204] "Km/h"  Vector__XXX
)";
	const auto filename = create_temporary_dbc_with(dbc_contents.c_str());

	Libdbc::DbcParser parser;
	parser.parse_file(filename);

	SECTION("Evaluating first message") {
		std::vector<double> out_values;
		CHECK(parser.parse_message(337, std::vector<uint8_t>({0, 4, 252, 19, 0, 0, 0, 0}), out_values) == Libdbc::Message::ParseSignalsStatus::Success);
		std::vector<double> refData{2, 0, 0, 1, 0, 0, 0};
		CHECK(refData.size() == 7);
		CHECK(out_values.size() == refData.size());
		for (size_t i = 0; i < refData.size(); i++) {
			CHECK(out_values.at(i) == refData.at(i));
		}
	}

	SECTION("Evaluating second message") {
		std::vector<double> out_values;
		CHECK(parser.parse_message(337, std::vector<uint8_t>({47, 4, 60, 29, 0, 0, 0, 0}), out_values) == Libdbc::Message::ParseSignalsStatus::Success);
		std::vector<double> refData{3, 32, 0, 1, 0, 0, 47};
		CHECK(refData.size() == 7);
		CHECK(out_values.size() == refData.size());
		for (size_t i = 0; i < refData.size(); i++) {
			CHECK(out_values.at(i) == refData.at(i));
		}
	}

	SECTION("Evaluating third message") {
		std::vector<double> out_values;
		CHECK(parser.parse_message(337, std::vector<uint8_t>({57, 4, 250, 29, 0, 0, 0, 0}), out_values) == Libdbc::Message::ParseSignalsStatus::Success);
		std::vector<double> refData{3, 51, 0, 1, 0, 0, 57};
		CHECK(refData.size() == 7);
		CHECK(out_values.size() == refData.size());
		for (size_t i = 0; i < refData.size(); i++) {
			CHECK(out_values.at(i) == refData.at(i));
		}
	}
}

TEST_CASE("Parse Message little endian") {
	std::string dbc_contents = PRIMITIVE_DBC + R"(BO_ 541 STATUS: 8 DEVICE1
 SG_ Temperature : 48|16@1+ (0.01,-40) [-40|125] "C"  DEVICE1
 SG_ SOH : 0|16@1+ (0.01,0) [0|100] "%"  DEVICE1
 SG_ SOE : 32|16@1+ (0.01,0) [0|100] "%"  DEVICE1
 SG_ SOC : 16|16@1+ (0.01,0) [0|100] "%"  DEVICE1)";
	const auto filename = create_temporary_dbc_with(dbc_contents.c_str());

	Libdbc::DbcParser parser;
	parser.parse_file(filename);

	std::vector<uint8_t> data{0x08, 0x27, 0xa3, 0x22, 0xe5, 0x1f, 0x45, 0x14}; // little endian
	std::vector<double> result_values;
	REQUIRE(parser.parse_message(0x21d, data, result_values) == Libdbc::Message::ParseSignalsStatus::Success);
	REQUIRE(result_values.size() == 4);

	REQUIRE(Catch::Approx(result_values.at(0)) == 11.89);
	REQUIRE(Catch::Approx(result_values.at(1)) == 99.92);
	REQUIRE(Catch::Approx(result_values.at(2)) == 81.65);
	REQUIRE(Catch::Approx(result_values.at(3)) == 88.67);
}

TEST_CASE("Parse Message big endian signed values") {
	std::string dbc_contents = PRIMITIVE_DBC + R"(BO_ 545 MSG: 8 BMS2
 SG_ Sig1 : 62|1@0+ (1,0) [0|0] "" Vector__XXX
 SG_ Sig2 : 49|2@0+ (1,0) [0|0] "" Vector__XXX
 SG_ Sig3 : 39|16@0- (0.1,0) [0|0] "A" Vector__XXX
 SG_ Sig4 : 60|1@0+ (1,0) [0|0] "" Vector__XXX
 SG_ Sig5 : 55|1@0+ (1,0) [0|0] "" Vector__XXX
 SG_ Sig6 : 58|1@0+ (1,0) [0|0] "" Vector__XXX
 SG_ Sig7 : 59|1@0+ (1,0) [0|0] "" Vector__XXX
 SG_ Sig8 : 57|1@0+ (1,0) [0|0] "" Vector__XXX
 SG_ Sig9 : 56|1@0+ (1,0) [0|0] "" Vector__XXX
 SG_ Sig10 : 61|1@0+ (1,0) [0|0] "" Vector__XXX
 SG_ Sig11 : 7|16@0+ (0.001,0) [0|65.535] "V" Vector__XXX
 SG_ Sig12 : 23|16@0+ (0.1,0) [0|6553.5] "A" Vector__XXX)";
	const auto filename = create_temporary_dbc_with(dbc_contents.c_str());

	Libdbc::DbcParser p;
	p.parse_file(filename.c_str());

	std::vector<uint8_t> data{13, 177, 0, 216, 251, 180, 0, 31}; // big endian
	std::vector<double> result_values;
	REQUIRE(p.parse_message(545, data, result_values) == Libdbc::Message::ParseSignalsStatus::Success);
	REQUIRE(result_values.size() == 12);
	REQUIRE(Catch::Approx(result_values.at(0)) == 0);
	REQUIRE(Catch::Approx(result_values.at(1)) == 0);
	REQUIRE(Catch::Approx(result_values.at(2)) == -110);
	REQUIRE(Catch::Approx(result_values.at(3)) == 1);
	REQUIRE(Catch::Approx(result_values.at(4)) == 0);
	REQUIRE(Catch::Approx(result_values.at(5)) == 1);
	REQUIRE(Catch::Approx(result_values.at(6)) == 1);
	REQUIRE(Catch::Approx(result_values.at(7)) == 1);
	REQUIRE(Catch::Approx(result_values.at(8)) == 1);
	REQUIRE(Catch::Approx(result_values.at(9)) == 0);
	REQUIRE(Catch::Approx(result_values.at(10)) == 3.5050);
	REQUIRE(Catch::Approx(result_values.at(11)) == 21.6);
}

TEST_CASE("Parse Message with non byte aligned values") {
	std::string dbc_contents = PRIMITIVE_DBC + R"(BO_ 403 INFORMATION: 8 Vector__XXX
 SG_ Voltage : 30|9@1+ (0.2,0) [0|102.2] "V"  Vector__XXX
 SG_ Phase_Current : 20|10@1- (1,0) [-512|512] "A"  Vector__XXX
 SG_ Iq_Current : 10|10@1- (1,0) [-512|512] "A"  Vector__XXX
 SG_ Id_Current : 0|10@1- (1,0) [-512|512] "A"  Vector__XXX)";
	const auto filename = create_temporary_dbc_with(dbc_contents.c_str());

	Libdbc::DbcParser p;
	p.parse_file(filename);

	std::vector<uint8_t> data{131, 51, 33, 9, 33, 0, 0, 0};
	std::vector<double> result_values;
	REQUIRE(p.parse_message(403, data, result_values) == Libdbc::Message::ParseSignalsStatus::Success);
	REQUIRE(result_values.size() == 4);
	REQUIRE(Catch::Approx(result_values.at(0)) == 26.4);
	REQUIRE(Catch::Approx(result_values.at(1)) == 146);
	REQUIRE(Catch::Approx(result_values.at(2)) == 76);
	REQUIRE(Catch::Approx(result_values.at(3)) == -125);
}

TEST_CASE("Parse Message data length < 8 unsigned") {
	std::string dbc_contents = PRIMITIVE_DBC + R"(BO_ 234 MSG1: 8 Vector__XXX
 SG_ Msg1Sig1 : 7|8@0+ (1,0) [-3276.8|-3276.7] "C" Vector__XXX
 SG_ Msg1Sig2 : 15|8@0+ (1,0) [-3276.8|-3276.7] "km/h" Vector__XXX)";
	const auto filename = create_temporary_dbc_with(dbc_contents.c_str());

	Libdbc::DbcParser p;
	p.parse_file(filename);

	std::vector<uint8_t> data{0x1, 0x2};
	std::vector<double> result_values;
	REQUIRE(p.parse_message(234, data, result_values) == Libdbc::Message::ParseSignalsStatus::Success);
	REQUIRE(result_values.size() == 2);
	REQUIRE(Catch::Approx(result_values.at(0)) == 0x1);
	REQUIRE(Catch::Approx(result_values.at(1)) == 0x2);
}

TEST_CASE("Parse message with BO_ on single line should fail.") {
	std::string dbc_contents = PRIMITIVE_DBC + R"(BO_
234 MSG1: 8 Vector__XXX
 SG_ State1 : 0|8@1+ (1,0) [0|200] "Km/h"  DEVICE1,DEVICE2,DEVICE3
 SG_ State2 : 0|8@1+ (1,0) [0|204] "" DEVICE1,DEVICE2,DEVICE3
VAL_ 234 State1 123 "Description 1" 0 "Description 2" 90903489 "Big value and special characters &$Â§())!")";
	const auto filename = create_temporary_dbc_with(dbc_contents.c_str());

	Libdbc::DbcParser p;
	p.parse_file(filename);

	std::vector<uint8_t> data{0x1, 0x2};
	std::vector<double> result_values;
	REQUIRE(p.get_messages().size() == 0);
	REQUIRE(p.parse_message(234, data, result_values) == Libdbc::Message::ParseSignalsStatus::ErrorUnknownID);
}

TEST_CASE("get_message_by_id returns the message for a known id, nullptr otherwise") {
	std::string dbc_contents = PRIMITIVE_DBC + R"(BO_ 234 MSG1: 8 Vector__XXX
 SG_ Msg1Sig1 : 0|8@0+ (1,0) [0|0] "" Vector__XXX)";
	const auto filename = create_temporary_dbc_with(dbc_contents.c_str());

	Libdbc::DbcParser p;
	p.parse_file(filename.c_str());

	const auto* found = p.get_message_by_id(234);
	REQUIRE(found != nullptr);
	REQUIRE(found->name() == "MSG1");

	REQUIRE(p.get_message_by_id(999) == nullptr);
}

TEST_CASE("Parse Message with a signal outside the data bounds returns ErrorSignalOutOfBounds") {
	std::string dbc_contents = PRIMITIVE_DBC + R"(BO_ 234 MSG1: 8 Vector__XXX
 SG_ Msg1Sig1 : 0|8@1+ (1,0) [0|0] "" Vector__XXX)";
	const auto filename = create_temporary_dbc_with(dbc_contents.c_str());

	Libdbc::DbcParser p;
	p.parse_file(filename);

	// Signal spans bits 0..7 and one byte is supplied, so it fits.
	std::vector<uint8_t> short_data{0x00};
	std::vector<double> out_values;
	REQUIRE(p.parse_message(234, short_data, out_values) == Libdbc::Message::ParseSignalsStatus::Success);

	SECTION("Little endian signal that does not fit the data returns an error") {
		std::string inner_dbc = PRIMITIVE_DBC + R"(BO_ 234 MSG1: 8 Vector__XXX
 SG_ Msg1Sig1 : 56|16@1+ (1,0) [0|0] "" Vector__XXX)";
		const auto inner_filename = create_temporary_dbc_with(inner_dbc.c_str());
		Libdbc::DbcParser p2;
		p2.parse_file(inner_filename);

		std::vector<uint8_t> one_byte{0xFF};
		REQUIRE(p2.parse_message(234, one_byte, out_values) == Libdbc::Message::ParseSignalsStatus::ErrorSignalOutOfBounds);
	}

	SECTION("Big endian signal that does not fit the data returns an error") {
		std::string inner_dbc = PRIMITIVE_DBC + R"(BO_ 234 MSG1: 8 Vector__XXX
 SG_ Msg1Sig1 : 71|16@0+ (1,0) [0|0] "" Vector__XXX)";
		const auto inner_filename = create_temporary_dbc_with(inner_dbc.c_str());
		Libdbc::DbcParser p2;
		p2.parse_file(inner_filename);

		std::vector<uint8_t> one_byte{0xFF};
		REQUIRE(p2.parse_message(234, one_byte, out_values) == Libdbc::Message::ParseSignalsStatus::ErrorSignalOutOfBounds);
	}
}

TEST_CASE("Parse Message data larger than 64 bytes returns ErrorMessageToLong") {
	std::string dbc_contents = PRIMITIVE_DBC + R"(BO_ 234 MSG1: 8 Vector__XXX
 SG_ Msg1Sig1 : 0|8@0+ (1,0) [0|0] "" Vector__XXX)";
	const auto filename = create_temporary_dbc_with(dbc_contents.c_str());

	Libdbc::DbcParser p;
	p.parse_file(filename.c_str());

	std::vector<uint8_t> too_long(65, 0);
	std::vector<double> out_values;
	REQUIRE(p.parse_message(234, too_long, out_values) == Libdbc::Message::ParseSignalsStatus::ErrorMessageToLong);

	std::vector<uint8_t> exactly_64(64, 0);
	REQUIRE(p.parse_message(234, exactly_64, out_values) == Libdbc::Message::ParseSignalsStatus::Success);
}

TEST_CASE("Parse Message full 64 bit wide signal, little endian") {
	std::string dbc_contents = PRIMITIVE_DBC + R"(BO_ 1 MSG1: 8 Vector__XXX
 SG_ Unsigned64 : 0|64@1+ (1,0) [0|0] "" Vector__XXX)";
	const auto filename = create_temporary_dbc_with(dbc_contents.c_str());

	Libdbc::DbcParser p;
	p.parse_file(filename.c_str());

	std::vector<uint8_t> data(8, 0xFF);
	std::vector<double> out_values;
	REQUIRE(p.parse_message(1, data, out_values) == Libdbc::Message::ParseSignalsStatus::Success);
	REQUIRE(out_values.size() == 1);
	REQUIRE(Catch::Approx(out_values.at(0)) == 18446744073709551615.0);
}

TEST_CASE("Parse Message 64 bit signal with sub-byte offset spanning 9 bytes, little endian") {
	// start_bit 4 + 64 bits covers bits 4..67, i.e. 9 bytes of payload. The Intel
	// fast path accumulates bytes into a 64-bit temporary, so this crossing must
	// still extract the high bits that live past the first 8 bytes.
	std::string dbc_contents = PRIMITIVE_DBC + R"(BO_ 1 MSG1: 16 Vector__XXX
 SG_ Offset64 : 4|64@1+ (1,0) [0|0] "" Vector__XXX)";
	const auto filename = create_temporary_dbc_with(dbc_contents.c_str());

	Libdbc::DbcParser p;
	p.parse_file(filename.c_str());

	SECTION("All covered bits set yields the maximum 64 bit value") {
		std::vector<uint8_t> data(9, 0xFF);
		std::vector<double> out_values;
		REQUIRE(p.parse_message(1, data, out_values) == Libdbc::Message::ParseSignalsStatus::Success);
		REQUIRE(out_values.size() == 1);
		REQUIRE(Catch::Approx(out_values.at(0)) == 18446744073709551615.0);
	}

	SECTION("Only the highest covered bits are set") {
		// Bits 67..64 live in byte 8, beyond the 8 bytes the fast path temporary holds.
		std::vector<uint8_t> data(9, 0);
		data[8] = 0x0F; // payload bits 67..64 -> raw bits 63..60
		std::vector<double> out_values;
		REQUIRE(p.parse_message(1, data, out_values) == Libdbc::Message::ParseSignalsStatus::Success);
		REQUIRE(out_values.size() == 1);
		REQUIRE(Catch::Approx(out_values.at(0)) == 17293822569102704640.0); // 0xF << 60
	}

	SECTION("Only the lowest covered bit is set") {
		std::vector<uint8_t> data(9, 0);
		data[0] = 0x10; // bit 4 set
		std::vector<double> out_values;
		REQUIRE(p.parse_message(1, data, out_values) == Libdbc::Message::ParseSignalsStatus::Success);
		REQUIRE(out_values.size() == 1);
		REQUIRE(Catch::Approx(out_values.at(0)) == 1.0);
	}
}

TEST_CASE("Parse Message 60 bit signal spanning 9 bytes, little endian") {
	// start_bit 12 + 60 bits covers bits 12..71: bytes 1 through 9.
	std::string dbc_contents = PRIMITIVE_DBC + R"(BO_ 1 MSG1: 16 Vector__XXX
 SG_ Span60 : 12|60@1+ (1,0) [0|0] "" Vector__XXX)";
	const auto filename = create_temporary_dbc_with(dbc_contents.c_str());

	Libdbc::DbcParser p;
	p.parse_file(filename.c_str());

	std::vector<uint8_t> data = {0x00, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0xA8};
	std::vector<double> out_values;
	REQUIRE(p.parse_message(1, data, out_values) == Libdbc::Message::ParseSignalsStatus::Success);
	REQUIRE(out_values.size() == 1);

	// The signal covers payload bits 12..71: bytes at indices 1..8 read little
	// endian, then shifted right by 4. data[9] is outside the signal and must be
	// ignored.
	uint64_t low = 0;
	for (int i = 1; i <= 8; ++i) {
		low |= static_cast<uint64_t>(data[i]) << ((i - 1) * 8);
	}
	const uint64_t expected = low >> 4;
	REQUIRE(Catch::Approx(out_values.at(0)) == static_cast<double>(expected));
}

TEST_CASE("Parse Message full 64 bit wide signed signal, little endian, all bits set is -1") {
	std::string dbc_contents = PRIMITIVE_DBC + R"(BO_ 1 MSG1: 8 Vector__XXX
 SG_ Signed64 : 0|64@1- (1,0) [0|0] "" Vector__XXX)";
	const auto filename = create_temporary_dbc_with(dbc_contents.c_str());

	Libdbc::DbcParser p;
	p.parse_file(filename.c_str());

	std::vector<uint8_t> data(8, 0xFF);
	std::vector<double> out_values;
	REQUIRE(p.parse_message(1, data, out_values) == Libdbc::Message::ParseSignalsStatus::Success);
	REQUIRE(out_values.size() == 1);
	REQUIRE(Catch::Approx(out_values.at(0)) == -1.0);
}

TEST_CASE("Parse Message full 64 bit wide signed signal, big endian, all bits set is -1") {
	std::string dbc_contents = PRIMITIVE_DBC + R"(BO_ 1 MSG1: 8 Vector__XXX
 SG_ Signed64Be : 7|64@0- (1,0) [0|0] "" Vector__XXX)";
	const auto filename = create_temporary_dbc_with(dbc_contents.c_str());

	Libdbc::DbcParser p;
	p.parse_file(filename.c_str());

	std::vector<uint8_t> data(8, 0xFF);
	std::vector<double> out_values;
	REQUIRE(p.parse_message(1, data, out_values) == Libdbc::Message::ParseSignalsStatus::Success);
	REQUIRE(out_values.size() == 1);
	REQUIRE(Catch::Approx(out_values.at(0)) == -1.0);
}

TEST_CASE("Parse Message single bit signals, both boolean states") {
	std::string dbc_contents = PRIMITIVE_DBC + R"(BO_ 1 MSG1: 1 Vector__XXX
 SG_ Flag : 0|1@1+ (1,0) [0|0] "" Vector__XXX)";
	const auto filename = create_temporary_dbc_with(dbc_contents.c_str());

	Libdbc::DbcParser p;
	p.parse_file(filename.c_str());

	std::vector<double> out_values;

	REQUIRE(p.parse_message(1, std::vector<uint8_t>{0x00}, out_values) == Libdbc::Message::ParseSignalsStatus::Success);
	REQUIRE(Catch::Approx(out_values.at(0)) == 0.0);

	REQUIRE(p.parse_message(1, std::vector<uint8_t>{0x01}, out_values) == Libdbc::Message::ParseSignalsStatus::Success);
	REQUIRE(Catch::Approx(out_values.at(0)) == 1.0);
}

TEST_CASE("Parse Message single bit signed signal treats set bit as -1") {
	std::string dbc_contents = PRIMITIVE_DBC + R"(BO_ 1 MSG1: 1 Vector__XXX
 SG_ Flag : 0|1@1- (1,0) [0|0] "" Vector__XXX)";
	const auto filename = create_temporary_dbc_with(dbc_contents.c_str());

	Libdbc::DbcParser p;
	p.parse_file(filename.c_str());

	std::vector<double> out_values;
	REQUIRE(p.parse_message(1, std::vector<uint8_t>{0x01}, out_values) == Libdbc::Message::ParseSignalsStatus::Success);
	REQUIRE(Catch::Approx(out_values.at(0)) == -1.0);
}

TEST_CASE("Parse Message with data shorter than the DBC declared size") {
	std::string dbc_contents = PRIMITIVE_DBC + R"(BO_ 234 MSG1: 8 Vector__XXX
 SG_ Fits : 0|8@1+ (2,10) [0|0] "" Vector__XXX)";
	const auto filename = create_temporary_dbc_with(dbc_contents.c_str());

	Libdbc::DbcParser p;
	p.parse_file(filename);

	// Only 2 bytes supplied though the message is declared as 8 bytes wide.
	std::vector<uint8_t> data{0x05, 0x00};
	std::vector<double> out_values;
	REQUIRE(p.parse_message(234, data, out_values) == Libdbc::Message::ParseSignalsStatus::Success);
	REQUIRE(out_values.size() == 1);
	REQUIRE(Catch::Approx(out_values.at(0)) == 20.0); // 0x05 * 2 + 10

	// A little-endian signal that doesn't fit within the supplied data makes the
	// whole parse fail instead of silently decoding a flat 0.0.
	std::string oob_dbc = PRIMITIVE_DBC + R"(BO_ 234 MSG1: 8 Vector__XXX
 SG_ TooFarLittleEndian : 16|16@1+ (2,100) [0|0] "" Vector__XXX)";
	const auto oob_filename = create_temporary_dbc_with(oob_dbc.c_str());
	Libdbc::DbcParser p2;
	p2.parse_file(oob_filename);
	REQUIRE(p2.parse_message(234, data, out_values) == Libdbc::Message::ParseSignalsStatus::ErrorSignalOutOfBounds);
}

TEST_CASE("Parse Message CAN FD sized message (64 bytes) with signals across the full payload") {
	// Classic CAN caps a message at 8 bytes; CAN FD allows up to 64. The DBC BO_ size
	// field and parse_message's data buffer both need to support the full range.
	std::string dbc_contents = PRIMITIVE_DBC + R"(BO_ 1 MSG_FD: 64 Vector__XXX
 SG_ FirstByte : 0|8@1+ (1,0) [0|0] "" Vector__XXX
 SG_ MidByte : 256|8@1+ (1,0) [0|0] "" Vector__XXX
 SG_ LastByteLittleEndian : 504|8@1+ (1,0) [0|0] "" Vector__XXX
 SG_ LastByteBigEndian : 511|8@0+ (1,0) [0|0] "" Vector__XXX
 SG_ LastBitLittleEndian : 511|1@1+ (1,0) [0|0] "" Vector__XXX)";
	const auto filename = create_temporary_dbc_with(dbc_contents.c_str());

	Libdbc::DbcParser p;
	p.parse_file(filename.c_str());

	REQUIRE(p.get_messages().size() == 1);
	REQUIRE(p.get_messages().at(0).size() == 64);

	std::vector<uint8_t> data(64, 0);
	data[0] = 0xAB; // FirstByte
	data[32] = 0xCD; // MidByte, byte 32 == bit 256
	data[63] = 0x81; // LastByte*/LastBit*, top and bottom bits set

	std::vector<double> out_values;
	REQUIRE(p.parse_message(1, data, out_values) == Libdbc::Message::ParseSignalsStatus::Success);
	REQUIRE(out_values.size() == 5);
	REQUIRE(Catch::Approx(out_values.at(0)) == 0xAB);
	REQUIRE(Catch::Approx(out_values.at(1)) == 0xCD);
	REQUIRE(Catch::Approx(out_values.at(2)) == 0x81);
	REQUIRE(Catch::Approx(out_values.at(3)) == 0x81);
	REQUIRE(Catch::Approx(out_values.at(4)) == 1);
}

TEST_CASE("Parse Message with an intermediate CAN FD DLC size (32 bytes)") {
	// CAN FD frames only take specific DLC-mapped sizes (0-8, 12, 16, 20, 24, 32, 48, 64);
	// this exercises one of the non-8, non-64 sizes to make sure nothing assumes 8 or 64.
	std::string dbc_contents = PRIMITIVE_DBC + R"(BO_ 2 MSG32: 32 Vector__XXX
 SG_ Last : 248|8@1+ (2,5) [0|0] "" Vector__XXX)";
	const auto filename = create_temporary_dbc_with(dbc_contents.c_str());

	Libdbc::DbcParser p;
	p.parse_file(filename.c_str());

	REQUIRE(p.get_messages().at(0).size() == 32);

	std::vector<uint8_t> data(32, 0);
	data[31] = 10;
	std::vector<double> out_values;
	REQUIRE(p.parse_message(2, data, out_values) == Libdbc::Message::ParseSignalsStatus::Success);
	REQUIRE(out_values.size() == 1);
	REQUIRE(Catch::Approx(out_values.at(0)) == 25.0); // 10 * 2 + 5
}

TEST_CASE("Parse signal with SG_ on single line should fail.") {
	std::string dbc_contents = PRIMITIVE_DBC + R"(BO_ 234 MSG1: 8 Vector__XXX
 SG_
State1 : 0|8@1+ (1,0) [0|200] "Km/h"  DEVICE1,DEVICE2,DEVICE3
 SG_ State2 : 0|8@1+ (1,0) [0|204] "" DEVICE1,DEVICE2,DEVICE3
VAL_ 234 State1 123 "Description 1" 0 "Description 2" 90903489 "Big value and special characters &$Â§())!")";
	const auto filename = create_temporary_dbc_with(dbc_contents.c_str());

	Libdbc::DbcParser p;
	p.parse_file(filename);

	std::vector<uint8_t> data{0x1, 0x2};
	std::vector<double> result_values;
	REQUIRE(p.get_messages().size() == 1);
	REQUIRE(p.parse_message(234, data, result_values) == Libdbc::Message::ParseSignalsStatus::Success);
	REQUIRE(result_values.size() == 1);
	REQUIRE(Catch::Approx(result_values.at(0)) == 0x1);
}
