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
        <message id="9916" name="UINT8_ONLY_MESSAGE">
            <field type="uint8_t" name="field1">description</field>
            <field type="uint8_t" name="field2">description</field>
            <field type="uint8_t" name="field3">description</field>
            <field type="uint8_t" name="field4">description</field>
        </message>
        <message id="9917" name="ARRAY_ONLY_MESSAGE">
            <field type="char[6]" name="field1">description</field>
            <field type="uint8_t[3]" name="field2">description</field>
            <field type="uint16_t[3]" name="field3">description</field>
            <field type="uint32_t[3]" name="field4">description</field>
            <field type="uint64_t[3]" name="field5">description</field>
            <field type="int8_t[3]" name="field6">description</field>
            <field type="int16_t[3]" name="field7">description</field>
            <field type="int32_t[3]" name="field8">description</field>
            <field type="int64_t[3]" name="field9">description</field>
            <field type="float[3]" name="field10">description</field>
            <field type="double[3]" name="field11">description</field>
        </message>
    </messages>
</mavlink>
)"""");

    REQUIRE(message_set.contains("BIG_MESSAGE"));
    REQUIRE_EQ(message_set.size(), 3);

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
        (void)message.finalize(1, {2, 3});
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
                            {"uint8_field",     0x12},
                            {"int8_field",      0x12},
                            {"uint16_field",    0x1234},
                            {"int16_field",     0x1234},
                            {"uint32_field",    0x12345678},
                            {"int32_field",     0x12345678},
                            {"uint64_field",    0x1234567890ABCDEF},
                            {"int64_field",     0x1234567890ABCDEF},
                            {"double_field",    0.123456789},
                            {"float_field",     0.123456789},
                            {"char_arr_field",  "Hello World!"},
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

        CHECK_THROWS_AS(message["float_field"].floatUnpack<std::string>(), std::runtime_error);
        CHECK_THROWS_AS(message["float_field"].floatUnpack<std::vector<int>>(), std::runtime_error);
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
        message.set("int8_field", 0x00);
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

        (void)message.finalize(5, {6, 7});
        CHECK_EQ(message.get<std::string>("char_arr_field"), "Hello World!");

        // check that it stays correct even after modifying the fields
        message.set("uint32_field", 0x1);
        CHECK_EQ(message.get<std::string>("char_arr_field"), "Hello World!");

        message.set("char_arr_field", "Hello Worldo!");
        CHECK_EQ(message.get<std::string>("char_arr_field"), "Hello Worldo!");
    }

    SUBCASE("Zero elision works on single byte at end of array") {

        auto this_test_message = message_set.create("UINT8_ONLY_MESSAGE");
        this_test_message.set("field1", 111);
        this_test_message.set("field2", 0);
        this_test_message.set("field3", 0);
        this_test_message.set("field4", 0);

        uint32_t wire_size = this_test_message.finalize(5, {6, 7});
        CHECK_EQ(wire_size, 13);
        CHECK_EQ(this_test_message.get<uint8_t>("field1"), 111);
        CHECK_EQ(this_test_message.get<uint8_t>("field2"), 0);
        CHECK_EQ(this_test_message.get<uint8_t>("field3"), 0);
        CHECK_EQ(this_test_message.get<uint8_t>("field4"), 0);
    }

    SUBCASE("Test all array conversions") {
        auto this_test_message = message_set.create("ARRAY_ONLY_MESSAGE");
        this_test_message["field1"] = "Hello";
        this_test_message["field2"] = std::vector<uint8_t>{1, 2, 3};
        this_test_message["field3"] = std::vector<uint16_t>{4, 5, 6};
        this_test_message["field4"] = std::vector<uint32_t>{7, 8, 9};
        this_test_message["field5"] = std::vector<uint64_t>{10, 11, 12};
        this_test_message["field6"] = std::vector<int8_t>{13, 14, 15};
        this_test_message["field7"] = std::vector<int16_t>{16, 17, 18};
        this_test_message["field8"] = std::vector<int32_t>{19, 20, 21};
        this_test_message["field9"] = std::vector<int64_t>{22, 23, 24};
        this_test_message["field10"] = std::vector<float>{25, 26, 27};
        this_test_message["field11"] = std::vector<double>{28, 29, 30};

        CHECK_EQ(this_test_message["field1"].as<std::string>(), "Hello");
        CHECK_EQ(this_test_message["field2"].as<std::vector<uint8_t>>(), std::vector<uint8_t>{1, 2, 3});
        CHECK_EQ(this_test_message["field3"].as<std::vector<uint16_t>>(), std::vector<uint16_t>{4, 5, 6});
        CHECK_EQ(this_test_message["field4"].as<std::vector<uint32_t>>(), std::vector<uint32_t>{7, 8, 9});
        CHECK_EQ(this_test_message["field5"].as<std::vector<uint64_t>>(), std::vector<uint64_t>{10, 11, 12});
        CHECK_EQ(this_test_message["field6"].as<std::vector<int8_t>>(), std::vector<int8_t>{13, 14, 15});
        CHECK_EQ(this_test_message["field7"].as<std::vector<int16_t>>(), std::vector<int16_t>{16, 17, 18});
        CHECK_EQ(this_test_message["field8"].as<std::vector<int32_t>>(), std::vector<int32_t>{19, 20, 21});
        CHECK_EQ(this_test_message["field9"].as<std::vector<int64_t>>(), std::vector<int64_t>{22, 23, 24});
        CHECK_EQ(this_test_message["field10"].as<std::vector<float>>(), std::vector<float>{25, 26, 27});
        CHECK_EQ(this_test_message["field11"].as<std::vector<double>>(), std::vector<double>{28, 29, 30});
    }

    SUBCASE("Can get as native type variant") {
        message.setFromNativeTypeVariant("uint8_field", {static_cast<uint8_t>(1)});
        message.setFromNativeTypeVariant("int8_field", {static_cast<int8_t>(2)});
        message.setFromNativeTypeVariant("uint16_field", {static_cast<uint16_t>(3)});
        message.setFromNativeTypeVariant("int16_field", {static_cast<int16_t>(4)});
        message.setFromNativeTypeVariant("uint32_field", {static_cast<uint32_t>(5)});
        message.setFromNativeTypeVariant("int32_field", {static_cast<int32_t>(6)});
        message.setFromNativeTypeVariant("uint64_field", {static_cast<uint64_t>(7)});
        message.setFromNativeTypeVariant("int64_field", {static_cast<int64_t>(8)});
        message.setFromNativeTypeVariant("double_field", {9.0});
        message.setFromNativeTypeVariant("float_field", {10.0f});
        message.setFromNativeTypeVariant("char_arr_field", {"Hello World!"});
        message.setFromNativeTypeVariant("float_arr_field", {std::vector<float>{1.0, 2.0, 3.0}});
        message.setFromNativeTypeVariant("int32_arr_field", {std::vector<int32_t>{4, 5, 6}});
        CHECK_EQ(message.get<uint8_t>("uint8_field"), 1);
        CHECK_EQ(message.get<int8_t>("int8_field"), 2);
        CHECK_EQ(message.get<uint16_t>("uint16_field"), 3);
        CHECK_EQ(message.get<int16_t>("int16_field"), 4);
        CHECK_EQ(message.get<uint32_t>("uint32_field"), 5);
        CHECK_EQ(message.get<int32_t>("int32_field"), 6);
        CHECK_EQ(message.get<uint64_t>("uint64_field"), 7);
        CHECK_EQ(message.get<int64_t>("int64_field"), 8);
        CHECK_EQ(message.get<double>("double_field"), doctest::Approx(9.0));
        CHECK_EQ(message.get<float>("float_field"), doctest::Approx(10.0));
        CHECK_EQ(message.get<std::string>("char_arr_field"), "Hello World!");
        CHECK_EQ(message.get<std::vector<float>>("float_arr_field"), std::vector<float>{1.0, 2.0, 3.0});
        CHECK_EQ(message.get<std::vector<int32_t>>("int32_arr_field"), std::vector<int32_t>{4, 5, 6});



        CHECK(std::holds_alternative<uint8_t>(message.getAsNativeTypeInVariant("uint8_field")));
        CHECK(std::holds_alternative<int8_t>(message.getAsNativeTypeInVariant("int8_field")));
        CHECK(std::holds_alternative<uint16_t>(message.getAsNativeTypeInVariant("uint16_field")));
        CHECK(std::holds_alternative<int16_t>(message.getAsNativeTypeInVariant("int16_field")));
        CHECK(std::holds_alternative<uint32_t>(message.getAsNativeTypeInVariant("uint32_field")));
        CHECK(std::holds_alternative<int32_t>(message.getAsNativeTypeInVariant("int32_field")));
        CHECK(std::holds_alternative<uint64_t>(message.getAsNativeTypeInVariant("uint64_field")));
        CHECK(std::holds_alternative<int64_t>(message.getAsNativeTypeInVariant("int64_field")));
        CHECK(std::holds_alternative<double>(message.getAsNativeTypeInVariant("double_field")));
        CHECK(std::holds_alternative<float>(message.getAsNativeTypeInVariant("float_field")));
        CHECK(std::holds_alternative<std::string>(message.getAsNativeTypeInVariant("char_arr_field")));
        CHECK(std::holds_alternative<std::vector<float>>(message.getAsNativeTypeInVariant("float_arr_field")));
        CHECK(std::holds_alternative<std::vector<int32_t>>(message.getAsNativeTypeInVariant("int32_arr_field")));


        auto this_test_message = message_set.create("ARRAY_ONLY_MESSAGE");
        CHECK(std::holds_alternative<std::string>(this_test_message.getAsNativeTypeInVariant("field1")));
        CHECK(std::holds_alternative<std::vector<uint8_t>>(this_test_message.getAsNativeTypeInVariant("field2")));
        CHECK(std::holds_alternative<std::vector<uint16_t>>(this_test_message.getAsNativeTypeInVariant("field3")));
        CHECK(std::holds_alternative<std::vector<uint32_t>>(this_test_message.getAsNativeTypeInVariant("field4")));
        CHECK(std::holds_alternative<std::vector<uint64_t>>(this_test_message.getAsNativeTypeInVariant("field5")));
        CHECK(std::holds_alternative<std::vector<int8_t>>(this_test_message.getAsNativeTypeInVariant("field6")));
        CHECK(std::holds_alternative<std::vector<int16_t>>(this_test_message.getAsNativeTypeInVariant("field7")));
        CHECK(std::holds_alternative<std::vector<int32_t>>(this_test_message.getAsNativeTypeInVariant("field8")));
        CHECK(std::holds_alternative<std::vector<int64_t>>(this_test_message.getAsNativeTypeInVariant("field9")));
        CHECK(std::holds_alternative<std::vector<float>>(this_test_message.getAsNativeTypeInVariant("field10")));
        CHECK(std::holds_alternative<std::vector<double>>(this_test_message.getAsNativeTypeInVariant("field11")));
    }

    SUBCASE("Test toString") {
        message["uint8_field"] = 1;
        message["int8_field"] = -2;
        message["uint16_field"] = 3;
        message["int16_field"] = -4;
        message["uint32_field"] = 5;
        message["int32_field"] = -6;
        message["uint64_field"] = 7;
        message["int64_field"] = 8;
        message["double_field"] = 9.0;
        message["float_field"] = 10.0;
        message["char_arr_field"] = "Hello World!";
        message["float_arr_field"] = std::vector<float>{1.0, 2.0, 3.0};
        message["int32_arr_field"] = std::vector<int32_t>{4, 5, 6};
        CHECK_EQ(message.toString(),
                 "Message ID 9915 (BIG_MESSAGE) \n  char_arr_field: \"Hello World!\"\n  double_field: 9\n  float_arr_field: 1, 2, 3\n  float_field: 10\n  int16_field: -4\n  int32_arr_field: 4, 5, 6\n  int32_field: -6\n  int64_field: 8\n  int8_field: -2\n  uint16_field: 3\n  uint32_field: 5\n  uint64_field: 7\n  uint8_field: 1\n");
    }

    SUBCASE("Sign a packet") {

        std::array<uint8_t, 32> key;
        for (int i = 0 ; i < 32; i++) key[i] = i;

        uint64_t timestamp = 770479200;

        // Attempt to access signature before signed (const & non-const versions)
        const auto const_message = message_set.create("UINT8_ONLY_MESSAGE");
        CHECK_THROWS_AS((void)message.signature(), std::runtime_error);
        CHECK_THROWS_AS((void)const_message.signature(), std::runtime_error);

        uint32_t wire_size = message.finalize(5, {6, 7}, key, timestamp);

        CHECK_EQ(wire_size, 26);
        CHECK(message.header().isSigned());
        CHECK_NE(message.signature().signature(), 0);
        CHECK_EQ(message.signature().timestamp(), timestamp);
        CHECK(message.validate(key));
    }

}
