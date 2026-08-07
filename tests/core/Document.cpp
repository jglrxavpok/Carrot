//
// Created by jglrxavpok on 05/02/2025.
//

#include <gtest/gtest.h>
#include <core/io/Document.h>
#include <core/utils/TOML.h>

using namespace Carrot;

TEST(Documents, CreateEmpty) {
    DocumentElement d;
    EXPECT_EQ(d.getType(), DocumentType::Object);
    EXPECT_EQ(d.getSubElementCount(), 0);
}

TEST(Documents, AddSimpleValue) {
    DocumentElement d;
    d["a"] = i64{50};
    EXPECT_EQ(d.getSubElementCount(), 1);
    EXPECT_TRUE(d.contains("a"));
    EXPECT_EQ(d["a"].getAsInt64(), 50);
}

TEST(Documents, ReplaceValue) {
    DocumentElement d;
    d["a"] = i64{50};
    EXPECT_EQ(d.getSubElementCount(), 1);
    EXPECT_EQ(d["a"].getAsInt64(), 50);
    d["a"] = i64{10};
    EXPECT_EQ(d.getSubElementCount(), 1);
    EXPECT_EQ(d["a"].getAsInt64(), 10);
}

TEST(Documents, ChangeTypeWithAssignment) {
    DocumentElement d{DocumentType::Bool};
    d = false;

    EXPECT_TRUE(d.isBool());
    EXPECT_TRUE(d.is(DocumentType::Bool));
    EXPECT_FALSE(d.getAsBool());
    d = true;
    EXPECT_TRUE(d.isBool());
    EXPECT_TRUE(d.is(DocumentType::Bool));
    EXPECT_TRUE(d.getAsBool());

    d = 3.1415;
    EXPECT_TRUE(d.isDouble());
    EXPECT_TRUE(d.is(DocumentType::Double));
    EXPECT_EQ(d.getAsDouble(), 3.1415);

    d = i64{-360};
    EXPECT_TRUE(d.isInt64());
    EXPECT_TRUE(d.is(DocumentType::Int64));
    EXPECT_EQ(d.getAsInt64(), -360);

    d = "Hello world!";
    EXPECT_TRUE(d.isString());
    EXPECT_TRUE(d.is(DocumentType::String));
    EXPECT_EQ(d.getAsString(), "Hello world!");
}

// TODO: array view

TEST(Documents, BasicArray) {
    DocumentElement d{DocumentType::Array};
    d.setReserve(3);
    d.pushBack(i64 { 1 });
    d.pushBack(i64 { 2 });
    d.pushBack(i64 { 3 });

    EXPECT_EQ(d.getSubElementCount(), 3);
    EXPECT_TRUE(d[0].isInt64());
    EXPECT_EQ(d[0].getAsInt64(), 1);

    EXPECT_TRUE(d[1].isInt64());
    EXPECT_EQ(d[1].getAsInt64(), 2);

    EXPECT_TRUE(d[2].isInt64());
    EXPECT_EQ(d[2].getAsInt64(), 3);
}

TEST(Documents, BasicArrayView) {
    DocumentElement d{DocumentType::Array};
    d.setReserve(3);
    d.pushBack(i64 { 1 });
    d.pushBack(i64 { 2 });
    d.pushBack(i64 { 3 });

    // iterators
    {
        auto arrayView = d.getAsArray();
        auto iter = arrayView.begin();
        EXPECT_NE(iter, arrayView.end());
        EXPECT_TRUE(iter->isInt64());
        EXPECT_EQ(iter->getAsInt64(), 1);

        ++iter;
        EXPECT_NE(iter, arrayView.end());
        EXPECT_TRUE(iter->isInt64());
        EXPECT_EQ(iter->getAsInt64(), 2);

        ++iter;
        EXPECT_NE(iter, arrayView.end());
        EXPECT_TRUE(iter->isInt64());
        EXPECT_EQ(iter->getAsInt64(), 3);

        ++iter;
        EXPECT_EQ(iter, arrayView.end());
    }

    // for each version
    {
        int index = 0;
        for (auto& element : d.getAsArray()) {
            EXPECT_LE(index, d.getSubElementCount());
            EXPECT_TRUE(element.isInt64());
            EXPECT_EQ(element.getAsInt64(), index+1);

            index++;
        }
    }
}
// TODO: object view

// Serialisation
TEST(Documents, SerializeEmptyToTOML) {
    DocumentElement d;
    toml::table out;
    out << d;
    EXPECT_EQ(out.size(), 0);
}

TEST(Documents, SerializeTrivialToTOML) {
    DocumentElement d;
    d["abc"] = i64{42};
    toml::table out;
    out << d;
    EXPECT_EQ(out.size(), 1);
    EXPECT_TRUE(out.contains("abc"));
    EXPECT_EQ(out["abc"].as_integer()->get(), 42ll);
}

TEST(Documents, SerializeWithArrayToTOML) {
    DocumentElement d;
    DocumentElement& nestedArray = d["abc"];
    nestedArray.reset(DocumentType::Array);
    nestedArray.setSize(4);

    for (int i = 0; i < 4; ++i) {
        nestedArray[i] = i64{42 + i * 100};
    }

    toml::table out;
    out << d;
    EXPECT_EQ(out.size(), 1);
    EXPECT_TRUE(out.contains("abc"));
    EXPECT_TRUE(out["abc"].is_array());

    auto asArray = *out["abc"].as_array();
    EXPECT_EQ(asArray.size(), 4);
    for (int i = 0; i < 4; ++i) {
        EXPECT_TRUE(asArray[i].is_integer());
        EXPECT_EQ(asArray[i].as_integer()->get(), i64{42 + i * 100});
    }
}

TEST(Documents, DeserializeTrivialFromTOML) {
    DocumentElement d;
    toml::table toml;
    toml.insert("abc", toml::value<i64>(42));
    toml >> d;
    EXPECT_EQ(d.getSubElementCount(), 1);
    EXPECT_TRUE(d.contains("abc"));
    EXPECT_EQ(d["abc"].getAsInt64(), 42ll);
}

TEST(Documents, DeserializeComplexFromTOML) {
    DocumentElement d;
    // TOML from https://marzer.github.io/tomlplusplus/#mainpage-example-manipulations
    static constexpr auto source = R"(
        str = "hello world"

        numbers = [ 1, 2, 3, "four", 5.0 ]
        vegetables = [ "tomato", "onion", "mushroom", "lettuce" ]
        minerals = [ "quartz", "iron", "copper", "diamond" ]

        [animals]
        cats = [ "tiger", "lion", "puma" ]
        birds = [ "macaw", "pigeon", "canary" ]
        fish = [ "salmon", "trout", "carp" ]
    )";
    toml::table tbl = toml::parse(source);
    tbl >> d;
    EXPECT_EQ(d.getSubElementCount(), 5);

    {
        EXPECT_TRUE(d.contains("str"));
        EXPECT_EQ(d["str"].getAsString(), "hello world");
    }

    {
        EXPECT_TRUE(d.contains("numbers"));
        EXPECT_TRUE(d["numbers"].isArray());
        auto arrayView = d["numbers"].getAsArray();
        EXPECT_EQ(arrayView.getSize(), 5);
        EXPECT_EQ(arrayView[0].getAsInt64(), 1);
        EXPECT_EQ(arrayView[1].getAsInt64(), 2);
        EXPECT_EQ(arrayView[2].getAsInt64(), 3);
        EXPECT_EQ(arrayView[3].getAsString(), "four");
        EXPECT_EQ(arrayView[4].getAsDouble(), 5.0);
    }

    {
        EXPECT_TRUE(d.contains("minerals"));
        EXPECT_TRUE(d["minerals"].isArray());
        auto arrayView = d["minerals"].getAsArray();
        EXPECT_EQ(arrayView.getSize(), 4);
        EXPECT_EQ(arrayView[0].getAsString(), "quartz");
        EXPECT_EQ(arrayView[1].getAsString(), "iron");
        EXPECT_EQ(arrayView[2].getAsString(), "copper");
        EXPECT_EQ(arrayView[3].getAsString(), "diamond");
    }
    {
        EXPECT_TRUE(d.contains("animals"));
        EXPECT_TRUE(d["animals"].isObject());

        const auto& obj = d["animals"];
        EXPECT_EQ(obj.getSubElementCount(), 3);
        {
            auto catsArray = obj.at("cats").getAsArray();
            EXPECT_EQ(catsArray.getSize(), 3);
            EXPECT_EQ(catsArray[0].getAsString(), "tiger");
            EXPECT_EQ(catsArray[1].getAsString(), "lion");
            EXPECT_EQ(catsArray[2].getAsString(), "puma");
        }
        {
            auto birdsArray = obj.at("birds").getAsArray();
            EXPECT_EQ(birdsArray.getSize(), 3);
            EXPECT_EQ(birdsArray[0].getAsString(), "macaw");
            EXPECT_EQ(birdsArray[1].getAsString(), "pigeon");
            EXPECT_EQ(birdsArray[2].getAsString(), "canary");
        }
        {
            auto fishArray = obj.at("fish").getAsArray();
            EXPECT_EQ(fishArray.getSize(), 3);
            EXPECT_EQ(fishArray[0].getAsString(), "salmon");
            EXPECT_EQ(fishArray[1].getAsString(), "trout");
            EXPECT_EQ(fishArray[2].getAsString(), "carp");
        }
    }
}

TEST(Documents, Merging) {
    // Objects get their fields merged
    {
        Carrot::DocumentElement a;
        Carrot::DocumentElement b;

        a["a"] = 42;
        a["b"] = 50;

        b["c"] = "some text";
        b["a"] = 99;

        a.mergeWith(b);
        EXPECT_TRUE(a.isObject());
        EXPECT_EQ(a["a"].getAsInt64(), 99); // overwritten from b
        EXPECT_EQ(a["b"].getAsInt64(), 50); // kept from a
        EXPECT_EQ(a["c"].getAsString(), "some text"); // imported from b
    }

    // Recursive merge
    {
        Carrot::DocumentElement a;
        Carrot::DocumentElement b;

        a["a"]["a"] = 99;
        a["b"] = 50;

        b["a"]["b"] = "some text";

        a.mergeWith(b);
        EXPECT_TRUE(a.isObject());
        EXPECT_EQ(a["a"]["a"].getAsInt64(), 99); // kept from a
        EXPECT_EQ(a["b"].getAsInt64(), 50); // kept from a
        EXPECT_EQ(a["a"]["b"].getAsString(), "some text"); // imported from b, nested
    }

    // Arrays are appended
    {
        Carrot::DocumentElement a{Carrot::DocumentType::Array};
        Carrot::DocumentElement b{Carrot::DocumentType::Array};

        for (int i = 0; i < 10; i++) {
            a.pushBack(static_cast<i64>(i));
        }
        for (int i = 10; i < 20; i++) {
            b.pushBack(static_cast<i64>(i));
        }

        a.mergeWith(b);
        EXPECT_TRUE(a.isArray());
        auto asArray = a.getAsArray();
        EXPECT_EQ(asArray.getSize(), 20);
        for (int i = 0; i < asArray.getSize(); i++) {
            EXPECT_EQ(asArray[i].getAsInt64(), static_cast<i64>(i));
        }
    }

    // other types get replaced
    {
        Carrot::DocumentElement a;
        Carrot::DocumentElement b;

        a = "abc";
        b = "def";
        a.mergeWith(b);
        EXPECT_TRUE(a.isString());
        EXPECT_EQ(a.getAsString(), "def");
    }
    {
        Carrot::DocumentElement a;
        Carrot::DocumentElement b;

        a = static_cast<i64>(42);
        b = static_cast<i64>(50);
        a.mergeWith(b);
        EXPECT_TRUE(a.isInt64());
        EXPECT_EQ(a.getAsInt64(), 50);
    }

    // Incompatible types are rejected
    {
        Carrot::DocumentElement a;
        Carrot::DocumentElement b;

        a = static_cast<i64>(42);
        b = "text";
        EXPECT_THROW(a.mergeWith(b), std::invalid_argument);
        EXPECT_TRUE(a.isInt64());
        EXPECT_EQ(a.getAsInt64(), 42);
    }
}