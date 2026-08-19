#ifndef DBC_HPP
#define DBC_HPP

#include <cstddef>
#include <cstdint>
#include <istream>
#include <libdbc/message.hpp>
#include <libdbc/signal.hpp>
#include <regex>
#include <string>
#include <unordered_map>
#include <vector>

namespace Libdbc {

class Parser {
public:
	virtual ~Parser() = default;

	virtual void parse_file(const std::string& file) = 0;
	virtual void parse_file(std::istream& file) = 0;
};

class DbcParser : public Parser {
public:
	DbcParser();

	void parse_file(const std::string& file_name) override;
	void parse_file(std::istream& stream) override;

	const std::string& get_version() const;
	const std::vector<std::string>& get_nodes() const;
	const std::vector<Libdbc::Message>& get_messages() const;
	const Message* get_message_by_id(uint32_t message_id) const;

	Message::ParseSignalsStatus parse_message(uint32_t message_id, const std::vector<uint8_t>& data, std::vector<double>& out_values) const;

	const std::vector<std::string>& unused_lines() const;

private:
	std::string version;
	std::vector<std::string> nodes;
	std::vector<Libdbc::Message> messages;
	std::unordered_map<uint32_t, size_t> message_id_to_index;

	std::regex version_re;
	std::regex bit_timing_re;
	std::regex name_space_re;
	std::regex node_re;
	std::regex message_re;
	std::regex value_re;
	std::regex signal_re;

	std::vector<std::string> missed_lines;

	struct Value {
		uint32_t can_id;
		std::string signal_name;
		std::vector<Signal::ValueDescription> value_descriptions;
	};

	void parse_dbc_header(std::istream& file_stream);
	void parse_dbc_nodes(std::istream& file_stream);
	void parse_dbc_messages(const std::vector<std::string>& lines);

	void parse_message_line(const std::string& line, const std::smatch& match);
	void parse_signal_line(const std::string& line, const std::smatch& match);
	static void parse_value_line(const std::string& line, const std::smatch& match, std::vector<Value>& signal_value);

	static std::string get_extension(const std::string& file_name);
};

}

#endif // DBC_HPP
