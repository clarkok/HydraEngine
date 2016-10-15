#include "catch.hpp"

#include "common/assert.hpp"

using namespace hydra;

TEST_CASE("Assert will failed when condition is false", "[Assert]")
{
    bool catched = false;

    try
    {
        Assert(false, "It failed");
    }
    catch (const AssertFailedException &e)
    {
        catched = true;
    }

    REQUIRE(catched);
}

TEST_CASE("Assert will not failed when condition is true", "[Assert]")
{
    bool catched = false;

    try
    {
        Assert(true, "It should not fail");
    }
    catch (const AssertFailedException &e)
    {
        catched = true;
    }

    REQUIRE(!catched);
}