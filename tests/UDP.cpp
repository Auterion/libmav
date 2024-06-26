/****************************************************************************
 *
 * Copyright (c) 2024, libmav development team
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

#include <future>
#include "doctest.h"
#include "mav/Message.h"
#include "mav/Network.h"
#include "mav/UDPClient.h"
#include "mav/UDPServer.h"

using namespace mav;


TEST_CASE("UDP server client") {
    MessageSet message_set;
    message_set.addFromXMLString(R"(
        <mavlink>
            <messages>
                <message id="0" name="HEARTBEAT">
                    <field type="uint8_t" name="type">Type of the MAV (quadrotor, helicopter, etc., up to 15 types, defined in MAV_TYPE ENUM)</field>
                    <field type="uint8_t" name="autopilot">Autopilot type / class. defined in MAV_AUTOPILOT ENUM</field>
                    <field type="uint8_t" name="base_mode">System mode bitfield, see MAV_MODE_FLAGS ENUM in mavlink/include/mavlink_types.h</field>
                    <field type="uint32_t" name="custom_mode">A bitfield for use for autopilot-specific flags.</field>
                    <field type="uint8_t" name="system_status">System status flag, see MAV_STATE ENUM</field>
                    <field type="uint8_t" name="mavlink_version">MAVLink version, not writable by user, gets added by protocol because of magic data type: uint8_t_mavlink_version</field>
                </message>
            </messages>
        </mavlink>
    )");
    message_set.addFromXMLString(R""""(
    <mavlink>
        <messages>
            <message id="9915" name="BIG_MESSAGE">
                <field type="uint8_t" name="uint8_field">description</field>
                <field type="int8_t" name="int8_field">description</field>
                <field type="uint16_t" name="uint16_field">description</field>
                <field type="int16_t" name="int16_field">description</field>
                <field type="uint32_t" name="uint32_field">description</field>
                <field type="int32_t" name="int32_field">description</field>
                <field type="uint64_t" name="uint64_field">description</field>
                <field type="int64_t" name="int64_field">description</field>
                <field type="double" name="double_field">description</field>
                <field type="float" name="float_field">description</field>
                <field type="char[20]" name="char_arr_field">description</field>
                <field type="float[3]" name="float_arr_field">description</field>
                <field type="int32_t[3]" name="int32_arr_field">description</field>
                <extensions/>
                <field type="uint8_t" name="extension_uint8_field">description</field>
            </message>
        </messages>
    </mavlink>
    )"""");

    REQUIRE(message_set.contains("BIG_MESSAGE"));
    REQUIRE_EQ(message_set.size(), 2);

    SUBCASE("Can connect server client UDP") {
        // setup server
        mav::UDPServer server_physical(19334);
        mav::NetworkRuntime server_runtime(message_set, server_physical);

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

        mav::UDPClient client_physical("127.0.0.1", 19334);
        mav::NetworkRuntime client_runtime(message_set, heartbeat, client_physical);

        CHECK((connection_called_future.wait_for(std::chrono::seconds(2)) != std::future_status::timeout));

        auto server = server_runtime.awaitConnection(100);
        server->send(heartbeat);

        auto client = client_runtime.awaitConnection(100);

        SUBCASE("Can send message from server to client with UDP") {
            auto client_expectation = client->expect("BIG_MESSAGE");
            server->send(message_set.create("BIG_MESSAGE")({
                                                                   {"uint8_field", 1},
                                                                   {"int8_field", 2},
                                                                   {"uint16_field", 3},
                                                                   {"int16_field", 4},
                                                                   {"uint32_field", 5},
                                                                   {"int32_field", 6},
                                                                   {"uint64_field", 7},
                                                                   {"int64_field", 8},
                                                                   {"double_field", 9},
                                                                   {"float_field", 10},
                                                                   {"char_arr_field", "hello client"},
                                                                   {"float_arr_field", std::vector<float>{1, 2, 3}},
                                                                   {"int32_arr_field", std::vector<int32_t>{4, 5, 6}},
                                                                   {"extension_uint8_field", 7},
                                                           }));
            auto client_message = client->receive(client_expectation, 100);

            CHECK_EQ(client_message["char_arr_field"].as<std::string>(), "hello client");
        }

        SUBCASE("Can send message from client to server with UDP") {
            auto server_expectation = server->expect("BIG_MESSAGE");
            client->send(message_set.create("BIG_MESSAGE")({
                                                                   {"uint8_field", 1},
                                                                   {"int8_field", 2},
                                                                   {"uint16_field", 3},
                                                                   {"int16_field", 4},
                                                                   {"uint32_field", 5},
                                                                   {"int32_field", 6},
                                                                   {"uint64_field", 7},
                                                                   {"int64_field", 8},
                                                                   {"double_field", 9},
                                                                   {"float_field", 10},
                                                                   {"char_arr_field", "hello server"},
                                                                   {"float_arr_field", std::vector<float>{1, 2, 3}},
                                                                   {"int32_arr_field", std::vector<int32_t>{4, 5, 6}},
                                                                   {"extension_uint8_field", 7},
                                                           }));
            auto server_message = server->receive(server_expectation, 100);
            CHECK_EQ(server_message["char_arr_field"].as<std::string>(), "hello server");
        }

    }

    SUBCASE("Connection is recycled when client disconnects but connection object stays referenced") {
        // setup server
        mav::UDPServer server_physical(19334);
        mav::NetworkRuntime server_runtime(message_set, server_physical);

        std::promise<void> connection_called_promise;
        auto connection_called_future = connection_called_promise.get_future();
        server_runtime.onConnection([&connection_called_promise](const std::shared_ptr<mav::Connection> &connection) {
            (void)connection;
            connection_called_promise.set_value();
        });

        // setup client
        auto heartbeat = message_set.create("HEARTBEAT")({
                                                                 {"type",            1},
                                                                 {"autopilot",       2},
                                                                 {"base_mode",       3},
                                                                 {"custom_mode",     4},
                                                                 {"system_status",   5},
                                                                 {"mavlink_version", 6}});
        mav::UDPClient client_physical("127.0.0.1", 19334);
        mav::NetworkRuntime client_runtime(message_set, heartbeat, client_physical);

        CHECK((connection_called_future.wait_for(std::chrono::seconds(2)) != std::future_status::timeout));

        auto server_connection = server_runtime.awaitConnection(100);
        server_connection->send(heartbeat);
        auto client_connection = client_runtime.awaitConnection(100);

        std::promise<void> connection_dropped_promise;
        auto connection_dropped_future = connection_dropped_promise.get_future();
        client_runtime.onConnectionLost([&connection_dropped_promise](const std::shared_ptr<mav::Connection> &connection) {
            (void)connection;
            connection_dropped_promise.set_value();
        });

        // kill server
        server_physical.stop();
        server_runtime.stop();

        // make the client connection drop
        std::this_thread::sleep_for(std::chrono::seconds(5));
        CHECK_FALSE(client_connection->alive());

        // make sure the connection dropped callback is called
        CHECK((connection_dropped_future.wait_for(std::chrono::seconds(2)) != std::future_status::timeout));
        client_runtime.onConnectionLost(nullptr);

        // setup new server
        mav::UDPServer server_physical2(19334);
        mav::NetworkRuntime server_runtime2(message_set, server_physical2);

        auto server2_connection = server_runtime2.awaitConnection(1100);
        server2_connection->send(heartbeat);

        auto client_connection2 = client_runtime.awaitConnection(100);
        CHECK_EQ(client_connection, client_connection2);

        // can still use expectation created on client_connection to receive on client_connection2, as they are the same
        CHECK(client_connection->alive());
    }

    SUBCASE("Connection is not recycled when client disconnects and reference drops") {
        // setup server
        mav::UDPServer server_physical(19334);
        mav::NetworkRuntime server_runtime(message_set, server_physical);

        std::promise<void> connection_called_promise;
        auto connection_called_future = connection_called_promise.get_future();
        server_runtime.onConnection([&connection_called_promise](const std::shared_ptr<mav::Connection> &connection) {
            (void)connection;
            connection_called_promise.set_value();
        });

        // setup client
        auto heartbeat = message_set.create("HEARTBEAT")({
                                                                 {"type",            1},
                                                                 {"autopilot",       2},
                                                                 {"base_mode",       3},
                                                                 {"custom_mode",     4},
                                                                 {"system_status",   5},
                                                                 {"mavlink_version", 6}});

        mav::UDPClient client_physical("127.0.0.1", 19334);
        mav::NetworkRuntime client_runtime(message_set, heartbeat, client_physical);

        CHECK((connection_called_future.wait_for(std::chrono::seconds(2)) != std::future_status::timeout));

        auto server_connection = server_runtime.awaitConnection(100);
        server_connection->send(heartbeat);

        std::weak_ptr<Connection> old_connection;
        mav::Connection::Expectation expectation;
        {
            auto client_connection = client_runtime.awaitConnection(100);
            expectation = client_connection->expect("BIG_MESSAGE");

            std::promise<void> connection_dropped_promise;
            auto connection_dropped_future = connection_dropped_promise.get_future();
            client_runtime.onConnectionLost([&connection_dropped_promise](const std::shared_ptr<mav::Connection> &connection) {
                (void)connection;
                connection_dropped_promise.set_value();
            });

            // kill server
            server_physical.stop();
            server_runtime.stop();

            // make the client connection drop
            std::this_thread::sleep_for(std::chrono::seconds(5));
            CHECK_FALSE(client_connection->alive());

            // make sure the connection dropped callback is called
            CHECK((connection_dropped_future.wait_for(std::chrono::seconds(2)) != std::future_status::timeout));
            client_runtime.onConnectionLost(nullptr);

            old_connection = std::weak_ptr<Connection>(client_connection);
            // client connection drops out of scope here. Should not be recycled.
        }


        // setup new server
        mav::UDPServer server_physical2(19334);
        mav::NetworkRuntime server_runtime2(message_set, server_physical2);

        auto server2_connection = server_runtime2.awaitConnection(1100);
        server2_connection->send(heartbeat);

        auto client_connection2 = client_runtime.awaitConnection(100);

        CHECK_NE(old_connection.lock(), client_connection2);

        CHECK(client_connection2->alive());
    }
}

