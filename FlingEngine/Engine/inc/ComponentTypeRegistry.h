#pragma once

#include "JsonArchive.h"
#include "Strings/Name.h"
#include "Strings/String.h"

#include <deque>
#include <entt/entity/registry.hpp>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Fling
{
	struct ComponentTypeInfo
	{
		const char* name = nullptr;

		/** The registered name as a Name, for fast compares. */
		Name TypeName;
		void (*save)(const entt::registry&, entt::entity, Json& out) = nullptr;
		void (*load)(entt::registry&, entt::entity, const Json& in) = nullptr;
		bool (*has)(const entt::registry&, entt::entity) = nullptr;

		/** Default-constructs and assigns this component type to an entity. */
		void (*create)(entt::registry&, entt::entity) = nullptr;

		/** Removes this component type from an entity. */
		void (*destroy)(entt::registry&, entt::entity) = nullptr;

		/**
		 * Optional ImGui draw callback for editor tooling. Left null for types
		 * that don't register one via ComponentTypeRegistry::SetEditorWidget.
		 */
		void (*drawEditorWidget)(entt::registry&, entt::entity) = nullptr;
	};

	/**
	 * Optional hook for game/plugin modules that want to register types without
	 * subclassing Fling::Game. Pointers are not owned.
	 */
	class IComponentRegistrar
	{
	public:
		virtual ~IComponentRegistrar() = default;
		virtual void RegisterComponents() = 0;
	};

	/**
	 * Runtime map of serializable component types. Types are registered explicitly
	 * (no static auto-registration), then sealed before the first level load.
	 */
	class ComponentTypeRegistry
	{
	public:
		static ComponentTypeRegistry& Get();

		/**
		 * Register a component type that implements void Serialize(JsonArchive&).
		 * @return false if the registry is sealed or the name is already registered
		 */
		template<typename T>
		bool Register(StringView name);

		/** Store an external registrar to run from World::Init. Asserts/logs if sealed. */
		void AddRegistrar(IComponentRegistrar* registrar);

		/**
		 * Attach an ImGui draw callback to an already-registered component type, so
		 * editor tooling can render it without the registry knowing about ImGui
		 * itself. No-op (with a warning) if the name hasn't been registered.
		 */
		void SetEditorWidget(StringView name, void(*fn)(entt::registry&, entt::entity));

		/** Invoke every registrar added via AddRegistrar. */
		void RunExternalRegistrars();

		/** Latch: further Register / AddRegistrar calls are rejected. */
		void Seal();
		bool IsSealed() const { return m_Sealed; }

		const ComponentTypeInfo* Find(StringView name) const;
		const std::vector<ComponentTypeInfo>& All() const { return m_Types; }

#ifdef FLING_WITH_TEST_HOOKS
		/**
		 * Test-only: clear types, registrars, and the seal latch so Catch2 cases
		 * can share the process-global singleton.
		 */
		void ResetForTests();
#endif

	private:
		ComponentTypeRegistry() = default;
		ComponentTypeRegistry(const ComponentTypeRegistry&) = delete;
		ComponentTypeRegistry& operator=(const ComponentTypeRegistry&) = delete;

		bool CanRegister(StringView name);
		void AddType(StringView name, ComponentTypeInfo info);

		std::vector<ComponentTypeInfo> m_Types;

		// A deque (not vector) so that growing it never relocates existing
		// elements, which would dangle the const char* pointers ComponentTypeInfo
		// entries hold into these strings via .c_str().
		std::deque<String> m_NameStorage;

		std::unordered_map<Name, std::size_t> m_NameToIndex;
		std::vector<IComponentRegistrar*> m_Registrars;
		bool m_Sealed = false;
	};

	/** Registers Transform and NameComponent. Called from Engine::Startup. */
	void RegisterEngineComponents();

	template<typename T>
	bool ComponentTypeRegistry::Register(const StringView name)
	{
		static_assert(HasSerialize<T>::value, "Component type must implement void Serialize(JsonArchive&)");

		if (!CanRegister(name))
		{
			return false;
		}

		ComponentTypeInfo info;
		info.has = [](const entt::registry& reg, entt::entity e) -> bool
		{
			return reg.has<T>(e);
		};
		info.save = [](const entt::registry& reg, entt::entity e, Json& out)
		{
			T comp = reg.get<T>(e);
			out = Json::Object();
			JsonArchive ar(out, JsonArchive::Mode::Saving);
			comp.Serialize(ar);
		};
		info.load = [](entt::registry& reg, entt::entity e, const Json& in)
		{
			T comp{};
			Json inCopy = in;
			JsonArchive ar(inCopy, JsonArchive::Mode::Loading);
			comp.Serialize(ar);

			if (reg.has<T>(e))
			{
				reg.replace<T>(e, std::move(comp));
			}
			else
			{
				reg.assign<T>(e, std::move(comp));
			}
		};
		info.create = [](entt::registry& reg, entt::entity e)
		{
			reg.assign<T>(e);
		};
		info.destroy = [](entt::registry& reg, entt::entity e)
		{
			reg.remove<T>(e);
		};

		AddType(name, std::move(info));
		return true;
	}
}	// namespace Fling
