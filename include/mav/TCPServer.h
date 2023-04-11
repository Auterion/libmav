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

#ifndef LIBMAVLINK_TCPSERVER_H
#define LIBMAVLINK_TCPSERVER_H
#include <sys/socket.h>
#include <sys/poll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <atomic>
#include <unistd.h>
#include <csignal>
#include "Network.h"
#include <sys/fcntl.h>

namespace mav {

    class TCPServer : public mav::NetworkInterface {

    private:
        mutable std::atomic_bool _should_terminate{false};
        int _master_socket = -1;
        mutable std::mutex _client_sockets_mutex;
        std::vector<struct pollfd> _poll_fds;

        int _current_client_socket = -1;
        ConnectionPartner _current_client;

        std::unordered_map<int, ConnectionPartner> _fd_to_partner;
        std::unordered_map<ConnectionPartner, int, _ConnectionPartnerHash> _partner_to_fd;


        void _addFd(int fd, int16_t events) {
            struct pollfd pfd = {};
            pfd.fd = fd;
            pfd.events = events;
            _poll_fds.push_back(pfd);
        }

        void _removeFd(int fd) {
            for (auto it = _poll_fds.begin(); it != _poll_fds.end(); ++it) {
                if (it->fd == fd) {
                    _poll_fds.erase(it);
                    break;
                }
            }
        }

        void _handleNewConnection() {
            struct sockaddr_in client_address{};
            socklen_t client_address_length = sizeof(client_address);
            auto client_socket = accept(_master_socket, (struct sockaddr *) &client_address,
                                        &client_address_length);
            if (client_socket < 0) {
                ::close(_master_socket);
                throw NetworkError("Could not accept connection");
            }
            struct sockaddr_in address{};
            int addrlen = sizeof(address);
            getpeername(client_socket , (struct sockaddr*)&address, (socklen_t*)&addrlen);
            ConnectionPartner partner = {address.sin_addr.s_addr, address.sin_port, false};
            _fd_to_partner.insert({client_socket, partner});
            _partner_to_fd.insert({partner, client_socket});
            _addFd(client_socket, POLLIN);
        }

        void _handleDisconnect(ConnectionPartner partner, int fd) {
            std::lock_guard<std::mutex> lock(_client_sockets_mutex);
            _partner_to_fd.erase(partner);
            _fd_to_partner.erase(fd);
            _removeFd(fd);
            ::close(fd);
        }

    public:

        TCPServer(int port) {
            _master_socket = socket(AF_INET, SOCK_STREAM, 0);
            if (_master_socket < 0) {
                throw NetworkError("Could not create socket: " + std::to_string(_master_socket));
            }

            // Mark socket as non-blocking
            if (fcntl(_master_socket, F_SETFL, O_NONBLOCK) < 0) {
                ::close(_master_socket);
                throw NetworkError("Could not set socket to non-blocking");
            }

            const int enable = 1;
            if (setsockopt(_master_socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
                ::close(_master_socket);
                throw NetworkError("Could not set socket options");
            }
            struct sockaddr_in server_address{};
            server_address.sin_family = AF_INET;
            server_address.sin_port = htons(port);
            server_address.sin_addr.s_addr = htonl(INADDR_ANY);

            if (bind(_master_socket, (struct sockaddr *) &server_address, sizeof(server_address)) < 0) {
                ::close(_master_socket);
                throw NetworkError("Could not bind socket");
            }

            if (listen(_master_socket, 32) < 0) {
                ::close(_master_socket);
                throw NetworkError("Could not listen on socket");
            }

            _addFd(_master_socket, POLLIN);
        }

        void stop() {
            _should_terminate.store(true);
            if (_master_socket >= 0) {
                _removeFd(_master_socket);
                ::shutdown(_master_socket, SHUT_RDWR);
                ::close(_master_socket);
                _master_socket = -1;
            }
            std::lock_guard<std::mutex> lock(_client_sockets_mutex);
            for (auto client_socket : _partner_to_fd) {
                _removeFd(client_socket.second);
                ::shutdown(client_socket.second, SHUT_RDWR);
                ::close(client_socket.second);
            }
            _partner_to_fd.clear();
            _fd_to_partner.clear();
        }

        void close() override {
            stop();
        }

        ConnectionPartner receive(uint8_t *destination, uint32_t size) override {
            int bytes_received = 0;

            while (bytes_received < size) {

                if (_should_terminate.load()) {
                    stop();
                    throw NetworkInterfaceInterrupt();
                }

                // check for activity on one of the sockets
                auto poll_ret = poll(_poll_fds.data(), _poll_fds.size(), 1000);
                if (poll_ret < 0) {
                    if (errno == EINTR) {
                        continue;
                    } else {
                        stop();
                        throw NetworkError("poll error");
                    }
                } else if (poll_ret == 0) {
                    continue;
                } else {
                    int socket_to_read_from = -1;
                    ConnectionPartner partner_to_read_from;

                    // iterate through the activity
                    for (int i = 0; i < _poll_fds.size(); i++) {
                        if (_poll_fds[i].revents == 0) {
                            continue;
                        }
                        if (_poll_fds[i].revents != POLLIN) {
                            // error on socket
                            stop();
                            throw NetworkError("Error on socket");
                        }
                        if (_poll_fds[i].fd == _master_socket) {
                            // this is a new connection
                            _handleNewConnection();
                        } else if (_poll_fds[i].fd == _current_client_socket) {
                            // this is the current connection
                            socket_to_read_from = _current_client_socket;
                            partner_to_read_from = _current_client;
                            // ignore other activity. We want to read from this socket.
                            break;
                        } else {
                            // this is a another connection
                            socket_to_read_from = _poll_fds[i].fd;
                            partner_to_read_from = _fd_to_partner.at(socket_to_read_from);
                        }
                    }

                    // do the actual read
                    if (socket_to_read_from >= 0) {
                        auto ret = read(socket_to_read_from, destination, size - bytes_received);
                        if (ret <= 0) {
                            // client disconnected
                            _handleDisconnect(partner_to_read_from, socket_to_read_from);
                            continue;
                        } else {
                            bytes_received += ret;
                            destination += ret;
                            _current_client_socket = socket_to_read_from;
                            _current_client = partner_to_read_from;
                        }
                    }
                }
            }

            return _current_client;
        }

        void _sendToSingleTarget(const uint8_t *data, uint32_t size, int partner_socket) {
            uint32_t sent = 0;
            while (sent < size && !_should_terminate.load()) {
                auto ret = write(partner_socket, data, size - sent);
                if (ret < 0) {
                    throw NetworkError("Could not write to socket");
                }
                sent += ret;
            }
            if (_should_terminate.load()) {
                stop();
                throw NetworkInterfaceInterrupt();
            }
        }

        void send(const uint8_t *data, uint32_t size, ConnectionPartner target) override {
            if (target.isBroadcast()) {
                std::lock_guard<std::mutex> lock(_client_sockets_mutex);
                for (auto client_socket : _partner_to_fd) {
                    _sendToSingleTarget(data, size, client_socket.second);
                }
            } else {
                std::lock_guard<std::mutex> lock(_client_sockets_mutex);
                auto partner = _partner_to_fd.find(target);
                if (partner == _partner_to_fd.end()) {
                    throw NetworkError("Could not find client socket");
                }
                _sendToSingleTarget(data, size, partner->second);
            }
        }

        bool isConnectionOriented() const override {
            return true;
        }


        void markMessageBoundary() override {
            // release the current socket, we can again accept data from any client socket
            _current_client_socket = -1;
        }

        virtual ~TCPServer() {
            stop();
        }

    };
}


#endif //LIBMAVLINK_TCPSERVER_H
