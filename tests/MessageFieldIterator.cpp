/****************************************************************************
 *
 * Copyright (c) 2024, libmav development team
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name libmav nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

#include <list>
#include "doctest.h"
#include "mav/MessageSet.h"
#include "mav/MessageFieldIterator.h"

using namespace mav;

TEST_CASE("Message field iterator") {
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
            <field type="float[3]" name="float_arr_field">description</field>
            <field type="int32_t[3]" name="int32_arr_field">description</field>
        </message>
    </messages>
</mavlink>
)"""");

    REQUIRE(message_set.contains("BIG_MESSAGE"));
    REQUIRE_EQ(message_set.size(), 1);

    auto message = message_set.create("BIG_MESSAGE");
    message.set("uint8_field", 1);
    message.set("int8_field", 2);
    message.set("uint16_field", 3);
    message.set("int16_field", 4);
    message.set("uint32_field", 5);
    message.set("int32_field", 6);
    message.set("uint64_field", 7);
    message.set("int64_field", 8);
    message.set("double_field", 9.0);
    message.set("float_field", 10.0);
    message.set("char_arr_field", "Hello World");
    message.set("float_arr_field", std::vector<float>{1.0, 2.0, 3.0});
    message.set("int32_arr_field", std::vector<int32_t>{4, 5, 6});

    SUBCASE("Can iterate through fields of message") {


        std::map<std::string, NativeVariantType> expect_set = {
                {"uint8_field", static_cast<uint8_t>(1)},
                {"int8_field", static_cast<int8_t>(2)},
                {"uint16_field", static_cast<uint16_t>(3)},
                {"int16_field", static_cast<int16_t>(4)},
                {"uint32_field", static_cast<uint32_t>(5)},
                {"int32_field", static_cast<int32_t>(6)},
                {"uint64_field", static_cast<uint64_t>(7)},
                {"int64_field", static_cast<int64_t>(8)},
                {"double_field", static_cast<double>(9.0)},
                {"float_field", static_cast<float>(10.0)},
                {"char_arr_field", "Hello World"},
                {"float_arr_field", std::vector<float>{1.0, 2.0, 3.0}},
                {"int32_arr_field", std::vector<int32_t>{4, 5, 6}}
        };



        for (const auto& [key, value] : FieldIterate(message)) {
            REQUIRE_GT(expect_set.size(), 0);

            auto expect = expect_set.find(key);
            CHECK_NE(expect, expect_set.end());
            CHECK_EQ(value, expect->second);
        }
    }

}