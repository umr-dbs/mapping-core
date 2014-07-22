#ifndef UTIL_MAKE_UNIQUE_H
#define UTIL_MAKE_UNIQUE_H

#if __cplusplus < 201400L

#include <memory>

namespace std {
	template<typename T, typename... Args>
	std::unique_ptr<T> make_unique(Args&&... args)
	{
		return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
	}
}

#endif


#endif
