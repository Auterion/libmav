//
// Created by thomas on 20.11.23.
//

#ifndef MAV_MESSAGEBUILDER_H
#define MAV_MESSAGEBUILDER_H

#include "mav/MessageSet.h"
#include "mav/Message.h"

namespace mav {
    template <typename BuilderType>
    class MessageBuilder {
    protected:
        mav::Message _message;

        template <typename ARG>
        BuilderType& _set(const std::string& key, ARG value) {
            _message[key] = value;
            return *static_cast<BuilderType*>(this);
        }

    public:
        MessageBuilder(const MessageSet &message_set, const std::string& message_name) :
                _message(message_set.create(message_name)) {}

        [[nodiscard]] mav::Message build() const {
            return _message;
        }

        operator Message() const {
            return _message;
        }
    };
}



#endif //MAV_MESSAGEBUILDER_H
