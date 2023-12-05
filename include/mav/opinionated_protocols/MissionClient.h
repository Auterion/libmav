//
// Created by thomas on 01.12.23.
//

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
            auto mission_count_message = _message_set.create("MISSION_COUNT").set({
                {"target_system", target.system_id},
                {"target_component", target.component_id},
                {"count", mission_messages.size()},
                {"mission_type", mission_type}});

            auto count_response = exchangeRetry(_connection, mission_count_message, "MISSION_REQUEST_INT",
                          target.system_id, target.component_id, retry_count, item_timeout);

            throwAssert(count_response["mission_type"].as<int>() == mission_type, "Mission type mismatch");
            throwAssert(count_response["seq"].as<int>() == 0, "Sequence number mismatch");

            int seq = 0;
            while (seq < static_cast<int>(mission_messages.size())) {
                auto mission_item_message = mission_messages[seq];
                mission_item_message["target_system"] = target.system_id;
                mission_item_message["target_component"] = target.component_id;
                mission_item_message["seq"] = seq;

                // we expect an ack for the last message, otherwise a request for the next one
                const auto expected_response = seq == static_cast<int>(mission_messages.size()) - 1 ?
                        "MISSION_ACK" : "MISSION_REQUEST_INT";
                auto item_response = exchangeRetry(_connection, mission_item_message, expected_response,
                              target.system_id, target.component_id, retry_count, item_timeout);

                if (seq == static_cast<int>(mission_messages.size()) - 1) {
                    // we expect an ack for the last message
                    throwAssert(item_response["type"].as<int>() == _message_set.e("MAV_MISSION_ACCEPTED"), "Mission upload failed");
                    seq++;
                } else {
                    // we expect a request for the next message
                    throwAssert(item_response["mission_type"].as<int>() == mission_type, "Mission type mismatch");
                    seq = item_response["seq"];
                }
            }
        }

        std::vector<Message> download(Identifier target={1, 1}, int mission_type=0, int retry_count=3, int item_timeout=1000) {
            // Send mission request list
            auto mission_request_list_message = _message_set.create("MISSION_REQUEST_LIST").set({
                {"target_system", target.system_id},
                {"target_component", target.component_id},
                {"mission_type", mission_type}});

            auto request_list_response = exchangeRetry(_connection, mission_request_list_message, "MISSION_COUNT",
                          target.system_id, target.component_id, retry_count, item_timeout);

            throwAssert(request_list_response["mission_type"].as<int>() == mission_type, "Mission type mismatch");

            int count = request_list_response["count"];
            std::vector<Message> mission_messages;
            for (int seq = 0; seq < count; seq++) {
                auto mission_request_message = _message_set.create("MISSION_REQUEST_INT").set({
                    {"target_system", target.system_id},
                    {"target_component", target.component_id},
                    {"seq", seq},
                    {"mission_type", 0}});

                auto request_response = exchangeRetry(_connection, mission_request_message, "MISSION_ITEM_INT",
                              target.system_id, target.component_id, retry_count, item_timeout);

                throwAssert(request_response["mission_type"].as<int>() == 0, "Mission type mismatch");
                throwAssert(request_response["seq"].as<int>() == seq, "Sequence number mismatch");

                mission_messages.push_back(request_response);
            }
            auto ack_message = _message_set.create("MISSION_ACK").set({
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
