#include "__CLASS__.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("__ID__ reports its identity") {
    xen::__NS__::__CLASS__ plugin;
    REQUIRE(plugin.info().id == "__ID__");
    REQUIRE(plugin.info().name == "__NAME__");
    REQUIRE(plugin.info().version == 1);
}
