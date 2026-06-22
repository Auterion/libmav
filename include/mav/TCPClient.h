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

#ifndef LIBMAVLINK_TCP_H
#define LIBMAVLINK_TCP_H

#include <atomic>
#include <string>
#include "SocketCompat.h"
#include "Network.h"

namespace mav {

    class TCPClient : public mav::NetworkInterface {

    private:
        mutable std::atomic_bool _should_terminate{false};
        socket_handle_t _socket = INVALID_SOCKET_HANDLE;
        ConnectionPartner _partner;

    public:

        TCPClient(const std::string& address, int port, int timeout = -1) {
            initSocketLibrary();
            _socket = socket(AF_INET, SOCK_STREAM, 0);
            if (_socket == INVALID_SOCKET_HANDLE) {
                throw NetworkError("Could not create socket", lastSocketError());
            }

            if (timeout > 0) {
                setSendTimeout(_socket, timeout); // timeout is in ms
            }

            // Resolve the host using getaddrinfo (portable and thread-safe).
            struct addrinfo hints{};
            hints.ai_family = AF_INET;
            hints.ai_socktype = SOCK_STREAM;
            struct addrinfo *res = nullptr;
            if (getaddrinfo(address.c_str(), std::to_string(port).c_str(), &hints, &res) != 0 || res == nullptr) {
                closeSocket(_socket);
                throw NetworkError("Could not resolve host");
            }

            struct sockaddr_in server_address{};
            std::copy(reinterpret_cast<const char*>(res->ai_addr),
                      reinterpret_cast<const char*>(res->ai_addr) + res->ai_addrlen,
                      reinterpret_cast<char*>(&server_address));
            freeaddrinfo(res);

            _partner = {server_address.sin_addr.s_addr, server_address.sin_port, false};

            if (connect(_socket, (struct sockaddr *) &server_address, sizeof(server_address)) < 0) {
                closeSocket(_socket);
                throw NetworkError("Could not connect to server", lastSocketError());
            }
        }

        void stop() {
            _should_terminate.store(true);
            if (_socket != INVALID_SOCKET_HANDLE) {
                ::shutdown(_socket, MAV_SHUT_RDWR);
                closeSocket(_socket);
                _socket = INVALID_SOCKET_HANDLE;
            }
        }

        void close() override {
          stop();
        }

        ConnectionPartner receive(uint8_t *destination, uint32_t size) override {
            uint32_t received = 0;
            while (received < size && !_should_terminate.load()) {
                auto ret = ::recv(_socket, (char*)destination, size - received, 0);
                if (ret < 0) {
                    closeSocket(_socket);
                    throw NetworkError("Could not read from socket", lastSocketError());
                }
                destination += ret;
                received += ret;
            }
            if (_should_terminate.load()) {
                closeSocket(_socket);
                throw NetworkInterfaceInterrupt();
            }
            return _partner;
        }

        void send(const uint8_t *data, uint32_t size, ConnectionPartner) override {
            uint32_t sent = 0;
            while (sent < size && !_should_terminate.load()) {
                auto ret = ::send(_socket, (const char*)data, size - sent, 0);
                if (ret < 0) {
                    closeSocket(_socket);
                    throw NetworkError("Could not write to socket", lastSocketError());
                }
                data += ret;
                sent += ret;
            }
            if (_should_terminate.load()) {
                closeSocket(_socket);
                throw NetworkInterfaceInterrupt();
            }
        }

        bool isConnectionOriented() const override {
            return true;
        }

        virtual ~TCPClient() {
            stop();
        }

    };
}

#endif //LIBMAVLINK_TCP_H
