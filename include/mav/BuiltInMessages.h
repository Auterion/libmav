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

#ifndef MAV_BUILTINMESSAGES_H
#define MAV_BUILTINMESSAGES_H

#include <sstream>

namespace mav {
    constexpr char MSG_HEARTBEAT[] =
            "    <message id=\"0\" name=\"HEARTBEAT\">\n"
            "      <description>The heartbeat message shows that a system or component is present and responding. The type and autopilot fields (along with the message component id), allow the receiving system to treat further messages from this system appropriately (e.g. by laying out the user interface based on the autopilot). This microservice is documented at https://mavlink.io/en/services/heartbeat.html</description>\n"
            "      <field type=\"uint8_t\" name=\"type\" enum=\"MAV_TYPE\">Vehicle or component type. For a flight controller component the vehicle type (quadrotor, helicopter, etc.). For other components the component type (e.g. camera, gimbal, etc.). This should be used in preference to component id for identifying the component type.</field>\n"
            "      <field type=\"uint8_t\" name=\"autopilot\" enum=\"MAV_AUTOPILOT\">Autopilot type / class. Use MAV_AUTOPILOT_INVALID for components that are not flight controllers.</field>\n"
            "      <field type=\"uint8_t\" name=\"base_mode\" enum=\"MAV_MODE_FLAG\" display=\"bitmask\">System mode bitmap.</field>\n"
            "      <field type=\"uint32_t\" name=\"custom_mode\">A bitfield for use for autopilot-specific flags</field>\n"
            "      <field type=\"uint8_t\" name=\"system_status\" enum=\"MAV_STATE\">System status flag.</field>\n"
            "      <field type=\"uint8_t_mavlink_version\" name=\"mavlink_version\">MAVLink version, not writable by user, gets added by protocol because of magic data type: uint8_t_mavlink_version</field>\n"
            "    </message>";


    std::string messageStringOf(std::initializer_list<const char*> init_list) {
        std::stringstream ss;
        ss << "<mavlink><messages>";
        for (const auto &item : init_list) {
            ss << item;
        }
        ss << "</messages></mavlink>";
        return ss.str();
    }

};


#endif //MAV_BUILTINMESSAGES_H
