/**
 * @file test_noexcept_api.cpp
 * @brief Tests for non-throwing libmav API
 * 
 * This test verifies that the new non-throwing methods work correctly
 * and have the expected signatures and behavior.
 */

#include "doctest.h"
#include "mav/MessageSet.h"
#include "mav/Message.h"
#include "mav/MessageDefinition.h"

using namespace mav;

TEST_CASE("Non-throwing MessageSet methods") {
    MessageSet message_set;
    
    SUBCASE("tryCreate method signatures") {
        // Test tryCreate methods exist and have correct signatures
        static_assert(std::is_same_v<
            decltype(message_set.tryCreate("test")),
            std::optional<Message>
        >, "tryCreate(string) should return optional<Message>");
        
        static_assert(std::is_same_v<
            decltype(message_set.tryCreate(0)),
            std::optional<Message>
        >, "tryCreate(int) should return optional<Message>");
        
        static_assert(std::is_same_v<
            decltype(message_set.tryGetEnum("test")),
            std::optional<uint64_t>
        >, "tryGetEnum should return optional<uint64_t>");
    }
    
    SUBCASE("tryCreate returns nullopt for non-existent messages") {
        auto message_opt = message_set.tryCreate("NONEXISTENT_MESSAGE");
        CHECK_FALSE(message_opt.has_value());
        
        auto message_opt_id = message_set.tryCreate(99999);
        CHECK_FALSE(message_opt_id.has_value());
    }
    
    SUBCASE("tryGetEnum returns nullopt for non-existent enums") {
        auto enum_opt = message_set.tryGetEnum("NONEXISTENT_ENUM");
        CHECK_FALSE(enum_opt.has_value());
    }
}

TEST_CASE("Non-throwing Message enums") {
    SUBCASE("SetResult enum values") {
        Message::SetResult set_result = Message::SetResult::SUCCESS;
        CHECK(set_result == Message::SetResult::SUCCESS);
        
        // Test all enum values exist
        set_result = Message::SetResult::FIELD_NOT_FOUND;
        set_result = Message::SetResult::TYPE_MISMATCH;
        set_result = Message::SetResult::OUT_OF_RANGE;
        set_result = Message::SetResult::INVALID_DATA;
    }
    
    SUBCASE("GetResult enum values") {
        Message::GetResult get_result = Message::GetResult::SUCCESS;
        CHECK(get_result == Message::GetResult::SUCCESS);
        
        get_result = Message::GetResult::FIELD_NOT_FOUND;
        get_result = Message::GetResult::TYPE_MISMATCH;
        get_result = Message::GetResult::OUT_OF_RANGE;
    }
}

TEST_CASE("Non-throwing Message methods with real data") {
    MessageSet message_set;
    message_set.addFromXMLString(R""""(
<mavlink>
    <messages>
        <message id="9915" name="TEST_MESSAGE">
            <field type="uint8_t" name="uint8_field">description</field>
            <field type="uint32_t" name="uint32_field">description</field>
            <field type="float" name="float_field">description</field>
            <field type="char[16]" name="string_field">description</field>
        </message>
    </messages>
</mavlink>
)"""");

    SUBCASE("tryCreate succeeds for valid message") {
        auto message_opt = message_set.tryCreate("TEST_MESSAGE");
        REQUIRE(message_opt.has_value());
        
        auto message = message_opt.value();
        CHECK(message.name() == "TEST_MESSAGE");
        CHECK(message.id() == 9915);
    }
    
    SUBCASE("trySet methods work correctly") {
        auto message_opt = message_set.tryCreate("TEST_MESSAGE");
        REQUIRE(message_opt.has_value());
        auto message = message_opt.value();
        
        // Test successful field setting
        CHECK(message.trySet("uint8_field", uint8_t{42}) == Message::SetResult::SUCCESS);
        CHECK(message.trySet("uint32_field", uint32_t{12345}) == Message::SetResult::SUCCESS);
        CHECK(message.trySet("float_field", 3.14f) == Message::SetResult::SUCCESS);
        
        // Test field not found
        CHECK(message.trySet("nonexistent_field", 42) == Message::SetResult::FIELD_NOT_FOUND);
        
        // Test type mismatch
        CHECK(message.trySet("uint8_field", 3.14f) == Message::SetResult::TYPE_MISMATCH);
    }
    
    SUBCASE("trySetString works correctly") {
        auto message_opt = message_set.tryCreate("TEST_MESSAGE");
        REQUIRE(message_opt.has_value());
        auto message = message_opt.value();
        
        // Test successful string setting
        CHECK(message.trySetString("string_field", "test") == Message::SetResult::SUCCESS);
        
        // Test field not found
        CHECK(message.trySetString("nonexistent_field", "test") == Message::SetResult::FIELD_NOT_FOUND);
        
        // Test type mismatch (setting string on non-char field)
        CHECK(message.trySetString("uint8_field", "test") == Message::SetResult::TYPE_MISMATCH);
        
        // Test string too long
        CHECK(message.trySetString("string_field", "this_string_is_way_too_long_for_16_chars") == Message::SetResult::OUT_OF_RANGE);
    }
    
    SUBCASE("tryGet methods work correctly") {
        auto message_opt = message_set.tryCreate("TEST_MESSAGE");
        REQUIRE(message_opt.has_value());
        auto message = message_opt.value();
        
        // Set some values first
        message.trySet("uint8_field", uint8_t{42});
        message.trySet("uint32_field", uint32_t{12345});
        message.trySet("float_field", 3.14f);
        message.trySetString("string_field", "test");
        
        // Test successful field getting
        uint8_t uint8_val;
        CHECK(message.tryGet("uint8_field", uint8_val) == Message::GetResult::SUCCESS);
        CHECK(uint8_val == 42);
        
        uint32_t uint32_val;
        CHECK(message.tryGet("uint32_field", uint32_val) == Message::GetResult::SUCCESS);
        CHECK(uint32_val == 12345);
        
        float float_val;
        CHECK(message.tryGet("float_field", float_val) == Message::GetResult::SUCCESS);
        CHECK(float_val == doctest::Approx(3.14f));
        
        // Test field not found
        uint8_t dummy;
        CHECK(message.tryGet("nonexistent_field", dummy) == Message::GetResult::FIELD_NOT_FOUND);
        
        // Test type mismatch
        float dummy_float;
        CHECK(message.tryGet("uint8_field", dummy_float) == Message::GetResult::TYPE_MISMATCH);
    }
    
    SUBCASE("tryGetString works correctly") {
        auto message_opt = message_set.tryCreate("TEST_MESSAGE");
        REQUIRE(message_opt.has_value());
        auto message = message_opt.value();
        
        // Set string value first
        message.trySetString("string_field", "test");
        
        // Test successful string getting
        std::string string_val;
        CHECK(message.tryGetString("string_field", string_val) == Message::GetResult::SUCCESS);
        CHECK(string_val == "test");
        
        // Test field not found
        std::string dummy;
        CHECK(message.tryGetString("nonexistent_field", dummy) == Message::GetResult::FIELD_NOT_FOUND);
        
        // Test type mismatch (getting string from non-char field)
        CHECK(message.tryGetString("uint8_field", dummy) == Message::GetResult::TYPE_MISMATCH);
    }
    
    SUBCASE("tryFinalize works correctly") {
        auto message_opt = message_set.tryCreate("TEST_MESSAGE");
        REQUIRE(message_opt.has_value());
        auto message = message_opt.value();
        
        // Set some fields
        message.trySet("uint8_field", uint8_t{42});
        message.trySet("uint32_field", uint32_t{12345});
        
        // Test successful finalization
        Identifier sender{1, 1};
        auto length_opt = message.tryFinalize(0, sender);
        REQUIRE(length_opt.has_value());
        CHECK(length_opt.value() > 0);
        
        // Test that data is available
        const uint8_t* data = message.data();
        CHECK(data != nullptr);
        CHECK(data[0] == 0xFD); // MAVLink v2 magic byte
    }
}

TEST_CASE("Non-throwing type validation compilation") {
    MessageSet message_set;
    auto message_opt = message_set.tryCreate("HEARTBEAT");
    
    SUBCASE("All numeric types compile") {
        if (message_opt) {
            auto message = message_opt.value();
            
            // Test that all types compile and have type validation
            message.trySet("test", uint8_t{42});
            message.trySet("test", uint16_t{42});
            message.trySet("test", uint32_t{42});
            message.trySet("test", uint64_t{42});
            message.trySet("test", int8_t{42});
            message.trySet("test", int16_t{42});
            message.trySet("test", int32_t{42});
            message.trySet("test", int64_t{42});
            message.trySet("test", float{42.0f});
            message.trySet("test", double{42.0});
            message.trySet("test", char{'A'});
            
            // Test that get methods compile
            uint8_t out1; message.tryGet("test", out1);
            uint16_t out2; message.tryGet("test", out2);
            uint32_t out3; message.tryGet("test", out3);
            uint64_t out4; message.tryGet("test", out4);
            int8_t out5; message.tryGet("test", out5);
            int16_t out6; message.tryGet("test", out6);
            int32_t out7; message.tryGet("test", out7);
            int64_t out8; message.tryGet("test", out8);
            float out9; message.tryGet("test", out9);
            double out10; message.tryGet("test", out10);
            char out11; message.tryGet("test", out11);
        }
    }
}