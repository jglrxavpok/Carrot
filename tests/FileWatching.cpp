#include <gtest/gtest.h>

#include <filesystem>
#include <iostream>
#include <fstream>
#include <thread>
#include <core/io/FileWatcher.h>

using namespace Carrot;
using namespace Carrot::IO;

TEST(FileWatching, DetectModification) {
    std::filesystem::path testFile = "mytestfile.txt";

    std::ofstream out(testFile);
    out << "Hello";
    out.flush();

    std::this_thread::sleep_for(std::chrono::duration<float>(0.1f));

    int detectedChanges = 0;

    FileWatcher watcher([&detectedChanges](const auto& path) {
        detectedChanges++;
    }, {testFile});
    watcher.tick();

    ASSERT_EQ(detectedChanges, 0);

    out << " world";
    out.flush();

    std::this_thread::sleep_for(std::chrono::duration<float>(0.1f));

    watcher.tick();
    ASSERT_EQ(detectedChanges, 1);

    out << "!";
    out.close();

    watcher.tick();
    ASSERT_EQ(detectedChanges, 2);

    std::filesystem::remove(testFile);
}
