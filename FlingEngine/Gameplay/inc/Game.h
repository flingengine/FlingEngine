#pragma once


#include "NonCopyable.hpp"

namespace Fling
{
    /**
     * @brief Base game class that will be updated from the core engine loop.
     */
    class FLING_API Game : public NonCopyable
    {
    public:
    
        virtual ~Game() = default;

        virtual void Start();

        /**
         * @brief Update game logic
         * 
         * @param t_DeltaTime Time between previous frame
         */
        virtual void Update(float t_DeltaTime);
    };
} // namespace Fling