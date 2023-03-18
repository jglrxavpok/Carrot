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
#include <core/scripting/csharp/CSAssembly.h>
#include <core/scripting/csharp/CSClass.h>
#include <core/scripting/csharp/CSMethod.h>
#include <core/scripting/csharp/CSObject.h>
#include <engine/scripting/CSharpBindings.h>

namespace fs = std::filesystem;

#define _START_ENGINE_INTERNAL(APP_NAME)                    \
Carrot::Configuration config;                               \
config.applicationName = APP_NAME;                          \
Carrot::Engine e{ config };

#define START_ENGINE() _START_ENGINE_INTERNAL(__FUNCTION__)

TEST(CSharpECS, EngineBoots) {
    START_ENGINE();
}

TEST(CSharpECS, ManualRegistration) {
    using namespace Carrot::ECS;

    std::string code = R"c#(
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

    // required for VFS
    START_ENGINE();

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
        EXPECT_NEAR(entityA.getComponent<TransformComponent>()->localTransform.position.x, 150.0f, 1.0f);
        EXPECT_NEAR(entityB.getComponent<TransformComponent>()->localTransform.position.x, 50.0f, 1.0f);

        // entity C does not match system signature -> no modifications
        EXPECT_EQ(entityC.getComponent<TransformComponent>()->localTransform.position.x, 0.0f);
    }
}

TEST(CSharpECS, HotReload) {
    using namespace Carrot::ECS;

    // required for VFS
    START_ENGINE();

    const std::string code1 = "public class TestClass { public static int GetTestValue() { return 10; } }";
    const std::string code2 = "public class TestClass { public static int GetTestValue() { return 42; } }";

    // also test that we can reference content from Carrot.dll
    const std::string code3 = "using Carrot; public class TestClass { public LogicSystem s; public static int GetTestValue() { return 100; } }";

    const std::string code4 = "public class OtherClass {}";

    // compile C# code
    fs::path tempDir = fs::temp_directory_path();
    GetVFS().addRoot("test", tempDir);
    fs::path testFile = tempDir / "TestHotReload2.cs";
    fs::path assemblyOutput = tempDir / "TestHotReload2.dll";

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
        GetCSharpBindings().loadGameAssembly("test://TestHotReload2.dll");

        checkResult(10);
    }
    {
        // reload directly
        compile(code2);
        auto contents = Carrot::IO::readFile(assemblyOutput.string());
        GetCSharpBindings().loadGameAssembly("test://TestHotReload2.dll");

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
        GetCSharpBindings().loadGameAssembly("test://TestHotReload2.dll");

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

TEST(CSharpECS, HotReloadLogicSystem) {
    using namespace Carrot::ECS;

    std::string code1 = R"c#(
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

    std::string code2 = R"c#(
using Carrot;
using System;

public class TestSystem : LogicSystem {

    public TestSystem(ulong handle): base(handle) {
        AddComponent<TransformComponent>();
        AddComponent<CameraComponent>();
    }

    public override void Tick(double deltaTime) {
        ForEachEntity<TransformComponent>((entity, transform) => {
                transform.LocalPosition = new Vec3(-42,-42,-42);
        });
    }
}
)c#";

    // required for VFS
    START_ENGINE();

    // compile C# code
    fs::path tempDir = fs::temp_directory_path();

    fs::path testFile = tempDir / "TestHotReloadLogicSystem.cs";

    Carrot::IO::writeFile(testFile.string(), (void*)code1.c_str(), code1.size());

    CLEANUP(fs::remove(testFile));
    std::vector<fs::path> sourceFiles = {
            testFile
    };
    std::vector<fs::path> references = {
            GetVFS().resolve("engine://scripting/Carrot.dll")
    };
    fs::path assemblyOutput = tempDir / "TestHotReloadLogicSystem.dll";

    GetVFS().addRoot("test", tempDir);
    Carrot::IO::VFS::Path gameDLL = "test://TestHotReloadLogicSystem.dll";

    bool r = GetCSharpScripting().compileFiles(assemblyOutput, sourceFiles, references);
    ASSERT_TRUE(r);

    EXPECT_TRUE(fs::exists(assemblyOutput));

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

    auto entityA = w.newEntity("A").addComponent<TransformComponent>().addComponent<CameraComponent>();
    auto entityB = w.newEntity("B").addComponent<TransformComponent>().addComponent<CameraComponent>();
    auto entityC = w.newEntity("C").addComponent<TransformComponent>();

    // dll is not loaded yet: no changes expected
    // check that system executes properly
    {
        entityA.getComponent<TransformComponent>()->localTransform.position = glm::vec3{ 50.0f, 0.0f, 0.0f };
        entityB.getComponent<TransformComponent>()->localTransform.position = glm::vec3{ -50.0f, 0.0f, 0.0f };
        entityC.getComponent<TransformComponent>()->localTransform.position = glm::vec3{ 0.0f, 0.0f, 0.0f };

        w.tick(100.0f);

        // entities should have moved
        EXPECT_NEAR(entityA.getComponent<TransformComponent>()->localTransform.position.x, 50.0f, 1.0f);
        EXPECT_NEAR(entityB.getComponent<TransformComponent>()->localTransform.position.x, -50.0f, 1.0f);

        // entity C does not match system signature -> no modifications
        EXPECT_EQ(entityC.getComponent<TransformComponent>()->localTransform.position.x, 0.0f);
    }

    // load C# code
    GetCSharpBindings().loadGameAssembly(gameDLL);

    // now dll is loaded
    // check that system executes properly
    {
        entityA.getComponent<TransformComponent>()->localTransform.position = glm::vec3{ 50.0f, 0.0f, 0.0f };
        entityB.getComponent<TransformComponent>()->localTransform.position = glm::vec3{ -50.0f, 0.0f, 0.0f };
        entityC.getComponent<TransformComponent>()->localTransform.position = glm::vec3{ 0.0f, 0.0f, 0.0f };

        w.tick(100.0f);

        // entities should have moved
        EXPECT_NEAR(entityA.getComponent<TransformComponent>()->localTransform.position.x, 150.0f, 1.0f);
        EXPECT_NEAR(entityB.getComponent<TransformComponent>()->localTransform.position.x, 50.0f, 1.0f);

        // entity C does not match system signature -> no modifications
        EXPECT_EQ(entityC.getComponent<TransformComponent>()->localTransform.position.x, 0.0f);
    }

    fs::remove(assemblyOutput); // to allow for recompilation
    // compile new code
    {
        Carrot::IO::writeFile(testFile.string(), (void*)code2.c_str(), code2.size());

        CLEANUP(fs::remove(testFile));
        r = GetCSharpScripting().compileFiles(assemblyOutput, sourceFiles, references);
        ASSERT_TRUE(r);
    }

    GetCSharpBindings().reloadGameAssembly();

    // now dll is reloaded
    // check that system executes properly with the new code
    {
        entityA.getComponent<TransformComponent>()->localTransform.position = glm::vec3{ 50.0f, 0.0f, 0.0f };
        entityB.getComponent<TransformComponent>()->localTransform.position = glm::vec3{ -50.0f, 0.0f, 0.0f };
        entityC.getComponent<TransformComponent>()->localTransform.position = glm::vec3{ 0.0f, 0.0f, 0.0f };

        w.tick(100.0f);

        // entities should have moved
        EXPECT_NEAR(entityA.getComponent<TransformComponent>()->localTransform.position.x, -42.0f, 1.0f);
        EXPECT_NEAR(entityB.getComponent<TransformComponent>()->localTransform.position.x, -42.0f, 1.0f);

        // entity C does not match system signature -> no modifications
        EXPECT_EQ(entityC.getComponent<TransformComponent>()->localTransform.position.x, 0.0f);
    }
}