#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace creation::ofxhost
{
// Backing storage for one OFX property set (plugin descriptor, clip, param, host context, ...) -
// every plugin/host exchange in OFX goes through OfxPropertySuiteV1, never a raw struct field, so
// this is the single mechanism the whole host's property surface is built on.
//
// A property is untyped by name alone in the C API - the host and plugin both just have to know
// which type a given property name holds, per the spec (kOfxPropName is always a string,
// kOfxImageEffectPropRenderScale is always a double pair, etc.). This class doesn't enforce that
// - it stores whichever type was written first for a given name and returns kOfxStatErrValue if a
// caller later asks for a different type, which is the practical equivalent without needing a
// static table of every property's declared type.
class OfxPropertySet
{
public:
    void setPointer(const std::string& name, int index, void* value);
    void setString(const std::string& name, int index, const std::string& value);
    void setDouble(const std::string& name, int index, double value);
    void setInt(const std::string& name, int index, int value);

    bool getPointer(const std::string& name, int index, void** outValue) const;
    bool getString(const std::string& name, int index, const char** outValue) const;
    bool getDouble(const std::string& name, int index, double* outValue) const;
    bool getInt(const std::string& name, int index, int* outValue) const;

    void reset(const std::string& name);
    int getDimension(const std::string& name) const;

private:
    enum class Kind { none, pointer, string, integer, real };

    struct Entry
    {
        Kind kind = Kind::none;
        std::vector<void*> pointers;
        std::vector<std::string> strings;
        std::vector<int> ints;
        std::vector<double> doubles;
    };

    Entry& entryFor(const std::string& name, Kind kind);

    std::unordered_map<std::string, Entry> entries;
};
}
