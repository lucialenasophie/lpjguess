#include "catch.hpp"
#include "kdtree.h"

TEST_CASE("General kd-tree implementation is working", "[kdtree]") {
    point<int, 2> points[] = { { 2, 3 }, { 5, 4 }, { 9, 6 }, { 4, 7 }, { 8, 1 }, { 7, 2 } };
    kdtree<int, 2> tree(std::begin(points), std::end(points));

    point<int, 2> n = tree.nearest({ 9, 2 });

    REQUIRE(n.get(0) == 8);
    REQUIRE(n.get(1) == 1);
}

TEST_CASE("Exact point is found", "[kdtree]") {
    point<int, 2> points[] = { { 2, 3 }, { 5, 4 }, { 9, 6 }, { 4, 7 }, { 8, 1 }, { 7, 2 } };
    kdtree<int, 2> tree(std::begin(points), std::end(points));

    point<int, 2> n = tree.nearest({ 5, 4 });

    REQUIRE(n.get(0) == 5);
    REQUIRE(n.get(1) == 4);
}

TEST_CASE("Single entry kdtree", "[kdtree]") {
    point<int, 2> points[] = { { 2, 3 } };
    kdtree<int, 2> tree(std::begin(points), std::end(points));

    point<int, 2> n = tree.nearest({ 5, 4 });

    REQUIRE(n.get(0) == 2);
    REQUIRE(n.get(1) == 3);
}