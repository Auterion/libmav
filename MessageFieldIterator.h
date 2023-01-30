//
// Created by thomas on 27.01.23.
//

#ifndef LIBMAVLINK_MESSAGEFIELDITERATOR_H
#define LIBMAVLINK_MESSAGEFIELDITERATOR_H

#include <iterator>
#include <tuple>
#include <type_traits>
#include <vector>
#include <map>
#include "Message.h"

namespace mav {

    class _MessageFieldIterator {
    private:
        const Message &_message;
        std::map<std::string, Field>::const_iterator _map_iterator;

    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = std::pair<const std::string, NativeVariantType>;
        using difference_type = std::ptrdiff_t;
        using pointer = value_type *;
        using reference = value_type &;

        _MessageFieldIterator() = delete;

        explicit _MessageFieldIterator(const Message &message, std::map<std::string, Field>::const_iterator map_iterator) :
                _message{message},
                _map_iterator(map_iterator) {}


        _MessageFieldIterator &operator++() {
            _map_iterator++;
            return *this;
        }

        const _MessageFieldIterator operator++(int) {
            auto tmp = *this;
            ++*this;
            return tmp;
        }

        bool operator==(_MessageFieldIterator const &other) {
            return this->_map_iterator == other._map_iterator;
        }

        bool operator!=(_MessageFieldIterator const &other) { return !(*this == other); }

        value_type operator*() const {
            return {_map_iterator->first, _message.getAsNativeTypeInVariant(_map_iterator->first)};
        }
    };


    class FieldIterate {
    private:
        const Message& _message;
    public:
        explicit FieldIterate(const Message &message) : _message(message) {
        }

        [[nodiscard]] _MessageFieldIterator begin() const {
            return _MessageFieldIterator(_message, _message.type().fieldDefinitions().begin());
        }

        [[nodiscard]] _MessageFieldIterator end() const {
            return _MessageFieldIterator(_message, _message.type().fieldDefinitions().end());
        }
    };

}


#endif //LIBMAVLINK_MESSAGEFIELDITERATOR_H
