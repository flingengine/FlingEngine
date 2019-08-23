#pragma once

#include "Platform.h"	// FLING_API def

namespace Fling
{
	/**
	 * Class that removes the copy operator and constructor. Leaves move 
	 */
	class FLING_API NonCopyable
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