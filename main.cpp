#include <iostream>
#include "DynamicMessage.h"
#include "tinyxml2/tinyxml2.h"


bool isPrefix(std::string_view prefix, std::string_view full)
{
    return prefix == full.substr(0, prefix.size());
}


libmavlink::FieldType parseFieldType(const std::string &field_type_string) {
    int size = 1;
    size_t array_notation_start_idx = field_type_string.find('[');
    std::string base_type_substr;
    if (array_notation_start_idx != std::string::npos) {
        auto size_substr = field_type_string.substr(
                array_notation_start_idx + 1, field_type_string.length() - array_notation_start_idx - 2);
        size = std::stoi(size_substr);
    }

    if (isPrefix("uint8_t", field_type_string)) {
        return {libmavlink::FieldType::BaseType::UINT8, size};
    } else if (isPrefix("uint16_t", field_type_string)) {
        return {libmavlink::FieldType::BaseType::UINT16, size};
    } else if (isPrefix("uint32_t", field_type_string)) {
        return {libmavlink::FieldType::BaseType::UINT32, size};
    } else if (isPrefix("uint64_t", field_type_string)) {
        return {libmavlink::FieldType::BaseType::UINT64, size};
    } else if (isPrefix("int8_t", field_type_string)) {
        return {libmavlink::FieldType::BaseType::INT8, size};
    } else if (isPrefix("int16_t", field_type_string)) {
        return {libmavlink::FieldType::BaseType::INT16, size};
    } else if (isPrefix("int32_t", field_type_string)) {
        return {libmavlink::FieldType::BaseType::INT32, size};
    } else if (isPrefix("int64_t", field_type_string)) {
        return {libmavlink::FieldType::BaseType::INT64, size};
    } else if (isPrefix("char", field_type_string)) {
        return {libmavlink::FieldType::BaseType::CHAR, size};
    } else if (isPrefix("float", field_type_string)) {
        return {libmavlink::FieldType::BaseType::FLOAT, size};
    } else if (isPrefix("double", field_type_string)) {
        return {libmavlink::FieldType::BaseType::DOUBLE, size};
    }
    return {libmavlink::FieldType::BaseType::UNKNOWN, 0};
}

int main() {

    tinyxml2::XMLDocument document;
    document.LoadFile("/home/thomas/projects/mavlink/message_definitions/v1.0/common.xml");

    auto messages_node = document.RootElement()->FirstChildElement("messages");

    std::vector<libmavlink::DynamicMessage> out_messages;

    for (auto message = messages_node->FirstChildElement();
        message != nullptr;
        message = message->NextSiblingElement()) {

        libmavlink::DynamicMessageBuilder builder{
            message->Attribute("name"),
            std::stoi(message->Attribute("id"))
        };

        std::string description;

        bool in_extension_fields = false;
        for (auto field = message->FirstChildElement();
            field != nullptr;
            field = field->NextSiblingElement()) {

            if (std::strcmp(field->Value(), "description") == 0) {
                description = field->GetText();
            } else if (std::strcmp(field->Value(), "extensions") == 0) {
                in_extension_fields = true;
            } else if (std::strcmp(field->Value(), "field") == 0) {
                // parse the field
                auto type = parseFieldType(field->Attribute("type"));
                auto name = field->Attribute("name");

                if (!in_extension_fields) {
                    builder.addField(name, type);
                } else {
                    builder.addExtensionField(name, type);
                }
            }
        }
        out_messages.emplace_back(builder.build());
    }

    for (const auto &m : out_messages) {
        std::cout << "." << std::endl;
    }

    return 0;
}
