#include <catch2/catch_test_macros.hpp>
#include <libdbc/message.hpp>
#include <libdbc/signal.hpp>
#include <sstream>
#include <vector>

// Direct unit tests for Message and Signal, independent of the DBC file parser.

using Libdbc::Message;
using Libdbc::Signal;

namespace {
Signal make_signal(const std::string& name = "Sig", double factor = 1, double offset = 0) {
	return Signal(name, false, 0, 8, false, false, factor, offset, 0, 0, "", {});
}
}

TEST_CASE("Message accessors reflect constructor arguments", "[message]") {
	Message m(42, "Foo", 8, "Bar");

	REQUIRE(m.id() == 42);
	REQUIRE(m.name() == "Foo");
	REQUIRE(m.size() == 8);
	REQUIRE(m.get_signals().empty());
}

TEST_CASE("Message equality compares id, name, size and node", "[message]") {
	Message base(100, "MSG", 8, "NODE");

	REQUIRE(base == Message(100, "MSG", 8, "NODE"));
	REQUIRE_FALSE(base == Message(101, "MSG", 8, "NODE"));
	REQUIRE_FALSE(base == Message(100, "OTHER", 8, "NODE"));
	REQUIRE_FALSE(base == Message(100, "MSG", 4, "NODE"));
	REQUIRE_FALSE(base == Message(100, "MSG", 8, "OTHER"));
}

TEST_CASE("Message equality does not consider appended signals", "[message]") {
	// Characterizes current behavior: Message::operator== only compares the
	// header fields (id/name/size/node), not the signal list.
	Message a(100, "MSG", 8, "NODE");
	Message b(100, "MSG", 8, "NODE");

	a.append_signal(make_signal("OnlyOnA"));

	REQUIRE(a == b);
	REQUIRE(a.get_signals().size() == 1);
	REQUIRE(b.get_signals().empty());
}

TEST_CASE("Message stream operator formats id, name, size and node", "[message]") {
	Message m(100, "MSG", 8, "NODE");
	std::ostringstream oss;
	oss << m;

	REQUIRE(oss.str() == "Message: {id: 100, name: MSG, size: 8, node: NODE}");
}

TEST_CASE("Message append_signal accumulates signals in order", "[message]") {
	Message m(1, "M", 8, "N");
	m.append_signal(make_signal("First"));
	m.append_signal(make_signal("Second"));

	auto signals = m.get_signals();
	REQUIRE(signals.size() == 2);
	REQUIRE(signals.at(0).name == "First");
	REQUIRE(signals.at(1).name == "Second");
}

TEST_CASE("Message add_value_description updates the matching signal only", "[message]") {
	Message m(1, "M", 8, "N");
	m.append_signal(make_signal("Sig1"));
	m.append_signal(make_signal("Sig2"));

	std::vector<Signal::ValueDescription> descriptions{{1, "one"}, {2, "two"}};
	m.add_value_description("Sig2", descriptions);

	auto signals = m.get_signals();
	REQUIRE(signals.at(0).value_descriptions.empty());
	REQUIRE(signals.at(1).value_descriptions.size() == 2);
	REQUIRE(signals.at(1).value_descriptions.at(0).value == 1);
	REQUIRE(signals.at(1).value_descriptions.at(0).description == "one");
}

TEST_CASE("Message add_value_description for an unknown signal name is a no-op", "[message]") {
	Message m(1, "M", 8, "N");
	m.append_signal(make_signal("Sig1"));

	m.add_value_description("DoesNotExist", {{1, "one"}});

	REQUIRE(m.get_signals().at(0).value_descriptions.empty());
}

TEST_CASE("Signal equality compares all constructor fields including factor", "[signal]") {
	Signal base = make_signal("Sig", 2.0, 1.0);

	REQUIRE(base == make_signal("Sig", 2.0, 1.0));
	REQUIRE_FALSE(base == make_signal("Other", 2.0, 1.0));
	REQUIRE_FALSE(base == make_signal("Sig", 3.0, 1.0));
	REQUIRE_FALSE(base == make_signal("Sig", 2.0, 5.0));
}

TEST_CASE("Signal operator< orders by start_bit", "[signal]") {
	Signal low("Low", false, 0, 8, false, false, 1, 0, 0, 0, "", {});
	Signal high("High", false, 16, 8, false, false, 1, 0, 0, 0, "", {});

	REQUIRE(low < high);
	REQUIRE_FALSE(high < low);
}

TEST_CASE("Signal stream operator includes key fields", "[signal]") {
	Signal s("MySignal", false, 3, 8, true, true, 1, 0, 0, 0, "km/h", {"NodeA", "NodeB"});
	std::ostringstream oss;
	oss << s;

	std::string output = oss.str();
	REQUIRE(output.find("MySignal") != std::string::npos);
	REQUIRE(output.find("Start bit: 3") != std::string::npos);
	REQUIRE(output.find("Big endian") != std::string::npos);
	REQUIRE(output.find("Signed") != std::string::npos);
	REQUIRE(output.find("km/h") != std::string::npos);
}
