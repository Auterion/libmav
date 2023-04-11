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

#include <atomic>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <poll.h>
#include <unordered_map>
#include "Network.h"

#ifndef MAV_SERIAL_H
#define MAV_SERIAL_H

namespace mav {
    class Serial : public NetworkInterface {
    private:
        int _fd;
        mutable std::atomic_bool _should_terminate{false};
        ConnectionPartner _partner;

    public:
        Serial(const std::string &device, int baud, bool flow_control = true) {
            std::hash<std::string> hasher;
            _partner = {static_cast<uint32_t>(hasher(device)), 0, true};

            _fd = open(device.c_str(), O_RDWR | O_NOCTTY);
            if (_fd == -1) {
                throw NetworkError(StringFormat() << "Failed to open " << device << " error " << _fd << StringFormat::end);
            }
            struct termios tc{};
            if (tcgetattr(_fd, &tc) != 0) {
                ::close(_fd);
                throw NetworkError(StringFormat() << "Failed to get tc attrs" << StringFormat::end);
            }

            tc.c_iflag &= ~(IGNBRK | BRKINT | ICRNL | INLCR | PARMRK | INPCK | ISTRIP | IXON);
            tc.c_oflag &= ~(OCRNL | ONLCR | ONLRET | ONOCR | OFILL | OPOST);
            tc.c_lflag &= ~(ECHO | ECHONL | ICANON | IEXTEN | ISIG | TOSTOP);
            tc.c_cflag &= ~(CSIZE | PARENB | CRTSCTS);
            tc.c_cflag |= CS8 | CLOCAL;

            tc.c_cc[VMIN] = 0; // We are ok with 0 bytes.
            tc.c_cc[VTIME] = 10; // Timeout after 1 second.

            if (flow_control) {
                tc.c_cflag |= CRTSCTS;
            }

            if (cfsetspeed(&tc, baud) != 0) {
                ::close(_fd);
                throw NetworkError(StringFormat() << "Failed to set baud rate to " << baud << StringFormat::end);
            }

            if (tcsetattr(_fd, TCSANOW, &tc) != 0) {
                ::close(_fd);
                throw NetworkError(StringFormat() << "Failed to set TCSANOW" << StringFormat::end);
            }
        }


        void send(const uint8_t* data, uint32_t size, ConnectionPartner partner) override {
            uint32_t sent = 0;
            while (sent < size && !_should_terminate.load()) {
                auto ret = write(_fd, data, size - sent);
                if (ret < 0) {
                    ::close(_fd);
                    throw NetworkClosed("Serial send failed");
                }
                sent += static_cast<uint32_t>(ret);
            }
            if (_should_terminate.load()) {
                throw NetworkInterfaceInterrupt();
            }
        };

        ConnectionPartner receive(uint8_t* data, uint32_t size) override {
            uint32_t received = 0;
            while (received < size && !_should_terminate.load()) {
                auto ret = read(_fd, data, size - received);
                if (ret < 0) {
                    ::close(_fd);
                    throw NetworkClosed("Serial read failed");
                }
                received += ret;
            }
            if (_should_terminate.load()) {
                throw NetworkInterfaceInterrupt();
            }
            return _partner;
        };

        void close() {
            _should_terminate.store(true);
            ::close(_fd);
        }

        bool isConnectionOriented() const override {
            return true;
        }

        virtual ~Serial() {
            ::close(_fd);
        }
    };

}





#endif //MAV_SERIAL_H
