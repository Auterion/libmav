//
// Created by thomas on 01.02.23.
//
#include <filesystem>
#include "doctest.h"
#include "include/mav/MessageSet.h"

using namespace mav;

TEST_CASE("Message set creation") {
    MessageSet message_set{std::filesystem::current_path() / "tests/minimal.xml"};

    CHECK(message_set.size() == 2);

    SUBCASE("Can not parse incomplete XML") {
        CHECK_THROWS_AS(message_set.addFromXMLString(""), std::runtime_error);
        CHECK_THROWS_AS(message_set.addFromXMLString("<mavlink></mavlink>"), std::runtime_error);
    }

    SUBCASE("Set contains message") {
        CHECK(message_set.contains("HEARTBEAT"));
    }

    SUBCASE("Set contains message by id") {
        CHECK(message_set.contains(0));
    }

    SUBCASE("Set does not contain message") {
        CHECK_FALSE(message_set.contains("NOT_A_MESSAGE"));
    }


    SUBCASE("Can extend message set with inline XML") {
        message_set.addFromXMLString(R""""(
<mavlink>
    <messages>
        <message id="9912" name="TEMPERATURE_MEASUREMENT">
            <field type="float" name="temperature">The measured temperature in degress C</field>
        </message>
    </messages>
</mavlink>
)"""");
        CHECK(message_set.contains("TEMPERATURE_MEASUREMENT"));
        SUBCASE("Can create message from name") {
            auto message = message_set.create("TEMPERATURE_MEASUREMENT");
            CHECK(message.id() == message_set.idForMessage("TEMPERATURE_MEASUREMENT"));
            CHECK(message.name() == "TEMPERATURE_MEASUREMENT");
        }
    }
}

TEST_CASE("Create messages from set") {
    MessageSet message_set{std::filesystem::current_path() / "tests/minimal.xml"};

    REQUIRE(message_set.contains("HEARTBEAT"));
    SUBCASE("Can create message from name") {
        auto message = message_set.create("HEARTBEAT");
        CHECK(message.id() == message_set.idForMessage("HEARTBEAT"));
        CHECK(message.name() == "HEARTBEAT");
    }

    SUBCASE("Can create message from id") {
        int id = message_set.idForMessage("PROTOCOL_VERSION");
        CHECK(id == 300);
        auto message = message_set.create(id);
        CHECK(message.id() == id);
        CHECK(message.name() == "PROTOCOL_VERSION");
    }

    SUBCASE("Can not get id for invalid message") {
        CHECK_THROWS_AS(message_set.idForMessage("NOT_A_MESSAGE"), std::out_of_range);
    }

    SUBCASE("Can not get invalid message definition") {
        auto definition = message_set.getMessageDefinition("NOT_A_MESSAGE");
        CHECK(definition == false);
        definition = message_set.getMessageDefinition(-1);
        CHECK(definition == false);
    }

    SUBCASE("Can get message definition from name") {
        auto definition = message_set.getMessageDefinition("HEARTBEAT");
        CHECK(definition != false);
        CHECK(definition.get().name() == "HEARTBEAT");

        SUBCASE("Message definition contains fields") {
            CHECK(definition.get().containsField("type"));
            CHECK(definition.get().containsField("autopilot"));
            CHECK(definition.get().containsField("base_mode"));
            CHECK(definition.get().containsField("custom_mode"));
            CHECK(definition.get().containsField("system_status"));
        }
    }

}

