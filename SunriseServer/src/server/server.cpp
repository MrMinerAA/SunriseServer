#include "server.h"

#include "bap/frame.h"

#include <array>
#include <chrono>
#include <iostream>
#include <thread>

#pragma comment(lib, "Ws2_32.lib")

namespace sunrise::multiplayer {

    Server::~Server() {
        shutdown();
    }

    bool Server::make_nonblocking(SOCKET socket) noexcept {
        u_long enabled = 1;

        return ioctlsocket(
            socket,
            FIONBIO,
            &enabled) != SOCKET_ERROR;
    }

    bool Server::initialize(std::uint16_t port) noexcept {
        WSADATA winsock{};

        const int startupResult =
            WSAStartup(MAKEWORD(2, 2), &winsock);

        if (startupResult != 0) {
            std::cerr
                << "WSAStartup failed: "
                << startupResult
                << '\n';

            return false;
        }

        listener_ = socket(
            AF_INET,
            SOCK_STREAM,
            IPPROTO_TCP);

        if (listener_ == INVALID_SOCKET) {
            std::cerr
                << "socket() failed: "
                << WSAGetLastError()
                << '\n';

            WSACleanup();
            return false;
        }

        BOOL reuse = TRUE;

        setsockopt(
            listener_,
            SOL_SOCKET,
            SO_REUSEADDR,
            reinterpret_cast<const char*>(&reuse),
            sizeof(reuse));

        if (!make_nonblocking(listener_)) {
            std::cerr
                << "Failed to make listener nonblocking.\n";

            closesocket(listener_);
            listener_ = INVALID_SOCKET;

            WSACleanup();
            return false;
        }

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_port = htons(port);
        address.sin_addr.s_addr = htonl(INADDR_ANY);

        if (bind(
            listener_,
            reinterpret_cast<const sockaddr*>(&address),
            sizeof(address))
            == SOCKET_ERROR) {

            std::cerr
                << "bind() failed: "
                << WSAGetLastError()
                << '\n';

            closesocket(listener_);
            listener_ = INVALID_SOCKET;

            WSACleanup();
            return false;
        }

        if (listen(
            listener_,
            static_cast<int>(kMaxClients))
            == SOCKET_ERROR) {

            std::cerr
                << "listen() failed: "
                << WSAGetLastError()
                << '\n';

            closesocket(listener_);
            listener_ = INVALID_SOCKET;

            WSACleanup();
            return false;
        }

        running_ = true;

        std::cout
            << "Listening on TCP port "
            << port
            << "...\n";

        std::cout
            << "Maximum clients: "
            << kMaxClients
            << '\n';

        return true;
    }

    void Server::run() noexcept {
        std::cout << "Server running.\n\n";

        while (running_) {
            accept_clients();
            service_clients();

            std::this_thread::sleep_for(
                std::chrono::milliseconds(10));
        }
    }

    void Server::accept_clients() noexcept {
        while (true) {
            SOCKET clientSocket = accept(
                listener_,
                nullptr,
                nullptr);

            if (clientSocket == INVALID_SOCKET) {
                const int error = WSAGetLastError();

                if (error == WSAEWOULDBLOCK) {
                    return;
                }

                std::cerr
                    << "accept() failed: "
                    << error
                    << '\n';

                return;
            }

            if (!make_nonblocking(clientSocket)) {
                std::cerr
                    << "Failed to make client socket nonblocking.\n";

                closesocket(clientSocket);
                continue;
            }

            std::size_t freeSlot = kMaxClients;

            for (std::size_t slot = 0;
                slot < clients_.size();
                ++slot) {

                if (clients_[slot].socket == INVALID_SOCKET) {
                    freeSlot = slot;
                    break;
                }
            }

            if (freeSlot == kMaxClients) {
                std::cout
                    << "Connection rejected: server full.\n";

                closesocket(clientSocket);
                continue;
            }

            Client& client = clients_[freeSlot];

            client.socket = clientSocket;
            client.connectionId = nextConnectionId_++;
            client.input.clear();
            client.output.clear();
            client.outputOffset = 0;
            client.state = ConnectionState::connected;

            std::cout
                << "Client connected: id="
                << client.connectionId
                << " slot="
                << freeSlot
                << '\n';
        }
    }

    void Server::service_clients() noexcept {
        for (std::size_t slot = 0;
            slot < clients_.size();
            ++slot) {

            Client& client = clients_[slot];

            if (client.socket == INVALID_SOCKET) {
                continue;
            }

            // ---------------------------------------------------------
            // Send any response that is waiting for this client.
            // ---------------------------------------------------------

            if (client.outputOffset < client.output.size()) {
                const std::size_t remaining =
                    client.output.size() -
                    client.outputOffset;

                const int sent = send(
                    client.socket,
                    reinterpret_cast<const char*>(
                        client.output.data() +
                        client.outputOffset),
                    static_cast<int>(remaining),
                    0);

                if (sent > 0) {
                    client.outputOffset +=
                        static_cast<std::size_t>(sent);

                    if (client.outputOffset ==
                        client.output.size()) {

                        client.output.clear();
                        client.outputOffset = 0;
                    }
                }
                else if (sent == SOCKET_ERROR) {
                    const int error = WSAGetLastError();

                    if (error != WSAEWOULDBLOCK) {
                        std::cout
                            << "Client send error: id="
                            << client.connectionId
                            << " error="
                            << error
                            << '\n';

                        close_client(slot);
                        continue;
                    }
                }
            }

            // ---------------------------------------------------------
            // Receive incoming data.
            // ---------------------------------------------------------

            std::byte buffer[4096];

            const int received = recv(
                client.socket,
                reinterpret_cast<char*>(buffer),
                static_cast<int>(sizeof(buffer)),
                0);

            if (received == 0) {
                std::cout
                    << "Client disconnected: id="
                    << client.connectionId
                    << '\n';

                close_client(slot);
                continue;
            }

            if (received == SOCKET_ERROR) {
                const int error = WSAGetLastError();

                if (error == WSAEWOULDBLOCK) {
                    continue;
                }

                std::cout
                    << "Client connection error: id="
                    << client.connectionId
                    << " error="
                    << error
                    << '\n';

                close_client(slot);
                continue;
            }

            // ---------------------------------------------------------
            // Append newly received bytes to persistent TCP buffer.
            // ---------------------------------------------------------

            client.input.insert(
                client.input.end(),
                buffer,
                buffer + received);

            // ---------------------------------------------------------
            // Prevent an incomplete frame from growing indefinitely.
            // ---------------------------------------------------------

            if (client.input.size() >
                bap::kOuterHeaderSize + bap::kMaxPayloadSize) {

                std::cout
                    << "Client sent oversized BAP frame: id="
                    << client.connectionId
                    << '\n';

                close_client(slot);
                continue;
            }

            // ---------------------------------------------------------
            // Parse as many complete BAP frames as are available.
            // ---------------------------------------------------------

            while (client.socket != INVALID_SOCKET) {
                bap::OuterFrame frame;

                const bap::ParseResult result =
                    bap::try_parse_frame(
                        client.input,
                        frame);

                if (result == bap::ParseResult::incomplete) {
                    break;
                }

                if (result == bap::ParseResult::invalid) {
                    std::cout
                        << "Invalid BAP frame: id="
                        << client.connectionId
                        << '\n';

                    close_client(slot);
                    break;
                }

                const std::size_t frameSize =
                    bap::kOuterHeaderSize +
                    frame.payload.size();

                handle_frame(
                    client,
                    frame);

                // frame.payload points into client.input, so remove the
                // frame only after handle_frame() has finished using it.
                client.input.erase(
                    client.input.begin(),
                    client.input.begin() +
                    static_cast<std::ptrdiff_t>(frameSize));
            }
        }
    }

    void Server::handle_frame(
        Client& client,
        const bap::OuterFrame& frame) noexcept {

        std::cout
            << "BAP frame received: id="
            << client.connectionId
            << " type="
            << static_cast<unsigned>(
                frame.frameType)
            << " payload="
            << frame.payload.size()
            << " bytes\n";

        // ---------------------------------------------------------
        // Validate the outer frame type.
        // ---------------------------------------------------------

        switch (frame.frameType) {

        case bap::FrameType::plaintext0:
            std::cout
                << "  BAP plaintext0\n";
            break;

        case bap::FrameType::encrypted:
            std::cout
                << "  BAP encrypted\n";
            break;

        case bap::FrameType::plaintext2:
            std::cout
                << "  BAP plaintext2\n";
            break;

        default:
            std::cout
                << "  Unknown BAP frame type\n";
            return;
        }

        // ---------------------------------------------------------
        // Parse the BAP request header.
        // ---------------------------------------------------------

        bap::RequestFrame request;

        if (!bap::parse_request_payload(
            frame.payload,
            frame.frameType,
            request)) {

            std::cout
                << "  Invalid BAP request payload: id="
                << client.connectionId
                << '\n';

            return;
        }

        std::cout
            << "  BAP request:"
            << " messageId="
            << request.messageId
            << " taskId="
            << request.taskId
            << " body="
            << request.body.size()
            << " bytes\n";

        // ---------------------------------------------------------
        // Response buffer.
        //
        // Maximum response size:
        //
        // outer header + response header + maximum body
        //
        // 6 + 8 + 64 KiB
        // ---------------------------------------------------------

        std::array<std::byte,
            bap::kOuterHeaderSize +
            bap::kResponseHeaderSize +
            bap::kMaxPayloadSize> responseBuffer{};

        // ---------------------------------------------------------
        // Helper for encoding and queueing a response.
        // ---------------------------------------------------------

        const auto queue_response =
            [&](bap::ResponseService service,
                std::span<const std::byte> body) noexcept {

                    std::size_t written = 0;

                    if (!bap::encode_response(
                        service,
                        request.taskId,
                        request.frameType,
                        body,
                        responseBuffer,
                        written)) {

                        std::cout
                            << "  Failed to encode BAP response: id="
                            << client.connectionId
                            << '\n';

                        return;
                    }

                    client.output.insert(
                        client.output.end(),
                        responseBuffer.begin(),
                        responseBuffer.begin() +
                        static_cast<std::ptrdiff_t>(written));

                    std::cout
                        << "  Queued BAP response:"
                        << " service="
                        << static_cast<std::uint16_t>(service)
                        << " taskId="
                        << request.taskId
                        << " bytes="
                        << written
                        << '\n';
            };

        // ---------------------------------------------------------
        // Dispatch BAP request service.
        // ---------------------------------------------------------

        switch (request.messageId) {

            // ---------------------------------------------------------
            // SERVER_HELLO
            // ---------------------------------------------------------

            case static_cast<std::uint16_t>(
            bap::RequestService::serverHello): {

                std::cout
                    << "  SERVER_HELLO from client "
                    << client.connectionId
                    << '\n';

                if (client.state !=
                    ConnectionState::connected) {

                    std::cout
                        << "  SERVER_HELLO rejected:"
                        << " invalid connection state\n";

                    return;
                }

                client.state =
                    ConnectionState::authenticated;

                std::cout
                    << "  Client "
                    << client.connectionId
                    << " state -> authenticated\n";

                queue_response(
                    bap::ResponseService::serverHello,
                    {});

                break;
            }

            // ---------------------------------------------------------
            // START
            // ---------------------------------------------------------

            case static_cast<std::uint16_t>(
            bap::RequestService::start): {

                std::cout
                    << "  START from client "
                    << client.connectionId
                    << '\n';

                if (client.state !=
                    ConnectionState::authenticated) {

                    std::cout
                        << "  START rejected:"
                        << " client is not authenticated\n";

                    return;
                }

                client.state =
                    ConnectionState::started;

                std::cout
                    << "  Client "
                    << client.connectionId
                    << " state -> started\n";

                queue_response(
                    bap::ResponseService::start,
                    {});

                break;
            }

            // ---------------------------------------------------------
// ECHO
// ---------------------------------------------------------

            case static_cast<std::uint16_t>(
            bap::RequestService::echo): {

                std::cout
                    << "  ECHO from client "
                    << client.connectionId
                    << '\n';

                if (client.state !=
                    ConnectionState::started) {

                    std::cout
                        << "  ECHO rejected:"
                        << " client has not started\n";

                    return;
                }

                queue_response(
                    bap::ResponseService::echo,
                    request.body);

                break;
            }

            // ---------------------------------------------------------
            // Unknown service
            // ---------------------------------------------------------

            default:
                std::cout
                    << "  Unknown BAP request service: "
                    << request.messageId
                    << '\n';
                break;
        }
    }

    void Server::close_client(
        std::size_t slot) noexcept {

        Client& client = clients_[slot];

        if (client.socket != INVALID_SOCKET) {
            closesocket(client.socket);
            client.socket = INVALID_SOCKET;
        }

        client.connectionId = 0;

        client.input.clear();

        client.output.clear();
        client.outputOffset = 0;

        client.state =
            ConnectionState::connected;
    }

    void Server::shutdown() noexcept {
        running_ = false;

        for (std::size_t slot = 0;
            slot < clients_.size();
            ++slot) {

            close_client(slot);
        }

        if (listener_ != INVALID_SOCKET) {
            closesocket(listener_);
            listener_ = INVALID_SOCKET;
        }

        WSACleanup();
    }

} // namespace sunrise::multiplayer