#include "ParserJSON.h"
#include "../compat/Strings.h"


auto ParserJSON::objectToChunk(const rb::Json& obj) -> Chunk
{
    return {
        stringifyObject(obj),
        obj.dumpString(true)
    };
}


auto ParserJSON::stringifyObject(const rb::Json& obj) -> rb::String
{
    rb::Vector<rb::String> parts;

    if (obj.contains("id"))
        parts.push_back(rb::from_std("id is ") + obj.stringValue("id"));

    if (obj.contains("name")) {
        const rb::Json nameVal = obj.value("name");
        const rb::String name = nameVal.isObject()
            ? nameVal.stringValue("str")
            : nameVal.toString();
        if (!rb::str_empty(name))
            parts.push_back(rb::from_std("name is ") + name);
    }

    rb::Vector<rb::String> details;
    extractTextRecursive(obj, details);
    for (const rb::String& d : details) {
        if (!rb::starts_with(d, rb::from_std("id is")) &&
            !rb::starts_with(d, rb::from_std("name is")))
            parts.push_back(d);
    }

    rb::String out;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) out += rb::from_std(" ");
        out += parts[i];
    }

    // Cap embedText so a single chunk fits in the embedding model's ~2048-token
    // training context. CDDA dialogue trees and dense monster descriptions can
    // otherwise produce 2000-3000+ token chunks that the server rejects with 500,
    // dropping the entire sub-batch.
    static constexpr int MaxEmbedChars { 6000 };
    if (static_cast<int>(out.size()) > MaxEmbedChars) {
        // Walk back to a UTF-8 character boundary so we don't split a multi-byte sequence.
        size_t pos = static_cast<size_t>(MaxEmbedChars);
        while (pos > 0 && (static_cast<unsigned char>(out[pos]) & 0xC0) == 0x80)
            --pos;
        out = out.substr(0, pos);
    }
    return out;
}


auto ParserJSON::extractTextRecursive(
        const rb::Json& value,
        rb::Vector<rb::String>& texts,
        const rb::String& prefix
) -> void
{
    if (value.isObject()) {
        for (const rb::String& key : value.keys()) {
            if (key == rb::from_std("//") || key == rb::from_std("type")) continue;

            const rb::String fullKey = rb::str_empty(prefix)
                ? key : prefix + rb::from_std(" ") + key;
            const rb::Json val = value.value(key);
            if (val.isObject() || val.isArray()) {
                extractTextRecursive(val, texts, fullKey);
            } else {
                // Replace underscores with spaces in the key portion, then append the value
                rb::String keyNorm = fullKey;
                for (char& ch : keyNorm) if (ch == '_') ch = ' ';
                texts.push_back(keyNorm + rb::from_std(" is ") + val.toString());
            }
        }
    } else if (value.isArray()) {
        rb::Vector<rb::String> items;
        for (const rb::Json& item : value.items()) {
            if (item.isString()) items.push_back(item.toString());
            else extractTextRecursive(item, texts, prefix);
        }
        if (!items.empty()) {
            rb::String keyNorm = prefix;
            for (char& ch : keyNorm) if (ch == '_') ch = ' ';
            rb::String joined = keyNorm + rb::from_std(" includes: ");
            for (size_t i = 0; i < items.size(); ++i) {
                if (i > 0) joined += rb::from_std(", ");
                joined += items[i];
            }
            texts.push_back(joined);
        }
    }
}
