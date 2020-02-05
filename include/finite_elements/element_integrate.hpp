#ifndef FINITE_ELEMENT_INTEGRATE_HPP
#define FINITE_ELEMENT_INTEGRATE_HPP

#include "element_1d.hpp"
#include "element_2d.hpp"

namespace finite_element {

template<class Type, template<class> class Element_Type>
class element_1d_integrate : public element_1d_integrate_base<Type>,
                            public element_1d<Type, Element_Type> {
    std::vector<Type> weights, NInQuad, NxiInQuad;

public:
    element_1d_integrate(const quadrature_base<Type> &quad) { set(quad); }

    void set(const quadrature_base<Type> &quad) override {
        Type jacobian = (this->boundary(side_1d::RIGHT) - this->boundary(side_1d::LEFT)) /
                        (quad. boundary(side_1d::RIGHT) - quad. boundary(side_1d::LEFT));
        std::vector<Type> xi(quad.nodes_count());

        weights.resize(quad.nodes_count());
        for(size_t q = 0; q < this->qnodes_count(); ++q) {
            xi[q] = this->boundary(side_1d::LEFT) + (quad.node(q)-quad.boundary(side_1d::LEFT)) * jacobian;
            weights[q] = quad.weight(q) * jacobian;
        }

        NInQuad.resize(qnodes_count() * this->nodes_count());
        NxiInQuad.resize(qnodes_count() * this->nodes_count());
        for(size_t i = 0; i < this->nodes_count(); ++i)
            for(size_t q = 0; q < qnodes_count(); ++q) {
                NInQuad  [i*qnodes_count() + q] = this->N  (i, xi[q]);
                NxiInQuad[i*qnodes_count() + q] = this->Nxi(i, xi[q]);
            }
    }

    size_t qnodes_count() const override { return weights.size(); }

    Type weight(const size_t q) const override { return weights[q]; }

    Type qN  (const size_t i, const size_t q) const override { return NInQuad  [i*qnodes_count() + q]; }
    Type qNxi(const size_t i, const size_t q) const override { return NxiInQuad[i*qnodes_count() + q]; }

    virtual ~element_1d_integrate() = default;
};

template<class Type, template<class> class Element_Type>
class element_2d_integrate : public element_2d_integrate_base<Type>,
                             public element_2d<Type, Element_Type> {
    std::vector<Type> weights, NInQuad, NxiInQuad, NetaInQuad;

public:
    element_2d_integrate(const quadrature_base<Type> &quadXi, const quadrature_base<Type> &quadEta) { set(quadXi, quadEta); }

    // Узлы в которых происходит расчёт получаются путём декартова произведения квадратур с учётом того,
    // что геометрия описывается таким образом, что параметрическую зависимость имеет верхняя и нижняя границы,
    // а правая и левая заданы константами.
    void set(const quadrature_base<Type> &quadXi, const quadrature_base<Type> &quadEta) override
    {
        Type jacobianXi = (this-> boundary(side_2d::RIGHT, 0) - this ->boundary(side_2d::LEFT, 0)) /
                          (quadXi.boundary(side_1d::RIGHT)    - quadXi.boundary(side_1d::LEFT));
        std::vector<Type> jacobianEta(quadXi. nodes_count()),
                          xi(quadXi. nodes_count()),
                          eta(quadEta.nodes_count());

        weights.resize(quadXi.nodes_count() * quadEta.nodes_count());
        for(size_t i = 0; i < quadXi.nodes_count(); ++i) {
            xi[i] = this->boundary(side_2d::LEFT, 0) + (quadXi.node(i) - quadXi.boundary(side_1d::LEFT)) * jacobianXi;
            jacobianEta[i] = (this  ->boundary(side_2d::UP, xi[i]) - this  ->boundary(side_2d::DOWN, xi[i])) /
                             (quadEta.boundary(side_1d::RIGHT)     - quadEta.boundary(side_1d::LEFT));
            for(size_t j = 0; j < quadEta.nodes_count(); ++j) {
                eta[j] = this->boundary(side_2d::DOWN, xi[i]) + (quadEta.node(j)-quadEta.boundary(side_1d::LEFT)) * jacobianEta[i];
                weights[i*quadEta.nodes_count() + j] = quadXi.weight(i) * jacobianXi * quadEta.weight(j) * jacobianEta[i];
            }
        }

        NInQuad.resize(qnodes_count() * this->nodes_count());
        NxiInQuad.resize(qnodes_count() * this->nodes_count());
        NetaInQuad.resize(qnodes_count() * this->nodes_count());
        for(size_t i = 0; i < this->nodes_count(); ++i)
            for(size_t j = 0; j < quadXi.nodes_count(); ++j)
                for(size_t k = 0; k < quadEta.nodes_count(); ++k) {
                    NInQuad   [i*qnodes_count() + j*quadEta.nodes_count() + k] = this->N   (i, xi[j], eta[k]);
                    NxiInQuad [i*qnodes_count() + j*quadEta.nodes_count() + k] = this->Nxi (i, xi[j], eta[k]);
                    NetaInQuad[i*qnodes_count() + j*quadEta.nodes_count() + k] = this->Neta(i, xi[j], eta[k]);
                }
    }

    size_t qnodes_count() const override { return weights.size(); }

    Type weight(const size_t q) const override { return weights[q]; }

    Type qN   (const size_t i, const size_t q) const override { return NInQuad   [i*qnodes_count() + q]; }
    Type qNxi (const size_t i, const size_t q) const override { return NxiInQuad [i*qnodes_count() + q]; }
    Type qNeta(const size_t i, const size_t q) const override { return NetaInQuad[i*qnodes_count() + q]; }

    virtual ~element_2d_integrate() = default;
};

// Частичная специализация класса описанного выше, на случай кубических серендиповых элементов. Необходима для экспериментальных целей.
template<class Type>
class element_2d_integrate<Type, qubic_serendip> : public element_2d_integrate_base<Type>,
                                                   public element_2d<Type, qubic_serendip> {
    std::vector<Type> weights, NInQuad, NxiInQuad, NetaInQuad,
                      Nxi2InQuad, NxietaInQuad, Neta2InQuad, Nxi3InQuad, Nxi2etaInQuad, Nxieta2InQuad, Neta3InQuad;

public:
    element_2d_integrate(const quadrature_base<Type> &quadXi, const quadrature_base<Type> &quadEta) { set(quadXi, quadEta); }

    // Узлы в которых происходит расчёт получаются путём декартова произведения квадратур с учётом того,
    // что геометрия описывается таким образом, что параметрическую зависимость имеет верхняя и нижняя границы,
    // а правая и левая заданы константами.
    void set(const quadrature_base<Type> &quadXi, const quadrature_base<Type> &quadEta) override
    {
        Type jacobianXi = (this-> boundary(side_2d::RIGHT, 0) - this ->boundary(side_2d::LEFT, 0)) /
                          (quadXi.boundary(side_1d::RIGHT)    - quadXi.boundary(side_1d::LEFT));
        std::vector<Type> jacobianEta(quadXi. nodes_count()),
                          xi(quadXi. nodes_count()),
                          eta(quadEta.nodes_count());

        weights.resize(quadXi.nodes_count() * quadEta.nodes_count());
        for(size_t i = 0; i < quadXi.nodes_count(); ++i) {
            xi[i] = this->boundary(side_2d::LEFT, 0) + (quadXi.node(i) - quadXi.boundary(side_1d::LEFT)) * jacobianXi;
            jacobianEta[i] = (this  ->boundary(side_2d::UP, xi[i]) - this  ->boundary(side_2d::DOWN, xi[i])) /
                             (quadEta.boundary(side_1d::RIGHT)     - quadEta.boundary(side_1d::LEFT));
            for(size_t j = 0; j < quadEta.nodes_count(); ++j) {
                eta[j] = this->boundary(side_2d::DOWN, xi[i]) + (quadEta.node(j)-quadEta.boundary(side_1d::LEFT)) * jacobianEta[i];
                weights[i*quadEta.nodes_count() + j] = quadXi.weight(i) * jacobianXi * quadEta.weight(j) * jacobianEta[i];
            }
        }

        NInQuad.resize(qnodes_count() * this->nodes_count());
        NxiInQuad.resize(qnodes_count() * this->nodes_count());
        NetaInQuad.resize(qnodes_count() * this->nodes_count());

        Nxi2InQuad.resize(qnodes_count() * this->nodes_count());
        NxietaInQuad.resize(qnodes_count() * this->nodes_count());
        Neta2InQuad.resize(qnodes_count() * this->nodes_count());
        Nxi3InQuad.resize(qnodes_count() * this->nodes_count());
        Nxi2etaInQuad.resize(qnodes_count() * this->nodes_count());
        Nxieta2InQuad.resize(qnodes_count() * this->nodes_count());
        Neta3InQuad.resize(qnodes_count() * this->nodes_count());

        for(size_t i = 0; i < this->nodes_count(); ++i)
            for(size_t j = 0; j < quadXi.nodes_count(); ++j)
                for(size_t k = 0; k < quadEta.nodes_count(); ++k) {
                    NInQuad   [i*qnodes_count() + j*quadEta.nodes_count() + k] = this->N   (i, xi[j], eta[k]);
                    NxiInQuad [i*qnodes_count() + j*quadEta.nodes_count() + k] = this->Nxi (i, xi[j], eta[k]);
                    NetaInQuad[i*qnodes_count() + j*quadEta.nodes_count() + k] = this->Neta(i, xi[j], eta[k]);

                    Nxi2InQuad   [i*qnodes_count() + j*quadEta.nodes_count() + k] = this->Nxi2   (i, xi[j], eta[k]);
                    NxietaInQuad [i*qnodes_count() + j*quadEta.nodes_count() + k] = this->Nxieta (i, xi[j], eta[k]);
                    Neta2InQuad  [i*qnodes_count() + j*quadEta.nodes_count() + k] = this->Neta2  (i, xi[j], eta[k]);
                    Nxi3InQuad   [i*qnodes_count() + j*quadEta.nodes_count() + k] = this->Nxi3   (i, xi[j], eta[k]);
                    Nxi2etaInQuad[i*qnodes_count() + j*quadEta.nodes_count() + k] = this->Nxi2eta(i, xi[j], eta[k]);
                    Nxieta2InQuad[i*qnodes_count() + j*quadEta.nodes_count() + k] = this->Nxieta2(i, xi[j], eta[k]);
                    Neta3InQuad  [i*qnodes_count() + j*quadEta.nodes_count() + k] = this->Neta3  (i, xi[j], eta[k]);
                }
    }

    size_t qnodes_count() const override { return weights.size(); }

    Type weight(const size_t q) const override { return weights[q]; }

    Type qN   (const size_t i, const size_t q) const override { return NInQuad   [i*qnodes_count() + q]; }
    Type qNxi (const size_t i, const size_t q) const override { return NxiInQuad [i*qnodes_count() + q]; }
    Type qNeta(const size_t i, const size_t q) const override { return NetaInQuad[i*qnodes_count() + q]; }

    Type qNxi2   (const size_t i, const size_t q) const { return Nxi2InQuad   [i*qnodes_count() + q]; }
    Type qNxieta (const size_t i, const size_t q) const { return NxietaInQuad [i*qnodes_count() + q]; }
    Type qNeta2  (const size_t i, const size_t q) const { return Neta2InQuad  [i*qnodes_count() + q]; }
    Type qNxi3   (const size_t i, const size_t q) const { return Nxi3InQuad   [i*qnodes_count() + q]; }
    Type qNxi2eta(const size_t i, const size_t q) const { return Nxi2etaInQuad[i*qnodes_count() + q]; }
    Type qNxieta2(const size_t i, const size_t q) const { return Nxieta2InQuad[i*qnodes_count() + q]; }
    Type qNeta3  (const size_t i, const size_t q) const { return Neta3InQuad  [i*qnodes_count() + q]; }

    virtual ~element_2d_integrate() = default;
};

}

#endif