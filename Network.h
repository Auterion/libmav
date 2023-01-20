//
// Created by thomas on 13.01.23.
//

#ifndef LIBMAVLINK_NETWORK_H
#define LIBMAVLINK_NETWORK_H

#include <list>
#include <thread>
#include <array>
#include "Connection.h"
#include "utils.h"

namespace libmavlink {


    class NetworkError : public std::runtime_error {
    public:
        explicit NetworkError(const char* message) : std::runtime_error(message) {}
        explicit NetworkError(const std::string& message) : std::runtime_error(message) {}
    };


    class NetworkInterface {
    public:
        virtual void send(const uint8_t* data, uint32_t size) const = 0;
        virtual void receive(uint8_t* destingation, uint32_t size) const = 0;
    };

    class TCP : public NetworkInterface {
    public:
        void send(const uint8_t* data, uint32_t size) const {

        };

        void receive(uint8_t* data, uint32_t size) const {

        };
    };

    class UDP : public NetworkInterface {
    public:
        void send(const uint8_t* data, uint32_t size) const {

        };

        void receive(uint8_t* data, uint32_t size) const {

        };
    };



    class StreamParser {
    private:

        NetworkInterface& _interface;
        MessageSet& _message_set;

        [[nodiscard]] bool _checkMagicByte() const {
            uint8_t v;
            _interface.receive(&v, 1);
            return v == 0xFD;
        }

    public:
        StreamParser(MessageSet &message_set, NetworkInterface &interface) :
        _interface(interface), _message_set(message_set) {}

        [[nodiscard]] Message next() const {
            while (true) {
                // synchronize
                while (!_checkMagicByte()) {}

                auto backing_memory = std::make_shared<std::vector<uint8_t>>(280);
                (*backing_memory)[0] = 0xFD;
                _interface.receive(backing_memory->data() + 1, MessageDefinition::HEADER_SIZE -1);
                Header header{backing_memory->data()};
                bool message_is_signed = header.incompatFlags() & 0x01;
                int wire_length = MessageDefinition::HEADER_SIZE + header.len() + MessageDefinition::CHECKSUM_SIZE +
                                  (message_is_signed ? MessageDefinition::SIGNATURE_SIZE : 0);
                _interface.receive(wire_length - MessageDefinition::HEADER_SIZE);
                int crc_offset = MessageDefinition::HEADER_SIZE + header.len();

                CRC crc;
                crc.accumulate(backing_memory->begin() + 1, backing_memory->begin() + crc_offset);
                auto crc_received = deserialize<uint16_t>(backing_memory->data() + crc_offset);
                if (crc_received != crc.crc16()) {
                    // crc error. Try to re-sync.
                    continue;
                }

                auto definition = _message_set.getMessageDefinition(header.msgId());

                if (!definition) {
                    // we do not know this message, do nothing and continue
                    continue;
                }

                return Message::instantiate(definition, backing_memory);
            }
        }
    };


    class NetworkRuntime {
    private:
        std::unique_ptr<NetworkInterface> _interface;
        MessageSet& _message_set;
        StreamParser _parser;
        Identifier _own_id;
        std::list<Connection> _connections;
        uint8_t _seq = 0;


        void _sendMessage(const Message &message) {
            int wire_length = message.finalize(_seq++, _own_id);
            _interface->send(message.data(), wire_length);
        }

        void _heartbeat_thread() {

        }

        void _receive_thread_function() {
            auto message = _parser.next();
            for (auto& connection : _connections) {
                connection.consumeMessageFromNetwork(message);
            }
        }


    public:
        template <typename InterfaceType, typename ...Args>
        NetworkRuntime(const Identifier &own_id, MessageSet &message_set, Args ...args) :
                _own_id(own_id), _message_set(message_set),
                _interface(std::make_unique<InterfaceType>(args...)), _parser(_message_set, *_interface) {}

        void addConnection(Connection &connection) {
            connection.template setSendMessageToNetworkFunc([this](const Message &message){
                this->_sendMessage(message);
            });
            _connections.push_back(connection);
        }
    };
}




#endif //LIBMAVLINK_NETWORK_H
