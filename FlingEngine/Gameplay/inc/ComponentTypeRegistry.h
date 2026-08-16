#pragma once

#include "JsonArchive.h"

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
		void (*save)(const entt::registry&, entt::entity, Json& out) = nullptr;
		void (*load)(entt::registry&, entt::entity, const Json& in) = nullptr;
		bool (*has)(const entt::registry&, entt::entity) = nullptr;
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
		bool Register(const char* name);

		/** Store an external registrar to run from World::Init. Asserts/logs if sealed. */
		void AddRegistrar(IComponentRegistrar* registrar);

		/** Invoke every registrar added via AddRegistrar. */
		void RunExternalRegistrars();

		/** Latch: further Register / AddRegistrar calls are rejected. */
		void Seal();
		bool IsSealed() const { return m_Sealed; }

		const ComponentTypeInfo* Find(const std::string& name) const;
		const std::vector<ComponentTypeInfo>& All() const { return m_Types; }

		/**
		 * Test-only: clear types, registrars, and the seal latch so Catch2 cases
		 * can share the process-global singleton.
		 */
		void ResetForTests();

	private:
		ComponentTypeRegistry() = default;
		ComponentTypeRegistry(const ComponentTypeRegistry&) = delete;
		ComponentTypeRegistry& operator=(const ComponentTypeRegistry&) = delete;

		bool CanRegister(const char* name);
		void AddType(const char* name, ComponentTypeInfo info);

		std::vector<ComponentTypeInfo> m_Types;
		std::vector<std::string> m_NameStorage;
		std::unordered_map<std::string, std::size_t> m_NameToIndex;
		std::vector<IComponentRegistrar*> m_Registrars;
		bool m_Sealed = false;
	};

	/** Registers Transform and NameComponent. Called from Engine::Startup. */
	void RegisterGameplayComponents();

	template<typename T>
	bool ComponentTypeRegistry::Register(const char* name)
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

		AddType(name, std::move(info));
		return true;
	}
}	// namespace Fling
