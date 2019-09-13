/******************************************************************************
 * objetive_feasibility_pump.cpp: Implementation of objective feasibility
 * pump as proposed by Achterberg and Berthold (2007). This implementation
 * does not make used of the perturbation step.
 *
 * Author: Carlos Eduardo de Andrade
 *         <carlos.andrade@gatech.edu / ce.andrade@gmail.com>
 *
 * (c) Copyright 2015-2019
 *     Industrial and Systems Engineering, Georgia Institute of Technology
 *     All Rights Reserved.
 *
 *  Created on : Mar 19, 2015 by andrade
 *  Last update: Jun 25, 2015 by andrade
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

#include "feasibility_pump_decoder.hpp"
#include "execution_stopper.hpp"

#include <iostream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <algorithm>
#include <utility>
#include <functional>
#include <numeric>
#include <cmath>
#include <omp.h>

#include "pragma_diagnostic_ignored_header.hpp"
#include <ilcp/cp.h>
#include "pragma_diagnostic_ignored_footer.hpp"

using namespace std;
ILOSTLBEGIN

//----------------------------------------------------------------------------//

double FeasibilityPump_Decoder::objectiveFeasibilityPump(Chromosome& chromosome,
                                                         const double phi,
                                                         const double delta) {
    #ifdef DEBUG
    cout << "\n--------------------------------\n"
         << "> Objective feasibility pump "
         << setiosflags(ios::fixed) << setprecision(6)
         << "\n> Decay (phi): " << phi
         << "\n> Minimum distance (delta): " << delta
         << endl;
    #endif

    #ifdef _OPENMP
    IloEnv &env  = environment_per_thread[omp_get_thread_num()];
    IloObjective &objective = fp_objective_per_thread[omp_get_thread_num()];
    IloObjective &original_objective = original_objective_per_thread[omp_get_thread_num()];
    IloCplex &cplex = cplex_per_thread[omp_get_thread_num()];
    IloBoolVarArray &binary_variables = binary_variables_per_thread[omp_get_thread_num()];
    Chromosome &best_rounding = best_rounding_per_thread[omp_get_thread_num()];
    string &hashstring = hashstring_per_thread[omp_get_thread_num()];
    unordered_map<string, double> &checked_solutions = checked_solutions_per_thread[omp_get_thread_num()];
    IloNumArray &current_values = current_values_per_thread[omp_get_thread_num()];
    IloNumArray &previous_values = previous_values_per_thread[omp_get_thread_num()];
    IloNumArray &rounded_values = rounded_values_per_thread[omp_get_thread_num()];
    MTRand &rng = rng_per_thread[omp_get_thread_num()];
    vector<std::pair<double, IloInt>> &sorted = sorted_per_thread[omp_get_thread_num()];
    unsigned &solved_lps = solved_lps_per_thread[omp_get_thread_num()];
    #else
    IloEnv &env = environment_per_thread[0];
    IloObjective &objective = fp_objective_per_thread[0];
    IloObjective &original_objective = original_objective_per_thread[0];
    IloCplex &cplex = cplex_per_thread[0];
    IloBoolVarArray &binary_variables = binary_variables_per_thread[0];
    Chromosome &best_rounding = best_rounding_per_thread[0];
    string &hashstring = hashstring_per_thread[0];
    unordered_map<string, double> &checked_solutions = checked_solutions_per_thread[0];
    IloNumArray &current_values = current_values_per_thread[0];
    IloNumArray &previous_values = previous_values_per_thread[0];
    IloNumArray &rounded_values = rounded_values_per_thread[0];
    MTRand &rng = rng_per_thread[0];
    vector<std::pair<double, IloInt>> &sorted = sorted_per_thread[0];
    unsigned &solved_lps = solved_lps_per_thread[0];
    #endif

    // Take the last key and use it as the seed for random number generator.
    const MTRand::uint32 local_seed = (MTRand::uint32)(chromosome.back() *
                                                       numeric_limits<MTRand::uint32>::max());
    rng.seed(local_seed);

    const IloInt NUM_BINARIES = binary_variables.getSize();

    // Skip the first rounding, if the chromosome is a rounding.
    bool skip_first_rounding = false;

    for(IloInt i = 0; i < NUM_BINARIES; ++i) {
        best_rounding[i] = chromosome[i];
        current_values[i] = chromosome[i];
        previous_values[i] = rng.rand();

        skip_first_rounding |= current_values[i] > EPS && current_values[i] < 1 - EPS;
    }
    best_rounding.back() = chromosome.back();
    skip_first_rounding = !skip_first_rounding;

    checked_solutions.clear();
    double best_value = numeric_limits<double>::max();
    double best_fractionality = numeric_limits<double>::max();
    unsigned best_violations = numeric_limits<unsigned>::max();
    double alpha = (fp_strategy == PumpStrategy::DEFAULT? 0.0 : 1.0);

    unsigned iteration = 1;
    unsigned iter_without_improvement = 0;

    while(true) {
        #ifdef DEBUG
        cout << "\n\n- Iteration " << iteration
             << "\n- Alpha: " << alpha
             << endl;
        #endif
        #ifdef FULLDEBUG
        cout << "\n- From cplex:\n";
        for(IloInt i = 0; i < NUM_BINARIES; ++i)
            cout << current_values[i] << " ";

        cout << "\n- Last rounding:\n";
        for(IloInt i = 0; i < NUM_BINARIES; ++i)
            cout << previous_values[i] << " ";
        cout << endl;
        #endif

        // First, round the current relaxation

//        if(!skip_first_rounding) {
//            simpleRounding(current_values, rounded_values);
            roundingWithConstraintPropagation(current_values, rounded_values);
//        }
//        else {
//            skip_first_rounding = false;
//        }

        // Let's see if the rounding is the same of that one from previous
        // Iteration. If so, we perform the weak perturbation.
        bool same_as_previous = true;
        for(IloInt i = 0; i < NUM_BINARIES; ++i) {
            if(abs(rounded_values[i] - previous_values[i]) > EPS) {
                same_as_previous = false;
                break;
            }
        }

        //////////////////////////////////
        // Perform the weak perturbation.
        //////////////////////////////////
        sorted.clear();
        if(same_as_previous) {
            if(!fp_params.perturb_when_cycling) {
                #ifdef DEBUG
                cout << "\n\n** Short cycling detected. Stopping..." << endl;
                #endif
                break;
            }

            #ifdef DEBUG
            cout << "\n\n** Short cycling detected. Performing weak perturbation..." << endl;
            #endif

            //sorted.reserve(NUM_BINARIES);
            for(IloInt i = 0; i < NUM_BINARIES; ++i)
                sorted.emplace_back(abs(current_values[i] - rounded_values[i]), i);

            sort(sorted.begin(), sorted.end(), less<pair<double, IloInt>>());

            auto it_end = sorted.begin() + rng.randInt(fp_params.t / 2,
                                                       3 * fp_params.t / 2);
            #ifdef FULLDEBUG
            cout << "\n> Flipping ";
            for(auto it = sorted.begin(); it != it_end; ++it)
                cout << rounded_values[it->second] << " ";
            #endif

            for(auto it = sorted.begin(); it != it_end; ++it)
                rounded_values[it->second] = 1.0 - rounded_values[it->second];

            #ifdef FULLDEBUG
            cout << "\n> To ";
            for(auto it = sorted.begin(); it != it_end; ++it)
                cout << rounded_values[it->second] << " ";
            #endif
        }

        //////////////////////////////////
        // Perform the strong perturbation.
        //////////////////////////////////

        // Let's create the hashstring to look for long cycling.
        for(IloInt i = 0; i < NUM_BINARIES; ++i)
            hashstring[i] = (rounded_values[i] < EPS? '0' : '1');

        // If the solution is already tested, the algorithm is cycling.
        // We may perform the strong perturbation.
        if(checked_solutions.find(hashstring) != checked_solutions.end()  &&
           (checked_solutions[hashstring] - alpha) < delta) {

            if(!fp_params.perturb_when_cycling) {
                #ifdef DEBUG
                cout << "\n\n** Long cycling detected. Stopping..." << endl;
                #endif
                break;
            }

            #ifdef DEBUG
            cout << "\n\n** Long cycling detected at iteration " << iteration
                 << ". Performing strong perturbation..."
                 << "\n^ checked_solutions[hashstring]: " << checked_solutions[hashstring]
                 << "\n^ alpha: " << alpha
                 << "\n^ diff: " << (checked_solutions[hashstring] - alpha)
                 << "\n^ delta: " << delta
                 << endl;
            #endif

            for(IloInt i = 0; i < NUM_BINARIES; ++i) {
                if(abs(rounded_values[i] - current_values[i]) +
                   max(rng.randDblExc(fp_params.rho_ub - fp_params.rho_lb) + fp_params.rho_lb , 0.0)
                   > 0.5) {
                    rounded_values[i] = 1.0 - rounded_values[i];

                    #ifdef FULLDEBUG
                    cout << "\n- " << binary_variables[i]
                         << ": " << (1.0 - rounded_values[i])
                         << " -> " << rounded_values[i];
                    cout.flush();
                    #endif
                }
            }
        }
        else {
            checked_solutions[hashstring] = alpha;
        }

        #ifdef FULLDEBUG
        cout << "\n- Rounded:\n";
        for(IloInt i = 0; i < NUM_BINARIES; ++i)
            cout << rounded_values[i] << " ";
        cout << endl;
        #endif

        //////////////////////////////////
        // Perform the LP projection
        //////////////////////////////////

        // Compute the norm.
        const double local_norm = (fp_strategy == PumpStrategy::DEFAULT? 0.0 :
                                   sqrt(binary_variables.getSize()));

        // Build the new obj function
        double fp_constant = 0.0;
        IloExpr obj_expr(env);
        IloExpr tmp_expr(env);

        for(IloInt i = 0; i < NUM_BINARIES; ++i) {
            if(rounded_values[i] + EPS > binary_variables[i].getUB()) {
                tmp_expr -= binary_variables[i];
                fp_constant += binary_variables[i].getUB();
            }
            else if(rounded_values[i] - EPS < binary_variables[i].getLB()) {
                tmp_expr += binary_variables[i];
                fp_constant += binary_variables[i].getLB();
            }
//            else {
//                // From the original FP source code.
//                throw runtime_error("Hey, we have a binary var that, once rounded, is not at a bound. This shouldn't happen!");
//            }
        }

        fp_constant *= 1 - alpha;

        // Add the original obj function pondered by alpha
        auto orig_expr = original_objective.getExpr();
        obj_expr += (1 - alpha) * tmp_expr  +
                    (alpha * local_norm / c_norm) *
                    ((original_objective.getSense() == IloObjective::Sense::Maximize)?
                     -orig_expr : orig_expr);

        obj_expr.normalize();
        objective.setExpr(obj_expr);
        objective.setSense(IloObjective::Sense::Minimize);
        obj_expr.end();
        tmp_expr.end();

        ++solved_lps;
        if(!cplex.solve()) {
             stringstream message;
             message << "Failed to optimize LP. Status: " << cplex.getStatus();
             throw IloCplex::Exception(cplex.getStatus(), message.str().c_str());
        }

        double dist = 0.0;
        unsigned violations = 0;
        double fractionality = 0.0;

        cplex.getValues(binary_variables, current_values);

        //  Compute the distance, violations, and fractionality.
        for(IloInt i = 0; i < NUM_BINARIES; ++i) {
            #ifdef FULLDEBUG
            cout << current_values[i] << " ";
            #endif

            if(current_values[i] > EPS && current_values[i] < 1 - EPS)
                ++violations;

            dist += abs(current_values[i] - rounded_values[i]);
            fractionality += abs(current_values[i] - floor(current_values[i] + 0.5));
        }

        #ifdef DEBUG
        cout << "\n> Full obj: " << (cplex.getObjValue() + fp_constant)
             << "\n> Origin. obj: " << cplex.getValue(original_objective.getExpr())
             << "\n> Distance (frac/int): " << dist
             << "\n> Fractionality: " << fractionality
             << "\n> Violations: " << violations
             << "\n> Best: ";

        if(best_value < numeric_limits<double>::max() - EPS)
            cout << best_value;
        else
            cout << "inf";
        cout << endl;
        #endif

        // If feasible, copy the integer solution and stop.
        if(violations == 0) {
            #ifdef DEBUG
            cout << "\n\n!!!! Found an integer solution" << endl;
            for(IloInt i = 0; i < NUM_BINARIES; ++i)
                cout << current_values[i] << " ";
            cout << endl;
            #endif

            for(IloInt i = 0; i < NUM_BINARIES; ++i) {
                chromosome[i] = current_values[i];
                chromosome.rounded[i] = (int) current_values[i];
            }

            chromosome.feasibility_pump_value = 0.0;
            chromosome.fractionality = 0.0;
            chromosome.num_non_integral_vars = 0;
            chromosome.num_iterations = iteration;
            return 0.0;
        }

        // Hold the best.
        if(best_value - dist > EPS) {
            best_value = dist;
            best_violations = violations;
            best_fractionality = fractionality;

            for(IloInt i = 0; i < NUM_BINARIES; ++i) {
                best_rounding[i] = current_values[i];
//                best_rounding[i] = rounded_values[i];
                best_rounding.rounded[i] = (int) rounded_values[i];
            }

            iter_without_improvement = 0;
        }
        else {
            ++iter_without_improvement;
        }

        alpha *= phi;

        ++iteration;

        // If we reach the iteration limit or maximum time, we stop.
        if(iter_without_improvement == fp_params.iteration_limit ||
           ExecutionStopper::mustStop())
            break;
    }

    chromosome.feasibility_pump_value = best_value;
    chromosome.fractionality = best_fractionality;
    chromosome.num_non_integral_vars = best_violations;
    chromosome.num_iterations = iteration;

    copy(begin(best_rounding), end(best_rounding), begin(chromosome));
    copy(begin(best_rounding.rounded), end(best_rounding.rounded), begin(chromosome.rounded));

    #ifdef DEBUG
    cout << "\n--------------------------------" << endl;
    #endif

    return best_value;
}
