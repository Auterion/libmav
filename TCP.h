//
// Created by thomas on 27.01.23.
//
#ifndef LIBMAVLINK_TCP_H
#define LIBMAVLINK_TCP_H

#include <sys/socket.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <atomic>
#include "Network.h"

namespace mav {

    class TCP : public mav::NetworkInterface {

    private:
        mutable std::atomic_bool _should_terminate{false};
        int _socket = -1;

    public:

        TCP(const std::string& address, int port) {
            _socket = socket(AF_INET, SOCK_STREAM, 0);
            if (_socket < 0) {
                throw NetworkError("Could not create socket");
            }
            struct sockaddr_in server_address{};
            server_address.sin_family = AF_INET;
            server_address.sin_port = htons(port);
            server_address.sin_addr.s_addr = inet_addr(address.c_str());

            if (connect(_socket, (struct sockaddr *) &server_address, sizeof(server_address)) < 0) {
                ::close(_socket);
                throw NetworkError("Could not connect to server");
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
            uint32_t received = 0;
            while (received < size && !_should_terminate.load()) {
                auto ret = read(_socket, destination, size - received);
                if (ret < 0) {
                    ::close(_socket);
                    throw NetworkError("Could not read from socket");
                }
                received += ret;
            }
            if (_should_terminate.load()) {
                ::close(_socket);
                throw NetworkInterfaceInterrupt();
            }
        }

        void send(const uint8_t *data, uint32_t size) override {
            uint32_t sent = 0;
            while (sent < size && !_should_terminate.load()) {
                auto ret = write(_socket, data, size - sent);
                if (ret < 0) {
                    ::close(_socket);
                    throw NetworkError("Could not write to socket");
                }
                sent += ret;
            }
            if (_should_terminate.load()) {
                ::close(_socket);
                throw NetworkInterfaceInterrupt();
            }
        }

        virtual ~TCP() {
            stop();
        }

    };
}

#endif //LIBMAVLINK_TCP_H
