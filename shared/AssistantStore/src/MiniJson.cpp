#include "MiniJson.h"

#include <cstdio>

namespace creation::assistant::detail
{
namespace
{
class Parser
{
public:
    explicit Parser(const std::string& text) : s(text) {}

    bool parse(JsonValue& out, std::string& error)
    {
        skip();
        if (! value(out, 0))
        {
            error = message.empty() ? "invalid JSON" : message;
            return false;
        }
        skip();
        if (pos != s.size())
        {
            error = "unexpected text after the JSON value at offset " + std::to_string(pos);
            return false;
        }
        return true;
    }

private:
    const std::string& s;
    size_t pos = 0;
    std::string message;

    bool fail(const std::string& why)
    {
        if (message.empty())
            message = why + " at offset " + std::to_string(pos);
        return false;
    }

    void skip()
    {
        while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t' || s[pos] == '\r' || s[pos] == '\n'))
            ++pos;
    }

    bool literal(const char* word)
    {
        size_t i = 0;
        while (word[i] != 0)
        {
            if (pos + i >= s.size() || s[pos + i] != word[i])
                return false;
            ++i;
        }
        pos += i;
        return true;
    }

    bool value(JsonValue& out, int depth)
    {
        if (depth > 64)
            return fail("nested too deeply");
        if (pos >= s.size())
            return fail("unexpected end");
        const char c = s[pos];
        if (c == '{')
            return object(out, depth);
        if (c == '[')
            return array(out, depth);
        if (c == '"')
        {
            out.type = JsonValue::Type::string;
            return string(out.string);
        }
        if (literal("true")) { out.type = JsonValue::Type::boolean; out.boolean = true; return true; }
        if (literal("false")) { out.type = JsonValue::Type::boolean; out.boolean = false; return true; }
        if (literal("null")) { out.type = JsonValue::Type::null; return true; }
        if (c == '-' || (c >= '0' && c <= '9'))
            return number(out);
        return fail("unexpected character");
    }

    bool number(JsonValue& out)
    {
        const size_t start = pos;
        if (s[pos] == '-')
            ++pos;
        while (pos < s.size() && s[pos] >= '0' && s[pos] <= '9')
            ++pos;
        if (pos == start || (pos == start + 1 && s[start] == '-'))
            return fail("bad number");
        if (pos < s.size() && (s[pos] == '.' || s[pos] == 'e' || s[pos] == 'E'))
            return fail("only whole numbers are supported");
        out.type = JsonValue::Type::integer;
        try { out.integer = std::stoll(s.substr(start, pos - start)); }
        catch (...) { return fail("number out of range"); }
        return true;
    }

    static void appendUtf8(std::string& out, unsigned int cp)
    {
        if (cp < 0x80) out.push_back((char) cp);
        else if (cp < 0x800) { out.push_back((char) (0xC0 | (cp >> 6))); out.push_back((char) (0x80 | (cp & 0x3F))); }
        else if (cp < 0x10000) { out.push_back((char) (0xE0 | (cp >> 12))); out.push_back((char) (0x80 | ((cp >> 6) & 0x3F))); out.push_back((char) (0x80 | (cp & 0x3F))); }
        else { out.push_back((char) (0xF0 | (cp >> 18))); out.push_back((char) (0x80 | ((cp >> 12) & 0x3F))); out.push_back((char) (0x80 | ((cp >> 6) & 0x3F))); out.push_back((char) (0x80 | (cp & 0x3F))); }
    }

    bool hex4(unsigned int& out)
    {
        if (pos + 4 > s.size())
            return fail("short \\u escape");
        out = 0;
        for (int i = 0; i < 4; ++i)
        {
            const char h = s[pos + (size_t) i];
            out <<= 4;
            if (h >= '0' && h <= '9') out |= (unsigned int) (h - '0');
            else if (h >= 'a' && h <= 'f') out |= (unsigned int) (h - 'a' + 10);
            else if (h >= 'A' && h <= 'F') out |= (unsigned int) (h - 'A' + 10);
            else return fail("bad \\u escape");
        }
        pos += 4;
        return true;
    }

    bool string(std::string& out)
    {
        ++pos;   // the opening quote
        out.clear();
        while (pos < s.size())
        {
            const char c = s[pos++];
            if (c == '"')
                return true;
            if ((unsigned char) c < 0x20)
                return fail("raw control character in a string");
            if (c != '\\')
            {
                out.push_back(c);
                continue;
            }
            if (pos >= s.size())
                break;
            const char e = s[pos++];
            switch (e)
            {
                case '"': out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case '/': out.push_back('/'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                case 'u':
                {
                    unsigned int cp = 0;
                    if (! hex4(cp))
                        return false;
                    if (cp >= 0xD800 && cp <= 0xDBFF)
                    {
                        unsigned int low = 0;
                        if (! (pos + 1 < s.size() && s[pos] == '\\' && s[pos + 1] == 'u'))
                            return fail("half of a surrogate pair");
                        pos += 2;
                        if (! hex4(low) || low < 0xDC00 || low > 0xDFFF)
                            return fail("bad surrogate pair");
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                    }
                    else if (cp >= 0xDC00 && cp <= 0xDFFF)
                        return fail("lone low surrogate");
                    appendUtf8(out, cp);
                    break;
                }
                default: return fail("unknown escape");
            }
        }
        return fail("unterminated string");
    }

    bool array(JsonValue& out, int depth)
    {
        ++pos;
        out.type = JsonValue::Type::array;
        skip();
        if (pos < s.size() && s[pos] == ']') { ++pos; return true; }
        while (true)
        {
            skip();
            JsonValue item;
            if (! value(item, depth + 1))
                return false;
            out.items.push_back(std::move(item));
            skip();
            if (pos < s.size() && s[pos] == ',') { ++pos; continue; }
            if (pos < s.size() && s[pos] == ']') { ++pos; return true; }
            return fail("expected , or ]");
        }
    }

    bool object(JsonValue& out, int depth)
    {
        ++pos;
        out.type = JsonValue::Type::object;
        skip();
        if (pos < s.size() && s[pos] == '}') { ++pos; return true; }
        while (true)
        {
            skip();
            if (pos >= s.size() || s[pos] != '"')
                return fail("expected a member name");
            std::string key;
            if (! string(key))
                return false;
            skip();
            if (pos >= s.size() || s[pos] != ':')
                return fail("expected :");
            ++pos;
            skip();
            JsonValue member;
            if (! value(member, depth + 1))
                return false;
            out.members.emplace_back(std::move(key), std::move(member));
            skip();
            if (pos < s.size() && s[pos] == ',') { ++pos; continue; }
            if (pos < s.size() && s[pos] == '}') { ++pos; return true; }
            return fail("expected , or }");
        }
    }
};
}

bool parseJson(const std::string& text, JsonValue& out, std::string& error)
{
    Parser parser(text);
    return parser.parse(out, error);
}

std::string quoteJson(const std::string& text)
{
    std::string out = "\"";
    for (char c : text)
    {
        switch (c)
        {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            default:
                if ((unsigned char) c < 0x20)
                {
                    char escape[8];
                    std::snprintf(escape, sizeof(escape), "\\u%04x", (unsigned int) (unsigned char) c);
                    out += escape;
                }
                else
                    out.push_back(c);
        }
    }
    out.push_back('"');
    return out;
}
}
