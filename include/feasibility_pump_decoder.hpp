/******************************************************************************
 * feasibility_pump_decoder.hpp: Interface for FeasibilityPump decoder.
 *
 * Author: Carlos Eduardo de Andrade
 *         <carlos.andrade@gatech.edu / ce.andrade@gmail.com>
 *
 * (c) Copyright 2015-2019
 *     Industrial and Systems Engineering, Georgia Institute of Technology
 *     All Rights Reserved.
 *
 *  Created on : Feb 18, 2015 by andrade
 *  Last update: Ago 21, 2015 by andrade
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

#ifndef FEASIBILITYPUMP_DECODER_HPP_
#define FEASIBILITYPUMP_DECODER_HPP_

#include "chromosome.hpp"
#include "brkga_decoder.hpp"
#include "mtrand.hpp"
#include "population.hpp"

#include <vector>
#include <unordered_map>

// FeasibilityPump has a lot of problems with these flags.
#include "pragma_diagnostic_ignored_header.hpp"
#include <cstring>
#include <ilcplex/ilocplex.h>
#include "fp_interface.h"
#include <boost/timer/timer.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include "pragma_diagnostic_ignored_footer.hpp"

using std::string;
using std::vector;
using std::unordered_map;
using std::pair;
using namespace BRKGA_ALG;

/**
 * \brief FeasibilityPump decoder.
 *
 * \author Carlos Eduardo de Andrade <ce.andrade@gmail.com>
 * \date 2015
 *
 * This class is a BRKGA decoder that aims to find feasible solutions of
 * general mixed integer programming whose feasibility is hard to achieve.
 * The chromosome represents a LP relaxation used to create integer (may be
 * no feasible) solution through rounding. These solution are used as guide
 * in the feasibility pump algorithm.
 */
class FeasibilityPump_Decoder: public BRKGA_Decoder {
    public:
        /** \name Enumerations */
        //@{
        /// Specifies objective as minimization or maximization.
        enum Sense {
            MINIMIZE = -1,  ///< Minimization
            MAXIMIZE = 1    ///< Maximization
        };

        /// The feasibility pump strategy.
        enum class PumpStrategy {
            /// Default feasibility pump using only the distance function.
            DEFAULT,
            /// Feasibility pump using a convex combination between the
            /// distance function and the original objective function.
            OBJECTIVE
        };

        /// Defines which function uses to build the fitness value for
        /// classification of individuals in the BRKGA.
        enum class FitnessType {
            /// Uses the convex combination
            /// \f$\beta \Delta(\tilde{x}, \bar{x}) + (1 - \beta) \zeta\f$
            /// where \f$\Delta(\tilde{x}, \bar{x})\f$ is the distance between
            /// a relaxation and a integer (not necessary feasible) solution,
            /// and \f$\zeta\f$ is the measure of infeasibility, usually the
            /// number of fractional variables.
            /// Note that \f$\beta =\f$ FeasibilityPump_Decoder::minimization_factor.
            CONVEX,
            /// Uses the convex combination
            /// \f$\Delta(\tilde{x}, \bar{x})^\beta \times \zeta^{1 - \beta}\f$
            /// where \f$\Delta(\tilde{x}, \bar{x})\f$ is the distance between
            /// a relaxation and a integer (not necessary feasible) solution,
            /// and \f$\zeta\f$ is the measure of infeasibility, usually the
            /// number of fractional variables.
            /// Note that \f$\beta =\f$ FeasibilityPump_Decoder::minimization_factor.
            GEOMETRIC
        };

        /// Defines the variables to be fixed.
        enum class FixingType {
            /// Fix variables such value is one in the most roundings.
            MOST_ONES,
            /// Fix variables such value is zero in the most roundings.
            MOST_ZEROS,
            /// Fix variables such value is splitted between zeros and ones
            /// among the roundings.
            MOST_FRACTIONALS,
            /// Let the algorithm determines which is supposed to be
            /// the most appropriate. This option only set to MORE_ONES or
            /// MORE_ZEROS. For MOST_FRACTIONALS, the use must set the parameter
            /// manually.
            AUTOMATIC
        };

        /// Defines which constraints consider during the unfix phase on
        /// MIP local search (FeasibilityPump_Decoder::performMIPLocalSearch()).
        /// Note that the variable-constraint relationship matrix os built
        /// in FeasibilityPump_Decoder::init().
        enum class ConstraintFilteringType {
            /// No filtering at all, consider all constraints.
            ALL,
            /// Consider only constraints with non-zero dual value in the LP
            /// relaxation, ordering in non-increasing order of their
            /// absolute value.
            ONLY_NONZERO_DUALS,
            /// Consider only constraints with non-zero dual value in the LP
            /// relaxation or with zero slack values. We order first by
            /// no-increasing order of the absolute dual value and then by
            /// no-decreasing absolute slack values.
            NONZERO_DUALS_NONZERO_SLACKS
        };
        //@}

        /** \name Parameter sets */
        //@{
        /// Common parameters for all feasibility pump methods.
        struct FPparams {
            /// Maximum number of iterations.
            unsigned iteration_limit;

            /// Indicates if a perturbation must be done when FP cycles.
            bool perturb_when_cycling;

            /// Parameter used to control the weak perturbation
            /// in the cycling detection. Usually, the \f$rand(t/2, 3t/2)\f$
            /// most fractional variables are flipped.
            int t;

            /// Parameter used to control the strong perturbation
            /// in the cycling detection. For each binary variable \f$j\f$,
            /// a value \f$\rho \in [\rho_{lb}, \rho_{ub}]\f$ is generated and
            /// if \f$|\bar{x}_j - \tilde{x}_j| + \max\{\rho, 0\} > 0.5\f$,
            /// variable \f$j\f$ is flipped. This is the lower bound.
            double rho_lb;

            /// Parameter used to control the strong perturbation
            /// in the cycling detection. For each binary variable \f$j\f$,
            /// a value \f$\rho \in [\rho_{lb}, \rho_{ub}]\f$ is generated and
            /// if \f$|\bar{x}_j - \tilde{x}_j| + \max\{\rho, 0\} > 0.5\f$,
            /// variable \f$j\f$ is flipped. This is the upper bound.
            double rho_ub;
        };

        /// The objective feasibility pump parameters.
        struct ObjFPparams{
            /// This is the decay factor used to change the objective function
            /// in the LP phase.
            double phi;

            /// This is the minimum difference between two iterations.
            /// This parameter is used to detect cycling.
            double delta;
        };
        //@}

    public:
        /** \name Constructor and Destructor */
        //@{
        /** \brief Default Constructor.
         * \param instance_file LP or MPS instance file.
         * \param num_threads Number of parallel decoding threads.
         * \param seed used in the propagation engine and rounding.
         * \param fp_strategy Defines the feasibility pump to be used.
         * \param fitness_type defines how the fitness is computed.
         * \param minimization_factor used to change the performance measure
         *        during the optimization.
         * \param decay used to modify the minimization_factor.
         * \param fp_params Common parameters for all feasibility pump methods.
         * \param objective_fp_params The objective feasibility pump parameters.
         * \param variable_fixing_percentage defines how many variables is supposed
         *        to be fixed in a given iteration. If zero, the decoder will
         *        try to guess the initial value from the relaxation during init().
         * \param variable_fixing_rate defines the change rate of
         *        variable_fixing_percentage.
         * \param var_fixing_type describes the type of fixing. If automatic,
         *        we try to guess using information from the LP relaxation.
         *        If such case, if the number of variables with value zero in
         *        the relaxations greater than the number of variables with
         *        value one, we will fix more zeros. If the opposite happens,
         *        we will fix more ones.
         * \param constraint_filtering_type defines which constraints consider
         *        on unfix variables during MIP local search.
         * \param discrepancy_level defines the discrepancy level to be used
         *        when fixing variable during MIP local search.
         */
        FeasibilityPump_Decoder(const char* instance_file, const int num_threads,
                                const uint64_t seed,
                                PumpStrategy fp_strategy,
                                FitnessType fitness_type,
                                double minimization_factor = 1.0,
                                double decay = 1.0,
                                FPparams fp_params = {1000, true, 20, -0.3, 0.7},
                                ObjFPparams objective_fp_params = {0.9, 0.0005},
                                double variable_fixing_percentage = 0.0,
                                double variable_fixing_rate = 0.0,
                                FixingType var_fixing_type = FixingType::AUTOMATIC,
                                ConstraintFilteringType constraint_filtering_type =
                                        ConstraintFilteringType::ALL,
                                double discrepancy_level = 0.0);

        /** \brief Destructor. */
        ~FeasibilityPump_Decoder();
        //@}

        /** \name Mandatory Method for BRKGA. */
        //@{
        /** \brief Applies the feasibility pump using the given relaxation
         * as start point.
         *
         * \param[in, out] chromosome A vector of doubles represent a relaxation
         *                 of the binary variables.
         * \param writeback indicates if the chromosome must be rewritten
         * \return a performance measure that depends of the type of feasibility
         * pump used. It can be either the distance between an integral solution
         * and LP feasible relaxation, or a measure of infeasibility, or
         * yet a convex combination of both using. In the last case, let
         * \f$\Delta\f$ be the distance and \f$\zeta\f$ be the measure of
         * infeasibility. Then, the convex combination is
         * \f$\beta\Delta + (1 - \beta)\zeta\f$ where \f$\beta\f$ is a factor
         * in the interval [0,1] (represented by FeasibilityPump_Decoder::minimization_factor).
         */
        virtual double decode(Chromosome& chromosome, bool writeback = true);
        //@}

        /** \name Informational methods. */
        //@{
        /// Return the size of the chromosome to be used.
        inline Chromosome::size_type getChromosomeSize() const {
            return chromosome_size;
        }

        /// Return the sense of the optimization
        inline Sense getSense() const {
            return sense;
        }

        /// Return the total number of variables.
        virtual unsigned getNumVariables() const;

        /// Return the number of binary variables.
        virtual unsigned getNumBinaryVariables() const;

        /// Return the number of constraints.
        virtual unsigned getNumConstraints() const;

        /// Return the worst solution value. May be infeasible.
        virtual double getWorstPrimalValue() const;

        /// Return the percentage of binary variables with value zero in the
        /// LP initial relaxation.
        virtual double getZerosPercentageInInitialRelaxation() const;

        /** \name Public Support methods. */
        //@{
        /** \brief Initialize the data structures and perform the
         * preprocessing. **NOTE**: call before decode any thing.
         */
        virtual void init();

        /** \brief Return a set of several relaxations of binary variables.
         * Note that it is the same size of the chromosome.
         * \param how_many number of relaxations to be done.
         */
        virtual vector<Chromosome> getRelaxBinaryValues(const int how_many);

        /// Change the performance measure considering the minimization factor
        /// and decay: minimizationfactor <- minimizationfactor * decay
        inline void changePerformanceMeasure() {
            minimization_factor *= decay;
        }

        /** \brief Builds a histogram of the alleles of genes from the best
         *  chromosomes and try to fix variables using this information.
         *  The number of fixings is defined by
         * \param population the chromosomes.
         * \param num_chromosomes the number of chromosomes to be considered.
         * \param fixing the fixing type.
         * \param[out] possible_feasible if this method found a feasible solution,
         *      it copies the solution into the parameter. Otherwise,
         *      possible_feasible is not modified. possible_feasible must be
         *      allocated and sized properly.
         * \param[out] num_fixings actual number of fixings performed.
         * \return either true if the fixing is possible or false if the
         *         fixing leads to infeasibility after a short CPLEX probing.
         */
        bool analyzeAndFixVars(const Population& population,
                               const unsigned num_chromosomes,
                               const FixingType fixing,
                               Chromosome& possible_feasible,
                               unsigned& num_fixings);

        /** \brief Insert cuts in the LPs prohibiting the infeasible roundings
         *         from previous iterations.
         * \param population the roundings from the chromosomes
         * \param num_cuts how many cuts to insert. This number must be
         * less or equal to the size of the population. The order of the
         * cuts is given by the fitness ordering of the chromosomes.
         */
        void addCutsFromRoudings(const Population& population,
                                 const unsigned num_cuts);

        /** \brief Perform iterated local search using fixing from the chromosomes.
         *
         * Fix the variables with common value across the given chromosomes.
         * Variables with different values are not fixed. Iteratively,
         * try to solve the model and, if infeasibility is detected, make
         * free variables of violated constraints. Note that this approach is
         * different from variable unfixing from performMIPLocalSearch().
         *
         * \param population the chromosomes.
         * \param num_chromosomes the number of chromosomes to be considered.
         * \param unfix_level controls the recursion on unfix variables.
         *        If zero, no unfix is performed. If 1, all varaibles that
         *        belong to constraints with free variables are unfixed.
         *        If greater than 2, the unfix is done recursively.
         * \param max_time in seconds, to perform the local search.
         * \param[out] possible_feasible if this method found a feasible solution,
         *       it copies the solution into the parameter. Otherwise,
         *       possible_feasible is not modified. possible_feasible must be
         *       allocated and sized properly.
         * \param[out] num_unfixed_vars number of variables not fixed or unfixed
         *       during the call.
         * \return true if found a feasible solution.
         */
        bool performMIPLocalSearch(const Population& population,
                                   const unsigned num_chromosomes,
                                   const unsigned unfix_level,
                                   const double max_time,
                                   Chromosome& possible_feasible,
                                   size_t& num_unfixed_vars);
        //@}

    private:
        /** \name Disabled methods */
        //@{
        FeasibilityPump_Decoder(const FeasibilityPump_Decoder&) = delete;
        FeasibilityPump_Decoder& operator=(const FeasibilityPump_Decoder&) = delete;
        //@}

    public:
        /** Local data structures */
        //@{
        /// Represents a coefficient of a variable given the constraint.
        class ConstraidID_Coefficient_Pair {
            public:
                IloInt ctr_id;  ///< Constraint ID
                IloNum coef;    ///< Coefficient
                ConstraidID_Coefficient_Pair(): ctr_id(0), coef(0.0) {}
                ConstraidID_Coefficient_Pair(IloInt _ctr_id, IloNum _coef):
                    ctr_id(_ctr_id), coef(_coef) {}
        };

        /// Upper and lower bounds of each constraint considering
        /// the domain of continuous variables. This class represents an
        /// interval of possible values to the constraint.
        class UpperLowerBounds {
            public:
                IloNum lb;  ///< Lower bound.
                IloNum ub;  ///< Upper bound.
                UpperLowerBounds(): lb(0.0), ub(0.0) {}
                UpperLowerBounds(IloNum _lb, IloNum _ub):
                    lb(_lb), ub(_ub) {}
        };
        //@}

        /** \name General constant attributes */
        //@{
        /// Instance file.
        const char *instance_file;

        /// Indicate the number of threads used to parallel decoding.
        const int num_threads;

        /// Small constant.
        static const double EPS;
        //@}

        /** \name Control attributes */
        //@{
        /// Defines the feasibility pump to be used.
        const PumpStrategy fp_strategy;

        /// Defines which function uses to build the fitness value for
        /// classification of individuals in the BRKGA.
        const FitnessType fitness_type;

        /// A factor \f$\beta\f$ in the range [0,1]. It is used to control
        /// the direction of the optimization through the convex combination
        /// \f$\beta\Delta + (1 - \beta)\zeta\f$ where
        /// \f$\Delta\f$ is the the distance between a LP feasible and an
        /// integer solution (as in the default feasibility pump), and
        /// \f$\zeta\f$ is the measure of infeasibility, usually the number of
        /// fractional variables. Note the when
        /// minimization_factor is 1.0, only \f$\Delta\f$ is used. When it is
        /// 0.0, only the measure of infeasibility is used.
        double minimization_factor;

        /// The decay factor for FeasibilityPump_Decoder::minimization_factor.
        /// It is used to change the performance measure
        /// during the optimization.
        double decay;

        /// Common parameters for all feasibility pump methods.
        const FPparams fp_params;

        /// The objective feasibility pump parameters.
        const ObjFPparams objective_fp_params;
        //@}

        /** \name Safe thread attributes */
        //@{
        /// CPLEX environment.
        vector<IloEnv> environment_per_thread;

        /// Models.
        vector<IloModel> model_per_thread;

        /// CPLEX algorithms.
        vector<IloCplex> cplex_per_thread;

        /// All variables.
        vector<IloNumVarArray> variables_per_thread;

        /// Maps the CPLEX ID to the index in variables_per_thread.
        unordered_map<IloInt, IloInt> variables_id_index;

        /// Binary variables.
        vector<IloBoolVarArray> binary_variables_per_thread;

        /// Holds the index of each binary var in the vector of all vars.
        vector<size_t> binary_variables_indices;

        /// Maps the CPLEX ID to the index in binary_variables_per_thread.
        unordered_map<IloInt, IloInt> binary_variables_id_index;

        /// Constraints / ranges.
        vector<IloRangeArray> constraints_per_thread;

        vector<IloObjective> original_objective_per_thread;

        /// Objectives for the feasibility pump.
        vector<IloObjective> fp_objective_per_thread;

        /// The CPlEX objects used relax integer/binary variables.
        vector<IloConversion> relaxer_per_thread;

        /// Holds the best variable fixing.
        vector<Chromosome> best_rounding_per_thread;

        /// Strings used to "hash" the vectors and detect cycling.
        vector<string> hashstring_per_thread;

        /// Holds the hash of solution already checked. Used to detect cycling.
        vector<unordered_map<string, double>> checked_solutions_per_thread;

        /// Used to get the values of relaxations.
        vector<IloNumArray> current_values_per_thread;

        /// Used to get the values of relaxations of previous iterations.
        vector<IloNumArray> previous_values_per_thread;

        /// Used to hold the rounded values.
        vector<IloNumArray> rounded_values_per_thread;

        /// Used to perform the perturbation steps. The seeds come from
        // the chromosome itself to ensure reproducibility.
        vector<MTRand> rng_per_thread;

        /// Used to sort the variable during the perturbation phase.
        vector<vector<std::pair<double, IloInt>>> sorted_per_thread;

        /// Used to send fractional values to the rounder.
        vector<vector<double>> frac_fp_per_thread;

        /// Used to get rounded values from the rounder.
        vector<vector<double>> rounded_fp_per_thread;

        /// Original propagation constraint transformer/rounder
        /// from Domenico Salvagnin.
        vector<dominiqs::SolutionTransformerPtr> frac2int_per_thread;

        /// Cuts from infeasible rounndings inserted during the optimization.
        vector<IloConstraintArray> cuts_per_thread;
        //@}

        /** \name Other attributes */
        //@{
        /// Keeps the cuts (and hash values) from roudings the lead to
        /// infeasible solutions. The cuts are associated to the thread 0
        /// CPLEX objects.
        unordered_map<size_t, IloConstraint> rounding_cuts;

        /// Maps the most important constraints to each binary variable.
        vector<vector<IloRange>> constraints_per_variable;

        /// Hold the values of the LP relaxation for all variables
        vector<IloNum> full_relaxation_variable_values;

        /// Dual values for constraints considering the full LP relaxation.
        vector<IloNum> duals;

        /// Slacks values for constraints considering the full LP relaxation.
        vector<IloNum> slacks;

        /// Describes the percentage of binary variables with value
        /// zero in the initial relaxation.
        double percentage_zeros_initial_relaxation;

        /// Describes the percentage of binary variables with value
        /// one in the initial relaxation.
        double percentage_ones_initial_relaxation;

        /// Defines the initial type of variable fixing.
        FixingType var_fixing_type;

        /// The time used to solve the full relaxation.
        boost::timer::cpu_times relaxation_time;

        /// Euclidean norm of \f$c\f$ vector of original objective function
        double c_norm;

        /// Defines how many variables is supposed to be fixed in a
        /// given iteration. This parameter is supposed to be in the
        /// interval [0, 1].
        double variable_fixing_percentage;

        /// Defines the change rate of variable_fixing_percentage. This
        /// parameter is supposed to be in the interval [0, 1].
        double variable_fixing_rate;

        /// Defines the mode of constraint filtering.
        ConstraintFilteringType constraint_filtering_type;

        /// Number of constraints used in the MIP local search after filtering.
        size_t num_constraints_used;

        /// Defines the discrepancy level to be used when fixing variable
        /// during MIP local search. Range [0,1]. For instance, at the
        /// discrepancy level of 0.05, variables such roundings have the same
        /// value in, at least, 95% of roudings, will be fixed to this value.
        /// Value 0.0 represents no tolerance to discrepancy, i.e., all
        /// roundings to a variable must be the same.
        double discrepancy_level;

        /// Holds the original bounds of binary variables after
        /// constraint propagation.
        vector<UpperLowerBounds> binary_variables_bounds;
        //@}

        /** Some statistical data */
        //@{
        /// Accumulates the number of LP solved. We accumulate per thread
        /// to avoid race conditions but sum all in the end.
        vector<unsigned> solved_lps_per_thread;

        /// Indicates is a feasible solution was found before unfix variables
        /// during the local MIP search.
        bool feasible_before_var_unfixing;
        //@}

    protected:
        /** \name Private attributes. */
        //@{
        /// Indicate if the decoder was initialized.
        bool initialized;

        /// The size of the chromosome.
        Chromosome::size_type chromosome_size;

        /// Optimization sense.
        Sense sense;

        // Indicate if the variable is fixed to 0, 1, or is free (-1).
        vector<int8_t> fixed_vars;
        //@}

    public:
        /** \name Feasibility pump variations */
        //@{
        /** \brief This is the objective feasibility pump as proposed by Achterberg
         * and Berthold (2007). This implementation only takes account of
         * Stage 1 which means that it deals only with binary variables.
         * \param[in, out] chromosome A vector of doubles represent a relaxation
         *                 of the binary variables.
         * \param phi this is the decay factor used to change the objective
         *        function in the LP phase.
         * \param delta this is the minimum difference between two iterations.
         *        This parameter is used to detect cycling.
         * \return a performance measure that depends of chosen parameters.
         * It can be the distance between an integral solution and
         * LP feasible relaxation, or a measure of infeasibility.
         */
        double objectiveFeasibilityPump(Chromosome& chromosome,
                                        const double phi,
                                        const double delta);
        //@}

        /** \name Rounding methods */
        //@{
        /** \brief Performs a simple round to the closest interger.
         * \param[in] in the input vector
         * \param[out] out the vector that will hold the rounded values.
         */
        void simpleRounding(const IloNumArray& in, IloNumArray& out);

        /** \brief Performs the rounding using constraint propagation. For each
         * rounded variable, a constraint propagation step is done trying to
         * reduce the number of roundings. If an infesibility is found,
         * the remain variables are rounded in the simple way.
         * \param[in] in the input vector
         * \param[out] out the vector that will hold the rounded values.
         * always is set again to ensure reproducibility.
         */
        void roundingWithConstraintPropagation(const IloNumArray& in,
                                               IloNumArray& out);
        //@}

        /** \name Initialization helper methods */
        //@{
        /// \brief Build FeasibilityPump_Decoder::constraints_per_variable.
        /// data structures.
        void buildConstraint2VariableMatrix();

        /// Determine the variable fixing percentage from trials using the
        /// initial relaxation values.
        void determineFixingPercentage();
        //@}

        /** \name Fixing helper methods */
        //@{
        /** \brief This is a recursive function to fix variables.
         * It works in divide and conquer fashion. First, the function tries to
         * fix all variables in the interval [begin, end]. If it has success,
         * the variables are fixed. If not, the function divide the interval
         * in two blocks and recurse on then. This approach reduces drastically
         * the number of call to CPLEX presolve which is the most consuming
         * time component.
         * \param begin of variable list.
         * \param end of variable list.
         * \param to_be_fixed the list of variables to be fixed.
         * \param histogram frequency of variables' values.
         * \param threshold used to determine the fixing to either 0 or 1.
         * \param[out] old_bounds hold the previous/old variable bounds.
         * \param[out] num_fixings number of variables successful fixed.
         */
        void fixDivideAndConquer(const IloInt begin, const IloInt end,
                                 const vector<pair<float, size_t>>& to_be_fixed,
                                 const vector<int>& histogram,
                                 const double threshold,
                                 vector<UpperLowerBounds>& old_bounds,
                                 unsigned& num_fixings);

        /** \brief This is a linear function to fix variables. In the worst
         * case, when we have several failures on fixing variables, this
         * function is faster than FeasibilityPump_Decoder::fixDivideAndConquer.
         * In the average case, this function can be slower than the last.
         * \param begin of variable list.
         * \param end of variable list.
         * \param to_be_fixed the list of variables to be fixed.
         * \param histogram frequency of variables' values.
         * \param threshold used to determine the fixing to either 0 or 1.
         * \param[out] old_bounds hold the previous/old variable bounds.
         * \param[out] num_fixings number of variables successful fixed.
         */
        void fixPerBlocks(const IloInt begin, const IloInt end,
                          const vector<pair<float, size_t>>& to_be_fixed,
                          const vector<int>& histogram,
                          const double threshold,
                          vector<UpperLowerBounds>& old_bounds,
                          unsigned& num_fixings);
        //@}
};

#endif //FEASIBILITYPUMP_DECODER_HPP_
