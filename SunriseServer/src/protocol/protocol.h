#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace sunrise::multiplayer {

    inline constexpr std::uint16_t kProtocolVersion = 1;

    inline constexpr std::size_t kHeaderSize = 8;
    inline constexpr std::size_t kMaxPayloadSize = 64 * 1024;

    enum class MessageType : std::uint16_t {
        hello = 1,
        joinSession = 2,
        playerJoin = 3,
        playerState = 4,
        playerLeave = 5,
        leave = 6,
        ping = 7,
    };

    struct PlayerState {
        std::uint32_t playerId{};
        std::uint32_t destinationId{};

        float positionX{};
        float positionY{};
        float positionZ{};

        float rotationX{};
        float rotationY{};
        float rotationZ{};
        float rotationW{ 1.0f };
    };

    struct Frame {
        MessageType type{};
        std::vector<std::byte> payload;
    };

    enum class ParseResult {
        incomplete,
        complete,
        invalid,
    };

    [[nodiscard]] ParseResult try_parse_frame(
        std::vector<std::byte>& stream,
        Frame& frame) noexcept;

    [[nodiscard]] std::vector<std::byte> make_frame(
        MessageType type,
        std::span<const std::byte> payload);

} // namespace sunrise::multiplayer