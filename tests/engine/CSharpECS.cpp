//
// Created by jglrxavpok on 11/03/2023.
//

#include <string>
#include <filesystem>
#include <gtest/gtest.h>
#include <engine/ecs/systems/System.h>
#include <engine/ecs/components/Component.h>
#include "engine/ecs/systems/CSharpLogicSystem.h"
#include "engine/Engine.h"
#include "engine/ecs/components/TransformComponent.h"
#include "engine/ecs/components/CameraComponent.h"
#include <core/io/vfs/VirtualFileSystem.h>
#include <core/io/IO.h>
#include <core/scripting/csharp/Engine.h>
#include <core/scripting/csharp/CSClass.h>
#include <core/scripting/csharp/CSMethod.h>
#include <core/scripting/csharp/CSObject.h>
#include <engine/scripting/CSharpBindings.h>

namespace fs = std::filesystem;

static std::string code = R"c#(
using Carrot;
using System;

public class TestSystem : LogicSystem {

    public TestSystem(ulong handle): base(handle) {
        AddComponent<TransformComponent>();
        AddComponent<CameraComponent>();
    }

    public override void Tick(double deltaTime) {
        ForEachEntity<TransformComponent>((entity, transform) => {
                Console.WriteLine("-- Entity: "+entity.GetName());
                var transformLocalPosition = transform.LocalPosition;
                Console.WriteLine("X = " + transformLocalPosition.X);
                transformLocalPosition.X += (float)deltaTime;
                Console.WriteLine("X' = " + transformLocalPosition.X);
                transform.LocalPosition = transformLocalPosition;
        });
    }
}
)c#";

TEST(CSharpECS, EngineBoots) {
    Carrot::Configuration config;
    Carrot::Engine e{ config };
}

TEST(CSharpECS, ManualRegistration) {
    using namespace Carrot::ECS;

    // required for VFS
    Carrot::Configuration config;
    Carrot::Engine e{ config };

    // compile C# code
    fs::path tempDir = fs::temp_directory_path();

    fs::path testFile = tempDir / "TestManualRegistration.cs";

    Carrot::IO::writeFile(testFile.string(), (void*)code.c_str(), code.size());

    CLEANUP(fs::remove(testFile));
    std::vector<fs::path> sourceFiles = {
            testFile
    };
    std::vector<fs::path> references = {
            GetVFS().resolve("engine://scripting/Carrot.dll")
    };
    fs::path assemblyOutput = tempDir / "TestManualRegistration.dll";
    bool r = GetCSharpScripting().compileFiles(assemblyOutput, sourceFiles, references);
    CLEANUP(fs::remove(assemblyOutput));
    ASSERT_TRUE(r);

    EXPECT_TRUE(fs::exists(assemblyOutput));

    // load C# code
    auto contents = Carrot::IO::readFile(assemblyOutput.string());
    auto module = GetCSharpScripting().loadAssembly(Carrot::IO::Resource(std::move(contents) /* don't go through VFS */));

    auto& systems = getSystemLibrary();

    // register TestSystem
    const std::string id = "C#/.TestSystem";
    systems.add(id,
                [&](const rapidjson::Value& json, World& world) {
                    return std::make_unique<CSharpLogicSystem>(json, world);
                },
                [&](Carrot::ECS::World& world) {
                    return std::make_unique<CSharpLogicSystem>(world, "", "TestSystem");
                }
    );

    EXPECT_TRUE(systems.has(id));

    World w;
    auto ptr = systems.create(id, w);
    ASSERT_NE(ptr, nullptr);

    // TODO: check that system has correct stuff

    w.addLogicSystem(std::move(ptr));

    // check that system executes properly
    {
        auto entityA = w.newEntity("A").addComponent<TransformComponent>().addComponent<CameraComponent>();
        auto entityB = w.newEntity("B").addComponent<TransformComponent>().addComponent<CameraComponent>();
        auto entityC = w.newEntity("C").addComponent<TransformComponent>();

        entityA.getComponent<TransformComponent>()->localTransform.position = glm::vec3{ 50.0f, 0.0f, 0.0f };
        entityB.getComponent<TransformComponent>()->localTransform.position = glm::vec3{ -50.0f, 0.0f, 0.0f };
        entityC.getComponent<TransformComponent>()->localTransform.position = glm::vec3{ 0.0f, 0.0f, 0.0f };

        w.tick(100.0f);

        // entities should have moved
        EXPECT_GE(entityA.getComponent<TransformComponent>()->localTransform.position.x, 149.0f);
        EXPECT_GE(entityB.getComponent<TransformComponent>()->localTransform.position.x, 49.0f);

        // entity C does not match system signature -> no modifications
        EXPECT_EQ(entityC.getComponent<TransformComponent>()->localTransform.position.x, 0.0f);
    }
}

TEST(CSharpECS, HotReload) {
    using namespace Carrot::ECS;

    // required for VFS
    Carrot::Configuration config;
    Carrot::Engine e{ config };

    const std::string code1 = "public class TestClass { public static int GetTestValue() { return 10; } }";
    const std::string code2 = "public class TestClass { public static int GetTestValue() { return 42; } }";
    const std::string code3 = "public class TestClass { public static int GetTestValue() { return 100; } }";
    const std::string code4 = "public class OtherClass {}";

    // compile C# code
    fs::path tempDir = fs::temp_directory_path();
    GetVFS().addRoot("test", tempDir);
    fs::path testFile = tempDir / "TestHotReload.cs";
    fs::path assemblyOutput = tempDir / "TestHotReload.dll";

    CLEANUP(fs::remove(assemblyOutput));

    auto compile = [&](const std::string& code) {

        Carrot::IO::writeFile(testFile.string(), (void*)code.c_str(), code.size());

        CLEANUP(fs::remove(testFile));
        std::vector<fs::path> sourceFiles = {
                testFile
        };
        std::vector<fs::path> references = {
                GetVFS().resolve("engine://scripting/Carrot.dll")
        };
        bool r = GetCSharpScripting().compileFiles(assemblyOutput, sourceFiles, references);
        ASSERT_TRUE(r);

        EXPECT_TRUE(fs::exists(assemblyOutput));
    };

    // load C# code
    auto checkResult = [&](int expectedValue) {
        auto* clazz = GetCSharpScripting().findClass("", "TestClass");
        auto* method = clazz->findMethod("GetTestValue");
        int result = *((int*)mono_object_unbox(method->staticInvoke({})));
        EXPECT_EQ(result, expectedValue);
    };

    {
        compile(code1);
        auto contents = Carrot::IO::readFile(assemblyOutput.string());
        GetCSharpBindings().loadGameAssembly("test://TestHotReload.dll");

        checkResult(10);
    }
    {
        // reload directly
        compile(code2);
        auto contents = Carrot::IO::readFile(assemblyOutput.string());
        GetCSharpBindings().loadGameAssembly("test://TestHotReload.dll");

        checkResult(42);
    }
    {
        // reload via reloadGameAssembly
        compile(code3);
        GetCSharpBindings().reloadGameAssembly();

        checkResult(100);
    }
    {
        // the class does not exist anymore
        compile(code4);
        GetCSharpBindings().reloadGameAssembly();

        auto* clazz = GetCSharpScripting().findClass("", "TestClass");
        EXPECT_EQ(clazz, nullptr);
    }
    {
        // class exists again
        compile(code1);
        GetCSharpBindings().loadGameAssembly("test://TestHotReload.dll");

        checkResult(10);
    }
    {
        // no more assembly
        GetCSharpBindings().unloadGameAssembly();

        // class does not exist anymore (again)
        auto* clazz = GetCSharpScripting().findClass("", "TestClass");
        EXPECT_EQ(clazz, nullptr);
    }
}