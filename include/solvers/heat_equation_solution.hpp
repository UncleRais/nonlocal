#ifndef NONLOCAL_HEAT_EQUATION_SOLUTION_HPP
#define NONLOCAL_HEAT_EQUATION_SOLUTION_HPP

#include "mesh_proxy.hpp"

namespace nonlocal::heat {

template<class T, class I>
class solution final {
    std::shared_ptr<mesh::mesh_proxy<T, I>> _mesh_proxy;
    std::vector<T> _temperature;

public:
    template<class Vector>
    explicit solution(const std::shared_ptr<mesh::mesh_proxy<T, I>>& mesh_proxy, const Vector& temperature);

    const std::vector<T>& get_temperature() const;

    T calc_energy() const;
    void save_as_vtk(const std::string& path) const;
};

template<class T, class I>
template<class Vector>
solution<T, I>::solution(const std::shared_ptr<mesh::mesh_proxy<T, I>>& mesh_proxy, const Vector& temperature)
    : _mesh_proxy{mesh_proxy} {
    _temperature.resize(_mesh_proxy->mesh().nodes_count());
    for(size_t i = 0; i < _mesh_proxy->mesh().nodes_count(); ++i)
        _temperature[i] = temperature[i];
}

template<class T, class I>
const std::vector<T>& solution<T, I>::get_temperature() const { return _temperature; }

template<class T, class I>
T solution<T, I>::calc_energy() const { return _mesh_proxy->integrate_solution(_temperature); }

template<class T, class I>
void solution<T, I>::save_as_vtk(const std::string& path) const {
    static constexpr std::string_view data_type = std::is_same_v<T, float> ? "float" : "double";

    std::ofstream fout{path};
    fout.precision(std::numeric_limits<T>::max_digits10);

    _mesh_proxy->mesh().save_as_vtk(fout);

    fout << "POINT_DATA " << _mesh_proxy->mesh().nodes_count() << '\n';
    fout << "SCALARS Temperature " << data_type << " 1\n"
         << "LOOKUP_TABLE default\n";
    for(size_t i = 0; i < _mesh_proxy->mesh().nodes_count(); ++i)
        fout << _temperature[i] << '\n';
}

}

#endif