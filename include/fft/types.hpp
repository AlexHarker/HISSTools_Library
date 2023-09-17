
#ifndef HISSTOOLS_LIBRARY_FFT_TYPES_HPP
#define HISSTOOLS_LIBRARY_FFT_TYPES_HPP

#include "../namespace.hpp"

HISSTOOLS_LIBRARY_NAMESPACE_START()

namespace impl
{
    // type_base is a helper utility for binding a type to a scalar type
    
    template <class T>
    struct type_base
    {
        using scalar_type = T;
    };
    
    // setup_base is a helper utility for binding FFT setups to a template type for a given type.
    
    template <class T, class U>
    struct setup_base
    {
        using scalar_type = T;
        
        setup_base() : m_setup(nullptr) {}
        setup_base(U setup) : m_setup(setup) {}
        
        operator U() { return m_setup; }
        
        U m_setup;
    };
}

HISSTOOLS_LIBRARY_NAMESPACE_END()

#endif /* HISSTOOLS_LIBRARY_FFT_TYPES_HPP */
