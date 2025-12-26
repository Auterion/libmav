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

#ifndef MAV_PROTOCOLUTILS_H
#define MAV_PROTOCOLUTILS_H

#include <string>
#include <stdexcept>
#include "mav/Message.h"
#include "mav/Connection.h"

namespace mav {

    class ProtocolException : public std::runtime_error {
    public:
        ProtocolException(const std::string &message) : std::runtime_error(message) {}
    };

    inline void ensureMessageInMessageSet(const MessageSet &message_set, const std::initializer_list<std::string> &message_names) {
        for (const auto &message_name : message_names) {
            if (!message_set.contains(message_name)) {
                throw std::runtime_error("Message " + message_name + " not present in message set");
            }
        }
    }

    inline void throwAssert(bool condition, const std::string& message) {
        if (!condition) {
            throw ProtocolException(message);
        }
    }

    inline Connection::Expectation expectAny(const std::shared_ptr<mav::Connection>& connection,
                                             const std::vector<int> &message_ids, int source_id=mav::ANY_ID, int component_id=mav::ANY_ID) {
        return connection->expect([message_ids, source_id, component_id](const Message& message) {
            return std::find(message_ids.begin(), message_ids.end(), message.id()) != message_ids.end() &&
                   (source_id == mav::ANY_ID || message.header().systemId() == source_id) &&
                   (component_id == mav::ANY_ID || message.header().componentId() == component_id);
        });
    }

    inline Connection::Expectation expectAny(const std::shared_ptr<mav::Connection>& connection, const MessageSet &message_set,
                                             const std::vector<std::string> &message_names, int source_id=mav::ANY_ID, int component_id=mav::ANY_ID) {
        std::vector<int> message_ids;
        for (const auto &message_name : message_names) {
            message_ids.push_back(message_set.idForMessage(message_name));
        }
        return expectAny(connection, message_ids, source_id, component_id);
    }

    inline Message exchange(
            const std::shared_ptr<mav::Connection> &connection,
            Message &request,
            const std::string &response_message_name,
            int source_id = mav::ANY_ID,
            int source_component = mav::ANY_ID,
            int timeout_ms = 1000) {
        auto expectation = connection->expect(response_message_name, source_id, source_component);
        connection->send(request);
        return connection->receive(expectation, timeout_ms);
    }

    inline Message exchangeAnyResponse(
            const std::shared_ptr<mav::Connection> &connection,
            const MessageSet &message_set,
            Message &request,
            const std::vector<std::string> &response_message_names,
            int source_id = mav::ANY_ID,
            int source_component = mav::ANY_ID,
            int timeout_ms = 1000) {
        auto expectation = expectAny(connection, message_set, response_message_names, source_id, source_component);
        connection->send(request);
        return connection->receive(expectation, timeout_ms);
    }

    template <typename Ret, typename... Arg>
    inline Ret _retry(int retries, Ret (*func)(Arg...), Arg... args) {
        for (int i = 0; i < retries; i++) {
            try {
                return func(args...);
            } catch (const TimeoutException& e) {
                if (i == retries - 1) {
                    throw;
                }
            }
        }
        throw TimeoutException(std::string{"Function failed after " + std::to_string(retries) + " retries"}.c_str());
    }

    inline Message exchangeRetry(
            const std::shared_ptr<mav::Connection> &connection,
            Message &request,
            const std::string &response_message_name,
            int source_id = mav::ANY_ID,
            int source_component = mav::ANY_ID,
            int retries = 3,
            int timeout_ms = 1000) {
        return _retry<Message, const std::shared_ptr<Connection>&, Message&, const std::string&, int, int, int>
                (retries, exchange, connection, request, response_message_name, source_id,
                     source_component, timeout_ms);
    }

    inline Message exchangeRetryAnyResponse(
            const std::shared_ptr<mav::Connection> &connection,
            const MessageSet &message_set,
            Message &request,
            const std::vector<std::string> &response_message_names,
            int source_id = mav::ANY_ID,
            int source_component = mav::ANY_ID,
            int retries = 3,
            int timeout_ms = 1000) {
        return _retry<Message, const std::shared_ptr<Connection>&, const MessageSet&, Message&, const std::vector<std::string>&
                , int, int, int>
                (retries, exchangeAnyResponse, connection, message_set, request, response_message_names, source_id,
                     source_component, timeout_ms);
    }
}

#endif //MAV_PROTOCOLUTILS_H
