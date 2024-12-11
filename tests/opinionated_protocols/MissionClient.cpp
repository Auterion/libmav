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

#include "../doctest.h"
#include "mav/opinionated_protocols/MissionClient.h"
#include "mav/TCPClient.h"
#include "mav/TCPServer.h"
#include "ProtocolTestSequencer.h"

using namespace mav;
#define PORT 13975


TEST_CASE("Mission protocol client") {
    MessageSet message_set;
    message_set.addFromXMLString(R""""(
<mavlink>
    <enums>
        <enum name="MAV_FRAME">
            <entry value="5" name="MAV_FRAME_GLOBAL_INT"></entry>
        </enum>
        <enum name="MAV_MISSION_TYPE">
            <description>Type of mission items being requested/sent in mission protocol.</description>
            <entry value="0" name="MAV_MISSION_TYPE_MISSION">
                <description>Items are mission commands for main mission.</description>
            </entry>
        </enum>
        <enum name="MAV_MISSION_RESULT">
          <description>Result of mission operation (in a MISSION_ACK message).</description>
          <entry value="0" name="MAV_MISSION_ACCEPTED">
            <description>mission accepted OK</description>
          </entry>
          <entry value="1" name="MAV_MISSION_ERROR">
            <description>Generic error / not accepting mission commands at all right now.</description>
          </entry>
          <entry value="2" name="MAV_MISSION_UNSUPPORTED_FRAME">
            <description>Coordinate frame is not supported.</description>
          </entry>
          <entry value="3" name="MAV_MISSION_UNSUPPORTED">
            <description>Command is not supported.</description>
          </entry>
          <entry value="4" name="MAV_MISSION_NO_SPACE">
            <description>Mission items exceed storage space.</description>
          </entry>
          <entry value="5" name="MAV_MISSION_INVALID">
            <description>One of the parameters has an invalid value.</description>
          </entry>
          <entry value="6" name="MAV_MISSION_INVALID_PARAM1">
            <description>param1 has an invalid value.</description>
          </entry>
          <entry value="7" name="MAV_MISSION_INVALID_PARAM2">
            <description>param2 has an invalid value.</description>
          </entry>
          <entry value="8" name="MAV_MISSION_INVALID_PARAM3">
            <description>param3 has an invalid value.</description>
          </entry>
          <entry value="9" name="MAV_MISSION_INVALID_PARAM4">
            <description>param4 has an invalid value.</description>
          </entry>
          <entry value="10" name="MAV_MISSION_INVALID_PARAM5_X">
            <description>x / param5 has an invalid value.</description>
          </entry>
          <entry value="11" name="MAV_MISSION_INVALID_PARAM6_Y">
            <description>y / param6 has an invalid value.</description>
          </entry>
          <entry value="12" name="MAV_MISSION_INVALID_PARAM7">
            <description>z / param7 has an invalid value.</description>
          </entry>
          <entry value="13" name="MAV_MISSION_INVALID_SEQUENCE">
            <description>Mission item received out of sequence</description>
          </entry>
          <entry value="14" name="MAV_MISSION_DENIED">
            <description>Not accepting any mission commands from this communication partner.</description>
          </entry>
          <entry value="15" name="MAV_MISSION_OPERATION_CANCELLED">
            <description>Current mission operation cancelled (e.g. mission upload, mission download).</description>
          </entry>
        </enum>

        <enum name="MAV_CMD">
          <description>Commands to be executed by the MAV. They can be executed on user request, or as part of a mission script. If the action is used in a mission, the parameter mapping to the waypoint/mission message is as follows: Param 1, Param 2, Param 3, Param 4, X: Param 5, Y:Param 6, Z:Param 7. This command list is similar what ARINC 424 is for commercial aircraft: A data format how to interpret waypoint/mission data. NaN and INT32_MAX may be used in float/integer params (respectively) to indicate optional/default values (e.g. to use the component's current yaw or latitude rather than a specific value). See https://mavlink.io/en/guide/xml_schema.html#MAV_CMD for information about the structure of the MAV_CMD entries</description>
          <entry value="16" name="MAV_CMD_NAV_WAYPOINT" hasLocation="true" isDestination="true">
            <description>Navigate to waypoint.</description>
            <param index="1" label="Hold" units="s" minValue="0">Hold time. (ignored by fixed wing, time to stay at waypoint for rotary wing)</param>
            <param index="2" label="Accept Radius" units="m" minValue="0">Acceptance radius (if the sphere with this radius is hit, the waypoint counts as reached)</param>
            <param index="3" label="Pass Radius" units="m">0 to pass through the WP, if &gt; 0 radius to pass by WP. Positive value for clockwise orbit, negative value for counter-clockwise orbit. Allows trajectory control.</param>
            <param index="4" label="Yaw" units="deg">Desired yaw angle at waypoint (rotary wing). NaN to use the current system yaw heading mode (e.g. yaw towards next waypoint, yaw to home, etc.).</param>
            <param index="5" label="Latitude">Latitude</param>
            <param index="6" label="Longitude">Longitude</param>
            <param index="7" label="Altitude" units="m">Altitude</param>
          </entry>
          <entry value="20" name="MAV_CMD_NAV_RETURN_TO_LAUNCH" hasLocation="false" isDestination="false">
            <description>Return to launch location</description>
            <param index="1">Empty</param>
            <param index="2">Empty</param>
            <param index="3">Empty</param>
            <param index="4">Empty</param>
            <param index="5">Empty</param>
            <param index="6">Empty</param>
            <param index="7">Empty</param>
          </entry>
          <entry value="21" name="MAV_CMD_NAV_LAND" hasLocation="true" isDestination="true">
            <description>Land at location.</description>
            <param index="1" label="Abort Alt" units="m">Minimum target altitude if landing is aborted (0 = undefined/use system default).</param>
            <param index="2" label="Land Mode" enum="PRECISION_LAND_MODE">Precision land mode.</param>
            <param index="3">Empty.</param>
            <param index="4" label="Yaw Angle" units="deg">Desired yaw angle. NaN to use the current system yaw heading mode (e.g. yaw towards next waypoint, yaw to home, etc.).</param>
            <param index="5" label="Latitude">Latitude.</param>
            <param index="6" label="Longitude">Longitude.</param>
            <param index="7" label="Altitude" units="m">Landing altitude (ground level in current frame).</param>
          </entry>
          <entry value="22" name="MAV_CMD_NAV_TAKEOFF" hasLocation="true" isDestination="true">
            <description>Takeoff from ground / hand. Vehicles that support multiple takeoff modes (e.g. VTOL quadplane) should take off using the currently configured mode.</description>
            <param index="1" label="Pitch" units="deg">Minimum pitch (if airspeed sensor present), desired pitch without sensor</param>
            <param index="2">Empty</param>
            <param index="3">Empty</param>
            <param index="4" label="Yaw" units="deg">Yaw angle (if magnetometer present), ignored without magnetometer. NaN to use the current system yaw heading mode (e.g. yaw towards next waypoint, yaw to home, etc.).</param>
            <param index="5" label="Latitude">Latitude</param>
            <param index="6" label="Longitude">Longitude</param>
            <param index="7" label="Altitude" units="m">Altitude</param>
          </entry>
        </enum>
    </enums>
    <messages>
        <message id="0" name="HEARTBEAT">
            <field type="uint8_t" name="type">Type of the MAV (quadrotor, helicopter, etc., up to 15 types, defined in MAV_TYPE ENUM)</field>
            <field type="uint8_t" name="autopilot">Autopilot type / class. defined in MAV_AUTOPILOT ENUM</field>
            <field type="uint8_t" name="base_mode">System mode bitfield, see MAV_MODE_FLAGS ENUM in mavlink/include/mavlink_types.h</field>
            <field type="uint32_t" name="custom_mode">A bitfield for use for autopilot-specific flags.</field>
            <field type="uint8_t" name="system_status">System status flag, see MAV_STATE ENUM</field>
            <field type="uint8_t" name="mavlink_version">MAVLink version, not writable by user, gets added by protocol because of magic data type: uint8_t_mavlink_version</field>
        </message>
        <message id="43" name="MISSION_REQUEST_LIST">
          <description>Request the overall list of mission items from the system/component.</description>
          <field type="uint8_t" name="target_system">System ID</field>
          <field type="uint8_t" name="target_component">Component ID</field>
          <extensions/>
          <field type="uint8_t" name="mission_type" enum="MAV_MISSION_TYPE">Mission type.</field>
        </message>
        <message id="44" name="MISSION_COUNT">
          <description>This message is emitted as response to MISSION_REQUEST_LIST by the MAV and to initiate a write transaction. The GCS can then request the individual mission item based on the knowledge of the total number of waypoints.</description>
          <field type="uint8_t" name="target_system">System ID</field>
          <field type="uint8_t" name="target_component">Component ID</field>
          <field type="uint16_t" name="count">Number of mission items in the sequence</field>
          <extensions/>
          <field type="uint8_t" name="mission_type" enum="MAV_MISSION_TYPE">Mission type.</field>
        </message>
        <message id="51" name="MISSION_REQUEST_INT">
          <description>Request the information of the mission item with the sequence number seq. The response of the system to this message should be a MISSION_ITEM_INT message. https://mavlink.io/en/services/mission.html</description>
          <field type="uint8_t" name="target_system">System ID</field>
          <field type="uint8_t" name="target_component">Component ID</field>
          <field type="uint16_t" name="seq">Sequence</field>
          <extensions/>
          <field type="uint8_t" name="mission_type" enum="MAV_MISSION_TYPE">Mission type.</field>
        </message>
        <message id="73" name="MISSION_ITEM_INT">
          <description>Message encoding a mission item. This message is emitted to announce
                    the presence of a mission item and to set a mission item on the system. The mission item can be either in x, y, z meters (type: LOCAL) or x:lat, y:lon, z:altitude. Local frame is Z-down, right handed (NED), global frame is Z-up, right handed (ENU). NaN or INT32_MAX may be used in float/integer params (respectively) to indicate optional/default values (e.g. to use the component's current latitude, yaw rather than a specific value). See also https://mavlink.io/en/services/mission.html.</description>
          <field type="uint8_t" name="target_system">System ID</field>
          <field type="uint8_t" name="target_component">Component ID</field>
          <field type="uint16_t" name="seq">Waypoint ID (sequence number). Starts at zero. Increases monotonically for each waypoint, no gaps in the sequence (0,1,2,3,4).</field>
          <field type="uint8_t" name="frame" enum="MAV_FRAME">The coordinate system of the waypoint.</field>
          <field type="uint16_t" name="command" enum="MAV_CMD">The scheduled action for the waypoint.</field>
          <field type="uint8_t" name="current">false:0, true:1</field>
          <field type="uint8_t" name="autocontinue">Autocontinue to next waypoint. 0: false, 1: true. Set false to pause mission after the item completes.</field>
          <field type="float" name="param1">PARAM1, see MAV_CMD enum</field>
          <field type="float" name="param2">PARAM2, see MAV_CMD enum</field>
          <field type="float" name="param3">PARAM3, see MAV_CMD enum</field>
          <field type="float" name="param4">PARAM4, see MAV_CMD enum</field>
          <field type="int32_t" name="x">PARAM5 / local: x position in meters * 1e4, global: latitude in degrees * 10^7</field>
          <field type="int32_t" name="y">PARAM6 / y position: local: x position in meters * 1e4, global: longitude in degrees *10^7</field>
          <field type="float" name="z">PARAM7 / z position: global: altitude in meters (relative or absolute, depending on frame.</field>
          <extensions/>
          <field type="uint8_t" name="mission_type" enum="MAV_MISSION_TYPE">Mission type.</field>
        </message>
        <message id="47" name="MISSION_ACK">
          <description>Acknowledgment message during waypoint handling. The type field states if this message is a positive ack (type=0) or if an error happened (type=non-zero).</description>
          <field type="uint8_t" name="target_system">System ID</field>
          <field type="uint8_t" name="target_component">Component ID</field>
          <field type="uint8_t" name="type" enum="MAV_MISSION_RESULT">Mission result.</field>
          <extensions/>
          <field type="uint8_t" name="mission_type" enum="MAV_MISSION_TYPE">Mission type.</field>
        </message>
    </messages>
</mavlink>
)"""");

    mav::TCPServer server_physical(PORT);
    mav::NetworkRuntime server_runtime({1, 1}, message_set, server_physical);

    std::promise<void> connection_called_promise;
    auto connection_called_future = connection_called_promise.get_future();
    server_runtime.onConnection([&connection_called_promise](const std::shared_ptr<mav::Connection>&) {
        connection_called_promise.set_value();
    });

    // setup client
    auto heartbeat = message_set.create("HEARTBEAT")({
                                                             {"type", 1},
                                                             {"autopilot", 2},
                                                             {"base_mode", 3},
                                                             {"custom_mode", 4},
                                                             {"system_status", 5},
                                                             {"mavlink_version", 6}});

    mav::TCPClient client_physical("localhost", PORT);
    mav::NetworkRuntime client_runtime(message_set, heartbeat, client_physical);

    CHECK((connection_called_future.wait_for(std::chrono::seconds(2)) != std::future_status::timeout));

    auto server_connection = server_runtime.awaitConnection(100);
    server_connection->send(heartbeat);

    auto client_connection = client_runtime.awaitConnection(100);

    std::vector<mav::Message> dummy_mission_short {
            mav::mission::TakeoffMessage(message_set).altitude_m(10),
    };

    std::vector<mav::Message> dummy_mission_long {
            mav::mission::TakeoffMessage(message_set).altitude_m(10),
            mav::mission::WaypointMessage(message_set).latitude_deg(47.397742).longitude_deg(8.545594).altitude_m(10),
            mav::mission::LandMessage(message_set).altitude_m(10)
    };




    SUBCASE("Can upload mission") {

        // mock the server response
        ProtocolTestSequencer server_sequence(server_connection);
        server_sequence
            .in("MISSION_COUNT", [](const mav::Message &msg) {
                CHECK_EQ(msg["count"].as<int>(), 3);
            })
            .out(message_set.create("MISSION_REQUEST_INT")({
                {"seq", 0},
                {"mission_type", message_set.e("MAV_MISSION_TYPE_MISSION")},
                {"target_system", LIBMAV_DEFAULT_ID},
                {"target_component", LIBMAV_DEFAULT_ID}
            }))
            .in("MISSION_ITEM_INT", [](const mav::Message &msg) {
                CHECK_EQ(msg["seq"].as<int>(), 0);
                CHECK_EQ(msg["mission_type"].as<int>(), 0);
            })
            .out(message_set.create("MISSION_REQUEST_INT")({
                {"seq", 1},
                {"mission_type", message_set.e("MAV_MISSION_TYPE_MISSION")},
                {"target_system", LIBMAV_DEFAULT_ID},
                {"target_component", LIBMAV_DEFAULT_ID}
            }))
            .in("MISSION_ITEM_INT", [](const mav::Message &msg) {
                CHECK_EQ(msg["seq"].as<int>(), 1);
                CHECK_EQ(msg["mission_type"].as<int>(), 0);
            })
            .out(message_set.create("MISSION_REQUEST_INT")({
                {"seq", 2},
                {"mission_type", message_set.e("MAV_MISSION_TYPE_MISSION")},
                {"target_system", LIBMAV_DEFAULT_ID},
                {"target_component", LIBMAV_DEFAULT_ID}
            }))
            .in("MISSION_ITEM_INT", [](const mav::Message &msg) {
                CHECK_EQ(msg["seq"].as<int>(), 2);
                CHECK_EQ(msg["mission_type"].as<int>(), 0);
            })
            .out(message_set.create("MISSION_ACK")({
                {"type", message_set.e("MAV_MISSION_ACCEPTED")},
                {"mission_type", message_set.e("MAV_MISSION_TYPE_MISSION")},
                {"target_system", LIBMAV_DEFAULT_ID},
                {"target_component", LIBMAV_DEFAULT_ID}
            }));

        server_sequence.start();
        mav::mission::MissionClient mission_client(client_connection, message_set);
        mission_client.upload(dummy_mission_long);
        server_sequence.finish();
    }

    SUBCASE("Can download mission") {

        // mock the server response
        ProtocolTestSequencer server_sequence(server_connection);
        server_sequence
            .in("MISSION_REQUEST_LIST")
            .out(message_set.create("MISSION_COUNT")({
                {"count", 3},
                {"mission_type", message_set.e("MAV_MISSION_TYPE_MISSION")},
                {"target_system", LIBMAV_DEFAULT_ID},
                {"target_component", LIBMAV_DEFAULT_ID}
            }))
            .in("MISSION_REQUEST_INT", [](auto &msg) {
                CHECK_EQ(msg.template get<int>("seq"), 0);
            })
            .out(message_set.create("MISSION_ITEM_INT")({
                {"seq", 0},
                {"frame", message_set.e("MAV_FRAME_GLOBAL_INT")},
                {"command", message_set.e("MAV_CMD_NAV_TAKEOFF")},
                {"current", 0},
                {"autocontinue", 1},
                {"param1", 0},
                {"param2", 0},
                {"param3", 0},
                {"param4", 0},
                {"x", 0},
                {"y", 0},
                {"z", 10},
                {"mission_type", message_set.e("MAV_MISSION_TYPE_MISSION")},
                {"target_system", LIBMAV_DEFAULT_ID},
                {"target_component", LIBMAV_DEFAULT_ID}
            }))
            .in("MISSION_REQUEST_INT", [](auto &msg) {
                CHECK_EQ(msg.template get<int>("seq"), 1);
            })
            .out(message_set.create("MISSION_ITEM_INT")({
                {"seq", 1},
                {"frame", message_set.e("MAV_FRAME_GLOBAL_INT")},
                {"command", message_set.e("MAV_CMD_NAV_WAYPOINT")},
                {"current", 0},
                {"autocontinue", 1},
                {"param1", 0},
                {"param2", 0},
                {"param3", 0},
                {"param4", 0},
                {"x", 47.397742 * 1e7},
                {"y", 8.545594 * 1e7},
                {"z", 10},
                {"mission_type", message_set.e("MAV_MISSION_TYPE_MISSION")},
                {"target_system", LIBMAV_DEFAULT_ID},
                {"target_component", LIBMAV_DEFAULT_ID}
            }))
            .in("MISSION_REQUEST_INT", [](auto &msg) {
                CHECK_EQ(msg.template get<int>("seq"), 2);
            })
            .out(message_set.create("MISSION_ITEM_INT")({
                {"seq", 2},
                {"frame", message_set.e("MAV_FRAME_GLOBAL_INT")},
                {"command", message_set.e("MAV_CMD_NAV_LAND")},
                {"current", 0},
                {"autocontinue", 1},
                {"param1", 0},
                {"param2", 0},
                {"param3", 0},
                {"param4", 0},
                {"x", 0},
                {"y", 0},
                {"z", 10},
                {"mission_type", message_set.e("MAV_MISSION_TYPE_MISSION")},
                {"target_system", LIBMAV_DEFAULT_ID},
                {"target_component", LIBMAV_DEFAULT_ID}
            })).in("MISSION_ACK", [&message_set](auto &msg) {
                CHECK_EQ(msg.template get<int>("type"), message_set.e("MAV_MISSION_ACCEPTED"));
            });

        mav::mission::MissionClient mission_client(client_connection, message_set);
        server_sequence.start();
        auto mission = mission_client.download();
        server_sequence.finish();

        CHECK_EQ(mission.size(), 3);
        CHECK_EQ(mission[0].name(), "MISSION_ITEM_INT");
        CHECK_EQ(mission[0]["command"].as<uint64_t>(), message_set.e("MAV_CMD_NAV_TAKEOFF"));
        CHECK_EQ(mission[1].name(), "MISSION_ITEM_INT");
        CHECK_EQ(mission[1]["command"].as<uint64_t>(), message_set.e("MAV_CMD_NAV_WAYPOINT"));
        CHECK_EQ(mission[2].name(), "MISSION_ITEM_INT");
        CHECK_EQ(mission[2]["command"].as<uint64_t>(), message_set.e("MAV_CMD_NAV_LAND"));
    }

    SUBCASE("Throws on receiving NACK on upload MISSION_COUNT") {
        ProtocolTestSequencer server_sequence(server_connection);
        server_sequence.in("MISSION_COUNT", [](auto &msg) {
            CHECK_EQ(msg.template get<int>("count"), 3);
        }).out(message_set.create("MISSION_ACK")({
            {"type", message_set.e("MAV_MISSION_ERROR")},
            {"mission_type", message_set.e("MAV_MISSION_TYPE_MISSION")},
            {"target_system", LIBMAV_DEFAULT_ID},
            {"target_component", LIBMAV_DEFAULT_ID}
        }));

        mav::mission::MissionClient mission_client(client_connection, message_set);
        server_sequence.start();
        CHECK_THROWS_AS(mission_client.upload(dummy_mission_long), mav::ProtocolException);
        server_sequence.finish();
    }

    SUBCASE("Throws on receiving NACK on upload MISSION_ITEM") {
        ProtocolTestSequencer server_sequence(server_connection);
        server_sequence
        .in("MISSION_COUNT", [](auto &msg) {
            CHECK_EQ(msg.template get<int>("count"), 3);
        })
        .out(message_set.create("MISSION_REQUEST_INT")({
            {"seq", 0},
            {"mission_type", message_set.e("MAV_MISSION_TYPE_MISSION")},
            {"target_system", LIBMAV_DEFAULT_ID},
            {"target_component", LIBMAV_DEFAULT_ID}
        }))
        .in("MISSION_ITEM_INT", [](auto &msg) {
            CHECK_EQ(msg.template get<int>("seq"), 0);
        })
        .out(message_set.create("MISSION_ACK")({
            {"type", message_set.e("MAV_MISSION_ERROR")},
            {"mission_type", message_set.e("MAV_MISSION_TYPE_MISSION")},
            {"target_system", LIBMAV_DEFAULT_ID},
            {"target_component", LIBMAV_DEFAULT_ID}
        }));

        mav::mission::MissionClient mission_client(client_connection, message_set);
        server_sequence.start();
        CHECK_THROWS_AS(mission_client.upload(dummy_mission_long), mav::ProtocolException);
        server_sequence.finish();
    }

    SUBCASE("Throws on a timeout after not receiving responses on upload") {
        ProtocolTestSequencer server_sequence(server_connection);
        server_sequence
        .in("MISSION_COUNT", [](auto &msg) {
            CHECK_EQ(msg.template get<int>("count"), 3);
        })
        .out(message_set.create("MISSION_REQUEST_INT")({
            {"seq", 0},
            {"mission_type", message_set.e("MAV_MISSION_TYPE_MISSION")},
            {"target_system", LIBMAV_DEFAULT_ID},
            {"target_component", LIBMAV_DEFAULT_ID}
        }))
        .in("MISSION_ITEM_INT", [](auto &msg) {
            CHECK_EQ(msg.template get<int>("seq"), 0);
        });

        mav::mission::MissionClient mission_client(client_connection, message_set);
        server_sequence.start();
        CHECK_THROWS_AS(mission_client.upload(dummy_mission_long), mav::TimeoutException);
        server_sequence.finish();
    }

    SUBCASE("Re-tries 3 times when no response from server") {
        ProtocolTestSequencer server_sequence(server_connection);
        server_sequence
        .in("MISSION_COUNT")
        .in("MISSION_COUNT")
        .in("MISSION_COUNT")
        .out(message_set.create("MISSION_REQUEST_INT")({
            {"seq", 0},
            {"mission_type", message_set.e("MAV_MISSION_TYPE_MISSION")},
            {"target_system", LIBMAV_DEFAULT_ID},
            {"target_component", LIBMAV_DEFAULT_ID}
        }))
        .in("MISSION_ITEM_INT")
        .in("MISSION_ITEM_INT")
        .in("MISSION_ITEM_INT")
        .out(message_set.create("MISSION_ACK")({
            {"type", message_set.e("MAV_MISSION_ACCEPTED")},
            {"mission_type", message_set.e("MAV_MISSION_TYPE_MISSION")},
            {"target_system", LIBMAV_DEFAULT_ID},
            {"target_component", LIBMAV_DEFAULT_ID}
        }));

        mav::mission::MissionClient mission_client(client_connection, message_set);
        server_sequence.start();
        mission_client.upload(dummy_mission_short);
        server_sequence.finish();
    }

    SUBCASE("Throws on early-accept of mission 1") {
        ProtocolTestSequencer server_sequence(server_connection);
        server_sequence
        .in("MISSION_COUNT", [](auto &msg) {
            CHECK_EQ(msg.template get<int>("count"), 3);
        })
        .out(message_set.create("MISSION_ACK")({
            {"type", message_set.e("MAV_MISSION_ACCEPTED")},
            {"mission_type", message_set.e("MAV_MISSION_TYPE_MISSION")},
            {"target_system", LIBMAV_DEFAULT_ID},
            {"target_component", LIBMAV_DEFAULT_ID}
        }));

        mav::mission::MissionClient mission_client(client_connection, message_set);
        server_sequence.start();
        CHECK_THROWS_AS(mission_client.upload(dummy_mission_long), mav::ProtocolException);
        server_sequence.finish();
    }

    SUBCASE("Throws on early-accept of mission 2") {
            ProtocolTestSequencer server_sequence(server_connection);
        server_sequence
        .in("MISSION_COUNT", [](auto &msg) {
            CHECK_EQ(msg.template get<int>("count"), 3);
        })
        .out(message_set.create("MISSION_REQUEST_INT")({
            {"seq", 0},
            {"mission_type", message_set.e("MAV_MISSION_TYPE_MISSION")},
            {"target_system", LIBMAV_DEFAULT_ID},
            {"target_component", LIBMAV_DEFAULT_ID}
        }))
        .in("MISSION_ITEM_INT", [](auto &msg) {
            CHECK_EQ(msg.template get<int>("seq"), 0);
        })
        .out(message_set.create("MISSION_ACK")({
            {"type", message_set.e("MAV_MISSION_ACCEPTED")},
            {"mission_type", message_set.e("MAV_MISSION_TYPE_MISSION")},
            {"target_system", LIBMAV_DEFAULT_ID},
            {"target_component", LIBMAV_DEFAULT_ID}
        }));

        mav::mission::MissionClient mission_client(client_connection, message_set);
        server_sequence.start();
        CHECK_THROWS_AS(mission_client.upload(dummy_mission_long), mav::ProtocolException);
        server_sequence.finish();
    }

    SUBCASE("Happy to re-upload missed items") {
        ProtocolTestSequencer server_sequence(server_connection);
        server_sequence
        .in("MISSION_COUNT", [](auto &msg) {
            CHECK_EQ(msg.template get<int>("count"), 3);
        })
        .out(message_set.create("MISSION_REQUEST_INT")({
            {"seq", 0},
            {"mission_type", message_set.e("MAV_MISSION_TYPE_MISSION")},
            {"target_system", LIBMAV_DEFAULT_ID},
            {"target_component", LIBMAV_DEFAULT_ID}
        }))
        .in("MISSION_ITEM_INT", [](auto &msg) {
            CHECK_EQ(msg.template get<int>("seq"), 0);
        })
        .out(message_set.create("MISSION_REQUEST_INT")({
            {"seq", 0},
            {"mission_type", message_set.e("MAV_MISSION_TYPE_MISSION")},
            {"target_system", LIBMAV_DEFAULT_ID},
            {"target_component", LIBMAV_DEFAULT_ID}
        }))
        .in("MISSION_ITEM_INT", [](auto &msg) {
            CHECK_EQ(msg.template get<int>("seq"), 0);
        }).out(message_set.create("MISSION_REQUEST_INT")({
            {"seq", 1},
            {"mission_type", message_set.e("MAV_MISSION_TYPE_MISSION")},
            {"target_system", LIBMAV_DEFAULT_ID},
            {"target_component", LIBMAV_DEFAULT_ID}
        })).in("MISSION_ITEM_INT", [](auto &msg) {
            CHECK_EQ(msg.template get<int>("seq"), 1);
        }).out(message_set.create("MISSION_REQUEST_INT")({
            {"seq", 2},
            {"mission_type", message_set.e("MAV_MISSION_TYPE_MISSION")},
            {"target_system", LIBMAV_DEFAULT_ID},
            {"target_component", LIBMAV_DEFAULT_ID}
        })).in("MISSION_ITEM_INT", [](auto &msg) {
            CHECK_EQ(msg.template get<int>("seq"), 2);
        }).out(message_set.create("MISSION_REQUEST_INT")({
            {"seq", 2},
            {"mission_type", message_set.e("MAV_MISSION_TYPE_MISSION")},
            {"target_system", LIBMAV_DEFAULT_ID},
            {"target_component", LIBMAV_DEFAULT_ID}
        })).in("MISSION_ITEM_INT", [](auto &msg) {
            CHECK_EQ(msg.template get<int>("seq"), 2);
        }).out(message_set.create("MISSION_ACK")({
            {"type", message_set.e("MAV_MISSION_ACCEPTED")},
            {"mission_type", message_set.e("MAV_MISSION_TYPE_MISSION")},
            {"target_system", LIBMAV_DEFAULT_ID},
            {"target_component", LIBMAV_DEFAULT_ID}
        }));

        mav::mission::MissionClient mission_client(client_connection, message_set);
        server_sequence.start();
        mission_client.upload(dummy_mission_long);
        server_sequence.finish();
    }

    SUBCASE("Throws on NACK on download mission count") {
        ProtocolTestSequencer server_sequence(server_connection);
        server_sequence
        .in("MISSION_REQUEST_LIST")
        .out(message_set.create("MISSION_ACK")({
            {"type", message_set.e("MAV_MISSION_ERROR")},
            {"mission_type", message_set.e("MAV_MISSION_TYPE_MISSION")},
            {"target_system", LIBMAV_DEFAULT_ID},
            {"target_component", LIBMAV_DEFAULT_ID}
        }));

        mav::mission::MissionClient mission_client(client_connection, message_set);
        server_sequence.start();
        CHECK_THROWS_AS(mission_client.download(), mav::ProtocolException);
        server_sequence.finish();
    }

    SUBCASE("Throws on NACK on download mission request item") {
        ProtocolTestSequencer server_sequence(server_connection);
        server_sequence
        .in("MISSION_REQUEST_LIST")
        .out(message_set.create("MISSION_COUNT")({
            {"count", 3},
            {"mission_type", message_set.e("MAV_MISSION_TYPE_MISSION")},
            {"target_system", LIBMAV_DEFAULT_ID},
            {"target_component", LIBMAV_DEFAULT_ID}
        }))
        .in("MISSION_REQUEST_INT", [](auto &msg) {
            CHECK_EQ(msg.template get<int>("seq"), 0);
        })
        .out(message_set.create("MISSION_ACK")({
            {"type", message_set.e("MAV_MISSION_ERROR")},
            {"mission_type", message_set.e("MAV_MISSION_TYPE_MISSION")},
            {"target_system", LIBMAV_DEFAULT_ID},
            {"target_component", LIBMAV_DEFAULT_ID}
        }));

        mav::mission::MissionClient mission_client(client_connection, message_set);
        server_sequence.start();
        CHECK_THROWS_AS(mission_client.download(), mav::ProtocolException);
        server_sequence.finish();
    }


    SUBCASE("Download times out if no response") {
        ProtocolTestSequencer server_sequence(server_connection);
        server_sequence
        .in("MISSION_REQUEST_LIST")
        .out(message_set.create("MISSION_COUNT")({
            {"count", 3},
            {"mission_type", message_set.e("MAV_MISSION_TYPE_MISSION")},
            {"target_system", LIBMAV_DEFAULT_ID},
            {"target_component", LIBMAV_DEFAULT_ID}
        }))
        .in("MISSION_REQUEST_INT", [](auto &msg) {
            CHECK_EQ(msg.template get<int>("seq"), 0);
        });

        mav::mission::MissionClient mission_client(client_connection, message_set);
        server_sequence.start();
        CHECK_THROWS_AS(mission_client.download(), mav::TimeoutException);
        server_sequence.finish();
    }

    SUBCASE("Download tries three times to download an item before giving up") {
        ProtocolTestSequencer server_sequence(server_connection);
        server_sequence
        .in("MISSION_REQUEST_LIST")
        .out(message_set.create("MISSION_COUNT")({
            {"count", 1},
            {"mission_type", message_set.e("MAV_MISSION_TYPE_MISSION")},
            {"target_system", LIBMAV_DEFAULT_ID},
            {"target_component", LIBMAV_DEFAULT_ID}
        }))
        .in("MISSION_REQUEST_INT")
        .in("MISSION_REQUEST_INT")
        .in("MISSION_REQUEST_INT")
        .out(message_set.create("MISSION_ITEM_INT")({
            {"seq", 0},
            {"frame", message_set.e("MAV_FRAME_GLOBAL_INT")},
            {"command", message_set.e("MAV_CMD_NAV_WAYPOINT")},
            {"autocontinue", 1},
            {"mission_type", message_set.e("MAV_MISSION_TYPE_MISSION")},
            {"target_system", LIBMAV_DEFAULT_ID},
            {"target_component", LIBMAV_DEFAULT_ID}
        }))
        .in("MISSION_ACK");

        mav::mission::MissionClient mission_client(client_connection, message_set);
        server_sequence.start();
        auto mission = mission_client.download();
        server_sequence.finish();
        CHECK_EQ(mission.size(), 1);
    }

}

