#ifndef FINITE_ELEMENT_BASE_HPP
#define FINITE_ELEMENT_BASE_HPP

// В данном модуле описаны базовые интерфейсы классов конечных элементов.

#include "geometry_1d.hpp"
#include "geometry_2d.hpp"
#include "quadrature.hpp"

namespace finite_element {

// Любой конечный элемент, вне зависимости от его размерности, имеет некоторое количество узлов.
class element_base {
public:
    virtual size_t nodes_count() const = 0;

    virtual ~element_base() = default;
};

template<class Type>
class element_integrate_base : public virtual element_base {
    static_assert(std::is_floating_point<Type>::value, "Type must be floating point.");

public:
    virtual size_t qnodes_count() const = 0;

    virtual Type weight(const size_t q) const = 0;

    virtual Type qN  (const size_t i, const size_t q) const = 0;
    virtual Type qNxi(const size_t i, const size_t q) const = 0;

    virtual ~element_integrate_base() = default;
};

template<class Type>
class element_1d_base : public virtual element_base {
    static_assert(std::is_floating_point<Type>::value, "Type must be floating point.");

public:
    virtual Type N  (const size_t i, const Type xi) const = 0; // Обращение к i-ой функции формы в точке xi.
    virtual Type Nxi(const size_t i, const Type xi) const = 0; // Аналогично для производной.

    virtual Type boundary(const side_1d bound) const = 0; // Геометрия элемента.

    virtual ~element_1d_base() = default;
};

template<class Type>
class element_1d_integrate_base : public element_integrate_base<Type>,
                                  public virtual element_1d_base<Type> {
public:
    virtual void set(const quadrature_base<Type> &quad) = 0;

    virtual ~element_1d_integrate_base() = default;
};

template<class Type>
class element_2d_base : public virtual element_base {
    static_assert(std::is_floating_point<Type>::value, "Type must be floating point.");

public:
    virtual Type N   (const size_t i, const Type xi, const Type eta) const = 0; // Обращение к i-ой функции формы в точке (xi, eta).
    virtual Type Nxi (const size_t i, const Type xi, const Type eta) const = 0; // Аналогично для производных.
    virtual Type Neta(const size_t i, const Type xi, const Type eta) const = 0;

    virtual Type boundary(const side_2d bound, const Type x) const = 0; // Геометрия элемента.

    virtual ~element_2d_base() = default;
};

template<class Type>
class element_2d_integrate_base : public element_integrate_base<Type>,
                                  public virtual element_2d_base<Type> {
public:
    virtual void set(const quadrature_base<Type> &quadXi, const quadrature_base<Type> &quadEta) = 0;

    virtual Type qNeta(const size_t i, const size_t q) const = 0;

    virtual ~element_2d_integrate_base() = default;
};

}

#endif