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

#ifndef LIBMAVLINK_DIRECTIO_H
#define LIBMAVLINK_DIRECTIO_H

#include <atomic>
#include <sys/types.h>
#include <utility>
#include <vector>
#include <array>
#include <functional>
#include <cstdint>
#include "Network.h"

namespace mav {

    class DirectIO : public mav::NetworkInterface {

    public:
        using SendCallback = std::function<void(const uint8_t* buf, uint32_t len)>;
        using ReceiveCallback = std::function<ssize_t(uint8_t* buf, uint32_t len)>;

    private:
        static constexpr size_t RX_BUFFER_SIZE = 2048;

        mutable std::atomic_bool _should_terminate{false};

        std::array<uint8_t, RX_BUFFER_SIZE> _rx_buffer;
        int _bytes_available = 0;
        ConnectionPartner _partner = {0, 0, false};

        SendCallback _send_callback{nullptr};
        ReceiveCallback _receive_callback{nullptr};

    public:
            DirectIO(SendCallback send_callback, ReceiveCallback receive_callback) :
             _send_callback(send_callback),
            _receive_callback(receive_callback)
        {
        }

        void stop() {
            _should_terminate.store(true);
        }

        void close() override {
            stop();
        }

        ConnectionPartner receive(uint8_t *destination, uint32_t size) override {
            if (!_receive_callback) {
                // TODO: use that to signal nothing?
                throw NetworkInterfaceInterrupt();
            }

            // Receive as many messages as needed to have enough bytes available (none if already enough bytes)
            while (_bytes_available < size && !_should_terminate.load()) {
                // If there are residual bytes from last packet, but not enough for parsing new packet, clear out
                _bytes_available = 0;
                const ssize_t ret = _receive_callback(_rx_buffer.data(), RX_BUFFER_SIZE);
                if (ret < 0) {
                    throw NetworkError("Could not receive from callback");
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
            if (!_send_callback) {
                return;
            }
            _send_callback(data, size);
        }

        void markSyncing() override {
            // we're out of sync. The beginning of a packet was not the magic number we expected,
            // therefore, we can discard the rest of the packet and just receive another datagram.
            _bytes_available = 0;
        }

        bool isConnectionOriented() const override {
            // even though UDP is not connection oriented, we can use it as if it was,
            return true;
        }

        virtual ~DirectIO() {
            stop();
        }
    };
}

#endif //LIBMAVLINK_DIRECT_IO
