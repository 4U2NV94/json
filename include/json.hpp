// Copyright 2015-2019 Elviss Strazdins. All rights reserved.

#ifndef JSON_HPP
#define JSON_HPP

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace json
{
    class ParseError final: public std::logic_error
    {
    public:
        explicit ParseError(const std::string& str): std::logic_error(str) {}
        explicit ParseError(const char* str): std::logic_error(str) {}
    };

    class TypeError final: public std::runtime_error
    {
    public:
        explicit TypeError(const std::string& str): std::runtime_error(str) {}
        explicit TypeError(const char* str): std::runtime_error(str) {}
    };

    class RangeError final: public std::runtime_error
    {
    public:
        explicit RangeError(const std::string& str): std::runtime_error(str) {}
        explicit RangeError(const char* str): std::runtime_error(str) {}
    };

    inline namespace detail
    {
        namespace utf8
        {
            template <class Iterator>
            inline std::vector<std::uint32_t> toUtf32(Iterator begin, Iterator end)
            {
                std::vector<std::uint32_t> result;

                for (auto i = begin; i != end; ++i)
                {
                    std::uint32_t cp = static_cast<std::uint32_t>(*i) & 0xFFU;

                    if (cp <= 0x7FU) // length = 1
                    {
                        // do nothing
                    }
                    else if ((cp >> 5) == 0x6U) // length = 2
                    {
                        if (++i == end) return result;
                        cp = ((cp << 6) & 0x7FFU) + (static_cast<std::uint32_t>(*i) & 0x3FU);
                    }
                    else if ((cp >> 4) == 0xEU) // length = 3
                    {
                        if (++i == end) return result;
                        cp = ((cp << 12) & 0xFFFFU) + (((static_cast<std::uint32_t>(*i) & 0xFFU) << 6) & 0x0FFFU);
                        if (++i == end) return result;
                        cp += static_cast<std::uint32_t>(*i) & 0x3FU;
                    }
                    else if ((cp >> 3) == 0x1EU) // length = 4
                    {
                        if (++i == end) return result;
                        cp = ((cp << 18) & 0x1FFFFFU) + (((static_cast<std::uint32_t>(*i) & 0xFFU) << 12) & 0x3FFFFU);
                        if (++i == end) return result;
                        cp += ((static_cast<std::uint32_t>(*i) & 0xFFU) << 6) & 0x0FFFU;
                        if (++i == end) return result;
                        cp += static_cast<std::uint32_t>(*i) & 0x3FU;
                    }

                    result.push_back(cp);
                }

                return result;
            }

            template <class T>
            inline std::vector<uint32_t> toUtf32(const T& text)
            {
                return toUtf32(std::begin(text), std::end(text));
            }

            inline std::string fromUtf32(std::uint32_t c)
            {
                std::string result;

                if (c <= 0x7FU)
                    result.push_back(static_cast<char>(c));
                else if (c <= 0x7FFU)
                {
                    result.push_back(static_cast<char>(0xC0U | ((c >> 6) & 0x1FU)));
                    result.push_back(static_cast<char>(0x80U | (c & 0x3FU)));
                }
                else if (c <= 0xFFFFU)
                {
                    result.push_back(static_cast<char>(0xE0U | ((c >> 12) & 0x0FU)));
                    result.push_back(static_cast<char>(0x80U | ((c >> 6) & 0x3FU)));
                    result.push_back(static_cast<char>(0x80U | (c & 0x3FU)));
                }
                else
                {
                    result.push_back(static_cast<char>(0xF0U | ((c >> 18) & 0x07U)));
                    result.push_back(static_cast<char>(0x80U | ((c >> 12) & 0x3FU)));
                    result.push_back(static_cast<char>(0x80U | ((c >> 6) & 0x3FU)));
                    result.push_back(static_cast<char>(0x80U | (c & 0x3FU)));
                }

                return result;
            }

            template <class Iterator>
            inline std::string fromUtf32(Iterator begin, Iterator end)
            {
                std::string result;

                for (auto i = begin; i != end; ++i)
                {
                    if (*i <= 0x7FU)
                        result.push_back(static_cast<char>(*i));
                    else if (*i <= 0x7FFU)
                    {
                        result.push_back(static_cast<char>(0xC0U | ((*i >> 6) & 0x1FU)));
                        result.push_back(static_cast<char>(0x80U | (*i & 0x3FU)));
                    }
                    else if (*i <= 0xFFFFU)
                    {
                        result.push_back(static_cast<char>(0xE0U | ((*i >> 12) & 0x0FU)));
                        result.push_back(static_cast<char>(0x80U | ((*i >> 6) & 0x3FU)));
                        result.push_back(static_cast<char>(0x80U | (*i & 0x3FU)));
                    }
                    else
                    {
                        result.push_back(static_cast<char>(0xF0U | ((*i >> 18) & 0x07U)));
                        result.push_back(static_cast<char>(0x80U | ((*i >> 12) & 0x3FU)));
                        result.push_back(static_cast<char>(0x80U | ((*i >> 6) & 0x3FU)));
                        result.push_back(static_cast<char>(0x80U | (*i & 0x3FU)));
                    }
                }

                return result;
            }

            template <class T>
            inline std::string fromUtf32(const T& text)
            {
                return fromUtf32(std::begin(text), std::end(text));
            }
        } // namespace utf8

        constexpr std::uint8_t UTF8_BOM[] = {0xEF, 0xBB, 0xBF};

        inline void encodeString(std::vector<std::uint8_t>& data,
                                 const std::vector<std::uint32_t>& str)
        {
            for (const auto c : str)
            {
                if (c == '"') data.insert(data.end(), {'\\', '"'});
                else if (c == '\\') data.insert(data.end(), {'\\', '\\'});
                else if (c == '/') data.insert(data.end(), {'\\', '/'});
                else if (c == '\b') data.insert(data.end(), {'\\', 'b'});
                else if (c == '\f') data.insert(data.end(), {'\\', 'f'});
                else if (c == '\n') data.insert(data.end(), {'\\', 'n'});
                else if (c == '\r') data.insert(data.end(), {'\\', 'r'});
                else if (c == '\t') data.insert(data.end(), {'\\', 't'});
                else if (c <= 0x1FU)
                {
                    data.insert(data.end(), {'\\', 'u'});

                    constexpr char digits[] = "0123456789abcdef";
                    for (std::uint32_t p = 0; p < 4; ++p)
                        data.push_back(static_cast<std::uint8_t>(digits[(c >> (12 - p * 4)) & 0x0FU]));
                }
                else
                {
                    const std::string encoded = utf8::fromUtf32(c);
                    data.insert(data.end(), encoded.begin(), encoded.end());
                }
            }
        }

        struct Token final
        {
            enum class Type
            {
                LiteralInteger, // integer
                LiteralFloat, // float
                LiteralString, // string
                KeywordTrue, // true
                KeywordFalse, // false
                KeywordNull, // null
                LeftBrace, // {
                RightBrace, // }
                LeftBracket, // [
                RightBracket, // ]
                Comma, // ,
                Colon // :
            };

            Type type;
            std::vector<std::uint32_t> value;
        };

        inline std::vector<Token> tokenize(const std::vector<std::uint32_t>& str)
        {
            std::vector<Token> tokens;

            static const std::map<std::vector<std::uint32_t>, Token::Type> keywordMap{
                {{'t', 'r', 'u', 'e'}, Token::Type::KeywordTrue},
                {{'f', 'a', 'l', 's', 'e'}, Token::Type::KeywordFalse},
                {{'n', 'u', 'l', 'l'}, Token::Type::KeywordNull}
            };

            // tokenize
            for (auto iterator = str.cbegin(); iterator != str.cend();)
            {
                Token token;

                if (*iterator == '-' ||
                    (*iterator >= '0' && *iterator <= '9'))
                {
                    token.type = Token::Type::LiteralInteger;

                    if (*iterator == '-')
                    {
                        token.value.push_back(*iterator);
                        if (++iterator == str.cend() ||
                            *iterator < '0' || *iterator > '9')
                            throw ParseError("Invalid number");
                    }

                    while (iterator != str.cend() &&
                           (*iterator >= '0' && *iterator <= '9'))
                    {
                        token.value.push_back(*iterator);
                        ++iterator;
                    }

                    if (iterator != str.cend() && *iterator == '.')
                    {
                        token.type = Token::Type::LiteralFloat;

                        token.value.push_back(*iterator);
                        ++iterator;

                        while (iterator != str.cend() &&
                               (*iterator >= '0' && *iterator <= '9'))
                        {
                            token.value.push_back(*iterator);
                            ++iterator;
                        }
                    }

                    // parse exponent
                    if (iterator != str.cend() &&
                        (*iterator == 'e' || *iterator == 'E'))
                    {
                        token.value.push_back(*iterator);

                        if (++iterator == str.cend())
                            throw ParseError("Invalid exponent");

                        if (*iterator == '+' || *iterator == '-')
                            token.value.push_back(*iterator++);

                        if (iterator == str.cend() || *iterator < '0' || *iterator > '9')
                            throw ParseError("Invalid exponent");

                        while (iterator != str.cend() && *iterator >= '0' && *iterator <= '9')
                        {
                            token.value.push_back(*iterator);
                            ++iterator;
                        }
                    }
                }
                else if (*iterator == '"') // string literal
                {
                    token.type = Token::Type::LiteralString;

                    for (;;)
                    {
                        if (++iterator == str.cend())
                            throw ParseError("Unterminated string literal");

                        if (*iterator == '"')
                        {
                            ++iterator;
                            break;
                        }
                        else if (*iterator == '\\')
                        {
                            if (++iterator == str.cend())
                                throw ParseError("Unterminated string literal");

                            switch (*iterator)
                            {
                                case '"': token.value.push_back('"'); break;
                                case '\\': token.value.push_back('\\'); break;
                                case '/': token.value.push_back('/'); break;
                                case 'b': token.value.push_back('\b'); break;
                                case 'f': token.value.push_back('\f'); break;
                                case 'n': token.value.push_back('\n'); break;
                                case 'r': token.value.push_back('\r'); break;
                                case 't': token.value.push_back('\t'); break;
                                case 'u':
                                {
                                    std::uint32_t c = 0;

                                    for (std::uint32_t i = 0; i < 4; ++i, ++iterator)
                                    {
                                        if (iterator == str.cend())
                                            throw ParseError("Unexpected end of data");

                                        std::uint8_t code = 0;

                                        if (*iterator >= '0' && *iterator <= '9') code = static_cast<std::uint8_t>(*iterator) - '0';
                                        else if (*iterator >= 'a' && *iterator <='f') code = static_cast<std::uint8_t>(*iterator) - 'a' + 10;
                                        else if (*iterator >= 'A' && *iterator <='F') code = static_cast<std::uint8_t>(*iterator) - 'A' + 10;
                                        else
                                            throw ParseError("Invalid character code");

                                        c = (c << 4) | code;
                                    }

                                    token.value.push_back(c);
                                    break;
                                }
                                default:
                                    throw ParseError("Unrecognized escape character");
                            }
                        }
                        else if (*iterator <= 0x1FU) // control char
                            throw ParseError("Unterminated string literal");
                        else
                            token.value.push_back(*iterator);
                    }
                }
                else if ((*iterator >= 'a' && *iterator <= 'z') ||
                         (*iterator >= 'A' && *iterator <= 'Z') ||
                         *iterator == '_')
                {
                    while (iterator != str.cend() &&
                           ((*iterator >= 'a' && *iterator <= 'z') ||
                            (*iterator >= 'A' && *iterator <= 'Z') ||
                            *iterator == '_' ||
                            (*iterator >= '0' && *iterator <= '9')))
                    {
                        token.value.push_back(*iterator);
                        ++iterator;
                    }

                    auto keywordIterator = keywordMap.find(token.value);

                    if (keywordIterator != keywordMap.end())
                        token.type = keywordIterator->second;
                    else
                        throw ParseError("Unknown keyword " + utf8::fromUtf32(token.value));
                }
                else if (*iterator == ' ' ||
                         *iterator == '\t' ||
                         *iterator == '\r' ||
                         *iterator == '\n') // whitespace or newline
                {
                    ++iterator;
                    continue;
                }
                else if (*iterator == '\0')
                    break;
                else
                {
                    switch (*iterator)
                    {
                        case '{': token.type = Token::Type::LeftBrace; break;
                        case '}': token.type = Token::Type::RightBrace; break;
                        case '[': token.type = Token::Type::LeftBracket; break;
                        case ']': token.type = Token::Type::RightBracket; break;
                        case ',': token.type = Token::Type::Comma; break;
                        case ':': token.type = Token::Type::Colon; break;
                        default: throw ParseError("Unknown character");
                    }

                    ++iterator;
                }

                tokens.push_back(token);
            }

            return tokens;
        }
    }

    class Value
    {
    public:
        using Array = std::vector<Value>;
        using Object = std::map<std::string, Value>;

        enum class Type
        {
            Null,
            Integer,
            Float,
            String,
            Object,
            Array,
            Boolean
        };

        Value() = default;

        template <typename T, typename std::enable_if<std::is_same<T, Type>::value>::type* = nullptr>
        Value(const T initType): type(initType) {}

        template <typename T, typename std::enable_if<std::is_floating_point<T>::value>::type* = nullptr>
        Value(const T value): type(Type::Float), doubleValue(isfinite(value) ? static_cast<double>(value) : 0.0) {}

        template <typename T, typename std::enable_if<std::is_integral<T>::value && !std::is_same<T, bool>::value>::type* = nullptr>
        Value(const T value): type(Type::Integer), intValue(static_cast<std::int64_t>(value)) {}

        template <typename T, typename std::enable_if<std::is_same<T, std::string>::value>::type* = nullptr>
        Value(const T& value): type(Type::String), stringValue(value) {}

        template <typename T, typename std::enable_if<std::is_same<T, char>::value>::type* = nullptr>
        Value(const T* value): type(Type::String), stringValue(value) {}

        template <typename T, typename std::enable_if<std::is_same<T, bool>::value>::type* = nullptr>
        Value(const T value): type(Type::Boolean), boolValue(value) {}

        template <typename T, typename std::enable_if<std::is_same<T, std::nullptr_t>::value>::type* = nullptr>
        Value(const T): type(Type::Null) {}

        template <typename T, typename std::enable_if<std::is_same<T, Array>::value>::type* = nullptr>
        Value(const T& value): type(Type::Array), arrayValue(value) {}

        template <typename T, typename std::enable_if<std::is_same<T, Object>::value>::type* = nullptr>
        Value(const T& value): type(Type::Object), objectValue(value) {}

        template <typename T, typename std::enable_if<std::is_same<T, Type>::value>::type* = nullptr>
        inline Value& operator=(const T newType) noexcept
        {
            type = newType;
            return *this;
        }

        template <typename T, typename std::enable_if<std::is_floating_point<T>::value>::type* = nullptr>
        inline Value& operator=(const T value) noexcept
        {
            type = Type::Float;
            doubleValue = isfinite(value) ? static_cast<double>(value) : 0.0;
            return *this;
        }

        template <typename T, typename std::enable_if<std::is_integral<T>::value && !std::is_same<T, bool>::value>::type* = nullptr>
        inline Value& operator=(const T value) noexcept
        {
            type = Type::Integer;
            intValue = static_cast<std::int64_t>(value);
            return *this;
        }

        template <typename T, typename std::enable_if<std::is_same<T, std::string>::value>::type* = nullptr>
        inline Value& operator=(const T& value)
        {
            type = Type::String;
            stringValue = value;
            return *this;
        }

        template <typename T, typename std::enable_if<std::is_same<T, char>::value>::type* = nullptr>
        inline Value& operator=(const T* value)
        {
            type = Type::String;
            stringValue = value;
            return *this;
        }

        template <typename T, typename std::enable_if<std::is_same<T, bool>::value>::type* = nullptr>
        inline Value& operator=(const T value) noexcept
        {
            type = Type::Boolean;
            boolValue = value;
            return *this;
        }

        template <typename T, typename std::enable_if<std::is_same<T, std::nullptr_t>::value>::type* = nullptr>
        inline Value& operator=(T) noexcept
        {
            type = Type::Null;
            objectValue.clear();
            return *this;
        }

        template <typename T, typename std::enable_if<std::is_same<T, Array>::value>::type* = nullptr>
        inline Value& operator=(const T& value)
        {
            type = Type::Array;
            arrayValue = value;
            return *this;
        }

        template <typename T, typename std::enable_if<std::is_same<T, Object>::value>::type* = nullptr>
        inline Value& operator=(const T& value)
        {
            type = Type::Object;
            objectValue = value;
            return *this;
        }

        inline Type getType() const noexcept { return type; }

        template <typename T, typename std::enable_if<std::is_same<T, std::string>::value>::type* = nullptr>
        inline std::string& as() noexcept
        {
            type = Type::String;
            return stringValue;
        }

        template <typename T, typename std::enable_if<std::is_same<T, std::string>::value>::type* = nullptr>
        inline const std::string& as() const
        {
            if (type != Type::String) throw TypeError("Wrong type");
            return stringValue;
        }

        template <typename T, typename std::enable_if<std::is_same<T, const char*>::value>::type* = nullptr>
        inline T as() const
        {
            if (type != Type::String) throw TypeError("Wrong type");
            return stringValue.c_str();
        }

        template <typename T, typename std::enable_if<std::is_same<T, bool>::value>::type* = nullptr>
        inline T as() const
        {
            if (type != Type::Boolean && type != Type::Integer && type != Type::Float)
                throw TypeError("Wrong type");
            if (type == Type::Boolean) return boolValue;
            else if (type == Type::Integer) return intValue != 0;
            else return doubleValue != 0.0;
        }

        template <typename T, typename std::enable_if<std::is_arithmetic<T>::value && !std::is_same<T, bool>::value>::type* = nullptr>
        inline T as() const
        {
            if (type != Type::Boolean && type != Type::Integer && type != Type::Float)
                throw TypeError("Wrong type");
            if (type == Type::Boolean) return boolValue;
            else if (type == Type::Integer) return static_cast<T>(intValue);
            else return static_cast<T>(doubleValue);
        }

        template <typename T, typename std::enable_if<std::is_same<T, Object>::value>::type* = nullptr>
        inline T& as() noexcept
        {
            type = Type::Object;
            return objectValue;
        }

        template <typename T, typename std::enable_if<std::is_same<T, Object>::value>::type* = nullptr>
        inline const T& as() const
        {
            if (type != Type::Object) throw TypeError("Wrong type");
            return objectValue;
        }

        template <typename T, typename std::enable_if<std::is_same<T, Array>::value>::type* = nullptr>
        inline T& as() noexcept
        {
            type = Type::Array;
            return arrayValue;
        }

        template <typename T, typename std::enable_if<std::is_same<T, Array>::value>::type* = nullptr>
        inline const T& as() const
        {
            if (type != Type::Array) throw TypeError("Wrong type");
            return arrayValue;
        }

        Array::iterator begin()
        {
            if (type != Type::Array) throw TypeError("Wrong type");
            return arrayValue.begin();
        }

        Array::iterator end()
        {
            if (type != Type::Array) throw TypeError("Wrong type");
            return arrayValue.end();
        }

        Array::const_iterator begin() const
        {
            if (type != Type::Array) throw TypeError("Wrong type");
            return arrayValue.begin();
        }

        Array::const_iterator end() const
        {
            if (type != Type::Array) throw TypeError("Wrong type");
            return arrayValue.end();
        }

        inline bool isNull() const noexcept
        {
            return type == Type::Null;
        }

        inline bool hasMember(const std::string& member) const
        {
            if (type != Type::Object) throw TypeError("Wrong type");
            return objectValue.find(member) != objectValue.end();
        }

        inline Value& operator[](const std::string& member)
        {
            type = Type::Object;
            return objectValue[member];
        }

        inline const Value& operator[](const std::string& member) const
        {
            if (type != Type::Object) throw TypeError("Wrong type");

            auto i = objectValue.find(member);
            if (i != objectValue.end())
                return i->second;
            else
                throw RangeError("Member does not exist");
        }

        inline Value& operator[](std::size_t index)
        {
            type = Type::Array;
            if (index >= arrayValue.size()) arrayValue.resize(index + 1);
            return arrayValue[index];
        }

        inline const Value& operator[](std::size_t index) const
        {
            if (type != Type::Array) throw TypeError("Wrong type");

            if (index < arrayValue.size())
                return arrayValue[index];
            else
                throw RangeError("Index out of range");
        }

        inline std::size_t getSize() const
        {
            if (type != Type::Array) throw TypeError("Wrong type");
            return arrayValue.size();
        }

    protected:
        template <class TokenIterator>
        void parseValue(TokenIterator& iterator,
                        TokenIterator end)
        {
            if (iterator == end)
                throw ParseError("Unexpected end of data");

            if (iterator->type == Token::Type::LeftBrace)
                return parseObject(iterator, end);
            else if (iterator->type == Token::Type::LeftBracket)
                return parseArray(iterator, end);
            else if (iterator->type == Token::Type::LiteralInteger)
            {
                type = Type::Integer;
                intValue = std::stoll(utf8::fromUtf32(iterator->value));
                ++iterator;
            }
            else if (iterator->type == Token::Type::LiteralFloat)
            {
                type = Type::Float;
                doubleValue = std::stod(utf8::fromUtf32(iterator->value));
                ++iterator;
            }
            else if (iterator->type == Token::Type::LiteralString)
            {
                type = Type::String;
                stringValue = utf8::fromUtf32(iterator->value);
                ++iterator;
            }
            else if (iterator->type == Token::Type::KeywordTrue ||
                     iterator->type == Token::Type::KeywordFalse)
            {
                type = Type::Boolean;
                boolValue = (iterator->type == Token::Type::KeywordTrue);
                ++iterator;
            }
            else if (iterator->type == Token::Type::KeywordNull)
            {
                type = Type::Null;
                ++iterator;
            }
            else
                throw ParseError("Expected a value");
        }

        template <class TokenIterator>
        void parseObject(TokenIterator& iterator,
                         TokenIterator end)
        {
            if (iterator == end)
                throw ParseError("Unexpected end of data");

            if (iterator->type != Token::Type::LeftBrace)
                throw ParseError("Expected a left brace");

            ++iterator; // skip the left brace

            bool first = true;

            for (;;)
            {
                if (iterator == end)
                    throw ParseError("Unexpected end of data");

                if (iterator->type == Token::Type::RightBrace)
                {
                    ++iterator;// skip the right brace
                    break;
                }

                if (first)
                    first = false;
                else
                {
                    if (iterator->type != Token::Type::Comma)
                        throw ParseError("Expected a comma");

                    if (++iterator == end)
                        throw ParseError("Unexpected end of data");
                }

                if (iterator->type != Token::Type::LiteralString)
                    throw ParseError("Expected a string literal");

                const std::string key = utf8::fromUtf32(iterator->value);

                if (objectValue.find(key) != objectValue.end())
                    throw ParseError("Duplicate key value " + key);

                if (++iterator == end)
                    throw ParseError("Unexpected end of data");

                if (iterator->type != Token::Type::Colon)
                    throw ParseError("Expected a colon");

                if (++iterator == end)
                    throw ParseError("Unexpected end of data");

                Value value;
                value.parseValue(iterator, end);

                objectValue[key] = value;
            }

            type = Type::Object;
        }

        template <class TokenIterator>
        void parseArray(TokenIterator& iterator,
                        TokenIterator end)
        {
            if (iterator == end)
                throw ParseError("Unexpected end of data");

            if (iterator->type != Token::Type::LeftBracket)
                throw ParseError("Expected a left bracket");

            ++iterator; // skip the left bracket

            bool first = true;

            for (;;)
            {
                if (iterator == end)
                    throw ParseError("Unexpected end of data");

                if (iterator->type == Token::Type::RightBracket)
                {
                    ++iterator;// skip the right bracket
                    break;
                }

                if (first)
                    first = false;
                else
                {
                    if (iterator->type != Token::Type::Comma)
                        throw ParseError("Expected a comma");

                    if (++iterator == end)
                        throw ParseError("Unexpected end of data");
                }

                Value value;
                value.parseValue(iterator, end);

                arrayValue.push_back(value);
            }

            type = Type::Array;
        }

        void encodeValue(std::vector<std::uint8_t>& data) const
        {
            switch (type)
            {
                case Type::Null:
                {
                    data.insert(data.end(), {'n', 'u', 'l', 'l'});
                    break;
                }
                case Type::Integer:
                {
                    std::string value = std::to_string(intValue);
                    data.insert(data.end(), value.begin(), value.end());
                    break;
                }
                case Type::Float:
                {
                    std::string value = std::to_string(doubleValue);
                    data.insert(data.end(), value.begin(), value.end());
                    break;
                }
                case Type::String:
                    data.push_back('"');
                    encodeString(data, utf8::toUtf32(stringValue));
                    data.push_back('"');
                    break;
                case Type::Object:
                {
                    data.push_back('{');

                    bool first = true;

                    for (const auto& value : objectValue)
                    {
                        if (first) first = false;
                        else data.push_back(',');

                        data.push_back('"');
                        encodeString(data, utf8::toUtf32(value.first));
                        data.insert(data.end(), {'"', ':'});
                        value.second.encodeValue(data);
                    }

                    data.push_back('}');
                    break;
                }
                case Type::Array:
                {
                    data.push_back('[');

                    bool first = true;

                    for (const auto& value : arrayValue)
                    {
                        if (first) first = false;
                        else data.push_back(',');

                        value.encodeValue(data);
                    }

                    data.push_back(']');
                    break;
                }
                case Type::Boolean:
                    if (boolValue) data.insert(data.end(), {'t', 'r', 'u', 'e'});
                    else data.insert(data.end(), {'f', 'a', 'l', 's', 'e'});
                    break;
                default:
                    throw ParseError("Unknown value type");
            }
        }

    private:
        Type type = Type::Object;
        union
        {
            bool boolValue = false;
            std::int64_t intValue;
            double doubleValue;
        };
        Object objectValue;
        Array arrayValue;
        std::string stringValue;
    };

    class Data final: public Value
    {
    public:
        Data(): Value(Value::Type::Object)
        {
        }

        template <class T>
        explicit Data(const T& data)
        {
            std::vector<std::uint32_t> str;

            // BOM
            if (std::distance(std::begin(data), std::end(data)) >= 3 &&
                std::equal(std::begin(data), std::begin(data) + 3,
                           std::begin(UTF8_BOM)))
            {
                bom = true;
                str = utf8::toUtf32(std::begin(data) + 3, std::end(data));
            }
            else
            {
                bom = false;
                str = utf8::toUtf32(data);
            }

            std::vector<Token> tokens = tokenize(str);

            auto iterator = tokens.cbegin();

            parseValue(iterator, tokens.cend());
        }

        std::vector<std::uint8_t> encode() const
        {
            std::vector<std::uint8_t> result;

            if (bom) result.assign(std::begin(UTF8_BOM), std::end(UTF8_BOM));

            encodeValue(result);

            return result;
        }

        inline bool hasBOM() const noexcept { return bom; }
        inline void setBOM(const bool newBOM) noexcept { bom = newBOM; }

    private:
        bool bom = false;
    };
} // namespace json

#endif // JSON_HPP
