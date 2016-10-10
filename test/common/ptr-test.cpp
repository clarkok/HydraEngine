#include "catch_with_main.hpp"
#include "common/ptr.hpp"

using namespace hydra;

TEST_CASE("Ptr can constructed, dereferenced, assigned and compared", "[Ptr]")
{
    SECTION("default constructing")
    {
        Ptr<int> uut;

        REQUIRE(uut.Value == nullptr);
    }

    SECTION("constructing with value")
    {
        int t = 1;
        Ptr<int> uut(&t);

        REQUIRE(uut.Value == &t);
    }

    SECTION("dereferencing")
    {
        int t = 1;
        Ptr<int> uut(&t);

        REQUIRE(*uut == 1);
    }

    SECTION("modifying value referenced")
    {
        int t = 1;
        Ptr<int> uut(&t);

        *uut = 2;
        REQUIRE(t == 2);
    }

    SECTION("assigning from Ptr")
    {
        Ptr<int> uuta;

        int t = 1;
        Ptr<int> uutb(&t);

        uuta = uutb;
        REQUIRE(uuta.Value == uutb.Value);
    }

    SECTION("assigning from pointer")
    {
        Ptr<int> uut;
        int t = 1;

        uut = &t;
        REQUIRE(uut.Value == &t);
    }

    SECTION("comparing bewteen Ptrs")
    {
        int t = 1;
        Ptr<int> uuta, uutb;

        REQUIRE(uuta == uutb);
        REQUIRE(uuta >= uutb);
        REQUIRE(uuta <= uutb);
        REQUIRE((uuta != uutb) == false);

        uuta = &t;
        REQUIRE(uuta > uutb);
        REQUIRE(uuta >= uutb);
        REQUIRE(uutb < uuta);
        REQUIRE(uutb <= uuta);
        REQUIRE((uuta != uutb) == true);
    }
}