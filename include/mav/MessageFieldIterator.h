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
