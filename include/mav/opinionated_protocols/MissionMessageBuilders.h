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
