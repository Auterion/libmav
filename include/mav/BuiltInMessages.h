//
// Created by thomas on 13.01.23.
//

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
