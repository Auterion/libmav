//
// Created by thomas on 17.02.23.
//

#ifndef MAVLINK_FUNNEL_UDPCLIENT_H
#define MAVLINK_FUNNEL_UDPCLIENT_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <atomic>
#include <vector>
#include <array>
#include <csignal>
#include "Network.h"

namespace mav {

    class UDPClient : public mav::NetworkInterface {

    private:
        static constexpr size_t RX_BUFFER_SIZE = 2048;

        mutable std::atomic_bool _should_terminate{false};
        int _socket = -1;
        struct sockaddr_in _server_address{};

        std::array<uint8_t, RX_BUFFER_SIZE> _rx_buffer;
        int _bytes_available = 0;
        ConnectionPartner _partner;

    public:
        UDPClient(const std::string &remote_address, int remote_port) {
            _socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            if (_socket < 0) {
                throw NetworkError("Could not create socket");
            }

            _server_address.sin_family = AF_INET;
            _server_address.sin_port = htons(remote_port);
            _server_address.sin_addr.s_addr = inet_addr(remote_address.c_str());

            // connect to server
            if(connect(_socket, (struct sockaddr *)&_server_address, sizeof(_server_address)) < 0) {
                throw NetworkError("UDP connect call failed");
            }

            _partner = {
                    _server_address.sin_addr.s_addr,
                    _server_address.sin_port,
                    false
            };
        }

        void stop() const {
            _should_terminate.store(true);
            if (_socket >= 0) {
                ::shutdown(_socket, SHUT_RDWR);
                ::close(_socket);
            }
        }

        void close() const override {
            stop();
        }

        ConnectionPartner receive(uint8_t *destination, uint32_t size) override {
            // Receive as many messages as needed to have enough bytes available (none if already enough bytes)
            while (_bytes_available < size && !_should_terminate.load()) {
                // If there are residual bytes from last packet, but not enough for parsing new packet, clear out
                _bytes_available = 0;
                ssize_t ret = ::recvfrom(_socket, _rx_buffer.data(), RX_BUFFER_SIZE, 0,
                                         (struct sockaddr *) nullptr, nullptr);
                if (ret < 0) {
                    throw NetworkError("Could not receive from socket");
                }
                _bytes_available += static_cast<int>(ret);
            }

            if (_should_terminate.load()) {
                throw NetworkInterfaceInterrupt();
            }

            std::copy(_rx_buffer.begin(), _rx_buffer.begin() + size, destination);
            _bytes_available -= static_cast<int>(size);
            std::copy(_rx_buffer.begin() + size, _rx_buffer.begin() + size + _bytes_available, _rx_buffer.begin());
            return _partner;
        }

        void send(const uint8_t *data, uint32_t size, ConnectionPartner target) override {
            // no need to specify target here, as we called the udp connect function in constructor
            if (sendto(_socket, data, size, 0, (struct sockaddr *) nullptr, 0) < 0) {
                ::close(_socket);
                throw NetworkError("Could not send to socket");
            }
        }

        void markSyncing() override {
            // we're out of sync. The beginning of a packet was not the magic number we expected,
            // therefore, we can discard the rest of the packet and just receive another datagram.
            _bytes_available = 0;
        }

        virtual ~UDPClient() {
            stop();
        }
    };
}

#endif //MAVLINK_FUNNEL_UDPCLIENT_H
