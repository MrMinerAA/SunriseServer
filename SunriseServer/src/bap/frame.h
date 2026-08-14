#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace sunrise::bap {

    // ---------------------------------------------------------
    // BAP wire-format sizes
    // ---------------------------------------------------------

    inline constexpr std::size_t kOuterHeaderSize = 6;
    inline constexpr std::size_t kRequestHeaderSize = 6;
    inline constexpr std::size_t kResponseHeaderSize = 8;
    inline constexpr std::size_t kMaxPayloadSize = 64 * 1024;

    // ---------------------------------------------------------
    // Outer-frame types
    // ---------------------------------------------------------

    enum class FrameType : std::uint8_t {
        plaintext0 = 0,
        encrypted = 1,
        plaintext2 = 2,
    };

    // ---------------------------------------------------------
    // Request services currently implemented
    // ---------------------------------------------------------

    enum class RequestService : std::uint16_t {
        serverHello = 25,
        start = 30,
        echo = 250,
    };

    // ---------------------------------------------------------
    // Response services currently implemented
    // ---------------------------------------------------------

    enum class ResponseService : std::uint16_t {
        serverHello = 26,
        start = 31,
        echo = 251,
    };

    // ---------------------------------------------------------
    // Parsed BAP outer frame
    // ---------------------------------------------------------

    struct OuterFrame {
        FrameType frameType{};
        std::span<const std::byte> payload{};
    };

    // ---------------------------------------------------------
    // Parsed BAP request
    // ---------------------------------------------------------

    struct RequestFrame {
        FrameType frameType{};
        std::uint16_t messageId{};
        std::uint32_t taskId{};
        std::span<const std::byte> body{};
    };

    // ---------------------------------------------------------
    // TCP stream parsing result
    // ---------------------------------------------------------

    enum class ParseResult {
        incomplete,
        complete,
        invalid,
    };

    // ---------------------------------------------------------
    // BAP parsing
    // ---------------------------------------------------------

    /**
     * Attempts to parse one complete BAP outer frame from a
     * persistent TCP stream buffer.
     *
     * The returned OuterFrame borrows memory from stream.
     * The caller must finish using frame before modifying stream.
     */
    [[nodiscard]]
    ParseResult try_parse_frame(
        std::vector<std::byte>& stream,
        OuterFrame& frame) noexcept;

    /**
     * Parses the 6-byte BAP request header and borrows its body.
     */
    [[nodiscard]]
    bool parse_request_payload(
        std::span<const std::byte> input,
        FrameType frameType,
        RequestFrame& request) noexcept;

    // ---------------------------------------------------------
    // BAP encoding
    // ---------------------------------------------------------

    /**
     * Encodes a BAP outer frame around an existing payload.
     */
    [[nodiscard]]
    bool encode_frame(
        FrameType frameType,
        std::span<const std::byte> payload,
        std::span<std::byte> output,
        std::size_t& written) noexcept;

    /**
     * Encodes a status-200 BAP response.
     */
    [[nodiscard]]
    bool encode_response(
        ResponseService service,
        std::uint32_t taskId,
        FrameType frameType,
        std::span<const std::byte> body,
        std::span<std::byte> output,
        std::size_t& written) noexcept;

} // namespace sunrise::bap