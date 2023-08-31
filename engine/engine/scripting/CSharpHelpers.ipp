//
// Created by jglrxavpok on 31/08/2023.
//

template<typename T>
T& getObject(MonoObject* obj) {
    verify(mono_object_isinst(obj, GetCSharpBindings().CSharpBindings::CarrotObjectClass->toMono()), "Input object is not a Carrot.Object instance!");

    Carrot::Scripting::CSObject handleObj = GetCSharpBindings().CarrotObjectHandleField->get(Carrot::Scripting::CSObject(obj));
    std::uint64_t handle = *((std::uint64_t*)mono_object_unbox(handleObj));
    auto* ptr = reinterpret_cast<T*>(handle);
    return *ptr;
}

template<typename T>
T& getReference(MonoObject* obj) {
    verify(mono_object_isinst(obj, GetCSharpBindings().CSharpBindings::CarrotReferenceClass->toMono()), "Input object is not a Carrot.Reference instance!");

    Carrot::Scripting::CSObject handleObj = GetCSharpBindings().CarrotReferenceHandleField->get(Carrot::Scripting::CSObject(obj));
    std::uint64_t handle = *((std::uint64_t*)mono_object_unbox(handleObj));
    auto* ptr = reinterpret_cast<T*>(handle);
    return *ptr;
}