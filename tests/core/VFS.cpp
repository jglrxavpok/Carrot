//
// Created by jglrxavpok on 01/05/2022.
//

#include <gtest/gtest.h>
#include <core/io/vfs/VirtualFileSystem.h>
#include <stdexcept>

using namespace Carrot::IO;

#ifdef _WIN32
#define WIN_PREFIX "c:"
#else
#define WIN_PREFIX ""
#endif

TEST(VFSPaths, ThrowOnAbsolutePath) {
    EXPECT_THROW(VFS::Path("root", "/rootpath"), std::invalid_argument);
}

TEST(VFSPaths, FromURI) {
    auto check = [&](const std::string& uri, const std::string& root, const Path& path) {
        VFS::Path p{uri};
        EXPECT_TRUE(p.getPath() == path);
        EXPECT_TRUE(p.getRoot() == root);
    };

    check("game://something/list.txt", "game", "something/list.txt");
    check("engine://something/list.txt", "engine", "something/list.txt");
    check("something/list.txt", "", "something/list.txt");
    check("foobar://path/to/so/other/file.mp3", "foobar", "path/to/so/other/file.mp3");
    EXPECT_THROW(check("invalid-uri://aaa:bbb", "invalid-uri", "aaa:bbb"), std::invalid_argument); // ':' not allowed multiple times
}

TEST(VFSPaths, RootsMustBeUnique) {
    VFS vfs;
    vfs.addRoot("foo", WIN_PREFIX "/root/folder");
    EXPECT_THROW(vfs.addRoot("foo", WIN_PREFIX "/root/folder"), Carrot::Assertions::Error);
}

TEST(VFSPaths, ValidateRoot) {
    // must match [a-z0-9_]+
    VFS vfs;
    vfs.addRoot("foo", WIN_PREFIX "/root/folder");
    vfs.addRoot("doiqzjdpoijqzdoizqj", WIN_PREFIX "/root/folder");
    vfs.addRoot("__root__", WIN_PREFIX "/root/folder");
    vfs.addRoot("42", WIN_PREFIX "/root/folder");
    vfs.addRoot("abc123", WIN_PREFIX "/root/folder");

    EXPECT_THROW(vfs.addRoot("FOO", WIN_PREFIX "/root/folder"), std::invalid_argument);
    EXPECT_THROW(vfs.addRoot("Some root", WIN_PREFIX "/root/folder"), std::invalid_argument);
    EXPECT_THROW(vfs.addRoot("", WIN_PREFIX "/root/folder"), std::invalid_argument);
    EXPECT_THROW(vfs.addRoot("foo-bar", WIN_PREFIX "/root/folder"), std::invalid_argument);
    EXPECT_THROW(vfs.addRoot("foo/bar", WIN_PREFIX "/root/folder"), std::invalid_argument);

    EXPECT_THROW(vfs.addRoot("foobar", "root/folder"), std::invalid_argument); // path must be absolute
}

TEST(VFS, RemoveInexistentRoot) {
    VFS vfs;
    EXPECT_FALSE(vfs.removeRoot("foo"));
}

TEST(VFS, RemoveExistentRoot) {
    VFS vfs;
    vfs.addRoot("foo", WIN_PREFIX "/root/folder");
    EXPECT_TRUE(vfs.removeRoot("foo"));
}

TEST(VFS, Resolve) {
    VFS vfs;
    vfs.addRoot("foo", WIN_PREFIX "/root/folder");
    vfs.addRoot("foobar", WIN_PREFIX "/aaa");

    EXPECT_EQ(vfs.resolve("foo://file.txt"), WIN_PREFIX "/root/folder/file.txt");
    EXPECT_EQ(vfs.resolve("foo://inside/folder/f"), WIN_PREFIX "/root/folder/inside/folder/f");
    EXPECT_EQ(vfs.resolve("foobar://file.txt"), WIN_PREFIX "/aaa/file.txt");
    EXPECT_THROW(vfs.resolve("foo://../file.txt"), std::invalid_argument); // not allowed to go outside of root
}

TEST(VFS, Represent) {
    VFS vfs;
    vfs.addRoot("foo", WIN_PREFIX "/root/folder");
    vfs.addRoot("foobar", WIN_PREFIX "/aaa");

    EXPECT_EQ(vfs.represent(WIN_PREFIX "/root/folder/file.txt"), "foo://file.txt");
    EXPECT_EQ(vfs.represent(WIN_PREFIX "/aaa/f.txt"), "foobar://f.txt");
    EXPECT_FALSE(vfs.represent(WIN_PREFIX "/bbb/f.txt").has_value()); // no root associated
}

TEST(VFS, Relative) {
    VFS vfs;
    vfs.addRoot("engine", WIN_PREFIX "/root/folder");
    vfs.addRoot("game", WIN_PREFIX "/aaa");

    EXPECT_EQ(VFS::Path{"game://models/test.gltf"}.relative("/models/a.gltf"), VFS::Path { "game://models/a.gltf" }); // absolute
    EXPECT_EQ(VFS::Path{"game://models/test.gltf"}.relative("a.gltf"), VFS::Path { "game://models/a.gltf" }); // actually relative
    EXPECT_EQ(VFS::Path{"game://models/test.gltf"}.relative("../a.txt"), VFS::Path { "game://a.txt" }); // actually relative
}