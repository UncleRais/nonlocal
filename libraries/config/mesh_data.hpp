#ifndef NONLOCAL_CONFIG_MESH_DATA_HPP
#define NONLOCAL_CONFIG_MESH_DATA_HPP

#include "config_utils.hpp"

#include <filesystem>

namespace nonlocal::config {

template<size_t Dimension>
struct mesh_data final {
    std::filesystem::path mesh; // required

    explicit mesh_data() = default;
    explicit mesh_data(const Json::Value& value) {
        check_required_fields(value, { "mesh" });
        mesh = value["mesh"].asString();
    }

    Json::Value to_json() const {
        Json::Value result;
        result["mesh"] = mesh.string();
        return result;
    }
};

template<>
struct mesh_data<1u> final {
    size_t element_order = 1;
    size_t quadrature_order = 1;

    explicit constexpr mesh_data() noexcept = default;
    explicit mesh_data(const Json::Value& value)
        : element_order{get_order(value.get("element_order", 1))}
        , quadrature_order{get_order(value.get("quadrature_order", element_order))} {}

    Json::Value to_json() const {
        Json::Value result;
        result["element_order"] = get_order(element_order);
        result["quadrature_order"] = get_order(quadrature_order);
        return result;
    }
};

}

#endif