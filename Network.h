//
// Created by thomas on 13.01.23.
//

#ifndef MAV_NETWORK_H
#define MAV_NETWORK_H

#include <list>
#include <thread>
#include <array>
#include <thread>
#include <atomic>
#include "Connection.h"
#include "utils.h"

namespace mav {


    class NetworkError : public std::runtime_error {
    public:
        explicit NetworkError(const char* message) : std::runtime_error(message) {}
        explicit NetworkError(const std::string& message) : std::runtime_error(message) {}
    };

    class NetworkClosed : public NetworkError {
    public:
        explicit NetworkClosed(const char* message) : NetworkError(message) {}
        explicit NetworkClosed(const std::string& message) : NetworkError(message) {}
    };

    class NetworkInterfaceInterrupt : public std::exception {};

    class NetworkInterface {
    public:
        virtual void close() const = 0;
        virtual void send(const uint8_t* data, uint32_t size) const = 0;
        virtual void receive(uint8_t* destination, uint32_t size) const = 0;
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

            std::array<uint8_t, MessageDefinition::MAX_MESSAGE_SIZE> backing_memory{};
            while (true) {
                // synchronize
                while (!_checkMagicByte()) {}

                //auto backing_memory = std::make_shared<std::vector<uint8_t>>(280);


                backing_memory[0] = 0xFD;
                _interface.receive(backing_memory.data() + 1, MessageDefinition::HEADER_SIZE -1);
                Header header{backing_memory.data()};
                const bool message_is_signed = header.incompatFlags() & 0x01;
                const int wire_length = MessageDefinition::HEADER_SIZE + header.len() + MessageDefinition::CHECKSUM_SIZE +
                                  (message_is_signed ? MessageDefinition::SIGNATURE_SIZE : 0);
                _interface.receive(backing_memory.data() + MessageDefinition::HEADER_SIZE,
                                   wire_length - MessageDefinition::HEADER_SIZE);
                const int crc_offset = MessageDefinition::HEADER_SIZE + header.len();

                auto definition_opt = _message_set.getMessageDefinition(header.msgId());
                if (!definition_opt) {
                    // we do not know this message we can not do anything here, not even check the CRC,
                    // do nothing and continue
                    continue;
                }
                auto &definition = definition_opt.get();

                CRC crc;
                crc.accumulate(backing_memory.begin() + 1, backing_memory.begin() + crc_offset);
                crc.accumulate(definition.crcExtra());
                auto crc_received = deserialize<uint16_t>(backing_memory.data() + crc_offset);
                if (crc_received != crc.crc16()) {
                    // crc error. Try to re-sync.
                    continue;
                }

                return Message::_instantiateFromMemory(definition, std::move(backing_memory));
            }
        }
    };


    class NetworkRuntime {
    private:
        std::atomic_bool _should_terminate{false};
        std::thread _receive_thread;

        NetworkInterface& _interface;
        MessageSet& _message_set;
        StreamParser _parser;
        Identifier _own_id;
        std::list<std::reference_wrapper<Connection>> _connections;
        uint8_t _seq = 0;


        void _sendMessage(Message &message) {
            int wire_length = static_cast<int>(message.finalize(_seq++, _own_id));
            _interface.send(message.data(), wire_length);
        }

        void _heartbeat_thread() {

        }

        void _receive_thread_function() {
            while (!_should_terminate.load()) {
                try {
                    auto message = _parser.next();
                    for (auto& connection : _connections) {
                        connection.get().consumeMessageFromNetwork(message);
                    }
                } catch (NetworkError &e) {
                    std::cerr << "Network failed " << e.what() << std::endl;
                    _should_terminate.store(true);
                } catch (NetworkInterfaceInterrupt &e) {
                    _should_terminate.store(true);
                }
            }
        }


    public:
        NetworkRuntime(const Identifier &own_id, MessageSet &message_set, NetworkInterface &interface) :
                _own_id(own_id), _message_set(message_set),
                _interface(interface), _parser(_message_set, _interface) {

            _receive_thread = std::thread{
                [this]() {
                    _receive_thread_function();
                }
            };
        }

        void addConnection(Connection &connection) {
            connection.template setSendMessageToNetworkFunc([this](Message &message){
                this->_sendMessage(message);
            });
            _connections.emplace_back(connection);
        }

        void stop() {
            _interface.close();
            _should_terminate.store(true);
            if (_receive_thread.joinable()) {
                _receive_thread.join();
            }
        }

        ~NetworkRuntime() {
            stop();
        }
    };
}




#endif //MAV_NETWORK_H
