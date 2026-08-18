// Minimal example: parse a DBC file and decode a CAN frame.
//
// Build & run (from the repo root, with a build dir configured with tests on):
//   ./build/examples/dbc_example path/to/file.dbc

#include <cstdio>
#include <libdbc/dbc.hpp>
#include <vector>

int main(int argc, char** argv) {
	if (argc < 2) {
		std::fprintf(stderr, "usage: %s <file.dbc>\n", argv[0]);
		return 1;
	}

	try {
		Libdbc::DbcParser parser;
		parser.parse_file(argv[1]);

		std::printf("Version: %s\n", parser.get_version().c_str());
		std::printf("Nodes (%zu):", parser.get_nodes().size());
		for (const auto& node : parser.get_nodes()) {
			std::printf(" %s", node.c_str());
		}
		std::printf("\n\nMessages (%zu):\n", parser.get_messages().size());

		for (const auto& message : parser.get_messages()) {
			std::printf("  %s (id=%u, %u bytes, %zu signals)\n",
						message.name().c_str(),
						message.id(),
						static_cast<unsigned>(message.size()),
						message.get_signals().size());
		}

		// Decode an 8-byte frame for the first message that fits a classic CAN frame.
		for (const auto& message : parser.get_messages()) {
			if (message.size() > 8 || message.get_signals().empty()) {
				continue;
			}
			const std::vector<uint8_t> data(message.size(), 0x42);
			std::vector<double> values;
			const auto status = parser.parse_message(message.id(), data, values);
			std::printf("\nDecoding %u zeroed-0x42 frame -> %s (%zu values):\n",
						message.id(),
						status == Libdbc::Message::ParseSignalsStatus::Success ? "success" : "error",
						values.size());
			for (size_t i = 0; i < values.size() && i < message.get_signals().size(); ++i) {
				const auto& signal = message.get_signals().at(i);
				std::printf("  %s = %f %s\n", signal.name.c_str(), values.at(i), signal.unit.c_str());
			}
			break;
		}

		if (!parser.unused_lines().empty()) {
			std::printf("\nNote: %zu lines were not recognized (unsupported DBC sections).\n", parser.unused_lines().size());
		}
	} catch (const std::exception& e) {
		std::fprintf(stderr, "Failed to parse dbc file: %s\n", e.what());
		return 1;
	}

	return 0;
}
