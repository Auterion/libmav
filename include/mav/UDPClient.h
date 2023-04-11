/****************************************************************************
 * 
 * Copyright (c) 2023, libmav development team
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright 
 *    notice, this list of conditions and the following disclaimer in 
 *    the documentation and/or other materials provided with the 
 *    distribution.
 * 3. Neither the name libmav nor the names of its contributors may be 
 *    used to endorse or promote products derived from this software 
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS 
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE 
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, 
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, 
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS 
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED 
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN 
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE 
 * POSSIBILITY OF SUCH DAMAGE.
 * 
 ****************************************************************************/

#ifndef LIBMAVLINK_UDPCLIENT_H
#define LIBMAVLINK_UDPCLIENT_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <atomic>
#include <vector>
#include <array>
#include <csignal>
#include <unistd.h>
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

        void stop() {
            _should_terminate.store(true);
            if (_socket >= 0) {
                ::shutdown(_socket, SHUT_RDWR);
                ::close(_socket);
            }
        }

        void close() override {
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

        bool isConnectionOriented() const override {
            // even though UDP is not connection oriented, we can use it as if it was,
            // since we're using the connect() function to bind to a specific remote address
            return true;
        }

        virtual ~UDPClient() {
            stop();
        }
    };
}

#endif //LIBMAVLINK_UDPCLIENT_H
