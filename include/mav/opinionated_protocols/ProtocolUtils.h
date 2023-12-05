//
// Created by thomas on 01.12.23.
//

#ifndef MAV_PROTOCOLUTILS_H
#define MAV_PROTOCOLUTILS_H

#include <string>
#include <stdexcept>
#include "mav/Message.h"
#include "mav/Connection.h"

namespace mav {

    class ProtocolError : public std::runtime_error {
    public:
        ProtocolError(const std::string &message) : std::runtime_error(message) {}
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
            throw ProtocolError(message);
        }
    }

    inline Message exchange(
            std::shared_ptr<mav::Connection> connection,
            Message &request,
            const std::string &response_message_name,
            int source_id = mav::ANY_ID,
            int source_component = mav::ANY_ID,
            int timeout_ms = 1000) {
        auto expectation = connection->expect(response_message_name, source_id, source_component);
        connection->send(request);
        return connection->receive(expectation, timeout_ms);
    }

    inline Message exchangeRetry(
            const std::shared_ptr<mav::Connection>& connection,
            Message &request,
            const std::string &response_message_name,
            int source_id = mav::ANY_ID,
            int source_component = mav::ANY_ID,
            int retries = 3,
            int timeout_ms = 1000) {
        for (int i = 0; i < retries; i++) {
            try {
                return exchange(connection, request, response_message_name, source_id, source_component, timeout_ms);
            } catch (const TimeoutException& e) {
                if (i == retries - 1) {
                    throw e;
                }
            }
        }
        throw ProtocolError("Exchange of message " + request.name() + " -> " + response_message_name +
                            " failed after " + std::to_string(retries) + " retries");
    }
}

#endif //MAV_PROTOCOLUTILS_H
