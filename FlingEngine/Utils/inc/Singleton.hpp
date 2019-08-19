#pragma once

#include "Platform.h"

namespace Fling
{
    /**
     * @brief Class that can have only one instance. 
     */
    template <class T>
    class Singleton
    {
    public:

        static T& Get();

        // Have separate virtual init and shutdown functions so that we 
        // can control the creation order

        FLING_API virtual void Init() {}

        FLING_API virtual void Shutdown() {}

    protected:

        /**
        * Every singleton must have a default constructor so that explicit
        * creation and destruction of them is maintained. Use Init and Shutdown to
        * handle initializing of singletons.
        */
        explicit Singleton<T>() = default;

    };

    template<typename T>
    T& Singleton<T>::Get()
    {
        static_assert(std::is_default_constructible<T>::value,
            "T is required to be default constructible");

        static T m_Instance;

        return m_Instance;
    }

}   // namespace Fling