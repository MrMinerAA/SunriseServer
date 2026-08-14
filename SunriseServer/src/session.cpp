#include "session.h"

namespace sunrise::multiplayer {

    Session::Session(std::uint32_t id)
        : id_(id) {
    }

    std::uint32_t Session::id() const noexcept {
        return id_;
    }

    Player* Session::add_player(SOCKET socket) {
        for (Player& player : players_) {
            if (!player.active) {
                player.active = true;
                player.socket = socket;
                player.state = {};
                return &player;
            }
        }

        return nullptr;
    }

    void Session::remove_player(std::uint32_t playerId) {
        for (Player& player : players_) {
            if (player.active && player.id == playerId) {
                player = {};
                player.socket = INVALID_SOCKET;
            }
        }
    }

    Player* Session::find_player(std::uint32_t playerId) noexcept {
        for (Player& player : players_) {
            if (player.active && player.id == playerId) {
                return &player;
            }
        }

        return nullptr;
    }

    std::array<Player, kMaxPlayers>& Session::players() noexcept {
        return players_;
    }

} // namespace sunrise::multiplayer