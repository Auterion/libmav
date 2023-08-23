//
// Created by thomas on 01.02.23.
//


#include "doctest.h"
#include "mav/Message.h"
#include "mav/Network.h"

using namespace mav;
using namespace std::string_literals;

class DummyInterface : public NetworkInterface {
private:
    std::string receive_queue;
    ConnectionPartner next_receive_partner;
    std::mutex receive_queue_mutex;
    mutable std::condition_variable receive_queue_cv;
    std::string send_sponge;
    ConnectionPartner last_send_partner;
    mutable std::atomic_bool should_interrupt{false};
    mutable std::atomic_bool should_fail{false};

public:

    void stop() const {
        should_interrupt.store(true);
        receive_queue_cv.notify_all();
    }

    void makeFailOnNextReceive() const {
        should_fail.store(true);
        receive_queue_cv.notify_all();
    }

    void close() override {
        stop();
    }
    void send(const uint8_t *data, uint32_t size, ConnectionPartner partner) override {
        send_sponge.append(reinterpret_cast<const char *>(data), size);
        last_send_partner = partner;
    }

    ConnectionPartner receive(uint8_t *destination, uint32_t size) override {
        std::unique_lock<std::mutex> lock(receive_queue_mutex);
        if (receive_queue.size() < size) {
            receive_queue_cv.wait(lock, [this, size] {
                return receive_queue.size() >= size || should_interrupt.load() || should_fail.load(); });
        }
        if (should_interrupt.load()) {
            throw NetworkInterfaceInterrupt();
        }
        if (should_fail.load()) {
            throw NetworkError("Failed to receive data");
        }
        std::copy(receive_queue.begin(), receive_queue.begin() + size, destination);
        receive_queue.erase(0, size);
        return next_receive_partner;
    }

    virtual ~DummyInterface() {
        stop();
    }

    void flushSendSponge() {
        send_sponge.clear();
    }

    void flushReceiveQueue() {
        std::unique_lock<std::mutex> lock(receive_queue_mutex);
        receive_queue.clear();
    }

    void reset() {
        flushSendSponge();
        flushReceiveQueue();
        should_fail.store(false);
    }

    void addToReceiveQueue(const std::string &data, const ConnectionPartner &partner) {
        std::unique_lock<std::mutex> lock(receive_queue_mutex);
        receive_queue.append(data);
        next_receive_partner = partner;
        lock.unlock();
        receive_queue_cv.notify_one();
    }

    bool sendSpongeContains(const std::string &data, const ConnectionPartner &partner) const {
        return send_sponge.find(data) != std::string::npos && last_send_partner == partner;
    }
};


TEST_CASE("Create network runtime") {

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
                <message id="9916" name="TEST_MESSAGE">
                    <field type="int32_t" name="value">description</field>
                    <field type="char[20]" name="text">description</field>
                </message>
            </messages>
        </mavlink>
    )");

    REQUIRE(message_set.contains("HEARTBEAT"));
    REQUIRE_EQ(message_set.size(), 2);

    ConnectionPartner interface_partner = {0x0a290101, 14550, false};
    ConnectionPartner interface_wrong_partner = {0x0a290103, 14550, false};

    DummyInterface interface;
    NetworkRuntime network({253, 1}, message_set, interface);

    // send a heartbeat message, to establish a connection
    interface.addToReceiveQueue("\xfd\x09\x00\x00\x00\xfd\x01\x00\x00\x00\x04\x00\x00\x00\x01\x02\x03\x05\x06\x77\x53"s, interface_partner);
    auto connection = network.awaitConnection();


    SUBCASE("Can send message") {
        interface.reset();
        auto message = message_set.create("TEST_MESSAGE")({
          {"value", 42},
          {"text", "Hello World!"}
        });
        connection->send(message);
        bool found = (interface.sendSpongeContains(
                "\xfd\x10\x00\x00\x00\xfd\x01\xbc\x26\x00\x2a\x00\x00\x00\x48\x65\x6c\x6c\x6f\x20\x57\x6f\x72\x6c\x64\x21\x86\x37"s, interface_partner));
        CHECK(found);
    }

    SUBCASE("Can receive message") {
        interface.reset();
        auto expectation = connection->expect("TEST_MESSAGE");
        interface.addToReceiveQueue("\xfd\x10\x00\x00\x01\x61\x61\xbc\x26\x00\x2a\x00\x00\x00\x48\x65\x6c\x6c\x6f\x20\x57\x6f\x72\x6c\x64\x21\x53\xd9"s, interface_partner);
        auto message = connection->receive(expectation);
        CHECK_EQ(message.name(), "TEST_MESSAGE");
        CHECK_EQ(message.get<int>("value"), 42);
        CHECK_EQ(message.get<std::string>("text"), "Hello World!");
    }


    SUBCASE("Message sent twice before receive") {
        interface.reset();
        auto expectation = connection->expect("TEST_MESSAGE");
        interface.addToReceiveQueue("\xfd\x10\x00\x00\x01\x61\x61\xbc\x26\x00\x2a\x00\x00\x00\x48\x65\x6c\x6c\x6f\x20\x57\x6f\x72\x6c\x64\x21\x53\xd9"s, interface_partner);
        interface.addToReceiveQueue("\xfd\x10\x00\x00\x01\x61\x61\xbc\x26\x00\x2a\x00\x00\x00\x48\x65\x6c\x6c\x6f\x20\x57\x6f\x72\x6c\x64\x21\x53\xd9"s, interface_partner);
        auto message = connection->receive(expectation);
        CHECK_EQ(message.name(), "TEST_MESSAGE");
    }

    SUBCASE("Can not receive message from wrong partner") {
        interface.reset();
        auto expectation = connection->expect("TEST_MESSAGE");
        interface.addToReceiveQueue("\xfd\x10\x00\x00\x01\x61\x61\xbc\x26\x00\x2a\x00\x00\x00\x48\x65\x6c\x6c\x6f\x20\x57\x6f\x72\x6c\x64\x21\x53\xd9"s, interface_wrong_partner);
        CHECK_THROWS_AS(auto message = connection->receive(expectation, 100), TimeoutException);
    }

    SUBCASE("Can not receive message with CRC error") {
        interface.reset();
        auto expectation = connection->expect("TEST_MESSAGE");
        interface.addToReceiveQueue("\xfd\x10\x00\x00\x01\x61\x61\xbc\x26\x00\x2a\x00\x00\x00\x48\x65\x6c\x6c\x6f\x20\x57\x6f\x72\x6c\x64\x21\x53\xda"s, interface_partner);
        CHECK_THROWS_AS(auto message = connection->receive(expectation, 100), TimeoutException);
    }

    SUBCASE("Can not receive message with unknown message ID") {
        interface.reset();
        auto expectation = connection->expect(9912);
        interface.addToReceiveQueue("\xfd\x04\x00\x00\x00\x01\x01\xb8\x26\x00\xcd\xcc\x54\x41\x59\x8e"s, interface_partner);
        CHECK_THROWS_AS(auto message = connection->receive(expectation, 100), TimeoutException);
    }

    SUBCASE("Receive throws a NetworkError if the interface fails") {
        interface.reset();
        auto expectation = connection->expect("TEST_MESSAGE");
        interface.makeFailOnNextReceive();
        CHECK_THROWS_AS(auto message = connection->receive(expectation), NetworkError);
    }

    SUBCASE("Connection recycled on recover after fail") {
        interface.reset();
        auto expectation = connection->expect("HEARTBEAT");
        interface.makeFailOnNextReceive();
        CHECK_THROWS_AS(auto message = connection->receive(expectation), NetworkError);
        CHECK_FALSE(connection->alive());
        interface.reset();
        expectation = connection->expect("HEARTBEAT");
        interface.addToReceiveQueue("\xfd\x09\x00\x00\x00\xfd\x01\x00\x00\x00\x04\x00\x00\x00\x01\x02\x03\x05\x06\x77\x53"s, interface_partner);
        auto message = connection->receive(expectation);
        CHECK_EQ(message.name(), "HEARTBEAT");
        CHECK(connection->alive());
    }
}
