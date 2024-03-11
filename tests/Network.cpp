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

uint64_t getTimestamp() {
    return 770479200;
}

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

    std::array<uint8_t, 32> key;
    for (int i = 0 ; i < 32; i++) key[i] = i;

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

    SUBCASE("Receiver re-synchronizes when garbage data between messages") {
        interface.reset();
        auto expectation_1 = connection->expect("TEST_MESSAGE");
        auto expectation_2 = connection->expect("TEST_MESSAGE");
        interface.addToReceiveQueue("\xfd\x10\x00\x00\x01\x61\x61\xbc\x26\x00\x2a\x00\x00\x00\x48\x65\x6c\x6c\x6f\x20\x57\x6f\x72\x6c\x64\x21\x53\xd9"s, interface_partner);
        interface.addToReceiveQueue("this is garbage data"s, interface_partner);
        interface.addToReceiveQueue("\xfd\x10\x00\x00\x01\x61\x61\xbc\x26\x00\x2a\x00\x00\x00\x48\x65\x6c\x6c\x6f\x20\x57\x6f\x72\x6c\x64\x21\x53\xd9"s, interface_partner);
        auto message_1 = connection->receive(expectation_1);
        CHECK_EQ(message_1.name(), "TEST_MESSAGE");
        auto message_2 = connection->receive(expectation_2);
        CHECK_EQ(message_2.name(), "TEST_MESSAGE");
    }

    SUBCASE("Can not receive message from wrong partner") {
        interface.reset();
        auto expectation = connection->expect("TEST_MESSAGE");
        interface.addToReceiveQueue("\xfd\x10\x00\x00\x01\x61\x61\xbc\x26\x00\x2a\x00\x00\x00\x48\x65\x6c\x6c\x6f\x20\x57\x6f\x72\x6c\x64\x21\x53\xd9"s, interface_wrong_partner);
        CHECK_THROWS_AS((void)connection->receive(expectation, 100), TimeoutException);
    }

    SUBCASE("Can not receive message with CRC error") {
        interface.reset();
        auto expectation = connection->expect("TEST_MESSAGE");
        interface.addToReceiveQueue("\xfd\x10\x00\x00\x01\x61\x61\xbc\x26\x00\x2a\x00\x00\x00\x48\x65\x6c\x6c\x6f\x20\x57\x6f\x72\x6c\x64\x21\x53\xda"s, interface_partner);
        CHECK_THROWS_AS((void)connection->receive(expectation, 100), TimeoutException);
    }

    SUBCASE("Can not receive message with unknown message ID") {
        interface.reset();
        auto expectation = connection->expect(9912);
        interface.addToReceiveQueue("\xfd\x04\x00\x00\x00\x01\x01\xb8\x26\x00\xcd\xcc\x54\x41\x59\x8e"s, interface_partner);
        CHECK_THROWS_AS((void)connection->receive(expectation, 100), TimeoutException);
    }

    SUBCASE("Receive throws a NetworkError if the interface fails, error callback gets called") {
        interface.reset();

        // add a callback using the callback API. The error should then call the error callback
        auto error_callback_called_promise = std::promise<void>();
        connection->addMessageCallback([](const Message &message) {
           // do nothing
           (void)message;
        }, [&error_callback_called_promise](const std::exception_ptr& exception) {
            error_callback_called_promise.set_value();
            CHECK_THROWS_AS(std::rethrow_exception(exception), NetworkError);
        });

        auto expectation = connection->expect("TEST_MESSAGE");
        interface.makeFailOnNextReceive();
        // Receive on the sync api. The receive should then throw an exception
        CHECK_THROWS_AS((void)connection->receive(expectation), NetworkError);
        CHECK((error_callback_called_promise.get_future().wait_for(std::chrono::seconds(2)) != std::future_status::timeout));
        connection->removeAllCallbacks();
    }

    SUBCASE("Connection recycled on recover after fail") {
        interface.reset();
        auto expectation = connection->expect("HEARTBEAT");
        interface.makeFailOnNextReceive();
        CHECK_THROWS_AS((void)connection->receive(expectation), NetworkError);
        CHECK_FALSE(connection->alive());
        interface.reset();
        expectation = connection->expect("HEARTBEAT");
        interface.addToReceiveQueue("\xfd\x09\x00\x00\x00\xfd\x01\x00\x00\x00\x04\x00\x00\x00\x01\x02\x03\x05\x06\x77\x53"s, interface_partner);
        auto message = connection->receive(expectation);
        CHECK_EQ(message.name(), "HEARTBEAT");
        CHECK(connection->alive());
    }

    SUBCASE("Enable message signing") {
        auto message = message_set.create("TEST_MESSAGE")({
          {"value", 42},
          {"text", "Hello World!"}
        });
        interface.reset();
        network.enableMessageSigning(key);
        connection->send(message);
        CHECK(message.header().isSigned());
        // don't check anything after link_id in signature as the timestamp is dependent on current time
        bool found = (interface.sendSpongeContains(
                "\xfd\x10\x01\x00\x00\xfd\x01\xbc\x26\x00\x2a\x00\x00\x00\x48\x65\x6c\x6c\x6f\x20\x57\x6f\x72\x6c\x64\x21\xfd\x33\x00"s,
                interface_partner));
        CHECK(found);
    }

    SUBCASE("Enable message signing with custom timestamp function") {
        auto message = message_set.create("TEST_MESSAGE")({
          {"value", 42},
          {"text", "Hello World!"}
        });
        interface.reset();
        network.enableMessageSigning(key, getTimestamp);
        connection->send(message);
        CHECK(message.header().isSigned());
        bool found = (interface.sendSpongeContains(
                "\xfd\x10\x01\x00\x00\xfd\x01\xbc\x26\x00\x2a\x00\x00\x00\x48\x65\x6c\x6c\x6f\x20\x57\x6f\x72\x6c\x64\x21\xfd\x33\x00\x60\x94\xec\x2d\x00\x00\x7b\xab\xfa\x1a\xed\xf9"s,
                interface_partner));
        CHECK(found);
    }

    SUBCASE("Disable message signing") {
        auto message = message_set.create("TEST_MESSAGE")({
          {"value", 42},
          {"text", "Hello World!"}
        });
        interface.reset();
        network.disableMessageSigning();
        connection->send(message);
        CHECK(!message.header().isSigned());
        bool found = (interface.sendSpongeContains(
                "\xfd\x10\x00\x00\x00\xfd\x01\xbc\x26\x00\x2a\x00\x00\x00\x48\x65\x6c\x6c\x6f\x20\x57\x6f\x72\x6c\x64\x21\x86\x37"s,
                interface_partner));
        CHECK(found);
    }

    SUBCASE("Correct callback called when message is received") {
        interface.reset();
        std::promise<void> callback_called_promise;
        auto callback_called_future = callback_called_promise.get_future();

        connection->addMessageCallback([&callback_called_promise](const Message &message) {
            if (message.name() == "TEST_MESSAGE") {
                callback_called_promise.set_value();
            }
        });

        interface.addToReceiveQueue("\xfd\x10\x00\x00\x01\x61\x61\xbc\x26\x00\x2a\x00\x00\x00\x48\x65\x6c\x6c\x6f\x20\x57\x6f\x72\x6c\x64\x21\x53\xd9"s, interface_partner);
        CHECK((callback_called_future.wait_for(std::chrono::seconds(2)) != std::future_status::timeout));
        connection->removeAllCallbacks();
    }

    SUBCASE("Callbacks are cleaned up on receive timeout") {
        interface.reset();
        for (int i = 0; i < 10; i++) {
            auto expectation = connection->expect("TEST_MESSAGE");
            CHECK_THROWS_AS((void)connection->receive(expectation, 100), TimeoutException);
        }
        // send a heartbeat. Any message will clear expired expectations
        interface.addToReceiveQueue("\xfd\x09\x00\x00\x00\xfd\x01\x00\x00\x00\x04\x00\x00\x00\x01\x02\x03\x05\x06\x77\x53"s, interface_partner);
        // wait for the heartbeat to be received, to make sure timing is correct in test
        connection->receive("HEARTBEAT");
        CHECK_EQ(connection->callbackCount(), 0);
    }
}
