//
// Created by jglrxavpok on 07/03/2023.
//

#include <gtest/gtest.h>
#include <core/scripting/csharp/Engine.h>
#include <core/scripting/csharp/Module.h>
#include <core/io/IO.h>

using namespace Carrot::Scripting;
namespace fs = std::filesystem;

class CSharpTests: public ::testing::Test {
protected:
    CSharpTests() {
        vfs.addRoot("test", fs::current_path());
        Carrot::IO::Resource::vfsToUse = &vfs;
    }

    ScriptingEngine engine;
    Carrot::IO::VirtualFileSystem vfs;
};

static void generateTestFile(const fs::path& destination) {
    std::string code = R"c#(
using System;

public class SimpleClass {
    public static void Main() {
        Console.WriteLine("Hello world");
    }
}
)c#";
    Carrot::IO::writeFile(destination.string(), (void*)code.c_str(), code.size());
}

TEST_F(CSharpTests, SimpleCompilation) {
    fs::path tempDir = fs::temp_directory_path();

    fs::path testFile = tempDir / "TestSimpleCompilation.cs";
    generateTestFile(testFile);
    CLEANUP(fs::remove(testFile));
    std::vector<fs::path> sourceFiles = {
            testFile
    };
    fs::path assemblyOutput = tempDir / "TestSimpleCompilation.dll";
    CLEANUP(fs::remove(assemblyOutput));
    bool r = engine.compileFiles(assemblyOutput, sourceFiles);
    ASSERT_TRUE(r);
    EXPECT_TRUE(fs::exists(assemblyOutput));
}

TEST_F(CSharpTests, SimpleCompilationAndExecution) {
    fs::path tempDir = fs::temp_directory_path();

    fs::path testFile = tempDir / "TestSimpleCompilation.cs";
    generateTestFile(testFile);
    CLEANUP(fs::remove(testFile));
    std::vector<fs::path> sourceFiles = {
            testFile
    };
    fs::path assemblyOutput = tempDir / "TestSimpleCompilation.dll";
    bool r = engine.compileFiles(assemblyOutput, sourceFiles);
    CLEANUP(fs::remove(assemblyOutput));
    ASSERT_TRUE(r);

    EXPECT_TRUE(fs::exists(assemblyOutput));
    auto contents = Carrot::IO::readFile(assemblyOutput.string());
    auto module = engine.loadAssembly(Carrot::IO::Resource(std::move(contents) /* don't go through VFS */));
    module->dumpTypes();
    module->staticInvoke(""/*no namespace*/, "SimpleClass", "Main", {}/* no arguments */);
}
