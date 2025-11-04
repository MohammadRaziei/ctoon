#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "ctoon.h"

#include <filesystem>
#include <variant>

namespace fs = std::filesystem;

const fs::path cwd = fs::path(__FILE__).parent_path();
const fs::path datapath = cwd.parent_path() / "data";

namespace {

const ctoon::Object& expectObject(const ctoon::Value& value) {
    REQUIRE(value.isObject());
    return value.asObject();
}

const ctoon::Array& expectArray(const ctoon::Value& value) {
    REQUIRE(value.isArray());
    return value.asArray();
}

const std::string& expectString(const ctoon::Value& value) {
    const auto& primitive = value.asPrimitive();
    REQUIRE(std::holds_alternative<std::string>(primitive));
    return std::get<std::string>(primitive);
}

std::string trim(const std::string& input) {
    const auto first = input.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "";
    }
    const auto last = input.find_last_not_of(" \t\r\n");
    return input.substr(first, last - first + 1);
}

double expectNumber(const ctoon::Value& value) {
    const auto& primitive = value.asPrimitive();
    if (std::holds_alternative<double>(primitive)) {
        return std::get<double>(primitive);
    }
    if (std::holds_alternative<int64_t>(primitive)) {
        return static_cast<double>(std::get<int64_t>(primitive));
    }
    FAIL_CHECK("Primitive is not numeric");
    return 0.0;
}

bool expectBool(const ctoon::Value& value) {
    const auto& primitive = value.asPrimitive();
    REQUIRE(std::holds_alternative<bool>(primitive));
    return std::get<bool>(primitive);
}

void checkSample1User(const ctoon::Value& value) {
    const auto& obj = expectObject(value);
    CHECK_EQ(expectString(obj.at("name")), "Alice");
    CHECK_EQ(expectNumber(obj.at("age")), doctest::Approx(30.0));
    CHECK(expectBool(obj.at("active")));

    const auto& tags = expectArray(obj.at("tags"));
    REQUIRE_EQ(tags.size(), 3);
    CHECK_EQ(expectString(tags[0]), "programming");
    CHECK_EQ(expectString(tags[1]), "c++");
    CHECK_EQ(expectString(tags[2]), "serialization");
}

void checkSample2Users(const ctoon::Value& value) {
    const auto& obj = expectObject(value);
    const auto& users = expectArray(obj.at("users"));
    REQUIRE_EQ(users.size(), 2);

    const auto& user1 = expectObject(users[0]);
    CHECK_EQ(expectNumber(user1.at("id")), doctest::Approx(1.0));
    CHECK_EQ(expectString(user1.at("name")), "Alice");
    CHECK_EQ(expectString(user1.at("role")), "admin");

    const auto& user2 = expectObject(users[1]);
    CHECK_EQ(expectNumber(user2.at("id")), doctest::Approx(2.0));
    CHECK_EQ(expectString(user2.at("name")), "Bob");
    CHECK_EQ(expectString(user2.at("role")), "user");
}

void checkSample3Nested(const ctoon::Value& value) {
    const auto& obj = expectObject(value);
    const auto& order = expectObject(obj.at("order"));

    CHECK_EQ(expectString(order.at("id")), "ORD-12345");
    CHECK_EQ(expectString(order.at("status")), "completed");

    const auto& customer = expectObject(order.at("customer"));
    CHECK_EQ(expectString(customer.at("name")), "John Doe");
    CHECK_EQ(expectString(customer.at("email")), "john@example.com");

    const auto& items = expectArray(order.at("items"));
    REQUIRE_EQ(items.size(), 2);

    const auto& item1 = expectObject(items[0]);
    CHECK_EQ(expectString(item1.at("product")), "Book");
    CHECK_EQ(expectNumber(item1.at("quantity")), doctest::Approx(2.0));
    CHECK_EQ(expectNumber(item1.at("price")), doctest::Approx(15.0));

    const auto& item2 = expectObject(items[1]);
    CHECK_EQ(expectString(item2.at("product")), "Pen");
    CHECK_EQ(expectNumber(item2.at("quantity")), doctest::Approx(5.0));
    CHECK_EQ(expectNumber(item2.at("price")), doctest::Approx(2.5));
}

} // namespace

template <typename DumpFn, typename LoadFn>
void expectConversion(const ctoon::Value& source,
                      DumpFn&& dumps,
                      LoadFn&& loads,
                      void (*validator)(const ctoon::Value&)) {
    const auto serialized = dumps(source);
    const auto converted = loads(serialized);
    validator(converted);
}

TEST_CASE("Sample 1 stays consistent across formats") {
    const auto jsonValue = ctoon::loadJson((datapath / "sample1_user.json").string());
    const auto toonValue = ctoon::loadToon((datapath / "sample1_user.toon").string());
    const auto& toonText = expectString(toonValue);

    checkSample1User(jsonValue);

    const auto canonicalToon = ctoon::dumpsToon(jsonValue);
    CHECK_EQ(trim(toonText), trim(canonicalToon));

    SUBCASE("JSON dumps can be parsed back") {
        ctoon::Object data;
        data["name"] = ctoon::Value("Alice");
        data["age"] = ctoon::Value(30.0);
        data["active"] = ctoon::Value(true);

        ctoon::Array tags;
        tags.push_back(ctoon::Value("programming"));
        tags.push_back(ctoon::Value("c++"));
        tags.push_back(ctoon::Value("serialization"));
        data["tags"] = ctoon::Value(tags);

        const ctoon::Value value(data);
        checkSample1User(ctoon::loadsJson(ctoon::dumpsJson(value)));
    }

    SUBCASE("TOON dumps can be parsed back") {
        ctoon::Object data;
        data["name"] = ctoon::Value("Alice");
        data["age"] = ctoon::Value(30.0);
        data["active"] = ctoon::Value(true);

        ctoon::Array tags;
        tags.push_back(ctoon::Value("programming"));
        tags.push_back(ctoon::Value("c++"));
        tags.push_back(ctoon::Value("serialization"));
        data["tags"] = ctoon::Value(tags);

        const ctoon::Value value(data);
        const auto toon = ctoon::dumpsToon(value);
        const auto parsed = ctoon::loadsToon(toon);
        CHECK_EQ(trim(expectString(parsed)), trim(toon));
    }

    SUBCASE("JSON to TOON conversion") {
        const auto generated = ctoon::dumpsToon(jsonValue);
        CHECK_EQ(trim(generated), trim(toonText));
    }

    SUBCASE("TOON to JSON conversion") {
        const auto jsonText = ctoon::dumpsJson(toonValue);
        const auto parsed = ctoon::loadsJson(jsonText);
        CHECK_EQ(expectString(parsed), toonText);
    }
}

TEST_CASE("Sample 2 stays consistent across formats") {
    const auto jsonValue = ctoon::loadJson((datapath / "sample2_users.json").string());
    const auto toonValue = ctoon::loadToon((datapath / "sample2_users.toon").string());
    const auto& toonText = expectString(toonValue);

    checkSample2Users(jsonValue);

    const auto canonicalToon = ctoon::dumpsToon(jsonValue);
    CHECK_EQ(trim(toonText), trim(canonicalToon));

    SUBCASE("JSON to TOON conversion") {
        const auto generated = ctoon::dumpsToon(jsonValue);
        CHECK_EQ(trim(generated), trim(toonText));
    }

    SUBCASE("TOON to JSON conversion") {
        const auto jsonText = ctoon::dumpsJson(toonValue);
        const auto parsed = ctoon::loadsJson(jsonText);
        CHECK_EQ(expectString(parsed), toonText);
    }
}

TEST_CASE("Sample 3 stays consistent across formats") {
    const auto jsonValue = ctoon::loadJson((datapath / "sample3_nested.json").string());
    const auto toonValue = ctoon::loadToon((datapath / "sample3_nested.toon").string());
    const auto& toonText = expectString(toonValue);

    checkSample3Nested(jsonValue);

    const auto canonicalToon = ctoon::dumpsToon(jsonValue);
    CHECK_EQ(trim(toonText), trim(canonicalToon));

    SUBCASE("JSON to TOON conversion") {
        const auto generated = ctoon::dumpsToon(jsonValue);
        CHECK_EQ(trim(generated), trim(toonText));
    }

    SUBCASE("TOON to JSON conversion") {
        const auto jsonText = ctoon::dumpsJson(toonValue);
        const auto parsed = ctoon::loadsJson(jsonText);
        CHECK_EQ(expectString(parsed), toonText);
    }
}

TEST_CASE("Toon options customize formatting supports alternate delimiters") {
    ctoon::Object obj;
    obj["name"] = ctoon::Value("Alice");

    ctoon::Array tags;
    tags.emplace_back(ctoon::Value("red"));
    tags.emplace_back(ctoon::Value("blue"));
    obj["tags"] = ctoon::Value(tags);

    ctoon::EncodeOptions options;
    options.delimiter = ctoon::Delimiter::Pipe; 
    options.indent = 4;
    
    auto toon = ctoon::dumpsToon(ctoon::Value(obj), options);
    CHECK(toon.find("tags[2]: red|blue") != std::string::npos);
    CHECK(toon.find("name: Alice") != std::string::npos);
}
