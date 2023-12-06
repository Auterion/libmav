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

#ifndef MAV_PROTOCOLTESTSEQUENCER_H
#define MAV_PROTOCOLTESTSEQUENCER_H

#include "mav/MessageSet.h"
#include "mav/Message.h"

class ProtocolTestSequencer {

public:
    explicit ProtocolTestSequencer(std::shared_ptr<mav::Connection> connection, bool debug = false) :
            _connection(std::move(connection)), _debug(debug) {
    }

    struct ReceiveItem {
        std::string name;
        std::function <void(const mav::Message&)> verification;
    };

    using SendItem = mav::Message;
    using SequenceItem = std::variant<SendItem, ReceiveItem>;

    std::vector<SequenceItem> sequence;

    ProtocolTestSequencer& out(const SendItem& item) {
        sequence.emplace_back(item);
        return *this;
    }

    ProtocolTestSequencer& in(const std::string& name, const std::function <void(const mav::Message&)>& verification) {
        sequence.emplace_back(ReceiveItem{name, verification});
        return *this;
    }

    ProtocolTestSequencer& in(const std::string &name) {
        sequence.emplace_back(ReceiveItem{name, [](const mav::Message&){}});
        return *this;
    }

    void start() {
        _thread = std::make_unique<std::thread>(&ProtocolTestSequencer::_run, this);
    }

    void finish() {
        stop();
        if (_exception) {
            std::rethrow_exception(_exception);
        }
    }

    ~ProtocolTestSequencer() {
        stop();
    }

private:
    void stop() {
        if (_thread) {
            _thread->join();
            _thread = nullptr;
        }
    }

    void _run() {
        for (auto& item : sequence) {
            try {
                if (std::holds_alternative<SendItem>(item)) {
                    _connection->send(std::get<SendItem>(item));
                    if (_debug) {
                        std::cout << "SENT: " << std::get<SendItem>(item).name() << std::endl;
                    }
                } else {
                    // RECEIVE case always receive with 5s timeout
                    ReceiveItem rec_item = std::get<ReceiveItem>(item);
                    auto message = _connection->receive(rec_item.name, 5000);
                    rec_item.verification(message);
                    if (_debug) {
                        std::cout << "RECEIVED: " << message.name() << std::endl;
                    }
                }
            } catch(...) {
                _exception = std::current_exception();
                return;
            }
        }
    }

    bool _debug;
    std::shared_ptr<mav::Connection> _connection;
    std::unique_ptr<std::thread> _thread;

    std::exception_ptr _exception;
};

#endif //MAV_PROTOCOLTESTSEQUENCER_H
