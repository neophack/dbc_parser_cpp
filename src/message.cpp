#include <cstddef>
#include <cstdint>
#include <libdbc/message.hpp>
#include <libdbc/signal.hpp>
#include <ostream>
#include <string>
#include <vector>

namespace Libdbc {

constexpr unsigned ONE_BYTE = 8;
constexpr unsigned TWO_BYTES = 16;
constexpr unsigned FOUR_BYTES = 32;
constexpr unsigned EIGHT_BYTES = 64;

constexpr unsigned SEVEN_BITS = 7;

Message::Message(uint32_t message_id, const std::string& name, uint8_t size, const std::string& node)
	: m_id(message_id)
	, m_name(name)
	, m_size(size)
	, m_node(node) {
}

bool Message::operator==(const Message& rhs) const {
	return (m_id == rhs.id()) && (m_name == rhs.m_name) && (m_size == rhs.m_size) && (m_node == rhs.m_node);
}
namespace {
// Returns a mask with the low `bits` bits set. Shifting a 64-bit value by 64
// is undefined behavior, so the full-width case is handled explicitly.
uint64_t low_bits_mask(uint32_t bits) {
    return bits >= 64 ? ~0ULL : ((1ULL << bits) - 1);
}
}

Message::ParseSignalsStatus Message::parse_signals(const std::vector<uint8_t>& data, std::vector<double>& values) const {
    const size_t size = data.size();
    if (size > 64) {
        return ParseSignalsStatus::ErrorMessageToLong;
    }

    values.clear();
    values.reserve(m_signals.size());

    const uint32_t total_bits = static_cast<uint32_t>(size * 8);

    for (const auto& signal : m_signals) {
        uint64_t raw_value = 0;

        if (signal.is_bigendian) {
            // Motorola (Big Endian)
            // In DBC, Motorola start_bit is the MSB.
            // We need to extract 'signal.size' bits starting from 'start_bit' going backwards in bit index (MSB to LSB).
            
            uint32_t current_bit = signal.start_bit;
            for (uint32_t i = 0; i < signal.size; ++i) {
                uint32_t byte_idx = current_bit / 8;
                uint32_t bit_in_byte = current_bit % 8;
                
                if (byte_idx < size) {
                    if (data[byte_idx] & (1u << bit_in_byte)) {
                        raw_value |= (1ULL << (signal.size - 1 - i));
                    }
                }
                
                // Motorola bit counting: MSB to LSB
                // If we are at bit 0 of a byte, the next bit (LSB-wards) is bit 15 of the same "word" if we think in 16-bit? 
                // No, in DBC Motorola: 7,6,5,4,3,2,1,0, 15,14,13,12,11,10,9,8, ...
                if (bit_in_byte == 0) {
                    current_bit += 15;
                } else {
                    current_bit -= 1;
                }
            }
        } else {
            // Intel (Little Endian)
            // In DBC, Intel start_bit is the LSB.
            uint32_t bit_pos = signal.start_bit;
            if (bit_pos + signal.size > total_bits) {
                values.push_back(0.0); // Or handle error
                continue;
            }

            // Fast path for Intel
            uint32_t start_byte = bit_pos / 8;
            uint32_t end_byte = (bit_pos + signal.size - 1) / 8;
            uint32_t bytes_to_read = end_byte - start_byte + 1;

            // The bytes are accumulated into a 64-bit temporary, so this fast path only
            // works when the signal (plus its sub-byte bit offset) fits within 8 bytes.
            if (end_byte < size && bytes_to_read <= 8) {
                uint64_t temp = 0;
                for (uint32_t i = 0; i < bytes_to_read; ++i) {
                    temp |= static_cast<uint64_t>(data[start_byte + i]) << (i * 8);
                }
                raw_value = (temp >> (bit_pos % 8)) & low_bits_mask(signal.size);
            } else {
                // Fallback (should not happen with the check above)
                for (uint32_t i = 0; i < signal.size; ++i) {
                    uint32_t byte_idx = (bit_pos + i) / 8;
                    uint32_t bit_in_byte = (bit_pos + i) % 8;
                    if (byte_idx < size && (data[byte_idx] & (1u << bit_in_byte))) {
                        raw_value |= (1ULL << i);
                    }
                }
            }
        }

        // Apply sign extension
        double final_value;
        if (signal.is_signed && (raw_value & (1ULL << (signal.size - 1)))) {
            int64_t signed_value = static_cast<int64_t>(raw_value | ~low_bits_mask(signal.size));
            final_value = static_cast<double>(signed_value) * signal.factor + signal.offset;
        } else {
            final_value = static_cast<double>(raw_value) * signal.factor + signal.offset;
        }
        values.push_back(final_value);
    }

    return ParseSignalsStatus::Success;
}

void Message::append_signal(const Signal& signal) {
	m_signals.push_back(signal);
}

std::vector<Signal> Message::get_signals() const {
	return m_signals;
}

uint32_t Message::id() const {
	return m_id;
}

uint8_t Message::size() const {
	return m_size;
}

const std::string& Message::name() const {
	return m_name;
}

void Message::add_value_description(const std::string& signal_name, const std::vector<Signal::ValueDescription>& value_descriptor) {
	for (auto& signal : m_signals) {
		if (signal.name == signal_name) {
			signal.value_descriptions = value_descriptor;
			return;
		}
	}
}

std::ostream& operator<<(std::ostream& out, const Message& msg) {
	out << "Message: {id: " << msg.id() << ", ";
	out << "name: " << msg.m_name << ", ";
	out << "size: " << static_cast<unsigned>(msg.m_size) << ", ";
	out << "node: " << msg.m_node << "}";
	return out;
}
}
