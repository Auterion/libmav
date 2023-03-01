//
// Created by thomas on 01.02.23.
//

#include "doctest.h"
#include "mav/MessageSet.h"
#include "mav/MessageDefinition.h"

using namespace mav;

TEST_CASE("Message set methods") {
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
                <extensions/>
                <field type="uint8_t" name="extension_uint8_field">description</field>
            </message>
        </messages>
    </mavlink>
    )"""");

    REQUIRE(message_set.contains("BIG_MESSAGE"));
    REQUIRE_EQ(message_set.size(), 1);
    auto message = message_set.create("BIG_MESSAGE");
    auto& definition = message.type();

    SUBCASE("Getters") {
        CHECK_EQ(definition.name(), "BIG_MESSAGE");
        CHECK_EQ(definition.id(), 9915);
        CHECK_EQ(definition.fieldDefinitions().size(), 14);
        CHECK_EQ(definition.fieldNames().size(), 14);
    }

    SUBCASE("MAVLink specs") {
        CHECK_EQ(definition.maxBufferLength(), 112);
        CHECK_EQ(definition.crcExtra(), 0x59);
    }

    SUBCASE("Get field for name") {
        CHECK_EQ(definition.fieldForName("uint8_field").type.base_type, FieldType::BaseType::UINT8);
        CHECK_EQ(definition.fieldForName("int8_field").type.base_type, FieldType::BaseType::INT8);
        CHECK_EQ(definition.fieldForName("uint16_field").type.base_type, FieldType::BaseType::UINT16);
        CHECK_EQ(definition.fieldForName("int16_field").type.base_type, FieldType::BaseType::INT16);
        CHECK_EQ(definition.fieldForName("uint32_field").type.base_type, FieldType::BaseType::UINT32);
        CHECK_EQ(definition.fieldForName("int32_field").type.base_type, FieldType::BaseType::INT32);
        CHECK_EQ(definition.fieldForName("uint64_field").type.base_type, FieldType::BaseType::UINT64);
        CHECK_EQ(definition.fieldForName("int64_field").type.base_type, FieldType::BaseType::INT64);
        CHECK_EQ(definition.fieldForName("double_field").type.base_type, FieldType::BaseType::DOUBLE);
        CHECK_EQ(definition.fieldForName("float_field").type.base_type, FieldType::BaseType::FLOAT);
        CHECK_EQ(definition.fieldForName("char_arr_field").type.base_type, FieldType::BaseType::CHAR);
        CHECK_EQ(definition.fieldForName("float_arr_field").type.base_type, FieldType::BaseType::FLOAT);
        CHECK_EQ(definition.fieldForName("int32_arr_field").type.base_type, FieldType::BaseType::INT32);
        CHECK_EQ(definition.fieldForName("char_arr_field").type.size, 20);
        CHECK_EQ(definition.fieldForName("float_arr_field").type.size, 3);
        CHECK_EQ(definition.fieldForName("int32_arr_field").type.size, 3);
    }

    SUBCASE("Get non existing field") {
        CHECK_THROWS_AS(definition.fieldForName("non_existing_field"), std::out_of_range);
    }

    SUBCASE("Contains fields") {
        CHECK(definition.containsField("uint8_field"));
        CHECK(definition.containsField("int8_field"));
        CHECK(definition.containsField("uint16_field"));
        CHECK(definition.containsField("int16_field"));
        CHECK(definition.containsField("uint32_field"));
        CHECK(definition.containsField("int32_field"));
        CHECK(definition.containsField("uint64_field"));
        CHECK(definition.containsField("int64_field"));
        CHECK(definition.containsField("double_field"));
        CHECK(definition.containsField("float_field"));
        CHECK(definition.containsField("char_arr_field"));
        CHECK(definition.containsField("float_arr_field"));
        CHECK(definition.containsField("int32_arr_field"));
        CHECK_FALSE(definition.containsField("non_existing_field"));
    }
}
