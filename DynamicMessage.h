//
// Created by thomas on 05.01.23.
//

#ifndef LIBMAVLINK_DYNAMICMESSAGE_H
#define LIBMAVLINK_DYNAMICMESSAGE_H

#include <map>
#include <vector>
#include <algorithm>
#include <tuple>

#include "CRC.h"

namespace libmavlink {



    class FieldType {
    public:
        enum class BaseType {
            CHAR,
            UINT8,
            UINT16,
            UINT32,
            UINT64,
            INT8,
            INT16,
            INT32,
            INT64,
            FLOAT,
            DOUBLE,
            UNKNOWN
        };

        BaseType base_type;
        int size;

        [[nodiscard]] int baseSize() const {
            switch(base_type) {
                case BaseType::CHAR: return 1;
                case BaseType::UINT8: return 1;
                case BaseType::UINT16: return 2;
                case BaseType::UINT32: return 4;
                case BaseType::UINT64: return 8;
                case BaseType::INT8: return 1;
                case BaseType::INT16: return 2;
                case BaseType::INT32: return 4;
                case BaseType::INT64: return 8;
                case BaseType::FLOAT: return 4;
                case BaseType::DOUBLE: return 8;
                case BaseType::UNKNOWN: return 0;
            }
            return 0;
        }

        FieldType(FieldType::BaseType base_type_, int size_) : base_type(base_type_), size(size_) {}
    };

    struct Field {
        FieldType type;
        int offset;
    };


    class Header {
    public:
        static constexpr int HEADER_SIZE = 18;
    };

    // Forward declared builder class
    class DynamicMessageBuilder;

    class DynamicMessage {
        friend DynamicMessageBuilder;
    private:
        std::string _name;
        int _id;
        char *_data;
        size_t _length;
        uint8_t _crc_extra = 0;
        std::map<std::string, Field> _fields;

    public:
        DynamicMessage(const std::string &name, int id) : _name(name), _id(id)
        {}
    };


    class DynamicMessageBuilder {
    private:

        struct NamedField {
            std::string name;
            FieldType type;
        };

        static const std::map<FieldType::BaseType, const std::string> TYPE_CRC_STRING_MAP;

        DynamicMessage _result;

        std::vector<NamedField> _fields;
        std::vector<NamedField> _extension_fields;

    public:
        DynamicMessageBuilder(const std::string &name, int id) : _result(name, id) {}

        DynamicMessageBuilder& addField(const std::string &name, const FieldType &type) {
            _fields.emplace_back(NamedField{name, type});
            return *this;
        }

        DynamicMessageBuilder& addExtensionField(const std::string &name, const FieldType &type) {
            _extension_fields.emplace_back(NamedField{name, type});
            return *this;
        }

        DynamicMessage build() {
            // As to mavlink spec, all main fields are sorted by their data type size
            std::stable_sort(_fields.begin(), _fields.end(),
                      [](const NamedField &a, const NamedField &b) -> bool {
                return a.type.baseSize() > b.type.baseSize();
            });

            int offset = Header::HEADER_SIZE;
            CRC crc_extra;
            crc_extra.accumulate(_result._name);
            crc_extra.accumulate(" ");

            for (const auto &field : _fields) {
                const auto &type = field.type;
                _result._fields.insert({field.name, {type, offset}});
                offset = offset + (type.baseSize() * type.size);
                crc_extra.accumulate(TYPE_CRC_STRING_MAP.at(type.base_type));
                crc_extra.accumulate(" ");
                crc_extra.accumulate(field.name);
                crc_extra.accumulate(" ");
                // in case this is an array (more than 1 element), we also have to add the array size
                if (type.size > 1) {
                    crc_extra.accumulate(static_cast<uint8_t>(type.size));
                }
            }
            _result._crc_extra = crc_extra.crc8();

            for (const auto &field : _extension_fields) {
                const auto &type = field.type;
                _result._fields.insert({field.name, {type, offset}});
                offset = offset + (type.baseSize() * type.size);
            }
            return _result;
        }
    };

    const std::map<FieldType::BaseType, const std::string> DynamicMessageBuilder::TYPE_CRC_STRING_MAP = {
            {FieldType::BaseType::FLOAT, "float"},
            {FieldType::BaseType::DOUBLE, "double"},
            {FieldType::BaseType::CHAR, "char"},
            {FieldType::BaseType::INT8, "int8_t"},
            {FieldType::BaseType::UINT8, "uint8_t"},
            {FieldType::BaseType::INT16, "int16_t"},
            {FieldType::BaseType::UINT16, "uint16_t"},
            {FieldType::BaseType::INT32, "int32_t"},
            {FieldType::BaseType::UINT32, "uint32_t"},
            {FieldType::BaseType::INT64, "int64_t"},
            {FieldType::BaseType::UINT64, "uint64_t"},
            {FieldType::BaseType::UNKNOWN, ""}
    };


#endif //LIBMAVLINK_DYNAMICMESSAGE_H

} // namespace libmavlink