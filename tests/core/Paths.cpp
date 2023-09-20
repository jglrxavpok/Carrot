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

TEST(Paths, GetFilename) {
    ASSERT_EQ(std::string(Path("").getFilename()), "");
    ASSERT_EQ(std::string(Path("/a").getFilename()), "a");
    ASSERT_EQ(std::string(Path("/a/").getFilename()), "");
    ASSERT_EQ(std::string(Path("abc").getFilename()), "abc");
}

TEST(Paths, GetExtension) {
    ASSERT_EQ(std::string(Path("").getExtension()), "");
    ASSERT_EQ(std::string(Path("/a").getExtension()), "");
    ASSERT_EQ(std::string(Path("/a.b/aa").getExtension()), "");
    ASSERT_EQ(std::string(Path("/a.b/aa.c").getExtension()), ".c");
    ASSERT_EQ(std::string(Path("/a/.txt").getExtension()), ""); // hidden files on Unix
    ASSERT_EQ(std::string(Path("/a/bbb.txt").getExtension()), ".txt");
    ASSERT_EQ(std::string(Path("aaa.gltf").getExtension()), ".gltf");
}

TEST(Paths, Relative) {
    ASSERT_EQ(Path("").relative("a"), Path("a"));
    ASSERT_EQ(Path("a").relative("b"), Path("b"));
    ASSERT_EQ(Path("folder/").relative("b"), Path("folder/b"));
    ASSERT_EQ(Path("folder/").relative("./b"), Path("folder/./b"));
    ASSERT_EQ(Path("folder/").relative("a/./b"), Path("folder/a/./b"));
    ASSERT_EQ(Path("folder/").relative("../b"), Path("folder/../b")); // no normalization
    ASSERT_EQ(Path("folder/").relative("folder2/.."), Path("folder/folder2/.."));
    ASSERT_EQ(Path("folder/").relative("folder2/../folder3/"), Path("folder/folder2/../folder3/"));
}

TEST(Paths, ExtensionReplace) {
    ASSERT_EQ(Path("").withExtension(".png"), Path(".png"));
    ASSERT_EQ(Path("a.txt").withExtension(".png"), Path("a.png"));
    ASSERT_EQ(Path("a.txt").withExtension("png"), Path("a.png")); // dot not necessary

    ASSERT_EQ(Path("a").withExtension(".png"), Path("a.png"));
    ASSERT_EQ(Path("/a/b/c.gltf").withExtension(".h"), Path("/a/b/c.h"));
    ASSERT_EQ(Path("/a/../b.cpp").withExtension(".h"), Path("/a/../b.h")); // does not normalize, only affects extension
    ASSERT_EQ(Path("/a.subfolder/b.cpp").withExtension(".h"), Path("/a.subfolder/b.h")); // does not modify path parts
    ASSERT_EQ(Path("/a.tar.gz").withExtension(".zip"), Path("/a.tar.zip")); // only the last '.xyz' is modified
    ASSERT_EQ(Path("/a").withExtension(".tar.gz"), Path("/a.tar.gz")); // can add any extension, even with multiple dots


    ASSERT_EQ(Path(".gitignore").withExtension(".zip"), Path(".gitignore.zip")); // special case: hidden files
}

TEST(Paths, GetParent) {
    ASSERT_EQ(Path("a/b").getParent(), Path("a"));
    ASSERT_EQ(Path("a/b/c/d.txt").getParent(), Path("a/b/c"));

    ASSERT_EQ(Path("abc").getParent(), Path{});
}