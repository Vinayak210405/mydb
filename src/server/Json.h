#pragma once
#include <string>
#include <sstream>
#include "../catalog/Type.h"

// Minimal streaming JSON writer — enough for the web console's API.
//
//   JsonWriter w;
//   w.begin_object().key("ok").value(true).key("rows").begin_array() ... .end_array().end_object();
//   w.str();
class JsonWriter {
public:
    JsonWriter& begin_object() { comma(); out_ << '{'; first_ = true; return *this; }
    JsonWriter& end_object()   { out_ << '}'; first_ = false; return *this; }
    JsonWriter& begin_array()  { comma(); out_ << '['; first_ = true; return *this; }
    JsonWriter& end_array()    { out_ << ']'; first_ = false; return *this; }

    JsonWriter& key(const std::string &k) {
        comma();
        write_string(k);
        out_ << ':';
        first_ = true;            // the value that follows needs no comma
        return *this;
    }

    JsonWriter& value(const std::string &s) { comma(); write_string(s); return *this; }
    JsonWriter& value(const char *s)        { return value(std::string(s)); }
    JsonWriter& value(bool b)               { comma(); out_ << (b ? "true" : "false"); return *this; }
    JsonWriter& value(int64_t n)            { comma(); out_ << n; return *this; }
    JsonWriter& value(int n)                { return value(static_cast<int64_t>(n)); }
    JsonWriter& value(uint64_t n)           { comma(); out_ << n; return *this; }
    JsonWriter& value(double d);
    JsonWriter& null()                      { comma(); out_ << "null"; return *this; }
    JsonWriter& value(const Value &v);      // SQL value → JSON (NULL → null)

    std::string str() const { return out_.str(); }

    static std::string escape(const std::string &s);

private:
    void comma() {
        if (!first_) out_ << ',';
        first_ = false;
    }
    void write_string(const std::string &s) { out_ << '"' << escape(s) << '"'; }

    std::ostringstream out_;
    bool               first_ = true;
};
