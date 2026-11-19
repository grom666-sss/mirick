#pragma once

#include <cctype>
#include <cstddef>
#include <string>

namespace Text
{
    inline bool IsStringText(const std::string& value)
    {
        if (value.empty())
        {
            return false;
        }

        for (const unsigned char c : value)
        {
            if (!std::isdigit(c) && c != '-' && c != '.')
            {
                return true;
            }
        }
        return false;
    }

    inline std::string ToLower(const std::string& value)
    {
        std::string result = value;
        for (char& c : result)
        {
            const unsigned char byte = static_cast<unsigned char>(c);
            if (byte < 0x80)
            {
                c = static_cast<char>(std::tolower(byte));
            }
        }
        return result;
    }

    inline std::size_t Utf8Length(const std::string& value)
    {
        std::size_t length = 0;
        for (std::size_t i = 0; i < value.size();)
        {
            const unsigned char c = static_cast<unsigned char>(value[i]);
            std::size_t bytes = 1;
            if ((c & 0xE0) == 0xC0) bytes = 2;
            else if ((c & 0xF0) == 0xE0) bytes = 3;
            else if ((c & 0xF8) == 0xF0) bytes = 4;

            // Invalid/truncated sequences count as one byte instead of reading
            // beyond the supplied string.
            if (i + bytes > value.size())
            {
                bytes = 1;
            }
            i += bytes;
            ++length;
        }
        return length;
    }
}
