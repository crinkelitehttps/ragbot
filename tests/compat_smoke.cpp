// Phase 1 smoke test — exercises every compat function in both build modes.
// Compiled as a standalone binary (compat_smoke) that links only the compat
// layer + its dependencies (no app code, no Qt-only sources).

#include "compat/Io.h"
#include "compat/Json.h"
#include "compat/Logging.h"
#include "compat/Sha.h"
#include "compat/Strings.h"
#include "compat/Types.h"

#include <cstdio>
#include <cstdlib>
#include <string>

namespace {

int failures = 0;

#define CHECK(cond, msg) do {                                                  \
    if (!(cond)) {                                                             \
        std::fprintf(stderr, "FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__);   \
        ++failures;                                                            \
    }                                                                          \
} while (0)

void test_strings()
{
    CHECK(rb::to_std(rb::from_std("hello")) == "hello",     "round-trip");
    CHECK(rb::starts_with(rb::from_std("hello world"),
                          rb::from_std("hello")),           "starts_with");
    CHECK(!rb::starts_with(rb::from_std("hi"),
                           rb::from_std("hello")),          "starts_with negative");
    CHECK(rb::ends_with(rb::from_std("foo.json"),
                        rb::from_std(".json")),             "ends_with");
    CHECK(rb::contains(rb::from_std("the quick brown"),
                       rb::from_std("quick")),              "contains");
    CHECK(rb::to_std(rb::to_lower(rb::from_std("ABC"))) == "abc", "to_lower");
    CHECK(rb::to_std(rb::trim(rb::from_std("  hi  "))) == "hi",   "trim");
    CHECK(rb::to_std(rb::squash_whitespace(rb::from_std("a  b\t c"))) == "a b c",
          "squash_whitespace");

    auto parts = rb::split(rb::from_std("a,b,c"), ',');
    CHECK(parts.size() == 3,                                "split size");
    CHECK(rb::to_std(parts[1]) == "b",                      "split content");

    CHECK(rb::parse_int(rb::from_std("42"), 0) == 42,       "parse_int");
    CHECK(rb::parse_int(rb::from_std("xx"), 7) == 7,        "parse_int default");

    const auto fmt = rb::format("hello {} the {} is {}", "world", "answer", 42);
    CHECK(rb::to_std(fmt) == "hello world the answer is 42", "format");

    const auto fixed = rb::format_fixed(0.5, 4);
    CHECK(rb::to_std(fixed) == "0.5000",                    "format_fixed");

    rb::Vector<rb::String> vals;
    vals.push_back(rb::from_std("Alice"));
    vals.push_back(rb::from_std("the answer"));
    vals.push_back(rb::from_std("42"));
    const auto rp = rb::replace_placeholders(
        rb::from_std("hi %1 — %2 is %3"), vals);
    CHECK(rb::to_std(rp) == "hi Alice — the answer is 42",  "replace_placeholders");
}

void test_json()
{
    const std::string raw = R"({"name":"survivor","age":42,"flags":[true,false],"nest":{"k":"v"}})";
    const rb::Json doc = rb::Json::parse(rb::from_std(raw));
    CHECK(doc.isValid() && doc.isObject(),                  "json parse + isObject");
    CHECK(doc.contains(rb::from_std("name")),               "json contains");
    CHECK(rb::to_std(doc.stringValue(rb::from_std("name"))) == "survivor",
          "json stringValue");
    CHECK(doc.intValue(rb::from_std("age")) == 42,          "json intValue");

    const auto flags = doc.value(rb::from_std("flags"));
    CHECK(flags.isArray() && flags.size() == 2,             "json array shape");
    CHECK(flags.at(0).toBool() == true,                     "json array elem");
    CHECK(flags.at(1).toBool() == false,                    "json array elem 2");

    const auto nest = doc.value(rb::from_std("nest"));
    CHECK(rb::to_std(nest.stringValue(rb::from_std("k"))) == "v",
          "json nested");

    rb::Json built = rb::Json::object();
    built.setString(rb::from_std("hello"), rb::from_std("world"));
    built.setInt(rb::from_std("n"), 7);
    rb::Json arr = rb::Json::array();
    arr.append(rb::Json::fromInt(1));
    arr.append(rb::Json::fromInt(2));
    built.set(rb::from_std("xs"), arr);
    const std::string dumped = rb::to_std(built.dumpString());
    CHECK(dumped.find("\"hello\"") != std::string::npos,    "json build dump");
    CHECK(dumped.find("\"n\":7")  != std::string::npos,     "json build int");
    CHECK(dumped.find("[1,2]")    != std::string::npos,     "json build array");

    const auto bad = rb::Json::parse(rb::from_std("not json"));
    CHECK(!bad.isValid(),                                   "json bad parse");
}

void test_io()
{
    const auto path = rb::from_std("/tmp/ragbot_compat_smoke.txt");
    const auto body = rb::from_std("hello compat layer\n");
    CHECK(rb::write_file_text(path, body),                  "write_file_text");
    bool ok = false;
    const auto back = rb::read_file_text(path, &ok);
    CHECK(ok && rb::to_std(back) == rb::to_std(body),       "read_file_text");
    CHECK(rb::path_exists(path),                            "path_exists");
    CHECK(rb::to_std(rb::path_filename(path)) == "ragbot_compat_smoke.txt",
          "path_filename");
    std::remove("/tmp/ragbot_compat_smoke.txt");
}

void test_sha()
{
    // Known: sha256("") = e3b0c442...b855
    const rb::Bytes empty;
    const auto h0 = rb::to_std(rb::sha256_hex(empty));
    CHECK(h0 == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
          "sha256 of empty");
    // Known: sha256("abc") = ba7816bf...ad15
    rb::Bytes abc;
    abc.push_back('a'); abc.push_back('b'); abc.push_back('c');
    const auto h1 = rb::to_std(rb::sha256_hex(abc));
    CHECK(h1 == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
          "sha256 of 'abc'");
}

void test_logging()
{
    // Just make sure it doesn't crash. (Output goes to stderr/qDebug; we
    // can't easily intercept either across Qt and non-Qt modes.)
    RAGBOT_LOG_INFO("smoke test info {}",  1);
    RAGBOT_LOG_WARN("smoke test warn {}",  "two");
    RAGBOT_LOG_ERROR("smoke test error {} of {}", 3, 4);
}

}  // namespace

int main()
{
    test_strings();
    test_json();
    test_io();
    test_sha();
    test_logging();
    if (failures == 0) {
        std::fprintf(stderr, "compat_smoke: all tests passed\n");
        return 0;
    }
    std::fprintf(stderr, "compat_smoke: %d failure(s)\n", failures);
    return 1;
}
