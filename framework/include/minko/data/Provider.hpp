/*
Copyright (c) 2014 Aerys

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
associated documentation files (the "Software"), to deal in the Software without restriction,
including without limitation the rights to use, copy, modify, merge, publish, distribute,
sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or
substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#pragma once

#include "minko/Common.hpp"
#include "minko/Any.hpp"
#include "minko/Signal.hpp"
#include "minko/Flyweight.hpp"
#include "minko/Uuid.hpp"

#include "google/sparse_hash_map"

namespace minko
{
	namespace data
	{
		class Provider :
			public std::enable_shared_from_this<Provider>,
            public Uuid::enable_uuid
		{
        private:
			template <class K, class V, typename... H>
			using map = google::sparse_hash_map<K, V, H...>;
			// template <class K, typename... V>
			// using map = std::unordered_map<K, V...>;

            template <typename T>
            struct is_shared_ptr : std::false_type {};
            template <typename T>
            struct is_shared_ptr<std::shared_ptr<T>> : std::true_type {};

            template <typename T>
            struct is_weak_ptr : std::false_type {};
            template <typename T>
            struct is_weak_ptr<std::weak_ptr<T>> : std::true_type{};

            template <typename T>
            struct is_valid {
                static const bool value = !std::is_pointer<T>::value && !std::is_reference<T>::value
                    && !is_shared_ptr<T>::value &&!is_weak_ptr<T>::value;
            };

		public:
			typedef std::shared_ptr<Provider>	Ptr;
			typedef Flyweight<std::string>		PropertyName;
			typedef map<PropertyName, Any> 		ValueMap;

		private:
            ValueMap							_values;

			Signal<Ptr, const PropertyName&>    _propertyAdded;
            Signal<Ptr, const PropertyName&>	_propertyChanged;
			Signal<Ptr, const PropertyName&>	_propertyRemoved;

		public:
			static
			Ptr
			create()
			{
				Ptr provider = std::shared_ptr<Provider>(new Provider());

				return provider;
			}

			static
			Ptr
			create(Ptr source)
			{
				return create()->copyFrom(source);
			}

			inline
			bool
            hasProperty(const std::string& propertyName) const
            {
                return _values.count(propertyName) != 0;
            }

			inline
            const ValueMap&
			values() const
			{
				return _values;
			}

			inline
			Signal<Ptr, const PropertyName&>&
			propertyAdded()
			{
				return _propertyAdded;
			}

            inline
			Signal<Ptr, const PropertyName&>&
			propertyChanged()
			{
				return _propertyChanged;
			}

			inline
			Signal<Ptr, const PropertyName&>&
			propertyRemoved()
			{
				return _propertyRemoved;
			}

			template <typename T>
			inline
            typename std::enable_if<is_valid<T>::value, const T&>::type
            get(const PropertyName& propertyName) const
			{
                return *Any::unsafe_cast<T>(&(_values.find(propertyName)->second));
			}

            template <typename T>
            inline
            typename std::enable_if<is_valid<T>::value, const T*>::type
            getPointer(const PropertyName& propertyName) const
            {
                return Any::unsafe_cast<T>(&(_values.find(propertyName)->second));
            }

            template <typename T>
            inline
            typename std::enable_if<is_valid<T>::value, T*>::type
            getUnsafePointer(const PropertyName& propertyName)
            {
                return Any::unsafe_cast<T>(&(_values.find(propertyName)->second));
            }

            template <typename T>
            typename std::enable_if<is_valid<T>::value, Ptr>::type
            set(const PropertyName& propertyName, T value)
            {
                if (_values.count(propertyName) != 0)
                {
                    T* ptr = Any::cast<T>(&_values[propertyName]);
                    auto changed = !(*ptr == value);

                    *ptr = value;
					// memcpy(ptr, &value, sizeof(T));
                    if (changed)
                        _propertyChanged.execute(shared_from_this(), propertyName);
                }
                else
                {
                    _values[propertyName] = value;
                    _propertyAdded.execute(shared_from_this(), propertyName);
                    _propertyChanged.execute(shared_from_this(), propertyName);
                }

                return shared_from_this();
            }

            template <typename T>
			bool
            propertyHasType(const PropertyName& propertyName) const
			{
                const auto foundIt = _values.find(propertyName);

				if (foundIt == _values.end())
					throw std::invalid_argument("propertyName");

				return Any::cast<T>(&foundIt->second) != nullptr;
			}

			virtual
			Ptr
            unset(const std::string& propertyName);

			Ptr
			clone();

			virtual
			Ptr
			copyFrom(Ptr source);

		protected:
			Provider();
		};
	}
}
