#include <creation/ofxhost/OfxPropertySet.h>

namespace creation::ofxhost
{
OfxPropertySet::Entry& OfxPropertySet::entryFor(const std::string& name, Kind kind)
{
    auto& entry = entries[name];
    if (entry.kind == Kind::none)
        entry.kind = kind;
    return entry;
}

namespace
{
template <typename Vec>
void ensureSize(Vec& vec, int index)
{
    if (index >= 0 && (size_t) index >= vec.size())
        vec.resize((size_t) index + 1);
}
}

void OfxPropertySet::setPointer(const std::string& name, int index, void* value)
{
    auto& entry = entryFor(name, Kind::pointer);
    if (entry.kind != Kind::pointer)
        return;
    ensureSize(entry.pointers, index);
    entry.pointers[(size_t) index] = value;
}

void OfxPropertySet::setString(const std::string& name, int index, const std::string& value)
{
    auto& entry = entryFor(name, Kind::string);
    if (entry.kind != Kind::string)
        return;
    ensureSize(entry.strings, index);
    entry.strings[(size_t) index] = value;
}

void OfxPropertySet::setDouble(const std::string& name, int index, double value)
{
    auto& entry = entryFor(name, Kind::real);
    if (entry.kind != Kind::real)
        return;
    ensureSize(entry.doubles, index);
    entry.doubles[(size_t) index] = value;
}

void OfxPropertySet::setInt(const std::string& name, int index, int value)
{
    auto& entry = entryFor(name, Kind::integer);
    if (entry.kind != Kind::integer)
        return;
    ensureSize(entry.ints, index);
    entry.ints[(size_t) index] = value;
}

bool OfxPropertySet::getPointer(const std::string& name, int index, void** outValue) const
{
    auto it = entries.find(name);
    if (it == entries.end() || it->second.kind != Kind::pointer || index < 0 || (size_t) index >= it->second.pointers.size())
        return false;
    *outValue = it->second.pointers[(size_t) index];
    return true;
}

bool OfxPropertySet::getString(const std::string& name, int index, const char** outValue) const
{
    auto it = entries.find(name);
    if (it == entries.end() || it->second.kind != Kind::string || index < 0 || (size_t) index >= it->second.strings.size())
        return false;
    *outValue = it->second.strings[(size_t) index].c_str();
    return true;
}

bool OfxPropertySet::getDouble(const std::string& name, int index, double* outValue) const
{
    auto it = entries.find(name);
    if (it == entries.end() || it->second.kind != Kind::real || index < 0 || (size_t) index >= it->second.doubles.size())
        return false;
    *outValue = it->second.doubles[(size_t) index];
    return true;
}

bool OfxPropertySet::getInt(const std::string& name, int index, int* outValue) const
{
    auto it = entries.find(name);
    if (it == entries.end() || it->second.kind != Kind::integer || index < 0 || (size_t) index >= it->second.ints.size())
        return false;
    *outValue = it->second.ints[(size_t) index];
    return true;
}

void OfxPropertySet::reset(const std::string& name)
{
    entries.erase(name);
}

int OfxPropertySet::getDimension(const std::string& name) const
{
    auto it = entries.find(name);
    if (it == entries.end())
        return 0;

    switch (it->second.kind)
    {
        case Kind::pointer: return (int) it->second.pointers.size();
        case Kind::string: return (int) it->second.strings.size();
        case Kind::integer: return (int) it->second.ints.size();
        case Kind::real: return (int) it->second.doubles.size();
        case Kind::none: default: return 0;
    }
}
}
