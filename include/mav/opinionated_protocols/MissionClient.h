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

#ifndef MAV_MISSIONCLIENT_H
#define MAV_MISSIONCLIENT_H

#include <vector>
#include <memory>
#include "MissionMessageBuilders.h"
#include "ProtocolUtils.h"


namespace mav {
namespace mission {

    class MissionClient {
    private:
        std::shared_ptr<mav::Connection> _connection;
        const MessageSet& _message_set;

        void _assertNotNack(const Message& message) {
            if (message.id() == _message_set.idForMessage("MISSION_ACK")) {
                if (message["type"].as<uint64_t>() != _message_set.e("MAV_MISSION_ACCEPTED")) {
                    throw mav::ProtocolException("Received NACK from server. Mission transaction failed.");
                }
            }
        }

    public:
        MissionClient(std::shared_ptr<Connection> connection, const MessageSet& message_set) :
                _connection(std::move(connection)),
                _message_set(message_set) {

            // Make sure all messages required for the protocol are present in the message set
            ensureMessageInMessageSet(_message_set, {
                "MISSION_COUNT",
                "MISSION_REQUEST_INT",
                "MISSION_REQUEST_LIST",
                "MISSION_ITEM_INT",
                "MISSION_ACK"});
        }

        void upload(const std::vector<Message> &mission_messages, Identifier target={1, 1},
                    int retry_count=3, int item_timeout=1000) {

            if (mission_messages.empty()) {
                // nothing to do
                return;
            }
            // get mission type from first message
            int mission_type = mission_messages[0]["mission_type"];

            // Send mission count
            auto mission_count_message = _message_set.create("MISSION_COUNT")({
                {"target_system", target.system_id},
                {"target_component", target.component_id},
                {"count", mission_messages.size()},
                {"mission_type", mission_type}});

            Message count_response = exchangeRetryAnyResponse(_connection, _message_set, mission_count_message,
                                                              {"MISSION_ACK", "MISSION_REQUEST_INT"},
                                                              target.system_id, target.component_id,
                                                              retry_count, item_timeout);
            _assertNotNack(count_response);
            throwAssert(count_response["mission_type"].as<int>() == mission_type, "Mission type mismatch");
            throwAssert(count_response["seq"].as<int>() == 0, "Sequence number mismatch");

            int seq = 0;
            while (seq < static_cast<int>(mission_messages.size())) {
                auto mission_item_message = mission_messages[seq];
                mission_item_message["target_system"] = target.system_id;
                mission_item_message["target_component"] = target.component_id;
                mission_item_message["seq"] = seq;

                auto item_response = exchangeRetryAnyResponse(_connection, _message_set, mission_item_message,
                                                              {"MISSION_ACK", "MISSION_REQUEST_INT"},
                              target.system_id, target.component_id, retry_count, item_timeout);
                // NACK is always bad, throw if we receive NACK
                _assertNotNack(item_response);

                if (seq == static_cast<int>(mission_messages.size()) - 1) {
                    // we're okay with an ack, only when we're at the last message
                    if (item_response.id() == _message_set.idForMessage("MISSION_ACK")) {
                        break;
                    }
                }
                // in general, we need a mission request int
                throwAssert(item_response.id() == _message_set.idForMessage("MISSION_REQUEST_INT"), "Unexpected message");
                // we expect a request for the next message
                throwAssert(item_response["mission_type"].as<int>() == mission_type, "Mission type mismatch");
                int response_seq = item_response["seq"];
                // we allow requests for the next message or the current message
                throwAssert((response_seq == seq + 1 || response_seq == seq)
                    && response_seq < static_cast<int>(mission_messages.size()), "Sequence number mismatch");
                seq = response_seq;
            }
        }

        std::vector<Message> download(Identifier target={1, 1}, int mission_type=0, int retry_count=3, int item_timeout=1000) {
            // Send mission request list
            auto mission_request_list_message = _message_set.create("MISSION_REQUEST_LIST")({
                {"target_system", target.system_id},
                {"target_component", target.component_id},
                {"mission_type", mission_type}});

            auto request_list_response = exchangeRetryAnyResponse(_connection, _message_set,
                          mission_request_list_message, {"MISSION_COUNT", "MISSION_ACK"},
                          target.system_id, target.component_id, retry_count, item_timeout);
            _assertNotNack(request_list_response);
            throwAssert(request_list_response.id() == _message_set.idForMessage("MISSION_COUNT"), "Unexpected message");
            throwAssert(request_list_response["mission_type"].as<int>() == mission_type, "Mission type mismatch");

            int count = request_list_response["count"];
            std::vector<Message> mission_messages;
            for (int seq = 0; seq < count; seq++) {
                auto mission_request_message = _message_set.create("MISSION_REQUEST_INT")({
                    {"target_system", target.system_id},
                    {"target_component", target.component_id},
                    {"seq", seq},
                    {"mission_type", 0}});

                auto request_response = exchangeRetryAnyResponse(_connection, _message_set, mission_request_message,
                              {"MISSION_ITEM_INT", "MISSION_ACK"},
                              target.system_id, target.component_id, retry_count, item_timeout);

                _assertNotNack(request_response);
                throwAssert(request_response.id() == _message_set.idForMessage("MISSION_ITEM_INT"), "Unexpected message");
                throwAssert(request_response["mission_type"].as<int>() == 0, "Mission type mismatch");
                throwAssert(request_response["seq"].as<int>() == seq, "Sequence number mismatch");

                mission_messages.push_back(request_response);
            }
            auto ack_message = _message_set.create("MISSION_ACK")({
                {"target_system", target.system_id},
                {"target_component", target.component_id},
                {"type", _message_set.e("MAV_MISSION_ACCEPTED")},
                {"mission_type", mission_type}});
            _connection->send(ack_message);

            return mission_messages;
        }
    };
};
}

#endif //MAV_MISSIONCLIENT_H
