//
// Created by thomas on 24.02.23.
//
#include <future>
#include "doctest.h"
#include "mav/Message.h"
#include "mav/Network.h"
#include "mav/TCPClient.h"
#include "mav/TCPServer.h"

using namespace mav;

#define PORT 13975

TEST_CASE("TCP server client") {
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
            <message id="9916" name="TEST_MESSAGE">
                <field type="char[25]" name="message">description</field>
            </message>
        </messages>
    </mavlink>
    )"""");

    REQUIRE(message_set.contains("TEST_MESSAGE"));
    REQUIRE_EQ(message_set.size(), 2);

    SUBCASE("Can connect TCP server client") {
        // setup server
        mav::TCPServer server_physical(PORT);
        mav::NetworkRuntime server_runtime(message_set, server_physical);

        std::promise<void> connection_called_promise;
        auto connection_called_future = connection_called_promise.get_future();
        server_runtime.onConnection([&connection_called_promise](const std::shared_ptr<mav::Connection> &connection) {
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

        mav::TCPClient client_physical("127.0.0.1", PORT);
        mav::NetworkRuntime client_runtime(message_set, heartbeat, client_physical);

        CHECK((connection_called_future.wait_for(std::chrono::seconds(2)) != std::future_status::timeout));

        auto server = server_runtime.awaitConnection(100);
        server->send(heartbeat);

        auto client = client_runtime.awaitConnection(100);

        SUBCASE("Can send message from server to client over TCP") {
            auto client_expectation = client->expect("TEST_MESSAGE");
            server->send(message_set.create("TEST_MESSAGE")({
                                                                   {"message", "hello client"}
                                                           }));
            auto client_message = client->receive(client_expectation, 100);
            std::string message = client_message["message"].as<std::string>();
            CHECK_EQ(message, "hello client");
        }

        SUBCASE("Can send message from client to server over TCP") {
            auto server_expectation = server->expect("TEST_MESSAGE");
            client->send(message_set.create("TEST_MESSAGE")({
                                                                   {"message", "hello server"},
                                                           }));
            auto server_message = server->receive(server_expectation, 100);
            CHECK_EQ(server_message["message"].as<std::string>(), "hello server");
        }


        SUBCASE("Can connect two TCP clients that both can send messages to server") {
            // setup client 2
            auto heartbeat2 = message_set.create("HEARTBEAT")({
                                                                  {"type", 1},
                                                                  {"autopilot", 2},
                                                                  {"base_mode", 3},
                                                                  {"custom_mode", 4},
                                                                  {"system_status", 5},
                                                                  {"mavlink_version", 6}});

            std::promise<std::shared_ptr<mav::Connection>> connection2_promise;
            server_runtime.onConnection([&heartbeat2, &connection2_promise](const std::shared_ptr<mav::Connection> &connection) {
                connection2_promise.set_value(connection);
            });

            mav::TCPClient client2_physical("127.0.0.1", PORT);
            mav::NetworkRuntime client2_runtime(message_set, heartbeat2, client2_physical);

            auto connection2_future = connection2_promise.get_future();
            connection2_future.wait_for(std::chrono::milliseconds (100));
            auto server2 = connection2_future.get();
            server2->send(heartbeat2);
            auto client2 = client2_runtime.awaitConnection(100);

            auto server_expectation1 = server->expect("TEST_MESSAGE");
            auto server_expectation2 = server2->expect("TEST_MESSAGE");

            client->send(message_set.create("TEST_MESSAGE")({
                                                                    {"message", "hello from client 1"},
                                                            }));
            client2->send(message_set.create("TEST_MESSAGE")({
                                                                    {"message", "hello from client 2"},
                                                            }));

            auto server_message1 = server->receive(server_expectation1, 100);
            auto server_message2 = server2->receive(server_expectation2, 100);

            CHECK_NE(server_message1.source(), server_message2.source());
            CHECK_EQ(server_message1["message"].as<std::string>(), "hello from client 1");
            CHECK_EQ(server_message2["message"].as<std::string>(), "hello from client 2");
        }

    }
}

