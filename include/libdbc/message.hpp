#ifndef MESSAGE_HPP
#define MESSAGE_HPP

#include <cstdint>
#include <iostream>
#include <libdbc/signal.hpp>
#include <string>
#include <vector>

namespace Libdbc {

// A received CAN / CAN FD frame. `is_fd` distinguishes a CAN FD frame from a
// classic CAN frame; `brs` (bit rate switch) and `esi` (error state indicator)
// are CAN FD control flags and are rejected on classic frames.
struct CanFrame {
	uint32_t id = 0;
	bool is_fd = false;
	bool brs = false;
	bool esi = false;
	std::vector<uint8_t> data;
};

struct Message {
	Message() = delete;
	~Message() = default;
	explicit Message(uint32_t message_id, const std::string& name, uint8_t size, const std::string& node);

	enum class ParseSignalsStatus {
		Success,
		ErrorMessageToLong,
		ErrorUnknownID,
		ErrorInvalidSignalSize,
		ErrorSignalOutOfBounds,
		ErrorClassicFrameTooLong,
		ErrorInvalidFlags,
	};

	// Decodes `data` into physical values, appended to `values` in signal
	// declaration order. If the message contains a multiplexor, only the
	// multiplexor and the multiplexed signals matching its raw value are
	// decoded, so `values` may have fewer entries than there are signals.
	ParseSignalsStatus parse_signals(const std::vector<uint8_t>& data, std::vector<double>& values) const;

	// Frame-aware overload: validates the frame type before decoding.
	//  - classic frame with more than 8 data bytes  -> ErrorClassicFrameTooLong
	//  - classic frame with brs/esi set              -> ErrorInvalidFlags
	//  - CAN FD frame with more than 64 data bytes   -> ErrorMessageToLong
	ParseSignalsStatus parse_signals(const CanFrame& frame, std::vector<double>& values) const;

	void append_signal(const Signal& signal);
	const std::vector<Signal>& get_signals() const;
	uint32_t id() const;
	uint8_t size() const;
	const std::string& name() const;
	void add_value_description(const std::string& signal_name, const std::vector<Signal::ValueDescription>&);

	bool operator==(const Message& rhs) const;

private:
	uint32_t m_id;
	std::string m_name;
	uint8_t m_size;
	std::string m_node;
	std::vector<Signal> m_signals;

	friend std::ostream& operator<<(std::ostream& out, const Message& msg);
};

std::ostream& operator<<(std::ostream& out, const Message& msg);

}

#endif // MESSAGE_HPP
