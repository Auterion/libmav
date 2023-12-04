//
// Created by thomas on 04.12.23.
//

#ifndef MAV_PARAMCLIENT_H
#define MAV_PARAMCLIENT_H

#include <memory>
#include <variant>
#include <optional>
#include "mav/Connection.h"
#include "ProtocolUtils.h"


namespace mav {
namespace param {
    class ParamClient {
    private:
        std::shared_ptr<mav::Connection> _connection;
        const MessageSet& _message_set;

        std::variant<int, float> _unpack(const mav::Message &message) {
            if (message["param_type"].as<uint64_t>() == _message_set.e("MAV_PARAM_TYPE_REAL32")) {
                return message["param_value"].as<float>();
            } else {
                return message["param_value"].floatUnpack<int>();
            }
        }

    public:
        ParamClient(std::shared_ptr<mav::Connection> connection, const MessageSet& message_set) :
                _connection(std::move(connection)),
                _message_set(message_set) {
            ensureMessageInMessageSet(message_set, {"PARAM_REQUEST_LIST", "PARAM_REQUEST_READ", "PARAM_SET", "PARAM_VALUE"});
        }

        mav::Message readRaw(mav::Message &message, int target_system = mav::ANY_ID, int target_component = mav::ANY_ID, int retry_count=3, int item_timeout=1000) {
            throwAssert(message.name() == "PARAM_REQUEST_READ", "Message must be of type PARAM_REQUEST_READ");
            auto response = exchangeRetry(_connection, message, "PARAM_VALUE", target_system, target_component, retry_count, item_timeout);
        }

        std::variant<int, float> read(const std::string &param_id, int target_system = mav::ANY_ID, int target_component = mav::ANY_ID, int retry_count=3, int item_timeout=1000) {
            auto message = _message_set.create("PARAM_REQUEST_READ").set({
                {"target_system", target_system},
                {"target_component", target_component},
                {"param_id", param_id},
                {"param_index", -1}});
            auto response = readRaw(message, target_system, target_component, retry_count, item_timeout);
            return _unpack(response);
        }

        mav::Message writeRaw(mav::Message &message, int target_system = mav::ANY_ID, int target_component = mav::ANY_ID, int retry_count=3, int item_timeout=1000) {
            throwAssert(message.name() == "PARAM_SET", "Message must be of type PARAM_SET");
            auto response = exchangeRetry(_connection, message, "PARAM_VALUE", target_system, target_component, retry_count, item_timeout);
            throwAssert(response["param_id"].as<std::string>() == message["param_id"].as<std::string>(), "Parameter ID mismatch");
            throwAssert(response["param_type"].as<float>() == message["param_type"].as<float>(), "Parameter type mismatch");
        }

        void write(const std::string &param_id, std::variant<int, float> value, int target_system = mav::ANY_ID, int target_component = mav::ANY_ID, int retry_count=3, int item_timeout=1000) {
            auto message = _message_set.create("PARAM_SET").set({
                {"target_system", target_system},
                {"target_component", target_component},
                {"param_id", param_id}});

            if (std::holds_alternative<int>(value)) {
                message["param_value"].floatPack(std::get<int>(value));
                message["param_type"] = _message_set.e("MAV_PARAM_TYPE_INT32");
            } else {
                message["param_value"] = std::get<float>(value);
                message["param_type"] = _message_set.e("MAV_PARAM_TYPE_REAL32");
            }

            writeRaw(message, target_system, target_component, retry_count, item_timeout);
        }

        std::vector<std::optional<mav::Message>> listRaw(int target_system = mav::ANY_ID, int target_component = mav::ANY_ID, int retry_count=3, int item_timeout=1000) {
            std::vector<std::optional<mav::Message>> result;

            auto list_message = _message_set.create("PARAM_REQUEST_LIST").set({
                {"target_system", target_system},
                {"target_component", target_component}});
            _connection->send(list_message);
            while (true) {
                try {
                    auto message = _connection->receive("PARAM_VALUE", item_timeout);
                    if (static_cast<int>(result.size()) < message["param_count"].as<int>()) {
                        result.resize(message["param_count"].as<int>());
                    }
                    throwAssert(message["param_index"].as<int>() < static_cast<int>(result.size()), "Index out of bounds");
                    result[message["param_index"].as<int>()] = message;
                } catch (const TimeoutException& e) {
                    // by the mavlink spec, the end of a param list transaction is detected by a timeout
                    break;
                }
            }

            // re-request missing parameters
            for (int i = 0; i < static_cast<int>(result.size()); i++) {
                if (!result[i].has_value()) {
                    result[i] = readRaw(_message_set.create("PARAM_REQUEST_READ").set({
                        {"target_system", target_system},
                        {"target_component", target_component},
                        {"param_index", i}}), target_system, target_component);
                }
            }
            return result;
        }

        std::map<std::string, std::variant<int, float>> list(int target_system = mav::ANY_ID, int target_component = mav::ANY_ID, int retry_count=3, int item_timeout=1000) {
            std::map<std::string, std::variant<int, float>> result;
            auto raw_result = listRaw(target_system, target_component, retry_count, item_timeout);
            for (auto &message : raw_result) {
                if (!message.has_value()) {
                    continue;
                }
                result[message.value()["param_id"].as<std::string>()] = _unpack(message.value());
            }
            return result;
        }
    };
}
}
#endif //MAV_PARAMCLIENT_H
