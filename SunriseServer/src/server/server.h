#pragma once

#include <WinSock2.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "bap/frame.h"

namespace sunrise::multiplayer {

    inline constexpr std::size_t kMaxClients = 8;

    // ---------------------------------------------------------
    // BAP connection state
    // ---------------------------------------------------------

    enum class ConnectionState {
        connected,
        authenticated,
        started
    };

    // ---------------------------------------------------------
    // Connected client
    // ---------------------------------------------------------

    struct Client {
        SOCKET socket{ INVALID_SOCKET };
        std::uint32_t connectionId{};

        // Persistent TCP receive stream.
        std::vector<std::byte> input;

        // Pending bytes waiting to be sent.
        std::vector<std::byte> output;
        std::size_t outputOffset{};

        // BAP connection state.
        ConnectionState state{
            ConnectionState::connected
        };
    };

    class Server {
    public:
        Server() = default;
        ~Server();

        Server(const Server&) = delete;
        Server& operator=(const Server&) = delete;

        [[nodiscard]]
        bool initialize(std::uint16_t port) noexcept;

        void run() noexcept;
        void shutdown() noexcept;

    private:
        void accept_clients() noexcept;
        void service_clients() noexcept;

        void handle_frame(
            Client& client,
            const bap::OuterFrame& frame) noexcept;

        void close_client(
            std::size_t slot) noexcept;

        [[nodiscard]]
        bool make_nonblocking(
            SOCKET socket) noexcept;

        SOCKET listener_{ INVALID_SOCKET };

        std::array<Client, kMaxClients> clients_{};

        std::uint32_t nextConnectionId_{ 1 };

        bool running_{ false };
    };

} // namespace sunrise::multiplayer