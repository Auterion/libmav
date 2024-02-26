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
#include "mav/opinionated_protocols/ParamClient.h"
#include "mav/TCPClient.h"
#include "mav/TCPServer.h"
#include "ProtocolTestSequencer.h"

using namespace mav;
#define PORT 13975


TEST_CASE("Param client") {
    MessageSet message_set;
    message_set.addFromXMLString(R""""(
<mavlink>
    <enums>
        <enum name="MAV_PARAM_TYPE">
            <description>Specifies the datatype of a MAVLink parameter.</description>
            <entry value="1" name="MAV_PARAM_TYPE_UINT8">
                <description>8-bit unsigned integer</description>
            </entry>
            <entry value="2" name="MAV_PARAM_TYPE_INT8">
                <description>8-bit signed integer</description>
            </entry>
            <entry value="3" name="MAV_PARAM_TYPE_UINT16">
                <description>16-bit unsigned integer</description>
            </entry>
            <entry value="4" name="MAV_PARAM_TYPE_INT16">
                <description>16-bit signed integer</description>
            </entry>
            <entry value="5" name="MAV_PARAM_TYPE_UINT32">
                <description>32-bit unsigned integer</description>
            </entry>
            <entry value="6" name="MAV_PARAM_TYPE_INT32">
                <description>32-bit signed integer</description>
            </entry>
            <entry value="7" name="MAV_PARAM_TYPE_UINT64">
                <description>64-bit unsigned integer</description>
            </entry>
            <entry value="8" name="MAV_PARAM_TYPE_INT64">
                <description>64-bit signed integer</description>
            </entry>
            <entry value="9" name="MAV_PARAM_TYPE_REAL32">
                <description>32-bit floating-point</description>
            </entry>
            <entry value="10" name="MAV_PARAM_TYPE_REAL64">
                <description>64-bit floating-point</description>
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
        <message id="20" name="PARAM_REQUEST_READ">
            <description>Request to read the onboard parameter with the param_id string id. Onboard parameters are stored as key[const char*] -&gt; value[float]. This allows to send a parameter to any other component (such as the GCS) without the need of previous knowledge of possible parameter names. Thus the same GCS can store different parameters for different autopilots. See also https://mavlink.io/en/services/parameter.html for a full documentation of QGroundControl and IMU code.</description>
            <field type="uint8_t" name="target_system">System ID</field>
            <field type="uint8_t" name="target_component">Component ID</field>
            <field type="char[16]" name="param_id">Onboard parameter id, terminated by NULL if the length is less than 16 human-readable chars and WITHOUT null termination (NULL) byte if the length is exactly 16 chars - applications have to provide 16+1 bytes storage if the ID is stored as string</field>
            <field type="int16_t" name="param_index" invalid="-1">Parameter index. Send -1 to use the param ID field as identifier (else the param id will be ignored)</field>
        </message>
        <message id="21" name="PARAM_REQUEST_LIST">
            <description>Request all parameters of this component. After this request, all parameters are emitted. The parameter microservice is documented at https://mavlink.io/en/services/parameter.html</description>
            <field type="uint8_t" name="target_system">System ID</field>
            <field type="uint8_t" name="target_component">Component ID</field>
        </message>
        <message id="22" name="PARAM_VALUE">
            <description>Emit the value of a onboard parameter. The inclusion of param_count and param_index in the message allows the recipient to keep track of received parameters and allows him to re-request missing parameters after a loss or timeout. The parameter microservice is documented at https://mavlink.io/en/services/parameter.html</description>
            <field type="char[16]" name="param_id">Onboard parameter id, terminated by NULL if the length is less than 16 human-readable chars and WITHOUT null termination (NULL) byte if the length is exactly 16 chars - applications have to provide 16+1 bytes storage if the ID is stored as string</field>
            <field type="float" name="param_value">Onboard parameter value</field>
            <field type="uint8_t" name="param_type" enum="MAV_PARAM_TYPE">Onboard parameter type.</field>
            <field type="uint16_t" name="param_count">Total number of onboard parameters</field>
            <field type="uint16_t" name="param_index">Index of this onboard parameter</field>
        </message>
        <message id="23" name="PARAM_SET">
        <description>Set a parameter value (write new value to permanent storage).
            The receiving component should acknowledge the new parameter value by broadcasting a PARAM_VALUE message (broadcasting ensures that multiple GCS all have an up-to-date list of all parameters). If the sending GCS did not receive a PARAM_VALUE within its timeout time, it should re-send the PARAM_SET message. The parameter microservice is documented at https://mavlink.io/en/services/parameter.html.
            PARAM_SET may also be called within the context of a transaction (started with MAV_CMD_PARAM_TRANSACTION). Within a transaction the receiving component should respond with PARAM_ACK_TRANSACTION to the setter component (instead of broadcasting PARAM_VALUE), and PARAM_SET should be re-sent if this is ACK not received.</description>
            <field type="uint8_t" name="target_system">System ID</field>
            <field type="uint8_t" name="target_component">Component ID</field>
            <field type="char[16]" name="param_id">Onboard parameter id, terminated by NULL if the length is less than 16 human-readable chars and WITHOUT null termination (NULL) byte if the length is exactly 16 chars - applications have to provide 16+1 bytes storage if the ID is stored as string</field>
            <field type="float" name="param_value">Onboard parameter value</field>
            <field type="uint8_t" name="param_type" enum="MAV_PARAM_TYPE">Onboard parameter type.</field>
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


    SUBCASE("Can read float param") {

        // mock the server response
        ProtocolTestSequencer server_sequence(server_connection);
        server_sequence
                .in("PARAM_REQUEST_READ", [](const mav::Message &msg) {
                    CHECK_EQ(msg["param_index"].as<int>(), -1);
                    CHECK_EQ(msg["param_id"].as<std::string>(), "DUMMY_PARAM");
                })
                .out(message_set.create("PARAM_VALUE")({
                                                                       {"param_id", "DUMMY_PARAM"},
                                                                       {"param_value", 3.14f},
                                                                       {"param_type", message_set.e("MAV_PARAM_TYPE_REAL32")},
                                                                       {"param_count", 1},
                                                                       {"param_index", 0}
                                                               }));

        server_sequence.start();
        mav::param::ParamClient param_client(client_connection, message_set);
        auto param = param_client.read("DUMMY_PARAM");
        server_sequence.finish();
        CHECK_EQ(std::holds_alternative<float>(param), true);
        CHECK_EQ(std::get<float>(param), 3.14f);
    }


    SUBCASE("Can read int param") {
        // mock the server response
        ProtocolTestSequencer server_sequence(server_connection);
        server_sequence
                .in("PARAM_REQUEST_READ", [](const mav::Message &msg) {
                    CHECK_EQ(msg["param_index"].as<int>(), -1);
                    CHECK_EQ(msg["param_id"].as<std::string>(), "DUMMY_PARAM");
                })
                .out(message_set.create("PARAM_VALUE")({
                                                                       {"param_id", "DUMMY_PARAM"},
                                                                       {"param_value", mav::floatPack(42)},
                                                                       {"param_type", message_set.e("MAV_PARAM_TYPE_INT32")},
                                                                       {"param_count", 1},
                                                                       {"param_index", 0}
                                                               }));

        server_sequence.start();
        mav::param::ParamClient param_client(client_connection, message_set);
        auto param = param_client.read("DUMMY_PARAM");
        server_sequence.finish();
        CHECK_EQ(std::holds_alternative<int>(param), true);
        CHECK_EQ(std::get<int>(param), 42);
    }


    SUBCASE("Throws on receiving wrong param") {
        // mock the server response
        ProtocolTestSequencer server_sequence(server_connection);
        server_sequence
                .in("PARAM_REQUEST_READ", [](const mav::Message &msg) {
                    CHECK_EQ(msg["param_index"].as<int>(), -1);
                    CHECK_EQ(msg["param_id"].as<std::string>(), "DUMMY_PARAM");
                })
                .out(message_set.create("PARAM_VALUE")({
                                                                       {"param_id", "WRONG_PARAM"},
                                                                       {"param_value", 3.14f},
                                                                       {"param_type", message_set.e("MAV_PARAM_TYPE_REAL32")},
                                                                       {"param_count", 1},
                                                                       {"param_index", 0}
                                                               }));

        server_sequence.start();
        mav::param::ParamClient param_client(client_connection, message_set);
        CHECK_THROWS_AS(param_client.read("DUMMY_PARAM"), mav::ProtocolException);
        server_sequence.finish();
    }

    SUBCASE("Read tries 3 times to obtain the parameter") {
            // mock the server response
        ProtocolTestSequencer server_sequence(server_connection);
        server_sequence
                .in("PARAM_REQUEST_READ", [](const mav::Message &msg) {
                    CHECK_EQ(msg["param_index"].as<int>(), -1);
                    CHECK_EQ(msg["param_id"].as<std::string>(), "DUMMY_PARAM");
                })
                .in("PARAM_REQUEST_READ", [](const mav::Message &msg) {
                    CHECK_EQ(msg["param_index"].as<int>(), -1);
                    CHECK_EQ(msg["param_id"].as<std::string>(), "DUMMY_PARAM");
                })
                .in("PARAM_REQUEST_READ", [](const mav::Message &msg) {
                    CHECK_EQ(msg["param_index"].as<int>(), -1);
                    CHECK_EQ(msg["param_id"].as<std::string>(), "DUMMY_PARAM");
                })
                .out(message_set.create("PARAM_VALUE")({
                                                                       {"param_id", "DUMMY_PARAM"},
                                                                       {"param_value", 3.14f},
                                                                       {"param_type", message_set.e("MAV_PARAM_TYPE_REAL32")},
                                                                       {"param_count", 1},
                                                                       {"param_index", 0}
                                                               }));

        server_sequence.start();
        mav::param::ParamClient param_client(client_connection, message_set);
        auto param = param_client.read("DUMMY_PARAM");
        server_sequence.finish();
        CHECK_EQ(std::holds_alternative<float>(param), true);
        CHECK_EQ(std::get<float>(param), 3.14f);
    }

    SUBCASE("Read times out if no response") {
            // mock the server response
        ProtocolTestSequencer server_sequence(server_connection);
        server_sequence
                .in("PARAM_REQUEST_READ", [](const mav::Message &msg) {
                    CHECK_EQ(msg["param_index"].as<int>(), -1);
                    CHECK_EQ(msg["param_id"].as<std::string>(), "DUMMY_PARAM");
                });

        server_sequence.start();
        mav::param::ParamClient param_client(client_connection, message_set);
        CHECK_THROWS_AS(param_client.read("DUMMY_PARAM"), mav::TimeoutException);
        server_sequence.finish();
    }


    SUBCASE("Can write float param") {
        // mock the server response
        ProtocolTestSequencer server_sequence(server_connection);
        server_sequence
                .in("PARAM_SET", [&message_set](const mav::Message &msg) {
                    CHECK_EQ(msg["param_id"].as<std::string>(), "DUMMY_PARAM");
                    CHECK_EQ(msg["param_type"].as<int>(), message_set.e("MAV_PARAM_TYPE_REAL32"));
                    CHECK_EQ(msg["param_value"].as<float>(), 3.14f);
                })
                .out(message_set.create("PARAM_VALUE")({
                                                                       {"param_id", "DUMMY_PARAM"},
                                                                       {"param_value", 3.14f},
                                                                       {"param_type", message_set.e("MAV_PARAM_TYPE_REAL32")},
                                                                       {"param_count", 1},
                                                                       {"param_index", 0}
                                                               }));

        server_sequence.start();
        mav::param::ParamClient param_client(client_connection, message_set);
        param_client.write("DUMMY_PARAM", 3.14f);
        server_sequence.finish();
    }

    SUBCASE("Can write int param") {
            // mock the server response
        ProtocolTestSequencer server_sequence(server_connection);
        server_sequence
                .in("PARAM_SET", [&message_set](const mav::Message &msg) {
                    CHECK_EQ(msg["param_id"].as<std::string>(), "DUMMY_PARAM");
                    CHECK_EQ(msg["param_type"].as<int>(), message_set.e("MAV_PARAM_TYPE_INT32"));
                    CHECK_EQ(msg["param_value"].floatUnpack<int>(), 42);
                })
                .out(message_set.create("PARAM_VALUE")({
                                                                       {"param_id", "DUMMY_PARAM"},
                                                                       {"param_value", mav::floatPack(42)},
                                                                       {"param_type", message_set.e("MAV_PARAM_TYPE_INT32")},
                                                                       {"param_count", 1},
                                                                       {"param_index", 0}
                                                               }));

        server_sequence.start();
        mav::param::ParamClient param_client(client_connection, message_set);
        param_client.write("DUMMY_PARAM", 42);
        server_sequence.finish(); }
}


