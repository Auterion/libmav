//
// Created by thomas on 06.01.23.
//

#ifndef LIBMAVLINK_CRC_H
#define LIBMAVLINK_CRC_H

namespace libmavlink {
    class CRC {
    private:
        uint16_t _crc = 0xFFFF;

    public:
        inline void accumulate(uint8_t d) {
            uint8_t tmp = d ^ (_crc & 0xFF);
            tmp ^=  (tmp << 4);
            _crc = (_crc >> 8) ^ (tmp << 8) ^ (tmp << 3) ^ (tmp >> 4);
        }

        void accumulate(const std::string &str) {
            for (const char c : str) {
                accumulate(static_cast<uint8_t>(c));
            }
        }

        [[nodiscard]] uint16_t crc16() const {
            return _crc;
        }

        [[nodiscard]] uint8_t crc8() const {
            return static_cast<uint8_t>(_crc & 0xFF) ^ static_cast<uint8_t>((_crc >> 8) & 0xFF);
        }
    };
}

#endif //LIBMAVLINK_CRC_H
