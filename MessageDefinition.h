//
// Created by thomas on 10.01.23.
//

#ifndef LIBMAVLINK_MESSAGEDEFINITION_H
#define LIBMAVLINK_MESSAGEDEFINITION_H

#include <map>
#include <vector>
#include <algorithm>

#include "utils.h"


namespace libmavlink {

    class Identifier {
    public:
        const uint8_t system_id;
        const uint8_t component_id;

        Identifier(uint8_t system_id_, uint8_t component_id_) : system_id(system_id_), component_id(component_id_) {}

        bool operator==(const Identifier &o) const {
            return system_id == o.system_id && component_id == o.component_id;
        }

        bool operator!=(const Identifier &o) const {
            return system_id != o.system_id || component_id != o.component_id;
        }
    };


    class Header {
    private:
        uint8_t *_backing_memory;

        class _MsgId {
        private:
            uint8_t* _ptr;
        public:
            explicit _MsgId(uint8_t *ptr) : _ptr(ptr) {}

            operator int() {
                return static_cast<int>((*static_cast<uint32_t*>(static_cast<void*>(_ptr))) & 0xFFFFFF);
            }

            void operator=(int v) {
                _ptr[0] = static_cast<uint8_t>(v & 0xFF);
                _ptr[1] = static_cast<uint8_t>((v >> 8) & 0xFF);
                _ptr[2] = static_cast<uint8_t>((v >> 16) & 0xFF);
            }
        };

    public:
        explicit Header(uint8_t *backing_memory) : _backing_memory(backing_memory) {}

        [[nodiscard]] inline uint8_t& magic() const {
            return _backing_memory[0];
        }

        [[nodiscard]] inline uint8_t& len() const {
            return _backing_memory[1];
        }

        [[nodiscard]] inline uint8_t& incompatFlags() const {
            return _backing_memory[2];
        }

        [[nodiscard]] inline uint8_t& compatFlags() const {
            return _backing_memory[3];
        }

        [[nodiscard]] inline uint8_t& seq() const {
            return _backing_memory[4];
        }

        [[nodiscard]] inline uint8_t& systemId() const {
            return _backing_memory[5];
        }

        [[nodiscard]] inline uint8_t& componentId() const {
            return _backing_memory[6];
        }

        [[nodiscard]] inline _MsgId msgId() const {
            return _MsgId(_backing_memory + 7);
        }

        [[nodiscard]] inline Identifier source() const {
            return {systemId(), componentId()};
        }
    };

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

        [[nodiscard]] const char* crcNameString() const {
            switch(base_type) {
                case BaseType::CHAR: return "char";
                case BaseType::UINT8: return "uint8_t";
                case BaseType::UINT16: return "uint16_t";
                case BaseType::UINT32: return "uint32_t";
                case BaseType::UINT64: return "uint64_t";
                case BaseType::INT8: return "int8_t";
                case BaseType::INT16: return "int16_t";
                case BaseType::INT32: return "int32_t";
                case BaseType::INT64: return "int64_t";
                case BaseType::FLOAT: return "float";
                case BaseType::DOUBLE: return "double";
                case BaseType::UNKNOWN: return "";
            }
            return "";
        };


        FieldType(FieldType::BaseType base_type_, int size_) : base_type(base_type_), size(size_) {}
    };

    struct Field {
        FieldType type;
        int offset;
    };


    // Forward declared builder class
    class MessageDefinitionBuilder;


    class MessageDefinition {
        friend MessageDefinitionBuilder;
    private:
        std::string _name;
        int _id;
        int _max_buffer_length;
        int _max_payload_length;
        uint8_t _crc_extra = 0;
        std::map<std::string, Field> _fields;


        MessageDefinition(const std::string &name, int id) : _name(name), _id(id)
        {}

    public:
        static constexpr int HEADER_SIZE = 10;
        static constexpr int CHECKSUM_SIZE = 2;
        static constexpr int SIGNATURE_SIZE = 13;

        [[nodiscard]] inline const std::string& name() const {
            return _name;
        }

        [[nodiscard]] inline int id() const {
            return _id;
        }

        [[nodiscard]] inline int maxBufferLength() const {
            return _max_buffer_length;
        }

        [[nodiscard]] inline int maxPayloadSize() const {
            return _max_payload_length;
        }

        [[nodiscard]] const Field& fieldForName(const std::string &field_key) const {
            auto it = _fields.find(field_key);
            if (it == _fields.end()) {
                throw std::out_of_range(StringFormat() << "Field " << field_key << " does not exist in message "
                                                       << _name << "." << StringFormat::end);
            }
            return it->second;
        }
    };


    class MessageDefinitionBuilder {
    private:

        struct NamedField {
            std::string name;
            FieldType type;
        };

        MessageDefinition _result;

        std::vector<NamedField> _fields;
        std::vector<NamedField> _extension_fields;

    public:
        MessageDefinitionBuilder(const std::string &name, int id) : _result(name, id) {}

        MessageDefinitionBuilder& addField(const std::string &name, const FieldType &type) {
            _fields.emplace_back(NamedField{name, type});
            return *this;
        }

        MessageDefinitionBuilder& addExtensionField(const std::string &name, const FieldType &type) {
            _extension_fields.emplace_back(NamedField{name, type});
            return *this;
        }

        MessageDefinition build() {
            // As to mavlink spec, all main fields are sorted by their data type size
            std::stable_sort(_fields.begin(), _fields.end(),
                             [](const NamedField &a, const NamedField &b) -> bool {
                                 return a.type.baseSize() > b.type.baseSize();
                             });

            int offset = MessageDefinition::HEADER_SIZE;
            libmavlink::CRC crc_extra;
            crc_extra.accumulate(_result._name);
            crc_extra.accumulate(" ");

            for (const auto &field : _fields) {
                const auto &type = field.type;
                _result._fields.insert({field.name, {type, offset}});
                offset = offset + (type.baseSize() * type.size);
                crc_extra.accumulate(type.crcNameString());
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
            _result._max_payload_length = offset - MessageDefinition::HEADER_SIZE;
            _result._max_buffer_length = offset + MessageDefinition::CHECKSUM_SIZE + MessageDefinition::SIGNATURE_SIZE;
            return _result;
        }
    };
}

#endif //LIBMAVLINK_MESSAGEDEFINITION_H
