// Copyright 2018 Avsyankaa <Avsyankaa@gmail.com>

#include <gtest/gtest.h>

#include <krauler.hpp>
TEST(Krauler, Test1) {

    krauler k(8, 4, 4, "github.com/Avsyankaa/Tests","file.log");
    SUCCEED();

}

TEST(Krauler, Test2) {

    krauler k(8, 4, 4, "github.com/Avsyankaa/Tests","file.log");
    k.make_krauling();
    SUCCEED();

}

