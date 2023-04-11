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

#ifndef LIBMAVLINK_UDPSERVER_H
#define LIBMAVLINK_UDPSERVER_H

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <atomic>
#include <unistd.h>
#include <vector>
#include <array>
#include <csignal>
#include "Network.h"

namespace mav {

    class UDPServer : public mav::NetworkInterface {

    private:
        static constexpr size_t RX_BUFFER_SIZE = 2048;

        mutable std::atomic_bool _should_terminate{false};
        int _socket = -1;

        std::array<uint8_t, RX_BUFFER_SIZE> _rx_buffer;
        int _bytes_available = 0;
        ConnectionPartner _current_partner;

    public:
        UDPServer(int local_port, const std::string& local_address="0.0.0.0") {
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
                throw NetworkError("Could not bind to socket. (Address already in use?)");
            }
        }

        void stop() const {
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
                struct sockaddr_in source_address{};
                socklen_t source_address_length = sizeof(source_address);
                ssize_t ret = ::recvfrom(_socket, _rx_buffer.data(), RX_BUFFER_SIZE, 0,
                                     (struct sockaddr*)&source_address, &source_address_length);
                if (ret < 0) {
                    throw NetworkError("Could not receive from socket");
                }
                _bytes_available += static_cast<int>(ret);

                // make sure this remote is in the set of known remotes
                _current_partner = {
                    source_address.sin_addr.s_addr,
                    source_address.sin_port,
                    false
                };
            }

            if (_should_terminate.load()) {
                throw NetworkInterfaceInterrupt();
            }

            std::copy(_rx_buffer.begin(), _rx_buffer.begin() + size, destination);
            _bytes_available -= static_cast<int>(size);
            std::copy(_rx_buffer.begin() + size, _rx_buffer.begin() + size + _bytes_available, _rx_buffer.begin());
            return _current_partner;
        }

        void send(const uint8_t *data, uint32_t size, ConnectionPartner target) override {
            if (target.isBroadcast()) {
                throw NetworkError("Sending without target not supported for UDP server");
            }

            struct sockaddr_in server_address{};
            server_address.sin_family = AF_INET;
            server_address.sin_port = target.port();
            server_address.sin_addr.s_addr = target.address();

            if (sendto(_socket, data, size, 0, (struct sockaddr *) &server_address, sizeof(server_address)) < 0) {
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
            return false;
        }


        virtual ~UDPServer() {
            stop();
        }
    };
}

#endif //LIBMAVLINK_UDPSERVER_H
