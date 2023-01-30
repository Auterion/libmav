//
// Created by thomas on 27.01.23.
//

#ifndef LIBMAVLINK_UDPPASSIVE_H
#define LIBMAVLINK_UDPPASSIVE_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <atomic>
#include <vector>
#include <array>
#include "Network.h"

namespace mav {

    class UDPPassive : public mav::NetworkInterface {

    private:
        static constexpr size_t RX_BUFFER_SIZE = 2048;

        mutable std::atomic_bool _should_terminate{false};
        int _socket = -1;

        struct Remote {
            in_addr_t address;
            uint16_t port;

            bool operator==(const Remote& other) const {
                return address == other.address && port == other.port;
            }
        };

        std::vector<Remote> _remotes;
        std::array<uint8_t, RX_BUFFER_SIZE> _rx_buffer;
        int _bytes_available = 0;

    public:

        UDPPassive(int local_port, const std::string& local_address="0.0.0.0") {
            _socket = socket(AF_INET, SOCK_DGRAM, 0);
            if (_socket < 0) {
                throw NetworkError("Could not create socket");
            }
            struct sockaddr_in server_address{};
            server_address.sin_family = AF_INET;
            server_address.sin_port = htons(local_port);
            server_address.sin_addr.s_addr = inet_addr(local_address.c_str());

            if (bind(_socket, (struct sockaddr *) &server_address, sizeof(server_address)) < 0) {
                ::close(_socket);
                throw NetworkError("Could not bind to server");
            }
        }

        void stop() const {
            _should_terminate.store(true);
            if (_socket >= 0) {
                ::close(_socket);
            }
        }

        void close() const override {
          stop();
        }

        void receive(uint8_t *destination, uint32_t size) override {
            // Receive as many messages as needed to have enough bytes available (none if already enough bytes)
            while (_bytes_available < size && !_should_terminate.load()) {
                struct sockaddr_in source_address{};
                socklen_t source_address_length = sizeof(source_address);
                ssize_t ret = ::recvfrom(_socket, _rx_buffer.data(), RX_BUFFER_SIZE, 0,
                                     (struct sockaddr*)&source_address, &source_address_length);
                if (ret < 0) {
                    throw NetworkError("Could not receive from socket");
                }
                _bytes_available += static_cast<int>(ret);

                // make sure this remote is in the set of known remotes
                Remote this_remote = {
                    source_address.sin_addr.s_addr,
                    source_address.sin_port
                };
                if (std::find(_remotes.begin(), _remotes.end(), this_remote) == _remotes.end()) {
                    _remotes.emplace_back(this_remote);
                }
            }

            if (_should_terminate.load()) {
                throw NetworkInterfaceInterrupt();
            }

            std::copy(_rx_buffer.begin(), _rx_buffer.begin() + size, destination);
            _bytes_available -= static_cast<int>(size);
            std::copy(_rx_buffer.begin() + size, _rx_buffer.begin() + size + _bytes_available, _rx_buffer.begin());
        }

        void send(const uint8_t *data, uint32_t size) override {
            for (auto & remote : _remotes) {
                struct sockaddr_in server_address{};
                server_address.sin_family = AF_INET;
                server_address.sin_port = remote.port;
                server_address.sin_addr.s_addr = remote.address;

                if (sendto(_socket, data, size, 0, (struct sockaddr *) &server_address, sizeof(server_address)) < 0) {
                    ::close(_socket);
                    throw NetworkError("Could not send to socket");
                }
            }
        }

        virtual ~UDPPassive() {
            stop();
        }
    };
}

#endif //LIBMAVLINK_UDPPASSIVE_H
