#pragma once

#include "Ecs/Component.h"
#include "JsonArchive.h"

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

		/** The stable id from the ComponentInfo. 0 for a type registered by name only. */
		TypeHash hash = 0;

		/** Table, Tag, Singleton, or Queue. A type registered by name only is a Table. */
		ComponentKind kind = ComponentKind::Table;

		/** True if the type is never saved to a level (save, load, and the singleton functions are null). */
		bool transient = false;

		/** True for Singleton and Queue types. They live in the registry context, not on entities. */
		bool IsSingleton() const { return kind == ComponentKind::Singleton || kind == ComponentKind::Queue; }

		/** Singleton types only: true if the world has the singleton. */
		bool (*hasSingleton)(const entt::registry&) = nullptr;

		/** Singleton types that are saved: write the singleton to a level's "singletons" object. */
		void (*saveSingleton)(const entt::registry&, Json& out) = nullptr;

		/** Singleton types that are saved: make or replace the singleton from a level. */
		void (*loadSingleton)(entt::registry&, const Json& in) = nullptr;

		// Table and Tag types only. They are null for singletons.

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
		 * Register a Fling::Component. The name, kind, and transient flag come from
		 * T::Component, so the saved name is written once. A type that is saved
		 * (not Transient) implements void Serialize(JsonArchive&). A Tag needs no Serialize.
		 * @return false if the registry is sealed or the name is already registered
		 */
		template<typename T>
		bool Register();

		/**
		 * Register a component type that implements void Serialize(JsonArchive&), with
		 * the name it has in level files. For types that do not declare a ComponentInfo
		 * yet. Prefer Register<T>() for new types.
		 * @return false if the registry is sealed or the name is already registered
		 */
		template<typename T>
		bool Register(const char* name);

		/** Store an external registrar to run from World::Init. Asserts/logs if sealed. */
		void AddRegistrar(IComponentRegistrar* registrar);

		/**
		 * Attach an ImGui draw callback to an already-registered component type, so
		 * editor tooling can render it without the registry knowing about ImGui
		 * itself. No-op (with a warning) if the name hasn't been registered.
		 */
		void SetEditorWidget(const char* name, void(*fn)(entt::registry&, entt::entity));

		/** Invoke every registrar added via AddRegistrar. */
		void RunExternalRegistrars();

		/** Latch: further Register / AddRegistrar calls are rejected. */
		void Seal();
		bool IsSealed() const { return m_Sealed; }

		const ComponentTypeInfo* Find(const std::string& name) const;
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

		bool CanRegister(const char* name);
		void AddType(const char* name, ComponentTypeInfo info);

		/** has, create, and destroy for a type that lives on entities. */
		template<typename T>
		static void AddEntityFunctions(ComponentTypeInfo& info);

		/** save and load through T::Serialize, for a type that lives on entities. */
		template<typename T>
		static void AddEntitySerialize(ComponentTypeInfo& info);

		std::vector<ComponentTypeInfo> m_Types;

		// A deque (not vector) so that growing it never relocates existing
		// elements, which would dangle the const char* pointers ComponentTypeInfo
		// entries hold into these strings via .c_str().
		std::deque<std::string> m_NameStorage;

		std::unordered_map<std::string, std::size_t> m_NameToIndex;
		std::vector<IComponentRegistrar*> m_Registrars;
		bool m_Sealed = false;
	};

	/** Registers Transform and NameComponent. Called from Engine::Startup. */
	void RegisterEngineComponents();

	template<typename T>
	void ComponentTypeRegistry::AddEntityFunctions(ComponentTypeInfo& info)
	{
		info.has = [](const entt::registry& reg, entt::entity e) -> bool
		{
			return reg.has<T>(e);
		};
		info.create = [](entt::registry& reg, entt::entity e)
		{
			reg.assign<T>(e);
		};
		info.destroy = [](entt::registry& reg, entt::entity e)
		{
			reg.remove<T>(e);
		};
	}

	template<typename T>
	void ComponentTypeRegistry::AddEntitySerialize(ComponentTypeInfo& info)
	{
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
	}

	template<typename T>
	bool ComponentTypeRegistry::Register()
	{
		static_assert(Component<T>, "Register<T>() needs a Fling::Component. Use Register<T>(name) for other types.");
		static_assert(IsWellFormedComponent<T>, "A Tag must be an empty struct, and only a Tag may be empty.");

		constexpr ComponentInfo declared = ComponentInfoOf<T>;
		constexpr bool saved = !declared.Transient;
		static_assert(!saved || declared.Kind == ComponentKind::Tag || HasSerialize<T>::value,
					  "A component that is saved implements void Serialize(JsonArchive&). Set Transient = true if it "
					  "is never saved.");

		if (!CanRegister(declared.Name))
		{
			return false;
		}

		ComponentTypeInfo info;
		info.hash = TypeHashOf<T>;
		info.kind = declared.Kind;
		info.transient = declared.Transient;

		if constexpr (IsSingletonComponent<T>)
		{
			info.hasSingleton = [](const entt::registry& reg) -> bool
			{
				return reg.try_ctx<T>() != nullptr;
			};
			if constexpr (saved)
			{
				info.saveSingleton = [](const entt::registry& reg, Json& out)
				{
					T comp = reg.ctx<T>();
					out = Json::Object();
					JsonArchive ar(out, JsonArchive::Mode::Saving);
					comp.Serialize(ar);
				};
				info.loadSingleton = [](entt::registry& reg, const Json& in)
				{
					T comp{};
					Json inCopy = in;
					JsonArchive ar(inCopy, JsonArchive::Mode::Loading);
					comp.Serialize(ar);
					reg.set<T>(std::move(comp));
				};
			}
		}
		else
		{
			AddEntityFunctions<T>(info);
			if constexpr (saved && declared.Kind == ComponentKind::Tag)
			{
				info.save = [](const entt::registry&, entt::entity, Json& out)
				{
					out = Json::Object();
				};
				info.load = [](entt::registry& reg, entt::entity e, const Json&)
				{
					if (!reg.has<T>(e))
					{
						reg.assign<T>(e);
					}
				};
			}
			else if constexpr (saved)
			{
				AddEntitySerialize<T>(info);
			}
		}

		AddType(declared.Name, std::move(info));
		return true;
	}

	template<typename T>
	bool ComponentTypeRegistry::Register(const char* name)
	{
		static_assert(HasSerialize<T>::value, "Component type must implement void Serialize(JsonArchive&)");

		if (!CanRegister(name))
		{
			return false;
		}

		ComponentTypeInfo info;
		if constexpr (Component<T>)
		{
			info.hash = TypeHashOf<T>;
		}
		AddEntityFunctions<T>(info);
		AddEntitySerialize<T>(info);

		AddType(name, std::move(info));
		return true;
	}
}	// namespace Fling
