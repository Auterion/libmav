//
// Created by thomas on 09.01.23.
//

#include <sstream>
#include <string_view>
#include <chrono>
#include <optional>

#ifndef MAV_UTILS_H
#define MAV_UTILS_H

namespace mav {

    class CRC {
    private:
        uint16_t _crc = 0xFFFF;

    public:
        inline void accumulate(uint8_t d) {
            uint8_t tmp = d ^ (_crc & 0xFF);
            tmp ^=  (tmp << 4);
            _crc = (_crc >> 8) ^ (tmp << 8) ^ (tmp << 3) ^ (tmp >> 4);
        }

        void accumulate(const std::string_view &str) {
            for (const char c : str) {
                accumulate(static_cast<uint8_t>(c));
            }
        }

        template <typename ForwardIterator>
        void accumulate(ForwardIterator begin, ForwardIterator end) {
            for (; begin != end; ++begin) {
                accumulate(static_cast<uint8_t>(*begin));
            }
        }


        [[nodiscard]] uint16_t crc16() const {
            return _crc;
        }

        [[nodiscard]] uint8_t crc8() const {
            return static_cast<uint8_t>(_crc & 0xFF) ^ static_cast<uint8_t>((_crc >> 8) & 0xFF);
        }
    };

    template <typename T>
    inline void serialize(const T &v, uint8_t* destination) {
        auto src_ptr = static_cast<const uint8_t*>(static_cast<const void*>(&v));
        std::copy(src_ptr, src_ptr + sizeof(T), destination);
    }


    template <typename T>
    inline T deserialize(const uint8_t* source) {
        return *static_cast<const T*>(static_cast<const void*>(source));
    }


    inline uint64_t millis() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
    }



    class StringFormat {
    private:
        std::stringstream _stream;

    public:
        struct formatEndType {};
        static constexpr formatEndType end{};
        static constexpr formatEndType toString{};

        template <typename T>
        StringFormat& operator<< (const T &value) {
            _stream << value;
            return *this;
        }

        std::string operator<< (const formatEndType&) {
            return _stream.str();
        }

        std::string str() const {
            return _stream.str();
        }

        explicit operator std::string() const {
            return _stream.str();
        }
    };


    template <typename T>
    class OptionalReference {
    private:
        T* _ptr;

    public:
        OptionalReference(T &value) : _ptr{std::addressof(value)} {}

        OptionalReference(std::nullopt_t) : _ptr{nullptr} {}

        operator bool() const {
            return _ptr != nullptr;
        }

        T& get() {
            return *_ptr;
        }
    };


    template<typename T, typename... Rest>
    struct is_any : std::false_type {};

    template<typename T, typename First>
    struct is_any<T, First> : std::is_same<T, First> {};

    template<typename T, typename First, typename... Rest>
    struct is_any<T, First, Rest...>
            : std::integral_constant<bool, std::is_same<T, First>::value || is_any<T, Rest...>::value>
    {};


    template <typename T, typename = void>
    struct is_iterable : std::false_type {};

    template <typename T>
    struct is_iterable<T, std::void_t<
            decltype(std::begin(std::declval<T>())),
            decltype(std::end(std::declval<T>()))>> : std::true_type {};


    template<typename T>
    struct is_string
            : public std::disjunction<
                    std::is_same<char *, typename std::decay_t<T>>,
                    std::is_same<const char *, typename std::decay_t<T>>,
                    std::is_same<std::string, typename std::decay_t<T>>
            > {};

    /**
     * Manual implementation of strnlen, since it does not exist in std.
     * Counts the number of characters in the string, excluding the terminal zero.
     * Returns at most `max_len`
     * @param ptr The pointer to the string in memory
     * @param max_len The maximum number of characters to report
     * @return The number of characters in the string until terminating zero, not including the zero
     * or max_len if no terminating zero found until max_len reached
     */
    inline int strnlen(const uint8_t* ptr, int max_len) {
        int len = 0;
        while ((*ptr != 0) && (len < max_len)) {
            ptr++;
            len++;
        }
        return len;
    }


    template<typename A, typename B>
    A _packUnpack(B b) {
        union U {
            A a;
            B b;
        };
        U u;
        u.b = b;
        return u.a;
    }

    template<typename T>
    T floatUnpack(float f) {
        return _packUnpack<T, float>(f);
    }

    template<typename T>
    float floatPack(T o) {
        return _packUnpack<float, T>(o);
    }


}


#endif //MAV_UTILS_H
