
#ifndef HISSTOOLS_FFT_TYPES_HPP
#define HISSTOOLS_FFT_TYPES_HPP

namespace impl
{
    // TypeBase is a helper utility for binding a type to a scalar type
    
    template <class T>
    struct TypeBase
    {
        using scalar_type = T;
    };
    
    // SetupBase is a helper utility for binding FFT setups to a template type for a given type.
    
    template <class T, class U>
    struct SetupBase
    {
        using scalar_type = T;
        
        SetupBase() : m_setup(nullptr) {}
        SetupBase(U setup) : m_setup(setup) {}
        
        operator U() { return m_setup; }
        
        U m_setup;
    };
}

#endif /* HISSTOOLS_FFT_TYPES_HPP */
