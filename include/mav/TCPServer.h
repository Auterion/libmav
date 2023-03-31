//
// Created by thomas on 24.02.23.
//

#ifndef LIBMAVLINK_TCPSERVER_H
#define LIBMAVLINK_TCPSERVER_H
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <atomic>
#include <unistd.h>
#include <csignal>
#include "Network.h"

namespace mav {

    class TCPServer : public mav::NetworkInterface {

    private:
        mutable std::atomic_bool _should_terminate{false};
        int _master_socket = -1;
        mutable std::mutex _client_sockets_mutex;

        int _current_client_socket = -1;
        ConnectionPartner _current_client;

        std::unordered_map<int, ConnectionPartner> _fd_to_partner;
        std::unordered_map<ConnectionPartner, int, _ConnectionPartnerHash> _partner_to_fd;


        int _epoll_fd = -1;

        void _addFd(int fd, int events) const {
            struct epoll_event epev = {};
            epev.events = events;
            epev.data.fd = fd;
            if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, fd, &epev) < 0) {
                throw NetworkError("Could not add fd");
            }
        }

        void _removeFd(int fd) const {
            if (epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, fd, nullptr) < 0) {
                throw NetworkError("Could not remove fd");
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
            _addFd(client_socket, EPOLLIN);
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

            _epoll_fd = epoll_create1(EPOLL_CLOEXEC);
            if (_epoll_fd < 0) {
                throw NetworkError("Could not create epoll");
            }

            _master_socket = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
            if (_master_socket < 0) {
                throw NetworkError("Could not create socket");
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
            _addFd(_master_socket, EPOLLIN);
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
            if (_epoll_fd >= 0) {
                ::close(_epoll_fd);
                _epoll_fd = -1;
            }
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
                struct epoll_event events[32];
                auto n_activity = epoll_wait(_epoll_fd, events, 32, 1000);
                if (n_activity < 0) {
                    if (errno == EINTR) {
                        continue;
                    } else {
                        stop();
                        throw NetworkError("epoll error");
                    }
                }

                int socket_to_read_from = -1;
                ConnectionPartner partner_to_read_from;

                // iterate through the activity
                for (int i=0; i<n_activity; i++) {
                    if (events[i].data.fd == _master_socket) {
                        // this is a new connection
                        _handleNewConnection();
                    } else if (events[i].data.fd == _current_client_socket) {
                        // this is the current connection
                        socket_to_read_from = _current_client_socket;
                        partner_to_read_from = _current_client;
                        // ignore other activity. We want to read from this socket.
                        break;
                    } else {
                        // this is a another connection
                        socket_to_read_from = events[i].data.fd;
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
