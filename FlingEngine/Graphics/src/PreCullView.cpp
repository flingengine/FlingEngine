#include "PreCullView.h"

namespace Fling
{
    namespace
    {
        Frustum g_View;
        bool g_HasView = false;
    } // namespace

    void PreCullView::Set(const Frustum* t_View)
    {
        g_HasView = t_View != nullptr;
        if (t_View != nullptr)
        {
            g_View = *t_View;
        }
    }

    const Frustum* PreCullView::Get()
    {
        return g_HasView ? &g_View : nullptr;
    }
} // namespace Fling
