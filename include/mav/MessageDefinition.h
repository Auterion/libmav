//
// Created by thomas on 10.01.23.
//

#ifndef MAV_MESSAGEDEFINITION_H
#define MAV_MESSAGEDEFINITION_H

#include <map>
#include <vector>
#include <algorithm>

#include "utils.h"


namespace mav {

    constexpr int ANY_ID = -1;
    constexpr int LIBMAV_DEFAULT_ID = 97;

    constexpr int V1_HEADER_OFFSET = 4;
    constexpr int HEADER_SIZE = 10;

    constexpr int MAX_PAYLOAD_SIZE = 255;
    constexpr int CHECKSUM_SIZE = 2;
    constexpr int SIGNATURE_SIZE = 13;
    constexpr int MAX_MESSAGE_SIZE = MAX_PAYLOAD_SIZE + HEADER_SIZE + CHECKSUM_SIZE + SIGNATURE_SIZE;

    struct ConnectionPartner {
        uint32_t _address;
        uint16_t _port;
        bool _is_uart;

    public:
        ConnectionPartner() = default;

        ConnectionPartner(uint32_t address_, uint16_t port_, bool is_uart_) :
        _address(address_), _port(port_), _is_uart(is_uart_) {}

        bool operator==(const ConnectionPartner& other) const {
            return _address == other._address && _port == other._port && _is_uart == other._is_uart;
        }

        bool operator!=(const ConnectionPartner& other) const {
            return !(*this == other);
        }

        [[nodiscard]] uint32_t address() const {
            return _address;
        }

        [[nodiscard]] uint16_t port() const {
            return _port;
        }

        [[nodiscard]] bool isUart() const {
            return _is_uart;
        }

        [[nodiscard]] bool isBroadcast() const {
            return _address == 0 && _port == 0;
        }

        [[nodiscard]] std::string toString() const {
            if (isUart()) {
                std::stringstream ss;
                ss << "UART " << std::hex << address();
                return ss.str();
            } else {
                std::stringstream ss;
                for (int i = 0; i < 4; i++) {
                    ss << ((address() >> (8 * i)) & 0xFF);
                    if (i < 3) {
                        ss << ".";
                    }
                }
                ss << ":" << port();
                return ss.str();
            }
        }
    };

    struct _ConnectionPartnerHash {
        std::size_t operator()(const ConnectionPartner& k) const {
            return (k.isUart() << 17) | (k.address() << 16) | k.port();
        }
    };


    class Identifier {
    public:
        const int system_id;
        const int component_id;

        Identifier(int system_id_, int component_id_) : system_id(system_id_), component_id(component_id_) {}

        bool operator==(const Identifier &o) const {
            return system_id == o.system_id && component_id == o.component_id;
        }

        bool operator!=(const Identifier &o) const {
            return system_id != o.system_id || component_id != o.component_id;
        }

        bool filter(const Identifier &o) const {
            return (system_id == ANY_ID || system_id == o.system_id) && (component_id == ANY_ID || component_id == o.component_id);
        }
    };

    template <typename BackingMemoryPointerType>
    class Header {
    private:
        BackingMemoryPointerType _backing_memory;
        bool _is_v1;

        class _MsgId {
        private:
            BackingMemoryPointerType _ptr;
            bool _is_v1;
        public:
            explicit _MsgId(BackingMemoryPointerType ptr, bool is_v1) : _ptr(ptr), _is_v1(is_v1) {}

            operator int() const {
                if (_is_v1) {
                    return static_cast<int>(_ptr[0]);
                }
                return static_cast<int>((*static_cast<const uint32_t*>(static_cast<const void*>(_ptr))) & 0xFFFFFF);
            }

            _MsgId& operator=(int v) {
                if (_is_v1) {
                    _ptr[0] = static_cast<uint8_t>(v & 0xFF);
                    return *this;
                }
                _ptr[0] = static_cast<uint8_t>(v & 0xFF);
                _ptr[1] = static_cast<uint8_t>((v >> 8) & 0xFF);
                _ptr[2] = static_cast<uint8_t>((v >> 16) & 0xFF);
                return *this;
            }
        };

    public:
        explicit Header(BackingMemoryPointerType backing_memory, bool is_v1) : _backing_memory(backing_memory), _is_v1(is_v1) {}

        inline uint8_t& magic(){
            if (_is_v1) {
                return _backing_memory[V1_HEADER_OFFSET];
            }
            return _backing_memory[0];
        }

        [[nodiscard]] inline uint8_t magic() const {
            if (_is_v1) {
                return _backing_memory[V1_HEADER_OFFSET];
            }
            return _backing_memory[0];
        }

        inline uint8_t& len() {
            if (_is_v1) {
                return _backing_memory[V1_HEADER_OFFSET + 1];
            }
            return _backing_memory[1];
        }

        [[nodiscard]] inline uint8_t len() const {
            if (_is_v1) {
                return _backing_memory[V1_HEADER_OFFSET + 1];
            }
            return _backing_memory[1];
        }

        inline uint8_t& incompatFlags() {
            return _backing_memory[2];
        }

        [[nodiscard]] inline uint8_t incompatFlags() const {
            return _backing_memory[2];
        }

        inline uint8_t& compatFlags() {
            return _backing_memory[3];
        }

        [[nodiscard]] inline uint8_t compatFlags() const {
            return _backing_memory[3];
        }

        inline uint8_t& seq() {
            if (_is_v1) {
                return _backing_memory[V1_HEADER_OFFSET + 2];
            }
            return _backing_memory[4];
        }

        [[nodiscard]] inline uint8_t seq() const {
            if (_is_v1) {
                return _backing_memory[V1_HEADER_OFFSET + 2];
            }
            return _backing_memory[4];
        }

        inline uint8_t& systemId() {
            if (_is_v1) {
                return _backing_memory[V1_HEADER_OFFSET + 3];
            }
            return _backing_memory[5];
        }

        [[nodiscard]] inline uint8_t systemId() const {
            if (_is_v1) {
                return _backing_memory[V1_HEADER_OFFSET + 3];
            }
            return _backing_memory[5];
        }

        inline uint8_t& componentId() {
            if (_is_v1) {
                return _backing_memory[V1_HEADER_OFFSET + 4];
            }
            return _backing_memory[6];
        }

        [[nodiscard]] inline uint8_t componentId() const {
            if (_is_v1) {
                return _backing_memory[V1_HEADER_OFFSET + 4];
            }
            return _backing_memory[6];
        }

        inline _MsgId msgId() {
            if (_is_v1) {
                return _MsgId(_backing_memory + V1_HEADER_OFFSET + 5, true);
            }
            return _MsgId(_backing_memory + 7, false);
        }

        [[nodiscard]] inline _MsgId msgId() const {
            if (_is_v1) {
                return _MsgId(_backing_memory + V1_HEADER_OFFSET + 5, true);
            }
            return _MsgId(_backing_memory + 7, false);
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
            DOUBLE
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

        [[nodiscard]] inline uint8_t crcExtra() const {
            return _crc_extra;
        }

        [[nodiscard]] const Field& fieldForName(const std::string &field_key) const {
            auto it = _fields.find(field_key);
            if (it == _fields.end()) {
                throw std::out_of_range(StringFormat() << "Field \"" << field_key << "\" does not exist in message "
                                                       << _name << "." << StringFormat::end);
            }
            return it->second;
        }

        [[nodiscard]] bool containsField(const std::string &field_key) const {
            auto it = _fields.find(field_key);
            if (it == _fields.end()) {
                return false;
            }
            return true;
        }

        [[nodiscard]] const std::map<std::string, Field>& fieldDefinitions() const {
            return _fields;
        }

        [[nodiscard]] std::vector<std::string> fieldNames() const {
            std::vector<std::string> keys;
            for(auto const& item: _fields) {
                keys.push_back(item.first);
            }

            return keys;
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

            int offset = HEADER_SIZE;
            CRC crc_extra;
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
            _result._max_payload_length = offset - HEADER_SIZE;
            _result._max_buffer_length = offset + CHECKSUM_SIZE + SIGNATURE_SIZE;
            return _result;
        }
    };
}

#endif //MAV_MESSAGEDEFINITION_H
