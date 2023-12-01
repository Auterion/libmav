//
// Created by thomas on 20.11.23.
//

#ifndef MAV_MISSION_PROTOCOL_MESSAGES_H
#define MAV_MISSION_PROTOCOL_MESSAGES_H

#include "mav/MessageSet.h"
#include "mav/Message.h"
#include "MessageBuilder.h"

namespace mav {
    namespace mission {
        template <typename T>
        class MissionItemIntMessage : public MessageBuilder<T> {
        public:
            explicit MissionItemIntMessage(const MessageSet &message_set) :
                    MessageBuilder<T>(message_set, "MISSION_ITEM_INT") {
                this->_message.set({
                    {"frame", message_set.e("MAV_FRAME_GLOBAL_INT")},
                    {"autocontinue", 1},
                    {"current", 0},
                    {"mission_type", message_set.e("MAV_MISSION_TYPE_MISSION")}
                });
            }

            auto &latitude_deg(double latitude) {
                return this->_set("x", static_cast<int32_t>(latitude * 1e7));
            }

            auto &longitude_deg(double longitude) {
                return this->_set("y", static_cast<int32_t>(longitude * 1e7));
            }

            auto &altitude_m(double altitude) {
                return this->_set("z", altitude);
            }
        };

        class TakeoffMessage : public MissionItemIntMessage<TakeoffMessage> {
        public:
            explicit TakeoffMessage(const MessageSet &message_set) :
                    MissionItemIntMessage(message_set) {
                _message["command"] = message_set.e("MAV_CMD_NAV_TAKEOFF");
            }

            auto &pitch_deg(double pitch) {
                return _set("param1", pitch);
            }

            auto &yaw_deg(double yaw) {
                return _set("param4", yaw);
            }
        };

        class LandMessage : public MissionItemIntMessage<LandMessage> {
        public:
            explicit LandMessage(const MessageSet &message_set) :
                    MissionItemIntMessage(message_set) {
                _message["command"] = message_set.e("MAV_CMD_NAV_LAND");
            }

            auto &abort_alt_m(double abort_alt) {
                return _set("param1", abort_alt);
            }

            auto &land_mode(int land_mode) {
                return _set("param2", land_mode);
            }

            auto &yaw_deg(double yaw) {
                return _set("param4", yaw);
            }
        };

        class WaypointMessage : public MissionItemIntMessage<WaypointMessage> {
        public:
            explicit WaypointMessage(const MessageSet &message_set) :
                    MissionItemIntMessage(message_set) {
                _message["command"] = message_set.e("MAV_CMD_NAV_WAYPOINT");
            }

            auto &hold_time_s(double hold_time) {
                return _set("param1", hold_time);
            }

            auto &acceptance_radius_m(double acceptance_radius) {
                return _set("param2", acceptance_radius);
            }

            auto &pass_radius_m(double pass_radius) {
                return _set("param3", pass_radius);
            }

            auto &yaw_deg(double yaw) {
                return _set("param4", yaw);
            }
        };
    }
}

#endif //MAV_MISSION_MESSAGES_H
