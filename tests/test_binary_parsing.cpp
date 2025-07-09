/**
 * @file test_binary_parsing.cpp
 * @brief Tests for binary MAVLink message parsing functionality
 * 
 * This test file verifies that the binary parsing methods work correctly
 * and handle various error conditions properly.
 */

#include "doctest.h"
#include "mav/MessageSet.h"
#include "mav/Message.h"
#include <vector>
#include <cstring>

using namespace mav;

TEST_CASE("Binary Message Parsing - Basic functionality") {
    MessageSet message_set;
    message_set.addFromXMLString(R""""(
<mavlink>
    <messages>
        <message id="0" name="HEARTBEAT">
            <field type="uint8_t" name="type">Type of the system</field>
            <field type="uint8_t" name="autopilot">Autopilot type / class</field>
            <field type="uint8_t" name="base_mode">System mode bitmap</field>
            <field type="uint32_t" name="custom_mode">Custom mode</field>
            <field type="uint8_t" name="system_status">System status flag</field>
            <field type="uint8_t" name="mavlink_version">MAVLink version</field>
        </message>
        <message id="9915" name="TEST_MESSAGE">
            <field type="uint8_t" name="uint8_field">Test field</field>
            <field type="uint32_t" name="uint32_field">Test field</field>
            <field type="char[16]" name="string_field">String field</field>
        </message>
    </messages>
</mavlink>
)"""");

    SUBCASE("Parse valid HEARTBEAT message") {
        // Create a valid HEARTBEAT message using existing API
        auto original_message = message_set.create("HEARTBEAT");
        original_message.set("type", uint8_t{2});
        original_message.set("autopilot", uint8_t{0});
        original_message.set("base_mode", uint8_t{0});
        original_message.set("custom_mode", uint32_t{0});
        original_message.set("system_status", uint8_t{4});
        original_message.set("mavlink_version", uint8_t{2});
        
        Identifier sender{1, 1};
        auto length = original_message.finalize(0, sender);
        
        auto message_opt = message_set.tryParseMessage(original_message.data(), length);
        REQUIRE(message_opt.has_value());
        
        auto message = message_opt.value();
        CHECK(message.name() == "HEARTBEAT");
        CHECK(message.id() == 0);
        CHECK(message.isFinalized());
        
        // Check field values
        uint8_t type_val;
        CHECK(message.tryGet("type", type_val) == Message::GetResult::SUCCESS);
        CHECK(type_val == 2);
        
        uint8_t autopilot_val;
        CHECK(message.tryGet("autopilot", autopilot_val) == Message::GetResult::SUCCESS);
        CHECK(autopilot_val == 0);
        
        uint8_t mavlink_version_val;
        CHECK(message.tryGet("mavlink_version", mavlink_version_val) == Message::GetResult::SUCCESS);
        CHECK(mavlink_version_val == 2);
    }
    
    SUBCASE("Parse with throwing method") {
        // Create a valid HEARTBEAT message using existing API
        auto original_message = message_set.create("HEARTBEAT");
        original_message.set("type", uint8_t{2});
        original_message.set("autopilot", uint8_t{0});
        original_message.set("base_mode", uint8_t{0});
        original_message.set("custom_mode", uint32_t{0});
        original_message.set("system_status", uint8_t{4});
        original_message.set("mavlink_version", uint8_t{2});
        
        Identifier sender{1, 1};
        auto length = original_message.finalize(0, sender);
        
        auto message = message_set.parseMessage(original_message.data(), length);
        CHECK(message.name() == "HEARTBEAT");
        CHECK(message.id() == 0);
    }
    
    SUBCASE("Parse with ParseResult method") {
        // Create a valid HEARTBEAT message using existing API
        auto original_message = message_set.create("HEARTBEAT");
        original_message.set("type", uint8_t{2});
        original_message.set("autopilot", uint8_t{0});
        original_message.set("base_mode", uint8_t{0});
        original_message.set("custom_mode", uint32_t{0});
        original_message.set("system_status", uint8_t{4});
        original_message.set("mavlink_version", uint8_t{2});
        
        Identifier sender{1, 1};
        auto length = original_message.finalize(0, sender);
        
        Message message;
        auto result = message_set.tryParseMessage(original_message.data(), length, message);
        CHECK(result == MessageSet::ParseResult::SUCCESS);
        CHECK(message.name() == "HEARTBEAT");
        CHECK(message.id() == 0);
    }
}

TEST_CASE("Binary Message Parsing - Error handling") {
    MessageSet message_set;
    message_set.addFromXMLString(R""""(
<mavlink>
    <messages>
        <message id="0" name="HEARTBEAT">
            <field type="uint8_t" name="type">Type of the system</field>
            <field type="uint8_t" name="autopilot">Autopilot type / class</field>
            <field type="uint8_t" name="base_mode">System mode bitmap</field>
            <field type="uint32_t" name="custom_mode">Custom mode</field>
            <field type="uint8_t" name="system_status">System status flag</field>
            <field type="uint8_t" name="mavlink_version">MAVLink version</field>
        </message>
    </messages>
</mavlink>
)"""");

    SUBCASE("Null pointer handling") {
        auto message_opt = message_set.tryParseMessage(nullptr, 100);
        CHECK_FALSE(message_opt.has_value());
        
        Message message;
        auto result = message_set.tryParseMessage(nullptr, 100, message);
        CHECK(result == MessageSet::ParseResult::BUFFER_TOO_SHORT);
        
        CHECK_THROWS_AS(message_set.parseMessage(nullptr, 100), ParseError);
    }
    
    SUBCASE("Buffer too short") {
        std::vector<uint8_t> short_data{0xFD, 0x09};
        
        auto message_opt = message_set.tryParseMessage(short_data.data(), short_data.size());
        CHECK_FALSE(message_opt.has_value());
        
        Message message;
        auto result = message_set.tryParseMessage(short_data.data(), short_data.size(), message);
        CHECK(result == MessageSet::ParseResult::BUFFER_TOO_SHORT);
        
        CHECK_THROWS_AS(message_set.parseMessage(short_data.data(), short_data.size()), ParseError);
    }
    
    SUBCASE("Invalid magic byte") {
        std::vector<uint8_t> invalid_magic{
            0xFE, 0x09, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00,
            0x02, 0x00, 0x00, 0x00, 0x04, 0x02, 0x47, 0x8D
        };
        
        auto message_opt = message_set.tryParseMessage(invalid_magic.data(), invalid_magic.size());
        CHECK_FALSE(message_opt.has_value());
        
        Message message;
        auto result = message_set.tryParseMessage(invalid_magic.data(), invalid_magic.size(), message);
        CHECK(result == MessageSet::ParseResult::INVALID_MAGIC);
        
        CHECK_THROWS_AS(message_set.parseMessage(invalid_magic.data(), invalid_magic.size()), ParseError);
    }
    
    SUBCASE("Unknown message ID") {
        // Create a valid HEARTBEAT message first
        auto original_message = message_set.create("HEARTBEAT");
        original_message.set("type", uint8_t{2});
        original_message.set("autopilot", uint8_t{0});
        original_message.set("base_mode", uint8_t{0});
        original_message.set("custom_mode", uint32_t{0});
        original_message.set("system_status", uint8_t{4});
        original_message.set("mavlink_version", uint8_t{2});
        
        Identifier sender{1, 1};
        auto length = original_message.finalize(0, sender);
        
        // Copy to vector and modify message ID to unknown value
        std::vector<uint8_t> unknown_id(original_message.data(), original_message.data() + length);
        unknown_id[7] = 0x99; // Change message ID to unknown value
        unknown_id[8] = 0x99;
        
        auto message_opt = message_set.tryParseMessage(unknown_id.data(), unknown_id.size());
        CHECK_FALSE(message_opt.has_value());
        
        Message message;
        auto result = message_set.tryParseMessage(unknown_id.data(), unknown_id.size(), message);
        CHECK(result == MessageSet::ParseResult::UNKNOWN_MESSAGE_ID);
        
        CHECK_THROWS_AS(message_set.parseMessage(unknown_id.data(), unknown_id.size()), ParseError);
    }
    
    SUBCASE("Invalid CRC") {
        // Create a valid HEARTBEAT message first
        auto original_message = message_set.create("HEARTBEAT");
        original_message.set("type", uint8_t{2});
        original_message.set("autopilot", uint8_t{0});
        original_message.set("base_mode", uint8_t{0});
        original_message.set("custom_mode", uint32_t{0});
        original_message.set("system_status", uint8_t{4});
        original_message.set("mavlink_version", uint8_t{2});
        
        Identifier sender{1, 1};
        auto length = original_message.finalize(0, sender);
        
        // Copy to vector and modify CRC to invalid value
        std::vector<uint8_t> invalid_crc(original_message.data(), original_message.data() + length);
        invalid_crc[length - 2] = 0xFF; // Change CRC to invalid value
        invalid_crc[length - 1] = 0xFF;
        
        auto message_opt = message_set.tryParseMessage(invalid_crc.data(), invalid_crc.size());
        CHECK_FALSE(message_opt.has_value());
        
        Message message;
        auto result = message_set.tryParseMessage(invalid_crc.data(), invalid_crc.size(), message);
        CHECK(result == MessageSet::ParseResult::INVALID_CRC);
        
        CHECK_THROWS_AS(message_set.parseMessage(invalid_crc.data(), invalid_crc.size()), ParseError);
    }
    
    SUBCASE("Buffer too short for complete message") {
        std::vector<uint8_t> incomplete{
            0xFD, 0x09, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00,
            0x02, 0x00, 0x00, 0x00  // Missing last few bytes
        };
        
        auto message_opt = message_set.tryParseMessage(incomplete.data(), incomplete.size());
        CHECK_FALSE(message_opt.has_value());
        
        Message message;
        auto result = message_set.tryParseMessage(incomplete.data(), incomplete.size(), message);
        CHECK(result == MessageSet::ParseResult::BUFFER_TOO_SHORT);
        
        CHECK_THROWS_AS(message_set.parseMessage(incomplete.data(), incomplete.size()), ParseError);
    }
}

TEST_CASE("Binary Message Parsing - Round-trip serialization") {
    MessageSet message_set;
    message_set.addFromXMLString(R""""(
<mavlink>
    <messages>
        <message id="9915" name="TEST_MESSAGE">
            <field type="uint8_t" name="uint8_field">Test field</field>
            <field type="uint32_t" name="uint32_field">Test field</field>
            <field type="float" name="float_field">Test field</field>
            <field type="char[16]" name="string_field">String field</field>
        </message>
    </messages>
</mavlink>
)"""");

    SUBCASE("Create, serialize, and parse back") {
        // Create a message
        auto original_message = message_set.create("TEST_MESSAGE");
        original_message.set("uint8_field", uint8_t{42});
        original_message.set("uint32_field", uint32_t{12345});
        original_message.set("float_field", 3.14f);
        original_message.setFromString("string_field", "hello");
        
        // Serialize it
        Identifier sender{1, 1};
        auto length = original_message.finalize(0, sender);
        
        // Parse it back
        auto parsed_opt = message_set.tryParseMessage(original_message.data(), length);
        REQUIRE(parsed_opt.has_value());
        
        auto parsed_message = parsed_opt.value();
        CHECK(parsed_message.name() == "TEST_MESSAGE");
        CHECK(parsed_message.id() == 9915);
        
        // Check field values
        uint8_t uint8_val;
        CHECK(parsed_message.tryGet("uint8_field", uint8_val) == Message::GetResult::SUCCESS);
        CHECK(uint8_val == 42);
        
        uint32_t uint32_val;
        CHECK(parsed_message.tryGet("uint32_field", uint32_val) == Message::GetResult::SUCCESS);
        CHECK(uint32_val == 12345);
        
        float float_val;
        CHECK(parsed_message.tryGet("float_field", float_val) == Message::GetResult::SUCCESS);
        CHECK(float_val == doctest::Approx(3.14f));
        
        std::string string_val;
        CHECK(parsed_message.tryGetString("string_field", string_val) == Message::GetResult::SUCCESS);
        CHECK(string_val == "hello");
    }
}

TEST_CASE("Binary Message Parsing - Performance and edge cases") {
    MessageSet message_set;
    message_set.addFromXMLString(R""""(
<mavlink>
    <messages>
        <message id="0" name="HEARTBEAT">
            <field type="uint8_t" name="type">Type of the system</field>
            <field type="uint8_t" name="autopilot">Autopilot type / class</field>
            <field type="uint8_t" name="base_mode">System mode bitmap</field>
            <field type="uint32_t" name="custom_mode">Custom mode</field>
            <field type="uint8_t" name="system_status">System status flag</field>
            <field type="uint8_t" name="mavlink_version">MAVLink version</field>
        </message>
    </messages>
</mavlink>
)"""");

    SUBCASE("Multiple parse operations") {
        // Create a valid HEARTBEAT message using existing API
        auto original_message = message_set.create("HEARTBEAT");
        original_message.set("type", uint8_t{2});
        original_message.set("autopilot", uint8_t{0});
        original_message.set("base_mode", uint8_t{0});
        original_message.set("custom_mode", uint32_t{0});
        original_message.set("system_status", uint8_t{4});
        original_message.set("mavlink_version", uint8_t{2});
        
        Identifier sender{1, 1};
        auto length = original_message.finalize(0, sender);
        
        // Parse the same message multiple times
        for (int i = 0; i < 100; ++i) {
            auto message_opt = message_set.tryParseMessage(original_message.data(), length);
            CHECK(message_opt.has_value());
            CHECK(message_opt.value().name() == "HEARTBEAT");
        }
    }
    
    SUBCASE("Zero-length payload") {
        // Create a message with minimal payload
        auto message = message_set.create("HEARTBEAT");
        message.set("type", uint8_t{0});
        message.set("autopilot", uint8_t{0});
        message.set("base_mode", uint8_t{0});
        message.set("custom_mode", uint32_t{0});
        message.set("system_status", uint8_t{0});
        message.set("mavlink_version", uint8_t{0});
        
        Identifier sender{1, 1};
        auto length = message.finalize(0, sender);
        
        // Parse it back
        auto parsed_opt = message_set.tryParseMessage(message.data(), length);
        CHECK(parsed_opt.has_value());
        CHECK(parsed_opt.value().name() == "HEARTBEAT");
    }
    
    SUBCASE("Exact buffer length") {
        // Create a valid HEARTBEAT message using existing API
        auto original_message = message_set.create("HEARTBEAT");
        original_message.set("type", uint8_t{2});
        original_message.set("autopilot", uint8_t{0});
        original_message.set("base_mode", uint8_t{0});
        original_message.set("custom_mode", uint32_t{0});
        original_message.set("system_status", uint8_t{4});
        original_message.set("mavlink_version", uint8_t{2});
        
        Identifier sender{1, 1};
        auto length = original_message.finalize(0, sender);
        
        // Parse with exact buffer length
        auto message_opt = message_set.tryParseMessage(original_message.data(), length);
        CHECK(message_opt.has_value());
        
        // Parse with buffer one byte longer (by copying to a vector)
        std::vector<uint8_t> extended_data(original_message.data(), original_message.data() + length);
        extended_data.push_back(0x00);
        message_opt = message_set.tryParseMessage(extended_data.data(), extended_data.size());
        CHECK(message_opt.has_value());
    }
}

TEST_CASE("Binary Message Parsing - fromBinary static method") {
    MessageSet message_set;
    message_set.addFromXMLString(R""""(
<mavlink>
    <messages>
        <message id="0" name="HEARTBEAT">
            <field type="uint8_t" name="type">Type of the system</field>
            <field type="uint8_t" name="autopilot">Autopilot type / class</field>
            <field type="uint8_t" name="base_mode">System mode bitmap</field>
            <field type="uint32_t" name="custom_mode">Custom mode</field>
            <field type="uint8_t" name="system_status">System status flag</field>
            <field type="uint8_t" name="mavlink_version">MAVLink version</field>
        </message>
    </messages>
</mavlink>
)"""");

    SUBCASE("fromBinary method works correctly") {
        // Create original message
        auto original_message = message_set.create("HEARTBEAT");
        original_message.set("type", uint8_t{2});
        original_message.set("autopilot", uint8_t{0});
        original_message.set("base_mode", uint8_t{0});
        original_message.set("custom_mode", uint32_t{0});
        original_message.set("system_status", uint8_t{4});
        original_message.set("mavlink_version", uint8_t{2});
        
        Identifier sender{1, 1};
        auto length = original_message.finalize(0, sender);
        
        // Use fromBinary directly
        auto definition_opt = message_set.getMessageDefinition("HEARTBEAT");
        REQUIRE(definition_opt);
        
        const int crc_offset = MessageDefinition::HEADER_SIZE + 9; // payload length is 9
        std::array<uint8_t, MessageDefinition::MAX_MESSAGE_SIZE> backing_memory{};
        std::copy(original_message.data(), original_message.data() + length, backing_memory.data());
        
        ConnectionPartner source_partner{};
        auto reconstructed_message = Message::fromBinary(definition_opt.get(), source_partner, crc_offset, std::move(backing_memory));
        
        CHECK(reconstructed_message.name() == "HEARTBEAT");
        CHECK(reconstructed_message.id() == 0);
        
        uint8_t type_val;
        CHECK(reconstructed_message.tryGet("type", type_val) == Message::GetResult::SUCCESS);
        CHECK(type_val == 2);
    }
}