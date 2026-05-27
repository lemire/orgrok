#define CATCH_CONFIG_MAIN
#include <catch2/catch_all.hpp>

TEST_CASE("Basic test setup works", "[setup]") {
    REQUIRE(1 + 1 == 2);
    REQUIRE(true);
}