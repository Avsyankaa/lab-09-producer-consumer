// Copyright 2018 Avsyankaa <Avsyankaa@gmail.com>

#include <gtest/gtest.h>

#include <krauler.hpp>
TEST(Krauler, Test1) {

    krauler k(8, 4, 4, "github.com/Avsyankaa/Tests","file.log");
    SUCCEED();

}

TEST(Krauler, Test2) {

    krauler k2(8, 4, 4, "github.com/Avsyankaa/Tests","file.log");
    k2.make_krauling();
    SUCCEED();

}

