#include "protocol.h"

#include <algorithm>

namespace sunrise::multiplayer {

    namespace {

        std::uint16_t read_u16(const std::byte* data) noexcept {
            return
                (static_cast<std::uint16_t>(
                    std::to_integer<unsigned char>(data[0]))
                    << 8)
                |
                static_cast<std::uint16_t>(
                    std::to_integer<unsigned char>(data[1]));
        }

        std::uint32_t read_u32(const std::byte* data) noexcept {
            return
                (static_cast<std::uint32_t>(
                    std::to_integer<unsigned char>(data[0]))
                    << 24)
                |
                (static_cast<std::uint32_t>(
                    std::to_integer<unsigned char>(data[1]))
                    << 16)
                |
                (static_cast<std::uint32_t>(
                    std::to_integer<unsigned char>(data[2]))
                    << 8)
                |
                static_cast<std::uint32_t>(
                    std::to_integer<unsigned char>(data[3]));
        }

        void write_u16(
            std::byte* data,
            std::uint16_t value) noexcept {

            data[0] = std::byte{
                static_cast<unsigned char>((value >> 8) & 0xff)
            };

            data[1] = std::byte{
                static_cast<unsigned char>(value & 0xff)
            };
        }

        void write_u32(
            std::byte* data,
            std::uint32_t value) noexcept {

            data[0] = std::byte{
                static_cast<unsigned char>((value >> 24) & 0xff)
            };

            data[1] = std::byte{
                static_cast<unsigned char>((value >> 16) & 0xff)
            };

            data[2] = std::byte{
                static_cast<unsigned char>((value >> 8) & 0xff)
            };

            data[3] = std::byte{
                static_cast<unsigned char>(value & 0xff)
            };
        }

    } // namespace

    ParseResult try_parse_frame(
        std::vector<std::byte>& stream,
        Frame& frame) noexcept {

        if (stream.size() < kHeaderSize) {
            return ParseResult::incomplete;
        }

        const std::byte* header = stream.data();

        const std::uint16_t version =
            read_u16(header);

        const std::uint16_t rawType =
            read_u16(header + 2);

        const std::uint32_t payloadSize =
            read_u32(header + 4);

        if (version != kProtocolVersion) {
            return ParseResult::invalid;
        }

        if (payloadSize > kMaxPayloadSize) {
            return ParseResult::invalid;
        }

        const std::size_t frameSize =
            kHeaderSize +
            static_cast<std::size_t>(payloadSize);

        if (stream.size() < frameSize) {
            return ParseResult::incomplete;
        }

        frame.type =
            static_cast<MessageType>(rawType);

        frame.payload.assign(
            stream.begin() + kHeaderSize,
            stream.begin() + frameSize);

        stream.erase(
            stream.begin(),
            stream.begin() +
            static_cast<std::ptrdiff_t>(frameSize));

        return ParseResult::complete;
    }

    std::vector<std::byte> make_frame(
        MessageType type,
        std::span<const std::byte> payload) {

        if (payload.size() > kMaxPayloadSize) {
            return {};
        }

        const auto payloadSize =
            static_cast<std::uint32_t>(payload.size());

        const auto rawType =
            static_cast<std::uint16_t>(type);

        std::vector<std::byte> frame(
            kHeaderSize + payload.size());

        // Bytes 0-1: protocol version
        write_u16(
            frame.data(),
            kProtocolVersion);

        // Bytes 2-3: message type
        write_u16(
            frame.data() + 2,
            rawType);

        // Bytes 4-7: payload length
        write_u32(
            frame.data() + 4,
            payloadSize);

        // Remaining bytes: payload
        if (!payload.empty()) {
            std::copy(
                payload.begin(),
                payload.end(),
                frame.begin() + kHeaderSize);
        }

        return frame;
    }

} // namespace sunrise::multiplayer