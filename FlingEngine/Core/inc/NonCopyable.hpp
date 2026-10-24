#pragma once

#include "CoreAPI.h"

namespace Fling
{
	/**
	 * Class that removes the copy operator and constructor. Leaves move 
	 */
	class CORE_API NonCopyable
	{
	protected:
		NonCopyable() = default;

		virtual ~NonCopyable() = default;

	public:
		NonCopyable(const NonCopyable&) = delete;

		NonCopyable(NonCopyable&&) = default;

		NonCopyable& operator=(const NonCopyable&) = delete;

		NonCopyable& operator=(NonCopyable&&) = default;
	};
}