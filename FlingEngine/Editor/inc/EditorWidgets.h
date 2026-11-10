#pragma once

#include "Ecs/Component.h"
#include "Ecs/EntityStorage.h"
#include "Ecs/Reflection.h"

#include <functional>
#include <unordered_map>
#include <utility>

namespace Fling
{
    /**
     * How the editor draws a component. By default the inspector is made from the
     * type's reflected fields (DrawReflected). A type that needs more (a file picker,
     * a button) gets an override.
     */
    class EditorWidgets
    {
    public:
        using DrawFn = std::function<void(EntityStorage&, Entity)>;

        /** Draw T with fn instead of the generated inspector. */
        template <::Fling::Component T>
        void Override(std::function<void(EntityStorage&, Entity, T&)> fn)
        {
            m_Overrides[TypeHashOf<T>] = [fn = std::move(fn)](EntityStorage& storage, const Entity entity)
            {
                if (T* object = storage.TryGet<T>(entity))
                {
                    fn(storage, entity, *object);
                }
            };
        }

        /** The override for a type, or null. */
        const DrawFn* Find(TypeHash hash) const;

        /**
         * Draw the component of a type on an entity: the override if there is one,
         * and the generated inspector otherwise. Runs the type's AfterLoad after a change.
         */
        void Draw(const TypeDesc& type, EntityStorage& storage, Entity entity) const;

        /**
         * Draw one widget per reflected field. Hidden fields are skipped. Transient and
         * read-only fields cannot be changed, and neither can anything when readOnly is
         * true. Returns true if a field changed.
         */
        static bool DrawReflected(const TypeDesc& type, void* object, bool readOnly);

    private:
        std::unordered_map<TypeHash, DrawFn> m_Overrides;
    };
} // namespace Fling
