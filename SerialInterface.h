//
// Created by Thomas Debrunner on 14.01.23.
//

#ifndef LIBMAVLINK_SERIALINTERFACE_H
#define LIBMAVLINK_SERIALINTERFACE_H

#include "Network.h"
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>

namespace libmavlink {
    class Serial : public NetworkInterface {
    private:
        int _fd = -1;

    public:
        Serial(const std::string &device, int baud_rate = 9600, bool flow_control = false) {
            _fd = open(device.c_str(), O_RDWR | O_NOCTTY);
            if (_fd < 0) {
                throw NetworkError(StringFormat() << "Failed to open serial device at "
                    << device << StringFormat::end);
            }

            struct termios tc{};
            if (tcgetattr(_fd, &tc) != 0) {
                throw NetworkError(StringFormat() << "Could not run tcgetattr on " << device << StringFormat::end);
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

            
        }

        void send(const uint8_t* data, uint32_t size) const {

        };

        void receive(uint8_t* data, uint32_t size) const {

        };
    };
}

#endif //LIBMAVLINK_SERIALINTERFACE_H
