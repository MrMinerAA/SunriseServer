#pragma once

#include "protocol/protocol.h"

#include <array>
#include <cstdint>
#include <winsock2.h>

namespace sunrise::multiplayer {

    inline constexpr std::size_t kMaxPlayers = 8;

    struct Player {
        bool active{};
        std::uint32_t id{};
        SOCKET socket{ INVALID_SOCKET };
        PlayerState state{};
    };

    class Session {
    public:
        explicit Session(std::uint32_t id);

        [[nodiscard]] std::uint32_t id() const noexcept;

        Player* add_player(SOCKET socket);
        void remove_player(std::uint32_t playerId);

        [[nodiscard]] Player* find_player(std::uint32_t playerId) noexcept;

        [[nodiscard]] std::array<Player, kMaxPlayers>& players() noexcept;

    private:
        std::uint32_t id_;
        std::array<Player, kMaxPlayers> players_{};
    };

} // namespace sunrise::multiplayer