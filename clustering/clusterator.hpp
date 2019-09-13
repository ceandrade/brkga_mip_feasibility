/******************************************************************************
 * clusterator.hpp: Interface for FeasibilityPump decoder.
 *
 * Author: Carlos Eduardo de Andrade
 *         <carlos.andrade@gatech.edu / ce.andrade@gmail.com>
 *
 * (c) Copyright 2015-2019
 *     Industrial and Systems Engineering, Georgia Institute of Technology
 *     All Rights Reserved.
 *
 *  Created on : May 22, 2015 by andrade
 *  Last update: May 26, 2015 by andrade
 *
 * This code is released under LICENSE.md.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 ******************************************************************************/

#ifndef CLUSTERATOR_HPP_
#define CLUSTERATOR_HPP_

#include "plain_ragged_matrix.hpp"
#include "cluster_tree.hpp"

#include <vector>
#include <memory>

// CPLEX and Eigen have a lot of problems with some compile flags.
// So, we just deactivate these flags here.
#include "pragma_diagnostic_ignored_header.hpp"
#include "Eigen/Dense"
#include <cstring>
#include <ilcplex/ilocplex.h>
#include "pragma_diagnostic_ignored_footer.hpp"

namespace ce_andrade {

/**
 * \brief Clusterator
 *
 * \author Carlos Eduardo de Andrade <ce.andrade@gmail.com>
 * \date 2015
 *
 * This class is a clustering machine for variables and constraints
 * from a mixed integer programming. It uses hierarchical clustering to build
 * the relationship between the objects. Since this classes uses large matrices
 * to store the distances between objects, it allocate a lot o memory. It is
 * a really good idea dispose an object of this class after use.
 *
 * \todo Implement clustering for constraints.
 * \todo Implement other types of clusterization.
 *
 */
class Clusterator {
    public:
        /** \name Enumerations */
        //@{
        /// This is the metric used to compute the distances.
        enum class Metric {
            /// This is the L1 norm, as known as Manhattan distance. It is
            /// computed over the features vectors of each object. If the
            /// object is a variable, the feature vector is the incidence
            /// vector of the variable, i.e., it indicates to which constraints
            /// the variable belongs. If the object is a constraint, the
            /// conversely is true.
            L1,

            /// This is the L2 norm, as known as Euclidean distance. The same
            /// observations of L1 hold for L2 too.
            L2,

            /// This is the L1 norm, as known as Manhattan distance. It is
            /// computed over the features vectors of each object. If the
            /// object is a variable, the feature vector represents
            /// the coefficients in the each constraint to which that variable
            /// belongs.
            WEIGHTED_L1,

            /// This is the L2 norm, as known as Euclidean distance. The same
            /// observations of WEIGHTED_L1 hold for WEIGHTED_L2 too.
            WEIGHTED_L2,

            /// This distance/dissimilarity is taken over the number of
            /// constraints (variables) that a pair of variables (constraints)
            /// have in common. We assume that objects that have nothing in
            /// common have distance 1.0. Objects with maximum of
            /// sharings have distance 0.0. In other words, if MAX is the
            /// maximum number of sharings over all pairs of objects, the
            /// distance between objects a and b is
            /// dist(a, b) = 1 - sharing(a, b) / MAX.
            SHARED
        };

        /// Indicates if the clusterization is done using variables or constraints.
        enum class ClusteringObject {
            VARIABLE,   ///< Do variable clustering.
            CONSTRAINT  ///< Do constraint clustering.
        };
        //@}

    public:
        /** \name Constructor and Destructor */
        //@{
        /// Default Constructor.
        Clusterator();

        /// Destructor.
        ~Clusterator();
        //@}

        /** Preprocessing methods */
        //@{

        /** Brief Build the incidence matrices from CPLEX objects. These
         * matrices are used as feature matrices to compute the
         * distance/dissimilarity between the variables or constraints
         * according to user choice. This method also computes the
         * shared_contraints_var_distances matrix.
         *
         * \param variables MIP variables
         * \param constraints MIP constraints
         * \param output_file_preffix write the matrices on files with this
         * preffix. If empty, the matrices are not written.x
         */
        void buildIncidenceMatrices(const IloNumVarArray &variables,
                                    const IloRangeArray &constraints,
                                    std::string output_file_preffix = std::string());
        //@}

        /** Clustering methods */
        //@{
        /** \brief Returns a binary tree describing the relationship
         * between variables or constraints.
         * \param type which type of object is to be clustered.
         * \param metric used to build the distance matrix.
         * \return a tree representing the clustering.
         */
        std::shared_ptr<ClusterTree> hierarchicalClustering(const ClusteringObject type,
                                                            const Metric metric);
        //@}

    private:
        Clusterator(const Clusterator&) = delete;
        Clusterator& operator=(const Clusterator&) = delete;

    protected:
        /** Type definitions */
        //@{
        /// A short name for the incidence matrices.
        typedef Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>
            IncidenceMatrix;

        /// A short name for distance matrices.
        typedef PlainRaggedMatrix<double, IloInt> DistanceMatrix;
        //@}

        /** The matrices */
        //@{
        /// This matrix holds the constraint coefficients. It can see as the
        /// transpose of the original constraint matrix. We store it using
        /// column major scheme.
        IncidenceMatrix weighted_incidence_matrix;

        /// The same as weighted_incidence_matrix, but it is a binary matrix
        /// indicating which variable belongs to each constraint.
        IncidenceMatrix incidence_matrix;

        /// This matrix holds the distance of two variables with respect to
        /// the number of constraints that they appear together.
        /// We assume that variables that not appear together have distance 0.0
        /// within. Variables with maximum of sharings have distance 0.0.
        /// In other words, if MAX is the maximum number of sharings over all
        /// pairs of variables, the distance between variables a and b is
        /// dist(a, b) = 1 - sharing(a, b) / MAX.
        /// This matrix is stored in ragged form used old plain C arrays due
        /// to the C clustering library.
        DistanceMatrix variables_distance;

        /// This matrix holds the distance of two constraints with respect to
        /// the number of variables that they share. We assume that constraints
        /// that have variable in common have distance 0.0 within. Constraints
        /// with maximum of sharings have distance 0.0.
        /// In other words, if MAX is the maximum number of sharings over all
        /// pairs of constraints, the distance between constraints a and b is
        /// dist(a, b) = 1 - sharing(a, b) / MAX.
        /// This matrix is stored in ragged form used old plain C arrays due
        /// to the C clustering library.
        DistanceMatrix constraints_distance;

        /// This is a symmetric matrix with dimensions of number of variables
        /// or constraints depending of the current clustering. This is stored
        /// in ragged form used old plain C arrays due to the C clustering library.
        DistanceMatrix metric_distance;
        //@}

        /** Dimensions */
        //@{
        /// Number of variables.
        IloInt num_vars;

        /// Number of constraints.
        IloInt num_ctrs;
        //@}

        /** Other helper structure */
        //@{
        /// Holds the variable names.
        std::vector<std::string> variable_names;

        /// Holds the constraint names.
        std::vector<std::string> constraint_names;
        //@}

    protected:
        /** Helper functions */
        //@{
        /// Compute the L1 norm distance, aka, Manhattan distance. The results
        /// are written on Clusterator::distance.
        /// <em>THIS METHOD IS VERY TIME CONSUMING</em>.
        /// \todo Find an algebraic form to compute L1 to used the
        ///       Eigen parallel machinery.
        /// \param matrix of features to be used to compute the distances.
        void computeL1Distance(const IncidenceMatrix &matrix);


        /// Compute the L2 norm distance, aka, Euclidean distance. The results
        /// are written on Clusterator::distance. This computation can take
        /// a while due to its nature. This method uses the full advantages of
        /// Eigen library, using as much cores as available in the system.
        /// \param matrix of features to be used to compute the distances.
        void computeL2Distance(const IncidenceMatrix &matrix);
        //@}
};

} //endnamaspace

#endif //CLUSTERATOR_HPP_
