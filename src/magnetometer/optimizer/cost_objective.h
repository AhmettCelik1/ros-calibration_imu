#ifndef COST_OBJECTIVE_H
#define COST_OBJECTIVE_H

#include <ifopt/cost_term.h>

#include "magnetometer/point.h"

namespace ifopt
{
    /// \brief The objective function for the ellipse fit.
    class cost_objective
        : public CostTerm
    {
    public:
        // CONSTRUCTORS
        /// \brief Instantiates a new cost_objective instance.
        /// \param points The points to fit.
        cost_objective(const points_t* points, double perturbation = 0.000001);
        ~cost_objective();

        // OVERRIDES
        double GetCost() const override;
        void FillJacobianBlock(std::string variable_set, Jacobian& jacobian) const override;

    private:
        // VARIABLES
        /// \brief Stores the points to fit.
        const points_t* m_points;

        // PARAMETERS
        /// \brief Stores the perturbation for gradient calculation.
        double p_perturbation;

        // PREALLOCATIONS
        /// \brief The fitted ellipse center point.
        Eigen::Matrix<double, 3, 1>* c;
        /// \brief A point of the ellipse.
        Eigen::Matrix<double, 3, 1>* p;
        /// \brief A transposed point of the ellipse.
        Eigen::Matrix<double, 1, 3>* p_t;
        /// \brief The rotation of the fitted ellipse.
        Eigen::Matrix<double, 3, 3>* r;
        /// \brief The transposed rotation of the fitted ellipse.
        Eigen::Matrix<double, 3, 3>* r_t;
        /// \brief The fitted ellipse radius matrix.
        Eigen::Matrix<double, 3, 3>* e;
        /// \brief A temporary matrix.
        Eigen::Matrix<double, 1, 3>* t1;
        /// \brief A temporary matrix.
        Eigen::Matrix<double, 1, 3>* t2;
        /// \brief The ellipse equation value.
        Eigen::Matrix<double, 1, 1>* v;
    };
}

#endif