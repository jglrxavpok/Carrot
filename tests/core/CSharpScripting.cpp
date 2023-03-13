//
// Created by jglrxavpok on 07/03/2023.
//

#include <gtest/gtest.h>
#include <unordered_set>
#include <core/scripting/csharp/Engine.h>
#include <core/scripting/csharp/CSAssembly.h>
#include <core/scripting/csharp/CSObject.h>
#include <core/io/IO.h>
#include "mono/metadata/class.h"
#include "core/utils/stringmanip.h"
#include "mono/metadata/object.h"

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

    public int publicField = 10;
    private int _privateField = 42;

    public SimpleClass() {
        Console.WriteLine("Default constructor");
    }

    public SimpleClass(float v) {
        Console.WriteLine("Constructor with 1 argument: "+v);
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
    bool r = engine.compileFiles(assemblyOutput, sourceFiles, {});
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
    bool r = engine.compileFiles(assemblyOutput, sourceFiles, {});
    CLEANUP(fs::remove(assemblyOutput));
    ASSERT_TRUE(r);

    EXPECT_TRUE(fs::exists(assemblyOutput));
    auto contents = Carrot::IO::readFile(assemblyOutput.string());
    auto module = engine.loadAssembly(Carrot::IO::Resource(std::move(contents) /* don't go through VFS */));
    module->dumpTypes();

    auto clazz = module->findClass("", "SimpleClass");
    ASSERT_NE(clazz, nullptr);

    auto method = clazz->findMethod("Main", 0);
    ASSERT_NE(method, nullptr);
    method->staticInvoke({});
}

// TODO: GC handles test

TEST_F(CSharpTests, ObjectInstantiation) {
    fs::path tempDir = fs::temp_directory_path();

    fs::path testFile = tempDir / "TestSimpleCompilation.cs";
    generateTestFile(testFile);
    CLEANUP(fs::remove(testFile));
    std::vector<fs::path> sourceFiles = {
            testFile
    };
    fs::path assemblyOutput = tempDir / "TestSimpleCompilation.dll";
    bool r = engine.compileFiles(assemblyOutput, sourceFiles, {});
    CLEANUP(fs::remove(assemblyOutput));
    ASSERT_TRUE(r);

    EXPECT_TRUE(fs::exists(assemblyOutput));
    auto contents = Carrot::IO::readFile(assemblyOutput.string());
    auto module = engine.loadAssembly(Carrot::IO::Resource(std::move(contents) /* don't go through VFS */));
    module->dumpTypes();

    auto clazz = module->findClass("", "SimpleClass");
    ASSERT_NE(clazz, nullptr);

    clazz->newObject({});

    float f = 42.0f;
    void* args1[1] = {
            &f,
    };
    void* args2[2] = {
            &f,
            &f,
    };
    clazz->newObject(args1);

    EXPECT_THROW(clazz->newObject(args2), Carrot::Assertions::Error);
}

TEST_F(CSharpTests, Fields) {
    fs::path tempDir = fs::temp_directory_path();

    fs::path testFile = tempDir / "TestSimpleCompilation.cs";
    generateTestFile(testFile);
    CLEANUP(fs::remove(testFile));
    std::vector<fs::path> sourceFiles = {
            testFile
    };
    fs::path assemblyOutput = tempDir / "TestSimpleCompilation.dll";
    bool r = engine.compileFiles(assemblyOutput, sourceFiles, {});
    CLEANUP(fs::remove(assemblyOutput));
    ASSERT_TRUE(r);

    EXPECT_TRUE(fs::exists(assemblyOutput));
    auto contents = Carrot::IO::readFile(assemblyOutput.string());
    auto module = engine.loadAssembly(Carrot::IO::Resource(std::move(contents) /* don't go through VFS */));
    module->dumpTypes();

    auto clazz = module->findClass("", "SimpleClass");
    ASSERT_NE(clazz, nullptr);

    auto instance = clazz->newObject({});

    auto publicField = clazz->findField("publicField");
    ASSERT_NE(publicField, nullptr);

    auto publicValueObj = publicField->get(*instance);
    int publicValue = *((int*)mono_object_unbox(publicValueObj));
    EXPECT_EQ(publicValue, 10);

    auto privateField = clazz->findField("_privateField");
    ASSERT_NE(privateField, nullptr);

    auto privateValueObj = privateField->get(*instance);
    int privateValue = *((int*)mono_object_unbox(privateValueObj));
    EXPECT_EQ(privateValue, 42);
}

TEST_F(CSharpTests, FindClassMultipleAssemblies) {
    fs::path tempDir = fs::temp_directory_path();

    auto makeSimpleModule = [&](const std::string& className) -> std::shared_ptr<CSAssembly> {
        const std::string csFilename = className + ".cs";
        const fs::path testFile = tempDir / csFilename;

        std::string code = Carrot::sprintf("public class %s {}", className.c_str());
        Carrot::IO::writeFile(testFile.string(), (void*)code.c_str(), code.size());

        CLEANUP(fs::remove(testFile));
        std::vector<fs::path> sourceFiles = {
                testFile
        };

        const std::string dllName = className + ".dll";
        const fs::path assemblyOutput = tempDir / dllName;
        bool r = engine.compileFiles(assemblyOutput, sourceFiles, {});
        CLEANUP(fs::remove(assemblyOutput));
        EXPECT_TRUE(r);
        if(!r) {
            return nullptr;
        }

        EXPECT_TRUE(fs::exists(assemblyOutput));
        auto contents = Carrot::IO::readFile(assemblyOutput.string());
        auto m = engine.loadAssembly(Carrot::IO::Resource(std::move(contents) /* don't go through VFS */));
        m->dumpTypes();
        return m;
    };

    auto module1 = makeSimpleModule("Test1");
    ASSERT_NE(module1, nullptr);

    auto module2 = makeSimpleModule("Test2");
    ASSERT_NE(module2, nullptr);

    // Only Test1 exists in module1
    EXPECT_NE(module1->findClass("", "Test1"), nullptr);
    EXPECT_EQ(module1->findClass("", "Test2"), nullptr);

    // Only Test2 exists in module2
    EXPECT_EQ(module2->findClass("", "Test1"), nullptr);
    EXPECT_NE(module2->findClass("", "Test2"), nullptr);

    // Going through ScriptingEngine exposes all available classes
    EXPECT_NE(engine.findClass("", "Test1"), nullptr);
    EXPECT_NE(engine.findClass("", "Test2"), nullptr);
}

TEST_F(CSharpTests, FindSubclasses) {
    fs::path tempDir = fs::temp_directory_path();

    fs::path testFile = tempDir / "TestFindSubclasses.cs";

    std::string code = R"c#(
using System;

public class SomeOtherClass {}

public class BaseClass {}

public class SubClass1: BaseClass {}

namespace InsideNamespace {
    public class SubClass2: BaseClass {}
}
)c#";
    Carrot::IO::writeFile(testFile.string(), (void*)code.c_str(), code.size());

    CLEANUP(fs::remove(testFile));
    std::vector<fs::path> sourceFiles = {
            testFile
    };
    fs::path assemblyOutput = tempDir / "TestFindSubclasses.dll";
    bool r = engine.compileFiles(assemblyOutput, sourceFiles, {});
    CLEANUP(fs::remove(assemblyOutput));
    ASSERT_TRUE(r);

    EXPECT_TRUE(fs::exists(assemblyOutput));
    auto contents = Carrot::IO::readFile(assemblyOutput.string());
    auto module = engine.loadAssembly(Carrot::IO::Resource(std::move(contents) /* don't go through VFS */));

    CSClass* subclass = module->findClass("", "BaseClass");
    ASSERT_NE(subclass, nullptr);

    std::vector<CSClass*> subclasses = module->findSubclasses(*subclass);
    ASSERT_GE(subclasses.size(), 2);
    EXPECT_EQ(subclasses.size(), 2);

    std::unordered_set<std::string> subclassesFullNames;
    subclassesFullNames.reserve(subclasses.size());
    for(auto* clazz : subclasses) {
        ASSERT_NE(clazz, nullptr);
        std::string fullName = clazz->getNamespaceName();
        fullName += '.';
        fullName += clazz->getName();
        subclassesFullNames.emplace(std::move(fullName));
    }

    EXPECT_TRUE(subclassesFullNames.contains(".SubClass1"));
    EXPECT_TRUE(subclassesFullNames.contains("InsideNamespace.SubClass2"));
    EXPECT_FALSE(subclassesFullNames.contains(".SomeOtherClass"));
    EXPECT_FALSE(subclassesFullNames.contains(".BaseClass"));
}
