//
// Created by thomas on 05.01.23.
//

#ifndef MAV_DYNAMICMESSAGE_H
#define MAV_DYNAMICMESSAGE_H

#include <memory>
#include <array>
#include <utility>
#include <variant>
#include "MessageDefinition.h"
#include "utils.h"

namespace mav {

    // forward declared MessageSet
    class MessageSet;

    class Message {
        friend MessageSet;
    private:

        std::array<uint8_t, MessageDefinition::MAX_MESSAGE_SIZE> _backing_memory{};
        const MessageDefinition& _message_definition;

        explicit Message(const MessageDefinition &message_definition) :
            _message_definition(message_definition) {
        }

        Message(const MessageDefinition &message_definition,
                std::array<uint8_t, MessageDefinition::MAX_MESSAGE_SIZE> &&backing_memory) :
                _message_definition(message_definition),
                _backing_memory(std::move(backing_memory)) {}


        template <typename T>
        void _writeSingle(const Field &field, const T &v, int in_field_offset = 0) {
            // make sure that we only have simplistic base types here
            static_assert(is_any<T,
                    char, uint8_t, int8_t, uint16_t, int16_t, uint32_t, int32_t, uint64_t, int64_t, float, double
            >::value, "Can not set this data type to a mavlink message field.");
            // We serialize to the data type given in the field definition, not the data type used in the API.
            // This allows to use compatible data types in the API, but have them serialized to the correct data type.
            int offset = field.offset + in_field_offset;
            uint8_t* target = _backing_memory.data() + offset;

            switch (field.type.base_type) {
                case FieldType::BaseType::CHAR: return serialize(static_cast<char>(v), target);
                case FieldType::BaseType::UINT8: return serialize(static_cast<uint8_t>(v), target);
                case FieldType::BaseType::UINT16: return serialize(static_cast<uint16_t>(v), target);
                case FieldType::BaseType::UINT32: return serialize(static_cast<uint32_t>(v), target);
                case FieldType::BaseType::UINT64: return serialize(static_cast<uint64_t>(v), target);
                case FieldType::BaseType::INT8: return serialize(static_cast<int8_t>(v), target);
                case FieldType::BaseType::INT16: return serialize(static_cast<int16_t>(v), target);
                case FieldType::BaseType::INT32: return serialize(static_cast<int32_t>(v), target);
                case FieldType::BaseType::INT64: return serialize(static_cast<int64_t>(v), target);
                case FieldType::BaseType::FLOAT: return serialize(static_cast<float>(v), target);
                case FieldType::BaseType::DOUBLE: return serialize(static_cast<double>(v), target);
                case FieldType::BaseType::UNKNOWN: return;
            }
        }

        template <typename T>
        inline T _readSingle(const Field &field, int in_field_offset = 0) const {
            const uint8_t* b_ptr = _backing_memory.data() + field.offset + in_field_offset;
            switch (field.type.base_type) {
                case FieldType::BaseType::CHAR: return static_cast<T>(deserialize<char>(b_ptr));
                case FieldType::BaseType::UINT8: return static_cast<T>(deserialize<uint8_t>(b_ptr));
                case FieldType::BaseType::UINT16: return static_cast<T>(deserialize<uint16_t>(b_ptr));
                case FieldType::BaseType::UINT32: return static_cast<T>(deserialize<uint32_t>(b_ptr));
                case FieldType::BaseType::UINT64: return static_cast<T>(deserialize<uint64_t>(b_ptr));
                case FieldType::BaseType::INT8: return static_cast<T>(deserialize<int8_t>(b_ptr));
                case FieldType::BaseType::INT16: return static_cast<T>(deserialize<int16_t>(b_ptr));
                case FieldType::BaseType::INT32: return static_cast<T>(deserialize<int32_t>(b_ptr));
                case FieldType::BaseType::INT64: return static_cast<T>(deserialize<int64_t>(b_ptr));
                case FieldType::BaseType::FLOAT: return static_cast<T>(deserialize<float>(b_ptr));
                case FieldType::BaseType::DOUBLE: return static_cast<T>(deserialize<double>(b_ptr));
                case FieldType::BaseType::UNKNOWN: return T{};
            }
            return T{};
        }

    public:

        static inline Message _instantiateFromMemory(const MessageDefinition &definition,
                                          std::array<uint8_t, MessageDefinition::MAX_MESSAGE_SIZE> &&backing_memory) {
            return Message{definition, std::move(backing_memory)};
        }

        class _initPairType {
        public:
            const std::string key;
            const std::variant<char, int, long, float, double, std::string> var;

            _initPairType(std::string key, const std::string &value) : key(std::move(key)), var{value} {};
            _initPairType(std::string key, int value) : key(std::move(key)), var{value} {};
            _initPairType(std::string key, float value) : key(std::move(key)), var{value} {};
            _initPairType(std::string key, double value) : key(std::move(key)), var{value} {};
        };

        template <typename MessageType>
        class _accessorType {
        private:
            const std::string &_field_name;
            MessageType &_message;
            int _array_index;

        public:
            _accessorType(const std::string &field_name, MessageType &message, int array_index) :
                _field_name(field_name), _message(message), _array_index(array_index) {}

            template <typename T>
            void operator=(const T& val) {
                _message.template set(_field_name, val, _array_index);
            }

            template <typename T>
            operator T() const {
                return _message.template get<T>(_field_name, _array_index);
            }

            _accessorType operator[](int array_index) const {
                return _accessorType{_field_name, _message, array_index};
            }
        };

        [[nodiscard]] const MessageDefinition& type() const {
            return _message_definition;
        }

        [[nodiscard]] int id() const {
            return _message_definition.id();
        }

        [[nodiscard]] const std::string& name() const {
            return _message_definition.name();
        }

        [[nodiscard]] Header<const uint8_t*> header() const {
            return Header<const uint8_t*>(_backing_memory.data());
        }

        [[nodiscard]] Header<uint8_t*> header() {
            return Header<uint8_t*>(_backing_memory.data());
        }

        void set(std::initializer_list<_initPairType> init) {
            for (const auto &pair : init) {
                if (auto* v_char = std::get_if<char>(&pair.var)) {
                    set(pair.key, *v_char);
                } else if (auto* v_int = std::get_if<int>(&pair.var)) {
                    set(pair.key, *v_int);
                } else if (auto* v_long = std::get_if<long>(&pair.var)) {
                    set(pair.key, *v_long);
                } else if (auto* v_float = std::get_if<float>(&pair.var)) {
                    set(pair.key, *v_float);
                } else if (auto* v_double = std::get_if<double>(&pair.var)) {
                    set(pair.key, *v_double);
                } else if (auto* v_string = std::get_if<std::string>(&pair.var)) {
                    setFromString(pair.key, *v_string);
                }
            }
        }

        template <typename T>
        Message& set(const std::string &field_key, T v, int array_index = 0) {
            auto field = _message_definition.fieldForName(field_key);

            if constexpr(is_string<T>::value) {
                setFromString(field_key, v);
            }

            else if constexpr(is_iterable<T>::value) {
                auto begin = std::begin(v);
                auto end = std::end(v);
                if ((end - begin) > field.type.size) {
                    throw std::out_of_range(StringFormat() << "Data of length " << (end - begin) <<
                        " does not fit in field with size " << field.type.size << StringFormat::end);
                }

                for (int i = 0; begin != end; (void)++begin, ++i) {
                    _writeSingle(field, *begin, (i * field.type.baseSize()));
                }
            } else {
                if (array_index < 0 || array_index >= field.type.size) {
                    throw std::out_of_range(StringFormat() << array_index <<
                        " is out of range for field " << field_key << StringFormat::end);
                }

                _writeSingle(field, v, array_index * field.type.baseSize());
                return *this;
            }
        }


        Message& setFromString(const std::string &field_key, const std::string &v) {
            auto field = _message_definition.fieldForName(field_key);
            if (field.type.base_type != FieldType::BaseType::CHAR) {
                throw std::runtime_error(StringFormat() << "Field " << field_key <<
                                                        " is not of type char" << StringFormat::end);
            }
            if (v.size() > field.type.size) {
                throw std::out_of_range(StringFormat() << "String of length " << v.size() <<
                                                       " does not fit in field with size " << field.type.size <<
                                                       StringFormat::end);
            }
            int i = 0;
            for (char c : v) {
                _writeSingle(field, c, i * field.type.baseSize());
                i++;
            }
            return *this;
        }


        template <typename T>
        T get(const std::string &field_key, int array_index = 0) const {
            if constexpr(is_string<T>::value) {
                return getAsString(field_key);
            } else if constexpr(is_iterable<T>::value) {
                auto field = _message_definition.fieldForName(field_key);
                std::decay_t<T> ret_value;

                // handle std::vector: Dynamically resize for convenience
                if constexpr(std::is_same<T, std::vector<typename T::value_type>>::value) {
                    ret_value.resize(field.type.size);
                }

                if (ret_value.size() < field.type.size) {
                    throw std::out_of_range(StringFormat() << "Array of size " << field.type.size <<
                        " can not fit in return type of size " << ret_value.size() << StringFormat::end);
                }

                for (int i=0; i<field.type.size; i++) {
                    ret_value[i] = _readSingle<typename T::value_type>(field, i * field.type.baseSize());
                }
                return ret_value;
            } else {
                auto field = _message_definition.fieldForName(field_key);
                if (array_index < 0 || array_index >= field.type.size) {
                    throw std::out_of_range(StringFormat() << "Index " << array_index <<
                                                           " is out of range for field " << field_key << StringFormat::end);
                }
                return _readSingle<T>(field, array_index * field.type.baseSize());
            }
        }

        [[nodiscard]] std::string getAsString(const std::string &field_key) const {
            auto field = _message_definition.fieldForName(field_key);
            if (field.type.base_type != FieldType::BaseType::CHAR) {
                throw std::runtime_error(StringFormat() << "Field " << field_key <<
                                                        " is not of type char" << StringFormat::end);
            }
            return std::string{reinterpret_cast<const char*>(_backing_memory.data() + field.offset),
                               static_cast<std::string::size_type>(field.type.size)};
        }


        _accessorType<const Message> operator[](const std::string &field_name) const {
            return _accessorType<const Message>{field_name, *this, 0};
        }

        _accessorType<Message> operator[](const std::string &field_name) {
            return _accessorType<Message>{field_name, *this, 0};
        }


        using variant_type = std::variant<
                char,
                int8_t,
                uint8_t,
                int16_t,
                uint16_t,
                int32_t,
                uint32_t,
                int64_t,
                uint64_t,
                float,
                double,
                std::string,
                std::vector<int8_t>,
                std::vector<uint8_t>,
                std::vector<int16_t>,
                std::vector<uint16_t>,
                std::vector<int32_t>,
                std::vector<uint32_t>,
                std::vector<int64_t>,
                std::vector<uint64_t>,
                std::vector<float>,
                std::vector<double>
        >;

        [[nodiscard]] variant_type getAsNativeTypeInVariant(const std::string &field_key) const {
            auto field = _message_definition.fieldForName(field_key);
            if (field.type.size <= 1) {
                switch (field.type.base_type) {
                    case FieldType::BaseType::CHAR: return get<char>(field_key);
                    case FieldType::BaseType::UINT8: return get<uint8_t>(field_key);
                    case FieldType::BaseType::UINT16: return get<uint16_t>(field_key);
                    case FieldType::BaseType::UINT32: return get<uint32_t>(field_key);
                    case FieldType::BaseType::UINT64: return get<uint64_t>(field_key);
                    case FieldType::BaseType::INT8: return get<int8_t>(field_key);
                    case FieldType::BaseType::INT16: return get<int16_t>(field_key);
                    case FieldType::BaseType::INT32: return get<int32_t>(field_key);
                    case FieldType::BaseType::INT64: return get<int64_t>(field_key);
                    case FieldType::BaseType::FLOAT: return get<float>(field_key);
                    case FieldType::BaseType::DOUBLE: return get<double>(field_key);
                    case FieldType::BaseType::UNKNOWN: return -1;
                }
            } else {
                switch (field.type.base_type) {
                    case FieldType::BaseType::CHAR: return get<std::string>(field_key);
                    case FieldType::BaseType::UINT8: return get<std::vector<uint8_t>>(field_key);
                    case FieldType::BaseType::UINT16: return get<std::vector<uint16_t>>(field_key);
                    case FieldType::BaseType::UINT32: return get<std::vector<uint32_t>>(field_key);
                    case FieldType::BaseType::UINT64: return get<std::vector<uint64_t>>(field_key);
                    case FieldType::BaseType::INT8: return get<std::vector<int8_t>>(field_key);
                    case FieldType::BaseType::INT16: return get<std::vector<int16_t>>(field_key);
                    case FieldType::BaseType::INT32: return get<std::vector<int32_t>>(field_key);
                    case FieldType::BaseType::INT64: return get<std::vector<int64_t>>(field_key);
                    case FieldType::BaseType::FLOAT: return get<std::vector<float>>(field_key);
                    case FieldType::BaseType::DOUBLE: return get<std::vector<double>>(field_key);
                    case FieldType::BaseType::UNKNOWN: return -1;
                }
            }
        }



        [[nodiscard]] uint32_t finalize(uint8_t seq, const Identifier &sender) {
            auto last_nonzero = std::find_if(_backing_memory.rend() -
                    MessageDefinition::HEADER_SIZE - _message_definition.maxPayloadSize(),
                    _backing_memory.rend(), [](const auto &v) {
                return v != 0;
            });

            int payload_size = std::max(
                    static_cast<int>(std::distance(last_nonzero, _backing_memory.rend()))
                            - MessageDefinition::HEADER_SIZE, 1);

            header().magic() = 0xFD;
            header().len() = payload_size;
            header().incompatFlags() = 0;
            header().compatFlags() = 0;
            header().seq() = seq;
            header().systemId() = sender.system_id;
            header().componentId() = sender.component_id;
            header().msgId() = _message_definition.id();

            CRC crc;
            crc.accumulate(_backing_memory.begin() + 1, _backing_memory.begin() + payload_size);
            uint8_t* crc_destination = _backing_memory.data() + MessageDefinition::HEADER_SIZE + payload_size;
            serialize(crc.crc16(), crc_destination);

            return MessageDefinition::HEADER_SIZE + payload_size + MessageDefinition::CHECKSUM_SIZE;
        }

        [[nodiscard]] const uint8_t* data() const {
            return _backing_memory.data();
        };
    };


#endif //MAV_DYNAMICMESSAGE_H

} // namespace libmavlink`