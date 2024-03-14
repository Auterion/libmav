//
// Created by thomas on 01.02.23.
//
#include <filesystem>
#include "doctest.h"
#include "mav/MessageSet.h"
#include "minimal.h"
#include <cstdio>
#include <fstream>

using namespace mav;

std::string dump_minimal_xml() {
    // write temporary file
    auto temp_path = std::filesystem::temp_directory_path();
    auto xml_file = temp_path / "minimal.xml";
    std::ofstream out(xml_file);
    out << minimal_xml;
    out.close();
    return xml_file;
}

TEST_CASE("Message set creation") {

    auto file_name = dump_minimal_xml();
    MessageSet message_set{file_name};
    std::remove(file_name.c_str());

    CHECK_EQ(message_set.size(), 2);

    SUBCASE("Can not parse incomplete XML") {
        // This is not a valid XML file
        CHECK_THROWS_AS(message_set.addFromXMLString(""), mav::ParseError);
        // XML file is missing the closing tag
        CHECK_THROWS_AS(message_set.addFromXMLString("<mavlink>"), mav::ParseError);
    }

    SUBCASE("Can not add message with unknown field type") {
        CHECK_THROWS_AS(message_set.addFromXMLString(R""""(
<mavlink>
    <messages>
        <message id="15321" name="ONLY_MESSAGE">
            <field type="unknown" name="field">Field</field>
        </message>
    </messages>
</mavlink>
)""""), mav::ParseError);

    }

    SUBCASE("Can add valid, partial XML") {
        // This is a valid XML file, but it does not contain any messages or enums
        message_set.addFromXMLString("<mavlink></mavlink>");
        // only messages
        message_set.addFromXMLString(R""""(
<mavlink>
    <messages>
        <message id="15321" name="ONLY_MESSAGE">
            <field type="uint8_t" name="field">Field</field>
        </message>
    </messages>
</mavlink>
)"""");
        CHECK(message_set.contains("ONLY_MESSAGE"));
        auto message = message_set.create("ONLY_MESSAGE");
        CHECK_EQ(message.id(), 15321);
        CHECK_EQ(message.name(), "ONLY_MESSAGE");

        // only enums
        message_set.addFromXMLString(R""""(
<mavlink>
    <enums>
        <enum name="MY_ENUM">
            <entry value="1" name="BIT0" />
        </enum>
    </enums>
</mavlink>
)"""");

        CHECK_EQ(message_set.e("BIT0"), 1);
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
            CHECK_EQ(message.id(), message_set.idForMessage("TEMPERATURE_MEASUREMENT"));
            CHECK_EQ(message.name(), "TEMPERATURE_MEASUREMENT");
        }
    }
}

TEST_CASE("Create messages from set") {
    auto file_name = dump_minimal_xml();
    MessageSet message_set{file_name};
    std::remove(file_name.c_str());

    REQUIRE(message_set.contains("HEARTBEAT"));
    SUBCASE("Can create message from name") {
        auto message = message_set.create("HEARTBEAT");
        CHECK_EQ(message.id(), message_set.idForMessage("HEARTBEAT"));
        CHECK_EQ(message.name(), "HEARTBEAT");
    }

    SUBCASE("Can create message from id") {
        int id = message_set.idForMessage("PROTOCOL_VERSION");
        CHECK_EQ(id, 300);
        auto message = message_set.create(id);
        CHECK_EQ(message.id(), id);
        CHECK_EQ(message.name(), "PROTOCOL_VERSION");
    }

    SUBCASE("Can not get id for invalid message") {
        CHECK_THROWS_AS((void) message_set.idForMessage("NOT_A_MESSAGE"), std::out_of_range);
    }

    SUBCASE("Can not get invalid message definition") {
        auto definition = message_set.getMessageDefinition("NOT_A_MESSAGE");
        CHECK_EQ(definition, false);
        definition = message_set.getMessageDefinition(-1);
        CHECK_EQ(definition, false);
    }

    SUBCASE("Can get message definition from name") {
        auto definition = message_set.getMessageDefinition("HEARTBEAT");
        CHECK_NE(definition, false);
        CHECK_EQ(definition.get().name(), "HEARTBEAT");

        SUBCASE("Message definition contains fields") {
            CHECK(definition.get().containsField("type"));
            CHECK(definition.get().containsField("autopilot"));
            CHECK(definition.get().containsField("base_mode"));
            CHECK(definition.get().containsField("custom_mode"));
            CHECK(definition.get().containsField("system_status"));
        }
    }
}

TEST_CASE("Enum value encoding") {

    SUBCASE("Happy path enum encoding") {

        std::string happy_path = R""""(
<mavlink>
    <enums>
        <enum name="MY_ENUM_HAPPY_PATH">
            <entry value="1" name="BIT0" />
            <entry value="2**4" name="BIT4" />
            <entry value="0b000100000000" name="BIT8" />
            <entry value="0x10000" name="BIT16" />
            <entry value="0B1000000000000000000000000000000000000000000000000000000000000" name="BIT60" />
            <entry value="2305843009213693952" name="BIT61" />
            <entry value="2**62" name="BIT62" />
            <entry value="0X8000000000000000" name="BIT63" />
        </enum>
    </enums>
</mavlink>
)"""";

        MessageSet message_set;
        message_set.addFromXMLString(happy_path);
        CHECK_EQ(message_set.e("BIT0"), 1);
        CHECK_EQ(message_set.e("BIT4"), 16);
        CHECK_EQ(message_set.e("BIT8"), 256);
        CHECK_EQ(message_set.e("BIT16"), 65536);
        CHECK_EQ(message_set.e("BIT60"), 1152921504606846976ULL);
        CHECK_EQ(message_set.e("BIT61"), 2305843009213693952ULL);
        CHECK_EQ(message_set.e("BIT62"), 4611686018427387904ULL);
        CHECK_EQ(message_set.e("BIT63"), 9223372036854775808ULL);
    }

    SUBCASE("Enum with empty value") {
        std::string empty_value = R""""(
<mavlink>
    <enums>
        <enum name="MY_ENUM_EMPTY_VALUE">
            <entry value="" name="EMPTY" />
        </enum>
    </enums>
</mavlink>
)"""";

        MessageSet message_set;
        CHECK_THROWS_AS(message_set.addFromXMLString(empty_value), mav::ParseError);
    }

    SUBCASE("Enum with invalid value 1") {
        std::string invalid_value = R""""(
<mavlink>
    <enums>
        <enum name="MY_ENUM_INVALID_VALUE">
            <entry value="0x" name="INVALID" />
        </enum>
    </enums>
</mavlink>
)"""";

        MessageSet message_set;
        CHECK_THROWS_AS(message_set.addFromXMLString(invalid_value), mav::ParseError);
    }

    SUBCASE("Enum with invalid value 2") {
        std::string invalid_value = R""""(
<mavlink>
    <enums>
        <enum name="MY_ENUM_INVALID_VALUE">
            <entry value="thisiswrong" name="INVALID" />
        </enum>
    </enums>
</mavlink>
)"""";

        MessageSet message_set;
        CHECK_THROWS_AS(message_set.addFromXMLString(invalid_value), mav::ParseError);
    }

    SUBCASE("Enum with invalid value 3") {
        std::string invalid_value = R""""(
<mavlink>
    <enums>
        <enum name="MY_ENUM_INVALID_VALUE">
            <entry value="128thereismorecontent" name="INVALID" />
        </enum>
    </enums>
</mavlink>
)"""";

        MessageSet message_set;
        CHECK_THROWS_AS(message_set.addFromXMLString(invalid_value), mav::ParseError);
    }

    SUBCASE("Enum with overflow value") {
        std::string invalid_value = R""""(
<mavlink>
    <enums>
        <enum name="MY_ENUM_INVALID_VALUE">
            <entry value="2**123" name="INVALID" />
        </enum>
    </enums>
</mavlink>
)"""";

        MessageSet message_set;
        CHECK_THROWS_AS(message_set.addFromXMLString(invalid_value), mav::ParseError);
    }

    SUBCASE("Enum with non-base-2 exponential value") {
        std::string invalid_value = R""""(
<mavlink>
    <enums>
        <enum name="MY_ENUM_INVALID_VALUE">
            <entry value="3**3" name="INVALID" />
        </enum>
    </enums>
</mavlink>
)"""";

        MessageSet message_set;
        CHECK_THROWS_AS(message_set.addFromXMLString(invalid_value), mav::ParseError);
    }

}

