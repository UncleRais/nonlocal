#ifndef FINITE_ELEMENT_ROUTINE_HPP
#define FINITE_ELEMENT_ROUTINE_HPP

#include <numeric>
#include <petsc.h>
#include <petscsystypes.h>
#include "../../Eigen/Eigen/Sparse"
#undef I // for new version GCC, when use I macros
#include "mesh.hpp"
#include "right_partition.hpp"
#include "boundary_condition.hpp"

namespace nonlocal {

template<class T, class I>
class finite_element_solver_base {
    std::shared_ptr<mesh::mesh_proxy<T, I>> _mesh_proxy;

protected:
    enum component : bool { X, Y };
    enum theory : bool { LOCAL, NONLOCAL };
    enum index_stage : bool { SHIFTS, NONZERO };

    template<size_t DoF>
    class indexator {
        const std::vector<bool>& _inner_nodes;
        const size_t _node_shift;
        Eigen::SparseMatrix<T, Eigen::RowMajor, I>& _K_inner,
                                                  & _K_bound;
        std::array<std::vector<bool>, DoF> _inner, _bound;
        std::array<size_t, DoF> _inner_index = {}, _bound_index = {};

    public:
        explicit indexator(Eigen::SparseMatrix<T, Eigen::RowMajor, I>& K_inner,
                           Eigen::SparseMatrix<T, Eigen::RowMajor, I>& K_bound,
                           const std::vector<bool>& inner_nodes, const size_t node_shift)
            : _inner_nodes{inner_nodes}
            , _node_shift{node_shift}
            , _K_inner{K_inner}
            , _K_bound{K_bound} {
            for(size_t i = 0; i < DoF; ++i) {
                _inner[i].resize(_inner_nodes.size());
                _bound[i].resize(_inner_nodes.size());
            }
        }

        void fill(const size_t node) {
            for(size_t i = 0; i < DoF; ++i) {
                _inner_index[i] = _K_inner.outerIndexPtr()[DoF * (node - _node_shift) + i];
                _bound_index[i] = _K_bound.outerIndexPtr()[DoF * (node - _node_shift) + i];
                std::fill(_bound[i].begin(), _bound[i].end(), false);
                std::fill(std::next(_inner[i].begin(), DoF * node), _inner[i].end(), false);
            }
        }

        template<index_stage Stage>
        void stage(Eigen::SparseMatrix<T, Eigen::RowMajor, I>& K,
                   std::array<std::vector<bool>, DoF>& inner,
                   std::array<size_t, DoF>& inner_index,
                   const size_t row, const size_t col) {
            const size_t i = row % DoF;
            if (!inner[i][col]) {
                if constexpr (Stage == index_stage::SHIFTS)
                    ++K.outerIndexPtr()[row - DoF * _node_shift + 1];
                if constexpr (Stage == index_stage::NONZERO) {
                    K.valuePtr()[inner_index[i]] = 0;
                    K.innerIndexPtr()[inner_index[i]++] = col;
                }
                inner[i][col] = true;
            }
        }

        template<index_stage Stage>
        void index(const size_t block_row, const size_t block_col) {
            for(size_t comp_row = 0; comp_row < DoF; ++comp_row)
                for(size_t comp_col = 0; comp_col < DoF; ++comp_col) {
                    const size_t row = DoF * block_row + comp_row,
                                 col = DoF * block_col + comp_col;
                    if (_inner_nodes[row] && _inner_nodes[col]) {
                        if (row <= col)
                            stage<Stage>(_K_inner, _inner, _inner_index, row, col);
                    } else if (row != col) {
                        if (!_inner_nodes[col])
                            stage<Stage>(_K_bound, _bound, _bound_index, row, col);
                    } else
                        stage<Stage>(_K_inner, _inner, _inner_index, row, col);
                }
        }
    };

    template<size_t DoF, index_stage Stage, theory Theory>
    void mesh_index(Eigen::SparseMatrix<T, Eigen::RowMajor, I>& K_inner,
                    Eigen::SparseMatrix<T, Eigen::RowMajor, I>& K_bound,
                    const std::vector<bool>& inner_nodes) const {
        indexator<DoF> ind{K_inner, K_bound, inner_nodes, first_node()};
#pragma omp parallel for default(none) firstprivate(ind) schedule(dynamic)
        for(size_t node = first_node(); node < last_node(); ++node) {
            ind.fill(node);
            for(const I eL : mesh_proxy()->nodes_elements_map(node)) {
                if constexpr (Theory == theory::LOCAL)
                    for(size_t jL = 0; jL < mesh().nodes_count(eL); ++jL)
                        ind.template index<Stage>(node, mesh().node_number(eL, jL));
                if constexpr (Theory == theory::NONLOCAL)
                    for(const I eNL : mesh_proxy()->neighbors(eL))
                        for(size_t jNL = 0; jNL < mesh().nodes_count(eNL); ++jNL)
                            ind.template index<Stage>(node, mesh().node_number(eNL, jNL));
            }
        }
    }

    static void prepare_memory(Eigen::SparseMatrix<T, Eigen::RowMajor, I>& K) {
        for(size_t i = 0; i < K.rows(); ++i)
            K.outerIndexPtr()[i+1] += K.outerIndexPtr()[i];
        K.data().resize(K.outerIndexPtr()[K.rows()]);
    }

    static void sort_indices(Eigen::SparseMatrix<T, Eigen::RowMajor, I>& K) {
#pragma omp parallel for default(none) shared(K) schedule(dynamic)
        for(size_t i = 0; i < K.rows(); ++i)
            std::sort(&K.innerIndexPtr()[K.outerIndexPtr()[i]], &K.innerIndexPtr()[K.outerIndexPtr()[i+1]]);
    }

    static constexpr T MAX_LOCAL_WEIGHT = 0.999;

    explicit finite_element_solver_base(const std::shared_ptr<mesh::mesh_proxy<T, I>>& mesh);
    virtual ~finite_element_solver_base() noexcept = default;

    const mesh::mesh_2d<T, I>&            mesh                     ()                     const { return _mesh_proxy->mesh(); }
    int                                   rank                     ()                     const { return _mesh_proxy->rank(); }
    int                                   size                     ()                     const { return _mesh_proxy->size(); }
    size_t                                first_node               ()                     const { return _mesh_proxy->first_node(); }
    size_t                                last_node                ()                     const { return _mesh_proxy->last_node(); }

    static T jacobian(const std::array<T, 4>& J) noexcept;
    static T jacobian(const std::array<T, 2>& J) noexcept;

    template<theory Type, class Callback>
    void mesh_run(const Callback& callback) const;

    // Функция обхода групп граничных элементов.
    // Callback - функтор с сигнатурой void(size_t, size_t, size_t)
    template<class Callback>
    void boundary_nodes_run(const Callback& callback) const;

    // Function - функтор с сигнатурой T(std::array<T, 2>&)
    template<class Function>
    T integrate_function(const size_t e, const size_t i, const Function& func) const;

    template<size_t DoF>
    void integrate_right_part(Eigen::Matrix<T, Eigen::Dynamic, 1>& f,
                              const right_partition<T, DoF>& right_part) const;

    // Boundary_Gradient - функтор с сигнатурой T(std::array<T, 2>&)
    template<class Boundary_Gradient>
    T integrate_boundary_gradient(const size_t b, const size_t e, const size_t i,
                                  const Boundary_Gradient& boundary_gradient) const;

    template<class B, size_t DoF>
    void boundary_condition_first_kind(Eigen::Matrix<T, Eigen::Dynamic, 1>& f,
                                       const std::vector<boundary_condition<T, B, DoF>>& bounds_cond,
                                       const Eigen::SparseMatrix<T, Eigen::RowMajor, I>& K_bound) const;

    template<class B, size_t DoF>
    void integrate_boundary_condition_second_kind(Eigen::Matrix<T, Eigen::Dynamic, 1>& f,
                                                  const std::vector<boundary_condition<T, B, DoF>>& bounds_cond) const;

    void PETSc_solver(Eigen::Matrix<T, Eigen::Dynamic, 1>& f,
                      const Eigen::SparseMatrix<T, Eigen::RowMajor, I>& K);

public:
    void set_mesh(const std::shared_ptr<mesh::mesh_proxy<T, I>>& mesh_proxy);
    const std::shared_ptr<mesh::mesh_proxy<T, I>>& mesh_proxy() const;
};

template<class T, class I>
finite_element_solver_base<T, I>::finite_element_solver_base(const std::shared_ptr<mesh::mesh_proxy<T, I>>& mesh) { set_mesh(mesh); }

template<class T, class I>
void finite_element_solver_base<T, I>::set_mesh(const std::shared_ptr<mesh::mesh_proxy<T, I>>& mesh_proxy) {
    if (mesh_proxy == nullptr)
        throw std::invalid_argument{"mesh_proxy can't nullptr"};
    _mesh_proxy = mesh_proxy;
}

template<class T, class I>
const std::shared_ptr<mesh::mesh_proxy<T, I>>& finite_element_solver_base<T, I>::mesh_proxy() const { return _mesh_proxy; }

template<class T, class I>
T finite_element_solver_base<T, I>::jacobian(const std::array<T, 4>& J) noexcept { return std::abs (J[0] * J[3] - J[1] * J[2]); }

template<class T, class I>
T finite_element_solver_base<T, I>::jacobian(const std::array<T, 2>& J) noexcept { return std::sqrt(J[0] * J[0] + J[1] * J[1]); }

template<class T, class I>
template<typename finite_element_solver_base<T, I>::theory Theory, class Callback>
void finite_element_solver_base<T, I>::mesh_run(const Callback& callback) const {
#pragma omp parallel for default(none) firstprivate(callback) schedule(dynamic)
    for(size_t node = first_node(); node < last_node(); ++node) {
        for(const I eL : mesh_proxy()->nodes_elements_map(node)) {
            const size_t iL = mesh_proxy()->global_to_local_numbering(eL).find(node)->second; // Проекционные функции
            if constexpr (Theory == theory::LOCAL)
                for(size_t jL = 0; jL < mesh().nodes_count(eL); ++jL) // Аппроксимационные функции
                    callback(eL, iL, jL);
            if constexpr (Theory == theory::NONLOCAL)
                for(const I eNL : mesh_proxy()->neighbors(eL))
                    for(size_t jNL = 0; jNL < mesh().nodes_count(eNL); ++jNL) // Аппроксимационные функции
                        callback(eL, eNL, iL, jNL);
        }
    }
}

template<class T, class I>
template<class Callback>
void finite_element_solver_base<T, I>::boundary_nodes_run(const Callback& callback) const {
    for(size_t b = 0; b < mesh().boundary_groups_count(); ++b)
        for(size_t el = 0; el < mesh().elements_count(b); ++el) {
            const auto& be = mesh().element_1d(b, el);
            for(size_t i = 0; i < be->nodes_count(); ++i)
                callback(b, el, i);
        }
}

template<class T, class I>
template<class Function>
T finite_element_solver_base<T, I>::integrate_function(const size_t e, const size_t i, const Function& func) const {
    T integral = 0;
    const auto& el = mesh().element_2d(e);
    auto J = mesh_proxy()->jacobi_matrix(e);
    auto qcoord = mesh_proxy()->quad_coord(e);
    for(size_t q = 0; q < el->qnodes_count(); ++q, ++J)
        integral += el->weight(q) * el->qN(i, q) * func(*qcoord) * jacobian(*J);
    return integral;
}

template<class T, class I>
template<size_t DoF>
void finite_element_solver_base<T, I>::integrate_right_part(Eigen::Matrix<T, Eigen::Dynamic, 1>& f,
                                                            const right_partition<T, DoF>& right_part) const {
#pragma omp parallel for default(none) shared(f, right_part)
    for(size_t node = first_node(); node < last_node(); ++node)
        for(const I e : mesh_proxy()->nodes_elements_map(node)) {
            const size_t i = mesh_proxy()->global_to_local_numbering(e).find(node)->second;
            for(size_t comp = 0; comp < DoF; ++comp)
                f[DoF * (node - first_node()) + comp] += integrate_function(e, i, right_part[comp]);
        }
}

template<class T, class I>
template<class Boundary_Gradient>
T finite_element_solver_base<T, I>::integrate_boundary_gradient(const size_t b, const size_t e, const size_t i,
                                                                const Boundary_Gradient& boundary_gradient) const {
    T integral = 0;
    const auto& be = mesh().element_1d(b, e);
    auto qcoord = mesh_proxy()->quad_coord(b, e);
    auto J      = mesh_proxy()->jacobi_matrix(b, e);
    for(size_t q = 0; q < be->qnodes_count(); ++q, ++qcoord, ++J)
        integral += be->weight(q) * be->qN(i, q) * boundary_gradient(*qcoord) * jacobian(*J);
    return integral;
}

template<class T, class I>
template<class B, size_t DoF>
void finite_element_solver_base<T, I>::boundary_condition_first_kind(Eigen::Matrix<T, Eigen::Dynamic, 1>& f,
                                                                     const std::vector<boundary_condition<T, B, DoF>>& bounds_cond,
                                                                     const Eigen::SparseMatrix<T, Eigen::RowMajor, I>& K_bound) const {
    Eigen::Matrix<T, Eigen::Dynamic, 1> x = Eigen::Matrix<T, Eigen::Dynamic, 1>::Zero(K_bound.cols());
    boundary_nodes_run(
        [this, &bounds_cond, &x](const size_t b, const size_t el, const size_t i) {
            for(size_t comp = 0; comp < DoF; ++comp)
                if (bounds_cond[b].type(comp) == B(boundary_type::FIRST_KIND)) {
                    const I node = DoF * mesh().node_number(b, el, i) + comp;
                    if (x[node] == 0)
                        x[node] = bounds_cond[b].func(comp)(mesh().node(node));
                }
        });

    f -= K_bound * x;

    boundary_nodes_run(
        [this, &bounds_cond, &x, &f](const size_t b, const size_t el, const size_t i) {
            for(size_t comp = 0; comp < DoF; ++comp)
                if (bounds_cond[b].type(comp) == B(boundary_type::FIRST_KIND)) {
                    const I node = DoF * mesh().node_number(b, el, i) + comp;
                    if (node >= DoF * first_node() && node < DoF * last_node())
                        f[node - DoF * first_node()] = x[node];
                }
        });
}

template<class T, class I>
template<class B, size_t DoF>
void finite_element_solver_base<T, I>::integrate_boundary_condition_second_kind(Eigen::Matrix<T, Eigen::Dynamic, 1>& f,
                                                                                const std::vector<boundary_condition<T, B, DoF>>& bounds_cond) const {
    for(size_t b = 0; b < bounds_cond.size(); ++b)
        for(size_t comp = 0; comp < DoF; ++comp)
            if(bounds_cond[b].type(comp) == B(boundary_type::SECOND_KIND)) {
                for(size_t e = 0; e < mesh().elements_count(b); ++e) {
                    const auto& be = mesh().element_1d(b, e);
                    for(size_t i = 0; i < be->nodes_count(); ++i) {
                        const I node = DoF * mesh().node_number(b, e, i) + comp;
                        if(node >= DoF * first_node() && node < DoF * last_node())
                            f[node - DoF * first_node()] += integrate_boundary_gradient(b, e, i, bounds_cond[b].func(comp));
                    }
                }
            }
}

template<class T, class I>
void finite_element_solver_base<T, I>::PETSc_solver(Eigen::Matrix<T, Eigen::Dynamic, 1>& f,
                                                    const Eigen::SparseMatrix<T, Eigen::RowMajor, I>& K) {
    Mat A = nullptr;
    MatCreateMPISBAIJWithArrays(PETSC_COMM_WORLD, 1, K.rows(), K.rows(), PETSC_DETERMINE, PETSC_DETERMINE,
                                K.outerIndexPtr(), K.innerIndexPtr(), K.valuePtr(), &A);

    Vec f_petsc = nullptr;
    VecCreate(PETSC_COMM_WORLD, &f_petsc);
    VecSetType(f_petsc, VECSTANDARD);
    VecSetSizes(f_petsc, K.rows(), K.cols());
    for(I i = first_node(); i < last_node(); ++i)
        VecSetValues(f_petsc, 1, &i, &f[i - first_node()], INSERT_VALUES);
    VecAssemblyBegin(f_petsc);
    VecAssemblyEnd(f_petsc);

    Vec x = nullptr;
    VecDuplicate(f_petsc, &x);
    VecAssemblyBegin(x);
    VecAssemblyEnd(x);

    KSP ksp = nullptr;
    KSPCreate(PETSC_COMM_WORLD, &ksp);
    KSPSetType(ksp, KSPSYMMLQ);
    KSPSetOperators(ksp, A, A);
    KSPSolve(ksp, f_petsc, x);

    Vec y = nullptr;
    VecScatter toall = nullptr;
    VecScatterCreateToAll(x, &toall, &y);
    VecScatterBegin(toall, x, y, INSERT_VALUES, SCATTER_FORWARD);
    VecScatterEnd(toall, x, y, INSERT_VALUES, SCATTER_FORWARD);

    f.resize(K.cols());
    PetscScalar* data = nullptr;
    VecGetArray(y, &data);
    for(I i = 0; i < f.size(); ++i)
        f[i] = data[i];

    VecScatterDestroy(&toall);
    KSPDestroy(&ksp);
    MatDestroy(&A);
    VecDestroy(&f_petsc);
    VecDestroy(&x);
    VecDestroy(&y);
}

}

#endif