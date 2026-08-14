#include "frame.h"

#include <limits>

namespace sunrise::bap {
    namespace {

        constexpr std::byte kMagic{ 0x01 };

        constexpr std::size_t kServiceOffset = 0;
        constexpr std::size_t kTaskOffset = 2;
        constexpr std::size_t kStatusOffset = 6;

        constexpr std::uint16_t kStatusOk = 200;

        [[nodiscard]]
        std::uint16_t read_u16_be(
            std::span<const std::byte> data) noexcept {

            return
                (static_cast<std::uint16_t>(
                    std::to_integer<std::uint8_t>(data[0]))
                    << 8)
                |
                static_cast<std::uint16_t>(
                    std::to_integer<std::uint8_t>(data[1]));
        }

        [[nodiscard]]
        std::uint32_t read_u32_be(
            std::span<const std::byte> data) noexcept {

            return
                (static_cast<std::uint32_t>(
                    std::to_integer<std::uint8_t>(data[0]))
                    << 24)
                |
                (static_cast<std::uint32_t>(
                    std::to_integer<std::uint8_t>(data[1]))
                    << 16)
                |
                (static_cast<std::uint32_t>(
                    std::to_integer<std::uint8_t>(data[2]))
                    << 8)
                |
                static_cast<std::uint32_t>(
                    std::to_integer<std::uint8_t>(data[3]));
        }

        void write_u16_be(
            std::span<std::byte> data,
            std::uint16_t value) noexcept {

            data[0] = std::byte{
                static_cast<std::uint8_t>((value >> 8) & 0xff)
            };

            data[1] = std::byte{
                static_cast<std::uint8_t>(value & 0xff)
            };
        }

        void write_u32_be(
            std::span<std::byte> data,
            std::uint32_t value) noexcept {

            data[0] = std::byte{
                static_cast<std::uint8_t>((value >> 24) & 0xff)
            };

            data[1] = std::byte{
                static_cast<std::uint8_t>((value >> 16) & 0xff)
            };

            data[2] = std::byte{
                static_cast<std::uint8_t>((value >> 8) & 0xff)
            };

            data[3] = std::byte{
                static_cast<std::uint8_t>(value & 0xff)
            };
        }

    } // namespace

    ParseResult try_parse_frame(
        std::vector<std::byte>& stream,
        OuterFrame& frame) noexcept {

        frame = {};

        if (stream.size() < kOuterHeaderSize) {
            return ParseResult::incomplete;
        }

        const auto input =
            std::span<const std::byte>(stream);

        if (input[0] != kMagic) {
            return ParseResult::invalid;
        }

        const auto rawType =
            std::to_integer<std::uint8_t>(input[1]);

        const std::size_t payloadSize =
            read_u32_be(input.subspan<2, 4>());

        if (payloadSize > kMaxPayloadSize) {
            return ParseResult::invalid;
        }

        const std::size_t frameSize =
            kOuterHeaderSize + payloadSize;

        if (stream.size() < frameSize) {
            return ParseResult::incomplete;
        }

        frame.frameType =
            static_cast<FrameType>(rawType);

        frame.payload =
            input.subspan(
                kOuterHeaderSize,
                payloadSize);

        // IMPORTANT:
        // Do not erase the stream here. frame.payload is a span into
        // stream, so erasing would invalidate the span before the server
        // has a chance to process the frame.
        return ParseResult::complete;
    }

    bool parse_request_payload(
        std::span<const std::byte> input,
        FrameType frameType,
        RequestFrame& request) noexcept {

        request = {};

        if (input.size() < kRequestHeaderSize) {
            return false;
        }

        request.frameType = frameType;

        request.messageId =
            read_u16_be(
                input.subspan<kServiceOffset, 2>());

        request.taskId =
            read_u32_be(
                input.subspan<kTaskOffset, 4>());

        request.body =
            input.subspan(kRequestHeaderSize);

        return true;
    }

    bool encode_frame(
        FrameType frameType,
        std::span<const std::byte> payload,
        std::span<std::byte> output,
        std::size_t& written) noexcept {

        written = 0;

        if (payload.size() >
            std::numeric_limits<std::uint32_t>::max()
            || output.size() <
            kOuterHeaderSize + payload.size()) {

            return false;
        }

        output[0] = kMagic;
        output[1] = static_cast<std::byte>(frameType);

        write_u32_be(
            output.subspan<2, 4>(),
            static_cast<std::uint32_t>(payload.size()));

        for (std::size_t i = 0; i < payload.size(); ++i) {
            output[kOuterHeaderSize + i] = payload[i];
        }

        written =
            kOuterHeaderSize + payload.size();

        return true;
    }

    bool encode_response(
        ResponseService service,
        std::uint32_t taskId,
        FrameType frameType,
        std::span<const std::byte> body,
        std::span<std::byte> output,
        std::size_t& written) noexcept {

        written = 0;

        if (output.size() <
            kOuterHeaderSize +
            kResponseHeaderSize +
            body.size()) {

            return false;
        }

        auto payload =
            output.subspan(kOuterHeaderSize);

        write_u16_be(
            payload.subspan<0, 2>(),
            static_cast<std::uint16_t>(service));

        write_u32_be(
            payload.subspan<2, 4>(),
            taskId);

        write_u16_be(
            payload.subspan<kStatusOffset, 2>(),
            kStatusOk);

        for (std::size_t i = 0; i < body.size(); ++i) {
            payload[kResponseHeaderSize + i] = body[i];
        }

        const std::size_t payloadSize =
            kResponseHeaderSize + body.size();

        return encode_frame(
            frameType,
            payload.first(payloadSize),
            output,
            written);
    }

} // namespace sunrise::bap