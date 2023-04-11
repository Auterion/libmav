/****************************************************************************
 * 
 * Copyright (c) 2023, libmav development team
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
#ifndef MAV_MESSAGESET_H
#define MAV_MESSAGESET_H

#include <utility>
#include <filesystem>
#include <memory>
#include <optional>

#include "MessageDefinition.h"
#include "Message.h"

#include "rapidxml/rapidxml.hpp"
#include "rapidxml/rapidxml_utils.hpp"

namespace mav {

    class XMLParser {
    private:

        std::shared_ptr<rapidxml::file<>> _source_file;
        std::shared_ptr<rapidxml::xml_document<>> _document;
        std::string _root_xml_folder_path;

        XMLParser(
                std::shared_ptr<rapidxml::file<>> source_file,
                std::shared_ptr<rapidxml::xml_document<>> document,
                const std::string &root_xml_folder_path) :
                _source_file(std::move(source_file)),
                _document(std::move(document)),
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
            throw std::runtime_error("Unknown field type: " + field_type_string);
        }

    public:

        static XMLParser forFile(const std::string &file_name) {
            auto file = std::make_shared<rapidxml::file<>>(file_name.c_str());
            auto doc = std::make_shared<rapidxml::xml_document<>>();
            doc->parse<0>(file->data());

            return {file, doc, std::filesystem::path{file_name}.parent_path().string()};
        }

        static XMLParser forXMLString(const std::string &xml_string) {
            // pass by value on purpose, rapidxml mutates the string on parse
            auto istream = std::istringstream(xml_string);
            auto file = std::make_shared<rapidxml::file<>>(istream);
            auto doc = std::make_shared<rapidxml::xml_document<>>();
            doc->parse<0>(file->data());
            return {file, doc, ""};
        }


        void parse(std::map<std::string, uint64_t> &out_enum,
                   std::map<std::string, std::shared_ptr<const MessageDefinition>> &out_messages,
                   std::map<int, std::shared_ptr<const MessageDefinition>> &out_message_ids) {

            auto root_node = _document->first_node("mavlink");
            if (!root_node) {
                throw std::runtime_error("Root node \"mavlink\" not found");
            }

            for (auto include_element = root_node->first_node("include");
                include_element != nullptr;
                include_element = include_element->next_sibling("include")) {

                const std::string include_name = include_element->value();
                auto sub_parser = XMLParser::forFile(
                        (std::filesystem::path{_root_xml_folder_path} / include_name).string());
                sub_parser.parse(out_enum, out_messages, out_message_ids);
            }

            auto enums_node = root_node->first_node("enums");
            if (enums_node) {
                for (auto enum_node = enums_node->first_node();
                    enum_node != nullptr;
                    enum_node = enum_node->next_sibling()) {

                    for (auto entry = enum_node->first_node();
                        entry != nullptr;
                        entry = entry->next_sibling()) {
                        if (std::string_view("entry") == entry->name()) {
                            auto entry_name = entry->first_attribute("name")->value();
                            uint64_t value = std::stoul(entry->first_attribute("value")->value());
                            out_enum[entry_name] = value;
                        }
                    }
                }
            }

            auto messages_node = root_node->first_node("messages");
            if (!messages_node) {
                throw std::runtime_error("Node \"messages\" not found");
            }

            for (auto message = messages_node->first_node();
                 message != nullptr;
                 message = message->next_sibling()) {

                const std::string message_name =  message->first_attribute("name")->value();
                MessageDefinitionBuilder builder{
                        message_name,
                        std::stoi(message->first_attribute("id")->value())
                };

                std::string description;

                bool in_extension_fields = false;
                for (auto field = message->first_node();
                     field != nullptr;
                     field = field->next_sibling()) {

                    if (std::string_view{"description"} == field->name()) {
                        description = field->value();
                    } else if (std::string_view{"extensions"} == field->name()) {
                        in_extension_fields = true;
                    } else if (std::string_view{"field"} == field->name()) {
                        // parse the field
                        auto field_type = _parseFieldType(field->first_attribute("type")->value());
                        auto field_name = field->first_attribute("name")->value();

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
        std::map<std::string, uint64_t> _enums;
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


        [[nodiscard]] OptionalReference<const MessageDefinition> getMessageDefinition(const std::string &message_name) const {
            auto message_definition = _messages.find(message_name);
            if (message_definition == _messages.end()) {
                return std::nullopt;
            }
            return *(message_definition->second);
        }

        [[nodiscard]] OptionalReference<const MessageDefinition> getMessageDefinition(int message_id) const {
            auto message_definition = _message_ids.find(message_id);
            if (message_definition == _message_ids.end()) {
                return std::nullopt;
            }
            return *(message_definition->second);
        }

        [[nodiscard]] Message create(const std::string &message_name) const {
            auto message_definition = getMessageDefinition(message_name);
            if (!message_definition) {
                throw std::out_of_range(StringFormat() << "No message of name " << message_name <<
                    " in message set." << StringFormat::end);
            }
            return Message{message_definition.get()};
        }

        [[nodiscard]] Message create(int message_id) const {
            auto message_definition = getMessageDefinition(message_id);
            if (!message_definition) {
                throw std::out_of_range(StringFormat() << "No message of id " << message_id <<
                    " in message set." << StringFormat::end);
            }
            return Message{message_definition.get()};
        }

        [[nodiscard]] uint64_t enum_for(const std::string &key) const {
            auto res = _enums.find(key);
            if (res == _enums.end()) {
                throw std::out_of_range(StringFormat() << "Enum " << key << " not in message set" << StringFormat::end);
            }
            return res->second;
        }

        [[nodiscard]] uint64_t e(const std::string &key) const {
            return enum_for(key);
        }

        [[nodiscard]] int idForMessage(const std::string &message_name) const {
            auto message_definition = _messages.find(message_name);
            if (message_definition == _messages.end()) {
                throw std::out_of_range(StringFormat() << "No message of name " << message_name <<
                                                       " in message set." << StringFormat::end);
            }
            return message_definition->second->id();
        }

        [[nodiscard]] bool contains(const std::string &message_name) const {
            return _messages.find(message_name) != _messages.end();
        }

        [[nodiscard]] bool contains(int message_id) const {
            return _message_ids.find(message_id) != _message_ids.end();
        }


        [[nodiscard]] size_t size() const {
            return _messages.size();
        }
    };
}




#endif //MAV_MESSAGESET_H
