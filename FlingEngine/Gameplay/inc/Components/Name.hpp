#pragma once

#include <string>

namespace Fling
{
	struct NameComponent
	{
		std::string Name;

		template<class Archive>
        void serialize(Archive & t_Archive)
        {
            t_Archive( 
                cereal::make_nvp("Name", Name)
            ); 
        }
	};
}   // namespace Fling