#pragma once

template <class T>
class Singleton
{
public:

	
	static T& instance();

	virtual void Init() {}

	virtual void Shutdown() {}

protected:

	explicit Singleton<T>() = default;

};

template<typename T>
T& Singleton<T>::instance()
{
	static_assert( std::is_default_constructible<T>::value,
		"T is required to be default constructible" );
	static T _instance;

	return _instance;
}