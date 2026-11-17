#include "Logger.h"
#include "Systems/MotionProbeSystem.h"

#include <cmath>

namespace Sandbox
{
    void MotionProbeSystem::OnPlayBegin(Context& ctx) const
    {
        ctx.Singleton<MotionProbe>() = MotionProbe{};
    }

    void MotionProbeSystem::Update(Context& ctx) const
    {
        const Fling::RenderSnapshot& snapshot = ctx.Singleton<const Fling::RenderSnapshot>();
        MotionProbe& probe = ctx.Singleton<MotionProbe>();

        // The same meshes in the same order as the last frame (a spawn or a removal skips one frame)
        const bool same = probe.Keys.size() == snapshot.Meshes.size();
        double turn = 0.0;
        probe.Keys.resize(snapshot.Meshes.size());
        probe.Axes.resize(snapshot.Meshes.size());
        for (size_t i = 0; i < snapshot.Meshes.size(); ++i)
        {
            const Fling::MeshDrawItem& item = snapshot.Meshes[i];
            const glm::vec3 axis = glm::normalize(glm::vec3(item.ResolveWorld(snapshot.BlendAlpha)[0]));
            if (same && probe.Keys[i] == item.Key)
            {
                // The angle between the two axes, from the chord: precise for small angles
                const double chord = static_cast<double>(glm::length(axis - probe.Axes[i]));
                turn = Fling::Math::Max(turn, glm::degrees(2.0 * std::asin(Fling::Math::Min(chord * 0.5, 1.0))));
            }
            probe.Keys[i] = item.Key;
            probe.Axes[i] = axis;
        }
        if (same)
        {
            probe.Add(turn);
        }
    }

    void MotionProbeSystem::OnPlayEnd(Context& ctx) const
    {
        const MotionProbe& probe = ctx.Singleton<const MotionProbe>();
        F_LOG_TRACE("Sandbox: rendered turn over {} frames, {:.6f} to {:.6f} degrees per frame, even {}, repeated "
                    "frames {}",
                    probe.TurnFrames,
                    probe.MinTurn,
                    probe.MaxTurn,
                    probe.Even() ? "yes" : "no",
                    probe.Repeated);
    }
} // namespace Sandbox
