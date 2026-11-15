// The other half of "A system id does not depend on class or struct inside template
// arguments" in EcsScheduleTests.cpp. It needs two files, because MSVC names a template
// argument with the class key of the declaration that each file sees.

#include "Ecs/Order.h"
#include "TypeName.h"

namespace SystemTypeIdTest
{
    template <typename T>
    class Box;

    // EcsScheduleTests.cpp defines it as a class.
    struct Item;

    Fling::TypeHash BoxOfItemIdInOtherFile()
    {
        return Fling::SystemTypeId<Box<Item>>();
    }

    Fling::StringView BoxOfItemNameInOtherFile()
    {
        return Fling::TypeNameOf<Box<Item>>();
    }
} // namespace SystemTypeIdTest
