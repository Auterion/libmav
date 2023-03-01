//
// Created by thomas on 01.02.23.
//

#include "doctest.h"
#include "mav/MessageSet.h"
#include "mav/Message.h"

using namespace mav;

TEST_CASE("Message set creation") {
    MessageSet message_set;
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
        </message>
    </messages>
</mavlink>
)"""");

    REQUIRE(message_set.contains("BIG_MESSAGE"));
    REQUIRE_EQ(message_set.size(), 1);

    auto message = message_set.create("BIG_MESSAGE");
    CHECK_EQ(message.id(), message_set.idForMessage("BIG_MESSAGE"));
    CHECK_EQ(message.name(), "BIG_MESSAGE");


    SUBCASE("Can set and get values with set / get API") {
        message.set("uint8_field", 0x12);
        message.set("int8_field", 0x12);
        message.set("uint16_field", 0x1234);
        message.set("int16_field", 0x1234);
        message.set("uint32_field", 0x12345678);
        message.set("int32_field", 0x12345678);
        message.set("uint64_field", 0x1234567890ABCDEF);
        message.set("int64_field", 0x1234567890ABCDEF);
        message.set("double_field", 0.123456789);
        message.set("float_field", 0.123456789);
        message.set("char_arr_field", "Hello World!");
        message.set("float_arr_field", std::vector<float>{1.0, 2.0, 3.0});
        message.set("int32_arr_field", std::vector<int32_t>{1, 2, 3});
        CHECK_EQ(message.get<uint8_t>("uint8_field"), 0x12);
        CHECK_EQ(message.get<int8_t>("int8_field"), 0x12);
        CHECK_EQ(message.get<uint16_t>("uint16_field"), 0x1234);
        CHECK_EQ(message.get<int16_t>("int16_field"), 0x1234);
        CHECK_EQ(message.get<uint32_t>("uint32_field"), 0x12345678);
        CHECK_EQ(message.get<int32_t>("int32_field"), 0x12345678);
        CHECK_EQ(message.get<uint64_t>("uint64_field"), 0x1234567890ABCDEF);
        CHECK_EQ(message.get<int64_t>("int64_field"), 0x1234567890ABCDEF);
        CHECK_EQ(message.get<double>("double_field"), doctest::Approx(0.123456789));
        CHECK_EQ(message.get<float>("float_field"), doctest::Approx(0.123456789));
        CHECK_EQ(message.get<std::string>("char_arr_field"), "Hello World!");
        CHECK_EQ(message.get<std::vector<float>>("float_arr_field"), std::vector<float>{1.0, 2.0, 3.0});
        CHECK_EQ(message.get<std::vector<int32_t>>("int32_arr_field"), std::vector<int32_t>{1, 2, 3});
    }

    SUBCASE("Have fields truncated by zero-elision") {
        message.set("int64_field", 34); // since largest field, will be at the end of the message
        CHECK_EQ(message.get<int64_t>("int64_field"), 34);
        auto ret = message.finalize(1, {2,3});
        CHECK_EQ(message.get<int64_t>("int64_field"), 34);
    }


    SUBCASE("Can set and get values with assignment API") {
        message["uint8_field"] = 0x12;
        message["int8_field"] = 0x12;
        message["uint16_field"] = 0x1234;
        message["int16_field"] = 0x1234;
        message["uint32_field"] = 0x12345678;
        message["int32_field"] = 0x12345678;
        message["uint64_field"] = 0x1234567890ABCDEF;
        message["int64_field"] = 0x1234567890ABCDEF;
        message["double_field"] = 0.123456789;
        message["float_field"] = 0.123456789;
        message["char_arr_field"] = "Hello World!";
        message["float_arr_field"] = std::vector<float>{1.0, 2.0, 3.0};
        message["int32_arr_field"] = std::vector<int32_t>{1, 2, 3};

        CHECK_EQ(message["uint8_field"].as<uint8_t>(), 0x12);
        CHECK_EQ(message["int8_field"].as<int8_t>(), 0x12);
        CHECK_EQ(message["uint16_field"].as<uint16_t>(), 0x1234);
        CHECK_EQ(message["int16_field"].as<int16_t>(), 0x1234);
        CHECK_EQ(message["uint32_field"].as<uint32_t>(), 0x12345678);
        CHECK_EQ(message["int32_field"].as<int32_t>(), 0x12345678);
        CHECK_EQ(message["uint64_field"].as<uint64_t>(), 0x1234567890ABCDEF);
        CHECK_EQ(message["int64_field"].as<int64_t>(), 0x1234567890ABCDEF);
        CHECK_EQ(message["double_field"].as<double>(), doctest::Approx(0.123456789));
        CHECK_EQ(message["float_field"].as<float>(), doctest::Approx(0.123456789));
        CHECK_EQ(message["char_arr_field"].as<std::string>(), "Hello World!");
        CHECK_EQ(message["float_arr_field"].as<std::vector<float>>(), std::vector<float>{1.0, 2.0, 3.0});
        CHECK_EQ(message["int32_arr_field"].as<std::vector<int32_t>>(), std::vector<int32_t>{1, 2, 3});

        // Check getting as static cast
        CHECK_EQ(static_cast<uint8_t>(message["uint8_field"]), 0x12);
        CHECK_EQ(static_cast<int8_t>(message["int8_field"]), 0x12);
        CHECK_EQ(static_cast<uint16_t>(message["uint16_field"]), 0x1234);
        CHECK_EQ(static_cast<int16_t>(message["int16_field"]), 0x1234);
        CHECK_EQ(static_cast<uint32_t>(message["uint32_field"]), 0x12345678);
        CHECK_EQ(static_cast<int32_t>(message["int32_field"]), 0x12345678);
        CHECK_EQ(static_cast<uint64_t>(message["uint64_field"]), 0x1234567890ABCDEF);
        CHECK_EQ(static_cast<int64_t>(message["int64_field"]), 0x1234567890ABCDEF);
        CHECK_EQ(static_cast<double>(message["double_field"]), doctest::Approx(0.123456789));
        CHECK_EQ(static_cast<float>(message["float_field"]), doctest::Approx(0.123456789));
        CHECK_EQ(static_cast<std::string>(message["char_arr_field"]), "Hello World!");
        CHECK_EQ(static_cast<std::vector<float>>(message["float_arr_field"]), std::vector<float>{1.0, 2.0, 3.0});
        CHECK_EQ(static_cast<std::vector<int32_t>>(message["int32_arr_field"]), std::vector<int32_t>{1, 2, 3});
    }

    SUBCASE("Can set values with initializer list API") {
        message.set({
            {"uint8_field", 0x12},
            {"int8_field", 0x12},
            {"uint16_field", 0x1234},
            {"int16_field", 0x1234},
            {"uint32_field", 0x12345678},
            {"int32_field", 0x12345678},
            {"uint64_field", 0x1234567890ABCDEF},
            {"int64_field", 0x1234567890ABCDEF},
            {"double_field", 0.123456789},
            {"float_field", 0.123456789},
            {"char_arr_field", "Hello World!"},
            {"float_arr_field", std::vector<float>{1.0, 2.0, 3.0}},
            {"int32_arr_field", std::vector<int32_t>{1, 2, 3}}});

        CHECK_EQ(static_cast<uint8_t>(message["uint8_field"]), 0x12);
        CHECK_EQ(static_cast<int8_t>(message["int8_field"]), 0x12);
        CHECK_EQ(static_cast<uint16_t>(message["uint16_field"]), 0x1234);
        CHECK_EQ(static_cast<int16_t>(message["int16_field"]), 0x1234);
        CHECK_EQ(static_cast<uint32_t>(message["uint32_field"]), 0x12345678);
        CHECK_EQ(static_cast<int32_t>(message["int32_field"]), 0x12345678);
        CHECK_EQ(static_cast<uint64_t>(message["uint64_field"]), 0x1234567890ABCDEF);
        CHECK_EQ(static_cast<int64_t>(message["int64_field"]), 0x1234567890ABCDEF);
        CHECK_EQ(static_cast<double>(message["double_field"]), doctest::Approx(0.123456789));
        CHECK_EQ(static_cast<float>(message["float_field"]), doctest::Approx(0.123456789));
        CHECK_EQ(static_cast<std::string>(message["char_arr_field"]), "Hello World!");
        CHECK_EQ(static_cast<std::vector<float>>(message["float_arr_field"]), std::vector<float>{1.0, 2.0, 3.0});
        CHECK_EQ(static_cast<std::vector<int32_t>>(message["int32_arr_field"]), std::vector<int32_t>{1, 2, 3});
    }

    SUBCASE("Can assign std::string to char array field") {
        std::string str = "Hello World!";
        message.set("char_arr_field", str);
        CHECK_EQ(message.get<std::string>("char_arr_field"), "Hello World!");
    }

    SUBCASE("Assign independent chars to char array field") {
        message.set("char_arr_field", "012345");
        message.set("char_arr_field", 'a', 0);
        CHECK_EQ(message.get<char>("char_arr_field", 0), 'a');
        CHECK_EQ(message.get<std::string>("char_arr_field"), "a12345");

        message.set("char_arr_field", 'b', 1);
        CHECK_EQ(message.get<std::string>("char_arr_field"), "ab2345");

        message["char_arr_field"][2] = 'c';
        CHECK_EQ(message.get<std::string>("char_arr_field"), "abc345");

        message["char_arr_field"][3] = 'd';
        char item3 = message["char_arr_field"][3];
        CHECK_EQ(item3, 'd');

        message["char_arr_field"][4] = 'e';
        CHECK_EQ(message.get<std::string>("char_arr_field"), "abcde5");
    }

    SUBCASE("Assigning too long string to char array field throws") {
        std::string str = "This is a very long string that will not fit in the char array field";
        CHECK_THROWS_AS(message.set("char_arr_field", str + str), std::out_of_range);
    }

    SUBCASE("Assigning a too long vector to an array field throws") {
        std::vector<float> vec(100);
        CHECK_THROWS_AS(message.set("float_arr_field", vec), std::out_of_range);
    }

    SUBCASE("Pack integer into float field") {
        message.setAsFloatPack("float_field", 0x12345678);
        CHECK_EQ(message.getAsFloatUnpack<uint32_t>("float_field"), 0x12345678);

        message["float_field"].floatPack<uint32_t>(0x23456789);
        CHECK_EQ(message["float_field"].floatUnpack<uint32_t>(), 0x23456789);
    }

    SUBCASE("Set and get a single field in array outside of range") {
        CHECK_THROWS_AS(message.set("float_arr_field", 1.0, 100), std::out_of_range);
        CHECK_THROWS_AS(message.get<int>("float_arr_field", 100), std::out_of_range);
    }

    SUBCASE("Set and get a string to a non-char field") {
        CHECK_THROWS_AS(message.set("float_field", "Hello World!"), std::invalid_argument);
        CHECK_THROWS_AS(message.get<std::string>("float_field"), std::invalid_argument);
    }

    SUBCASE("Read an array into a too short return container") {
        CHECK_THROWS_AS((message.get<std::array<float, 2>>("float_arr_field")), std::out_of_range);
    }

    SUBCASE("String at the end of message") {
        message.set("uint32_field", 0x0);
        message.set("int8_field", 0x0);
        message.set("uint16_field", 0x0);
        message.set("int16_field", 0x0);
        message.set("uint32_field", 0x0);
        message.set("int32_field", 0x0);
        message.set("uint64_field", 0x0);
        message.set("int64_field", 0x0);
        message.set("double_field", 0.0);
        message.set("float_field", 0.0);
        message.set("char_arr_field", "Hello World!");
        message.set("float_arr_field", std::vector<float>{0.0, 0.0, 0.0});
        message.set("int32_arr_field", std::vector<int32_t>{0, 0, 0});

        int wire_size = message.finalize(5, {6, 7});
        CHECK_EQ(message.get<std::string>("char_arr_field"), "Hello World!");

        // check that it stays correct even after modifying the fields
        message.set("uint32_field", 0x1);
        CHECK_EQ(message.get<std::string>("char_arr_field"), "Hello World!");

        message.set("char_arr_field", "Hello Worldo!");
        CHECK_EQ(message.get<std::string>("char_arr_field"), "Hello Worldo!");
    }
}
