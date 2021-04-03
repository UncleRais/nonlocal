#include <petsc.h>
#include "influence_functions.hpp"
#include "heat_equation_solver.hpp"

namespace {

void save_raw_data(const std::shared_ptr<mesh::mesh_2d<double>>& mesh,
                   const std::vector<double>& T,
                   const std::array<std::vector<double>, 2>& gradient) {
    std::ofstream Tout{"T.csv"};
    std::ofstream Tx{"Tx.csv"};
    std::ofstream Ty{"Ty.csv"};
    for(size_t i = 0; i < mesh->nodes_count(); ++i) {
        Tout << mesh->node(i)[0] << ',' << mesh->node(i)[1] << ',' << T[i] << '\n';
        Tx   << mesh->node(i)[0] << ',' << mesh->node(i)[1] << ',' << gradient[0][i] << '\n';
        Ty   << mesh->node(i)[0] << ',' << mesh->node(i)[1] << ',' << gradient[1][i] << '\n';
    }
}

}

int main(int argc, char** argv) {
    PetscErrorCode ierr = PetscInitialize(&argc, &argv, nullptr, nullptr); CHKERRQ(ierr);

    if(argc < 5) {
        std::cerr << "Input format [program name] <path to mesh> <num_threads> <r> <p1>";
        return EXIT_FAILURE;
    }

    try {
        std::cout.precision(7);

        omp_set_num_threads(std::stoi(argv[2]));
        const double r = std::stod(argv[3]), p1 = std::stod(argv[4]);
        static const nonlocal::influence::polynomial<double, 2, 1> bell(r);

        auto mesh = std::make_shared<mesh::mesh_2d<double>>(argv[1]);
        auto mesh_proxy = std::make_shared<mesh::mesh_proxy<double, int>>(mesh);
        if (p1 < 0.999)
            mesh_proxy->find_neighbours(r, mesh::balancing_t::MEMORY);

        nonlocal::heat::heat_equation_solver<double, int> fem_sol{mesh_proxy};

        auto T = fem_sol.stationary(
                { // Граничные условия
                        {   // Down
                                nonlocal::heat::boundary_t::FLOW,
                                [](const std::array<double, 2>& x) { return -1; },
                        },

                        {   // Right
                                nonlocal::heat::boundary_t::FLOW,
                                [](const std::array<double, 2>& x) { return 0; },
                        },

                        {   // Up
                                nonlocal::heat::boundary_t::FLOW,
                                [](const std::array<double, 2>& x) { return 1; },
                        },

                        {   // Left
                                nonlocal::heat::boundary_t::FLOW,
                                [](const std::array<double, 2>& x) { return 0; },
                        }
                },
                {[](const std::array<double, 2>&) { return 0; }}, // Правая часть
                p1, // Вес
                bell // Функция влияния
        );

//        auto T = fem_sol.stationary(
//            { // Граничные условия
//                {   // Down
//                    nonlocal::heat::boundary_t::TEMPERATURE,
//                    [](const std::array<double, 2>& x) { return x[0]*x[0] + x[1]*x[1]; },
//                },
//
//                {   // Right
//                    nonlocal::heat::boundary_t::TEMPERATURE,
//                    [](const std::array<double, 2>& x) { return x[0]*x[0] + x[1]*x[1]; },
//                },
//
//                {   // Up
//                    nonlocal::heat::boundary_t::TEMPERATURE,
//                    [](const std::array<double, 2>& x) { return x[0]*x[0] + x[1]*x[1]; },
//                },
//
//                {   // Left
//                    nonlocal::heat::boundary_t::TEMPERATURE,
//                    [](const std::array<double, 2>& x) { return x[0]*x[0] + x[1]*x[1]; },
//                }
//            },
//            {[](const std::array<double, 2>&) { return -4; }}, // Правая часть
//            p1, // Вес
//            bell // Функция влияния
//        );

        int rank = -1;
        MPI_Comm_rank(MPI_COMM_WORLD, &rank);
        if (rank == 0) {
            std::cout << "Energy = " << T.calc_energy() << std::endl;
            save_raw_data(mesh, T.get_temperature(), mesh_proxy->calc_gradient(T.get_temperature()));
            T.save_as_vtk("heat.vtk");
        }
    } catch(const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    } catch(...) {
        std::cerr << "Unknown error." << std::endl;
        return EXIT_FAILURE;
    }

    return PetscFinalize();
}