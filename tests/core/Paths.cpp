//
// Created by jglrxavpok on 01/05/2022.
//

#include <gtest/gtest.h>
#include <core/io/Path.h>
#include <stdexcept>

using namespace Carrot::IO;

TEST(Paths, IterationOnRelativePath) {
    Path p = "a/b/c";
    PathPartIterator it = p.begin();
    ASSERT_NE(it, p.end());
    ASSERT_EQ("a", *it);
    it++;
    ASSERT_NE(it, p.end());
    ASSERT_EQ("b", *it);
    it++;
    ASSERT_NE(it, p.end());
    ASSERT_EQ("c", *it);
    it++;
    ASSERT_EQ(it, p.end());
    ASSERT_THROW(*it, std::runtime_error);
}

TEST(Paths, IterationOnAbsolutePath) {
    Path p = "/a/b/c";
    PathPartIterator it = p.begin();
    ASSERT_NE(it, p.end());
    ASSERT_EQ("/", *it);
    it++;
    ASSERT_NE(it, p.end());
    ASSERT_EQ("a", *it);
    it++;
    ASSERT_NE(it, p.end());
    ASSERT_EQ("b", *it);
    it++;
    ASSERT_NE(it, p.end());
    ASSERT_EQ("c", *it);
    it++;
    ASSERT_EQ(it, p.end());
    ASSERT_THROW(*it, std::runtime_error);
}

TEST(Paths, IterationOnEmptyParts) {
    Path p = "/a//c";
    PathPartIterator it = p.begin();
    ASSERT_NE(it, p.end());
    ASSERT_EQ("/", *it);
    it++;
    ASSERT_NE(it, p.end());
    ASSERT_EQ("a", *it);
    it++;
    ASSERT_NE(it, p.end());
    ASSERT_EQ("", *it);
    it++;
    ASSERT_NE(it, p.end());
    ASSERT_EQ("c", *it);
    it++;
    ASSERT_EQ(it, p.end());
    ASSERT_THROW(*it, std::runtime_error);
}

TEST(Paths, RelativeOrAbsolute) {
    ASSERT_TRUE(Path("/a").isAbsolute());
    ASSERT_FALSE(Path("/a").isRelative());

    ASSERT_TRUE(Path("/a/b").isAbsolute());
    ASSERT_FALSE(Path("/a/b").isRelative());

    ASSERT_TRUE(Path("/").isAbsolute());
    ASSERT_FALSE(Path("/").isRelative());

    ASSERT_FALSE(Path("").isAbsolute());
    ASSERT_TRUE(Path("").isRelative());

    ASSERT_FALSE(Path("somefile").isAbsolute());
    ASSERT_TRUE(Path("somefile").isRelative());

    ASSERT_FALSE(Path("folder/somefile").isAbsolute());
    ASSERT_TRUE(Path("folder/somefile").isRelative());

    ASSERT_FALSE(Path("c:/somepath").isAbsolute()); // we do not handle OS paths
}

TEST(Paths, Append) {
    ASSERT_EQ(Path("/a") / "b", Path("/a/b"));
    ASSERT_EQ(Path("a") / "b", Path("a/b"));
    ASSERT_EQ(Path("a") / "b/c", Path("a/b/c"));
    ASSERT_EQ(Path("a") / "", Path("a"));
    ASSERT_EQ(Path("") / "b", Path("b"));


    ASSERT_EQ(Path("a/") / "b", Path("a/b")); // trailing separator must not affect result
    ASSERT_EQ(Path("a") / "/b", Path("a/b")); // leading separator must not affect result
    ASSERT_EQ(Path("a/") / "/b", Path("a/b")); // trailing and trailing separators must not affect result
}

TEST(Paths, NormalizeMustNotChangeNormalPath) {
    ASSERT_EQ(Path("already/normal/path").normalize(), Path("already/normal/path"));
}

TEST(Paths, NormalizeRemovesTrailingSeparator) {
    ASSERT_EQ(Path("mypath/").normalize(), Path("mypath"));
}

TEST(Paths, NormalizeRemovesEmptyParts) {
    ASSERT_EQ(Path("//////a").normalize(), Path("/a"));
}

TEST(Paths, NormalizeRemovesDot) {
    ASSERT_EQ(Path("mypath/./aa/./b/.gitignore").normalize(), Path("mypath/aa/b/.gitignore"));
    ASSERT_EQ(Path("/mypath/./b/.gitignore").normalize(), Path("/mypath/b/.gitignore"));
}

TEST(Paths, NormalizeGoesBackUp) {
    ASSERT_EQ(Path("mypath/../file").normalize(), Path("file"));
    ASSERT_EQ(Path("/a/b/../file").normalize(), Path("/a/file"));
    ASSERT_EQ(Path("a/b/c/../../file").normalize(), Path("a/file"));
    ASSERT_THROW(Path("a/../..").normalize(), std::invalid_argument); // must not go outside of root
}