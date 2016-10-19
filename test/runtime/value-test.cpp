#include "catch.hpp"

#include "runtime/value.hpp"

#include <limits>
#include <cmath>

using namespace hydra;

class ValueTest : public Value
{
public:
    static uint64_t RawContent(Value v)
    {
        return *reinterpret_cast<uint64_t *>(&v);
    }
};

TEST_CASE_METHOD(ValueTest, "Value can be assigned", "[Value]")
{
    Value uut;

    SECTION("Value can be default constructed")
    {
        REQUIRE(RawContent(uut) == ~(uint64_t)(0));
    }

    SECTION("Value can be made from Int")
    {
        for (int i = 0; i < 100; ++i)
        {
            uut = Value::FromInt(i);

            REQUIRE(uut.IsInteger());
            REQUIRE(RawContent(uut) == ((0xFFFC000000000000) | i));
        }

        uut = Value::FromInt(std::numeric_limits<int>::max());
        REQUIRE(uut.IsInteger());
        REQUIRE(RawContent(uut) == 0xFFFC00007FFFFFFF);

        uut = Value::FromInt(std::numeric_limits<int>::min());
        REQUIRE(uut.IsInteger());
        REQUIRE(RawContent(uut) == 0xFFFC000080000000);
    }

    SECTION("Value can be made from Double")
    {
        for (double i = 0.1; i < 100; i = i + 2.1)
        {
            uut = Value::FromDouble(i);
            REQUIRE(uut.IsDouble());
            REQUIRE(!uut.IsNaN());
            uint64_t content = RawContent(uut);
            REQUIRE(*reinterpret_cast<double *>(&content) == i);
        }

        uut = Value::FromDouble(std::numeric_limits<double>::quiet_NaN());
        REQUIRE(uut.IsDouble());
        REQUIRE(uut.IsNaN());
        uint64_t content = RawContent(uut);
        REQUIRE(std::isnan(*reinterpret_cast<double *>(&content)));
    }

    SECTION("Value can be made from Undefined")
    {
        uut = Value::FromUndefined();
        REQUIRE(uut.IsUndefined());
        REQUIRE(RawContent(uut) == ~(uint64_t)(0));
    }

    SECTION("Value can be made from bool")
    {
        uut = Value::FromBool(true);
        REQUIRE(uut.IsBool());
        REQUIRE(RawContent(uut) == 0xFFFB000000000000);

        uut = Value::FromBool(false);
        REQUIRE(uut.IsBool());
        REQUIRE(RawContent(uut) == 0xFFFA000000000000);
    }

    SECTION("Value can be made from Symbol")
    {
        for (uint32_t i = 0; i < 100; ++i)
        {
            uut = Value::FromSymbol(i);
            REQUIRE(uut.IsSymbol());
            REQUIRE(RawContent(uut) == (0xFFFD000000000000 | i));
        }
    }
}

TEST_CASE_METHOD(ValueTest, "Value can be retrived", "[Value]")
{
    Value uut;

    SECTION("Value can be retrived as Int")
    {
        for (int i = 0; i < 100; ++i)
        {
            uut = Value::FromInt(i);
            REQUIRE(i == uut.AsInt());
        }
    }

    SECTION("Value can be retrived as Double")
    {
        for (double i = 0.2; i < 100; i += 1.3)
        {
            uut = Value::FromDouble(i);
            REQUIRE(i == uut.AsDouble());
        }

        uut = Value::FromNaN();
        REQUIRE(uut.IsNaN());
    }

    SECTION("Value can be retrived as Bool")
    {
        uut = Value::FromBool(true);
        REQUIRE(true == uut.AsBool());

        uut = Value::FromBool(false);
        REQUIRE(false == uut.AsBool());
    }

    SECTION("Value can be retrived as Symbol")
    {
        for (uint32_t i = 0; i < 100; ++i)
        {
            uut = Value::FromSymbol(i);
            REQUIRE(i == uut.AsSymbol());
        }
    }
}
