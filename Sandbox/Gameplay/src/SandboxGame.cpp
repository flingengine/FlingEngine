#include "BallControl.h"
#include "Components/Name.hpp"
#include "Components/Transform.h"
#include "Lighting/DirectionalLight.hpp"
#include "Lighting/PointLight.hpp"
#include "MeshRenderer.h"
#include "PhysicsComponents.h"
#include "Random.h"
#include "SandboxGame.h"
#include "Stats.h"
#include "Strings/String.h"
#include "VulkanApp.h"
#include "World.h"
// For getting some lighting info
#include "Lighting/DeferredLightSettings.h"
#include "Mover.h"

// Test command line args
#include "FlingWindow.h"
#include "Input/Input.h"
#include "Logger.h"
#include "Misc/CommandLine.h"

namespace Sandbox
{
    using namespace Fling;

    void Game::Init(Fling::EntityStorage& t_Reg)
    {
        // Lets create an entity! 
        F_LOG_TRACE("Sandbox Init!");

		{
			using namespace Fling;
			const CommandLine& CmdLineOpts = CommandLine::Get();
			const bool bMyOption = CmdLineOpts.GetValueAs<bool>("BenTest", false);
			F_LOG_TRACE("Sandbox:Ben Test : {}\t", bMyOption ? "true" : "false");

			const double DoubleOpt = CmdLineOpts.GetValueAs<double>("TestDouble", -1.0);
			F_LOG_TRACE("Sandbox:Ben Test : {}\t", DoubleOpt);
		}

		F_LOG_TRACE("Done cmd line testing");
    }

	void Game::OnStartGame(Fling::EntityStorage& t_Reg)
	{
		F_LOG_TRACE("Sandbox OnStartGame!");

        // A level with physics bodies (for example Levels/PhysicsTest.json) is a
        // scene of its own. Only an empty level gets the lighting test grid.
        if (t_Reg.View<RigidBody>().Empty())
        {
            LightingTest(t_Reg);
        }
	}

	void Game::OnStopGame(Fling::EntityStorage& t_Reg)
	{
		F_LOG_TRACE("Sandbox OnStopGame!");

        // The world removes the entities that play made. The Sandbox systems reset their
        // singletons in OnPlayBegin. Play can hide the cursor (M): show it again for the editor.
        FlingWindow* CurrentWindow = VulkanApp::Get().GetCurrentWindow();
        if (CurrentWindow && !CurrentWindow->GetMouseVisible())
        {
            CurrentWindow->SetMouseVisible(true);
        }
    }

    void Game::Shutdown(Fling::EntityStorage& t_Reg)
	{
		F_LOG_TRACE("Sandbox Shutdown!");

		// A checksum of every transform, to check that a change to gameplay code gives the same motion
		double Checksum = 0.0;
		std::size_t Count = 0;
		t_Reg.View<Transform>().Each([&](auto, Transform& t_Trans)
		{
			const glm::vec3 Pos = t_Trans.Position;
			const glm::quat Rot = t_Trans.Rotation;
			Checksum += Pos.x + 3.0 * Pos.y + 7.0 * Pos.z + 11.0 * Rot.x + 13.0 * Rot.y + 17.0 * Rot.z + 19.0 * Rot.w;
			++Count;
		});
		F_LOG_TRACE("Sandbox: {} transforms, checksum {:.9f}", Count, Checksum);
	}

	void Game::AddFloor(Fling::EntityStorage& t_Reg, const Fling::StringView t_Model, const Fling::StringView t_Mat, const glm::vec3 t_Scale /* = glm::vec3(1.0f) */)
	{
		Fling::Entity e0 = t_Reg.Create();
		t_Reg.Add<MeshRenderer>(e0, MeshRenderer(Fling::String(t_Model), Fling::String(t_Mat)));
		Transform& t0 = t_Reg.Add<Transform>(e0);
		t0.Position = glm::vec3(0.0f, -1.0f, 0.0f);
		t0.Scale = t_Scale;
	}

    void Game::LightingTest(Fling::EntityStorage& t_Reg)
    {
        
        auto AddRandomPointLight = [&]()
        {
            Fling::Entity e0 = t_Reg.Create();

            glm::vec3 min = { -15.0f, 0.2f, -15.0f };
			glm::vec3 max = { 15.0f, 1.0f, 15.0f };
			const glm::vec3 startPos = Fling::Random::GetRandomVec3(min, max);

            // Each Add moves the entity to a new archetype, so build each component
            // whole and add it, instead of keeping a reference across adds
            Transform t0;
            t0.Position = startPos;
            t_Reg.AddOrReplace<Transform>(e0, t0);

            Mover m0;
            m0.TargetPos = Fling::Random::GetRandomVec3(min, max);
            t_Reg.Add<Mover>(e0, m0);

            PointLight Light;
            Light.DiffuseColor = glm::vec4(Fling::Random::GetRandomVec3(glm::vec3(0.0f), glm::vec3(1.0f)), 1.0f);
            Light.Intensity = 5.0f;
            Light.Range = 3.0f;
            t_Reg.Add<PointLight>(e0, Light);
        };

		

		AddFloor(t_Reg, "Models/cube.obj", "Materials/Cobblestone.mat", glm::vec3(40.0f, 0.1f, 40.0f));

		// Add a bunch of random light bois
		for (size_t i = 0; i < DeferredLightSettings::MaxPointLights; i++)
		{
			AddRandomPointLight();
		}

		// Spawn a little grid of spheres
		float Spacing = 1.5f;
		int32 GridSize = 25;
		
		for (int32 i = 0; i < GridSize; i++)
		{
			Fling::String ModelPath = "Models/sphere.obj";
			Fling::String MatPath = "Materials/DeferredBronzeMat.mat";

			for (int32 j = 0; j < GridSize; j++)
			{
				if (j == 0)
				{
					MatPath = "Materials/Damascus.mat";
				}
				else if (j == 1)
				{
					MatPath = "Materials/SheetMetal.mat";
				}
				else if (j == 2)
				{
					MatPath = "Materials/Snow.mat";
				}
				else if (j == 3)
				{
					MatPath = "Materials/Cobblestone.mat";
				}
				else if (j == 4)
				{
					MatPath = "Materials/DotMetal.mat";
				}
				else
				{
					MatPath = "Materials/DeferredBronzeMat.mat";
				}

				if (i == 1)
				{
					ModelPath = "Models/cube.obj";
				}
				else if(i == 2)
				{
					ModelPath = "Models/cone.obj";
				}
				else if (i == 3)
				{
					ModelPath = "Models/RoundedCube.obj";
				}
				else if (i == 4)
				{
					ModelPath = "Models/torus.obj";
				}
				else
				{
					ModelPath = "Models/sphere.obj";
				}

				Fling::Entity e0 = t_Reg.Create();
				t_Reg.Add<MeshRenderer>(e0, ModelPath.c_str(), MatPath.c_str());
				t_Reg.Add<Rotator>(e0);
				Transform& t0 = t_Reg.Add<Transform>(e0);
				t0.Position =
					glm::vec3(( -(float)(GridSize / 2) + ((float)i * Spacing)), (i > GridSize / 2) ? 1.0f : 0.0f, -(float)(GridSize / 2) + float(j) * Spacing );
			}
		}
    }
}	// namespace Sandbox