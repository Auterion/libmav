//
// Created by thomas on 10.01.23.
//

#ifndef MAV_MESSAGESET_H
#define MAV_MESSAGESET_H

#include <utility>
#include <filesystem>

#include "MessageDefinition.h"
#include "Message.h"

#include "tinyxml2/tinyxml2.h"

namespace mav {

    class XMLParser {
    private:
        std::shared_ptr<tinyxml2::XMLDocument> _document;
        std::string _root_xml_folder_path;

        XMLParser(
                std::shared_ptr<tinyxml2::XMLDocument> doc,
                const std::string &root_xml_folder_path) :
                _document(std::move(doc)),
                _root_xml_folder_path(root_xml_folder_path) {}


        static bool _isPrefix(std::string_view prefix, std::string_view full) {
            return prefix == full.substr(0, prefix.size());
        }

        static FieldType _parseFieldType(const std::string &field_type_string) {
            int size = 1;
            size_t array_notation_start_idx = field_type_string.find('[');
            std::string base_type_substr;
            if (array_notation_start_idx != std::string::npos) {
                auto size_substr = field_type_string.substr(
                        array_notation_start_idx + 1, field_type_string.length() - array_notation_start_idx - 2);
                size = std::stoi(size_substr);
            }

            if (_isPrefix("uint8_t", field_type_string)) {
                return {FieldType::BaseType::UINT8, size};
            } else if (_isPrefix("uint16_t", field_type_string)) {
                return {FieldType::BaseType::UINT16, size};
            } else if (_isPrefix("uint32_t", field_type_string)) {
                return {FieldType::BaseType::UINT32, size};
            } else if (_isPrefix("uint64_t", field_type_string)) {
                return {FieldType::BaseType::UINT64, size};
            } else if (_isPrefix("int8_t", field_type_string)) {
                return {FieldType::BaseType::INT8, size};
            } else if (_isPrefix("int16_t", field_type_string)) {
                return {FieldType::BaseType::INT16, size};
            } else if (_isPrefix("int32_t", field_type_string)) {
                return {FieldType::BaseType::INT32, size};
            } else if (_isPrefix("int64_t", field_type_string)) {
                return {FieldType::BaseType::INT64, size};
            } else if (_isPrefix("char", field_type_string)) {
                return {FieldType::BaseType::CHAR, size};
            } else if (_isPrefix("float", field_type_string)) {
                return {FieldType::BaseType::FLOAT, size};
            } else if (_isPrefix("double", field_type_string)) {
                return {FieldType::BaseType::DOUBLE, size};
            }
            return {FieldType::BaseType::UNKNOWN, 0};
        }

    public:

        static XMLParser forFile(const std::string &file_name) {
            auto doc = std::make_shared<tinyxml2::XMLDocument>();
            doc->LoadFile(file_name.c_str());
            if (doc->ErrorID() != 0) {
                throw std::runtime_error(doc->ErrorStr());
            }
            return {doc, std::filesystem::path{file_name}.parent_path().string()};
        }

        static XMLParser forXMLString(const std::string &xml_string) {
            auto doc = std::make_shared<tinyxml2::XMLDocument>();
            doc->Parse(xml_string.c_str(), xml_string.size());
            return {doc, ""};
        }


        void parse(std::map<std::string, int> &out_enum,
                   std::map<std::string, std::shared_ptr<const MessageDefinition>> &out_messages,
                   std::map<int, std::shared_ptr<const MessageDefinition>> &out_message_ids) {

            for (auto include_element = _document->RootElement()->FirstChildElement("include");
                include_element != nullptr;
                include_element = include_element->NextSiblingElement("include")) {

                const std::string include_name = include_element->GetText();
                auto sub_parser = XMLParser::forFile(
                        (std::filesystem::path{_root_xml_folder_path} / include_name).string());
                sub_parser.parse(out_enum, out_messages, out_message_ids);
            }

            auto messages_node = _document->RootElement()->FirstChildElement("messages");

            for (auto message = messages_node->FirstChildElement();
                 message != nullptr;
                 message = message->NextSiblingElement()) {

                const std::string message_name =  message->Attribute("name");
                MessageDefinitionBuilder builder{
                        message_name,
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
                        auto field_type = _parseFieldType(field->Attribute("type"));
                        auto field_name = field->Attribute("name");

                        if (!in_extension_fields) {
                            builder.addField(field_name, field_type);
                        } else {
                            builder.addExtensionField(field_name, field_type);
                        }
                    }
                }
                auto definition = std::make_shared<const MessageDefinition>(builder.build());
                out_messages.emplace(message_name, definition);
                out_message_ids.emplace(definition->id(), definition);
            }
        }
    };

    class MessageSet {
    private:
        std::map<std::string, int> _enums;
        std::map<std::string, std::shared_ptr<const MessageDefinition>> _messages;
        std::map<int, std::shared_ptr<const MessageDefinition>> _message_ids;

    public:
        MessageSet() = default;

        explicit MessageSet(const std::string &xml_path) {
            addFromXML(xml_path);
        }

        void addFromXML(const std::string &file_path) {
            XMLParser parser = XMLParser::forFile(file_path);
            parser.parse(_enums, _messages, _message_ids);
        }

        void addFromXMLString(const std::string &xml_string) {
            XMLParser parser = XMLParser::forXMLString(xml_string);
            parser.parse(_enums, _messages,_message_ids);
        }


        [[nodiscard]] std::shared_ptr<const MessageDefinition> getMessageDefinition(const std::string &message_name) const {
            auto message_definition = _messages.find(message_name);
            if (message_definition == _messages.end()) {
                return nullptr;
            }
            return message_definition->second;
        }

        [[nodiscard]] std::shared_ptr<const MessageDefinition> getMessageDefinition(int message_id) const {
            auto message_definition = _message_ids.find(message_id);
            if (message_definition == _message_ids.end()) {
                return nullptr;
            }
            return message_definition->second;
        }

        Message createMessage(const std::string &message_name) {
            auto message_definition = getMessageDefinition(message_name);
            if (!message_definition) {
                throw std::out_of_range(StringFormat() << "No message of name " << message_name <<
                    " in message set." << StringFormat::end);
            }
            return Message{message_definition};
        }

        [[nodiscard]] int idForMessage(const std::string &message_name) const {
            auto message_definition = _messages.find(message_name);
            if (message_definition == _messages.end()) {
                throw std::out_of_range(StringFormat() << "No message of name " << message_name <<
                                                       " in message set." << StringFormat::end);
            }
            return message_definition->second->id();
        }


        [[nodiscard]] size_t size() const {
            return _messages.size();
        }
    };
}




#endif //MAV_MESSAGESET_H
