//
// Created by thomas on 24.02.23.
//
#include <cstdint>
#include <future>
#include <chrono>
#include <thread>
#include "doctest.h"
#include "mav/Message.h"
#include "mav/Network.h"
#include "mav/DirectIO.h"

using namespace mav;


TEST_CASE("Direct IO") {
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

    SUBCASE("Can connect server client") {

        // setup client
        auto heartbeat = message_set.create("HEARTBEAT")({
            {"type", 1},
            {"autopilot", 2},
            {"base_mode", 3},
            {"custom_mode", 4},
            {"system_status", 5},
            {"mavlink_version", 6}});

        // setup server
        uint8_t seq = 0;

        mav::Identifier identifier{42, 99};
        mav::DirectIO server_physical(
            nullptr,
            [&](uint8_t* buf, uint32_t len) {
                if (seq > 0) {
                    std::cout << "done" <<std::endl;
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    return static_cast<ssize_t>(0);
                }

                ssize_t size = heartbeat.finalize(seq++, identifier);
                if (size <= len) {
                    std::cout << "memcpy" <<std::endl;
                    std::memcpy(buf, heartbeat.data(), size);
                    return size;
                } else {
                    return static_cast<ssize_t>(-1);
                }
            });
        mav::NetworkRuntime server_runtime(message_set, server_physical);

        std::array<uint8_t, 1024> buffer;
        auto received_message = server_physical.receive(buffer.data(), buffer.size());
        // TODO: never reached
        std::cout << "Received!" << std::endl;
    }
}

