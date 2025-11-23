//
// Created by jglrxavpok on 23/11/25.
//

#include <core/containers/HandleStorage.hpp>
#include <gtest/gtest.h>

using namespace Carrot;

static i32 AliveObjects = 0;

struct TestObject {
    TestObject() {
        AliveObjects++;
    }

    ~TestObject() {
        AliveObjects--;
    }

    i32 value = 0;
    HandleDetails::Weak<TestObject> self;

    void setHandle(HandleDetails::Weak<TestObject> h) {
        self = h;
    }

    Handle<TestObject> getHandle() {
        return Carrot::Handle(self);
    }
};

using TestHandle = Handle<TestObject>;

TEST(Handles, CreateSingleHandle) {
    HandleStorage<TestObject> storage;

    // create a handle, and access it
    TestHandle handle = storage.emplace();
    handle->value = 42;
    EXPECT_EQ(handle->value, 42);
}

TEST(Handles, RefCountAfterCreation) {
    HandleStorage<TestObject> storage;

    // create a handle, and access it
    TestHandle handle = storage.emplace();
    HandleDetails::Slot<TestObject>* pSlot = storage.getSlot(handle);
    EXPECT_EQ(pSlot->getRefCount(), 1);

    TestHandle handle2 = handle; // creating a new handle should increase ref count
    EXPECT_EQ(pSlot->getRefCount(), 2);
}

TEST(Handles, RefCountAfterDestruction) {
    HandleStorage<TestObject> storage;

    HandleDetails::Slot<TestObject>* pSlot = nullptr;

    // create a handle, then delete it
    {
        TestHandle handle = storage.emplace();
        pSlot = storage.getSlot(handle);
        EXPECT_EQ(pSlot->getRefCount(), 1);
    } // handle gets destroyed

    EXPECT_EQ(pSlot->getRefCount(), 0);
}

TEST(Handles, RefCountAfterMove) {
    HandleStorage<TestObject> storage;

    // create a handle, then move it
    TestHandle handle = storage.emplace();
    HandleDetails::Slot<TestObject>* pSlot = storage.getSlot(handle);
    EXPECT_EQ(pSlot->getRefCount(), 1);

    TestHandle handleMoved = std::move(handle);
    EXPECT_EQ(pSlot->getRefCount(), 1);
    EXPECT_FALSE(handle); // moved, no longer valid
    EXPECT_TRUE(handleMoved); // moved, no longer valid
}

TEST(Handles, RefCountAfterCopyAndDestruction) {
    HandleStorage<TestObject> storage;

    HandleDetails::Slot<TestObject>* pSlot = nullptr;

    // create a handle, then delete it
    {
        TestHandle handle = storage.emplace();
        pSlot = storage.getSlot(handle);
        EXPECT_EQ(pSlot->getRefCount(), 1);

        TestHandle handle2 = handle;
        EXPECT_EQ(pSlot->getRefCount(), 2);
    } // handles get invalid

    EXPECT_EQ(pSlot->getRefCount(), 0);
}

TEST(Handles, CleanupTest) {
    HandleStorage<TestObject> storage;

    HandleDetails::Slot<TestObject>* pSlot = nullptr;

    // create a handle, then delete it
    {
        TestHandle handle = storage.emplace();
        pSlot = storage.getSlot(handle);
        EXPECT_EQ(pSlot->getRefCount(), 1);
    } // handles get invalid

    EXPECT_EQ(pSlot->getRefCount(), 0);
    EXPECT_TRUE(pSlot->pObject.has_value()); // no ref but not destroyed
    EXPECT_EQ(AliveObjects, 1);
    storage.cleanup();
    EXPECT_FALSE(pSlot->pObject.has_value());
    EXPECT_EQ(AliveObjects, 0);
}