/******************************************************************************
 * feasibility_pump_decoder.cpp: Implementation for FeasibilityPump decoder.
 *
 * Author: Carlos Eduardo de Andrade
 *         <carlos.andrade@gatech.edu / ce.andrade@gmail.com>
 *
 * (c) Copyright 2015-2019
 *     Industrial and Systems Engineering, Georgia Institute of Technology
 *     All Rights Reserved.
 *
 *  Created on : Feb 18, 2015 by andrade
 *  Last update: Ago 10, 2015 by andrade
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
#include <unordered_set>
#include <omp.h>

#include "pragma_diagnostic_ignored_header.hpp"
#include <ilcp/cp.h>
#include "cpxutils.h"
#include "fp_interface.h"
#include "transformers.h"
#include "logger.h"
#include "xmlconfig.h"
#include "pragma_diagnostic_ignored_footer.hpp"

using namespace std;
ILOSTLBEGIN

const double FeasibilityPump_Decoder::EPS = 1e-10;

//----------------------------------------------------------------------------//
// Default Constructor and Destructor
//----------------------------------------------------------------------------//

FeasibilityPump_Decoder::FeasibilityPump_Decoder(const char* _instance_file,
                             const int _num_threads, const uint64_t seed,
                             PumpStrategy _fp_strategy,
                             FitnessType _fitness_type,
                             double _minimization_factor,
                             double _decay,
                             FPparams _fp_params,
                             ObjFPparams _objective_fp_params,
                             double _variable_fixing_percentage,
                             double _variable_fixing_rate,
                             FixingType _var_fixing_type,
                             ConstraintFilteringType _constraint_filtering_type,
                             double _discrepancy_level):
        instance_file(_instance_file),
        num_threads(_num_threads),
        fp_strategy(_fp_strategy),
        fitness_type(_fitness_type),
        minimization_factor(_minimization_factor),
        decay(_decay),
        fp_params(_fp_params),
        objective_fp_params({_objective_fp_params.phi,
                             (_fp_strategy == PumpStrategy::DEFAULT)?
                                     1.0 : _objective_fp_params.delta}),
        environment_per_thread(_num_threads),
        model_per_thread(),
        cplex_per_thread(),
        variables_per_thread(),
        variables_id_index(),
        binary_variables_per_thread(),
        binary_variables_indices(),
        binary_variables_id_index(),
        constraints_per_thread(),
        original_objective_per_thread(),
        fp_objective_per_thread(),
        relaxer_per_thread(_num_threads),
        best_rounding_per_thread(_num_threads),
        hashstring_per_thread(_num_threads),
        checked_solutions_per_thread(_num_threads),
        current_values_per_thread(),
        previous_values_per_thread(),
        rounded_values_per_thread(),
        rng_per_thread(_num_threads),
        sorted_per_thread(_num_threads),
        frac_fp_per_thread(_num_threads),
        rounded_fp_per_thread(_num_threads),
        frac2int_per_thread(_num_threads, nullptr),
        cuts_per_thread(),
        rounding_cuts(),
        constraints_per_variable(),
        full_relaxation_variable_values(),
        duals(),
        slacks(),
        percentage_zeros_initial_relaxation(0.0),
        percentage_ones_initial_relaxation(0.0),
        var_fixing_type(_var_fixing_type),
        relaxation_time(),
        c_norm(1.0),
        variable_fixing_percentage(_variable_fixing_percentage),
        variable_fixing_rate(_variable_fixing_rate),
        constraint_filtering_type(_constraint_filtering_type),
        num_constraints_used(0),
        discrepancy_level(_discrepancy_level),
        binary_variables_bounds(),
        solved_lps_per_thread(_num_threads, 0),
        feasible_before_var_unfixing(false),
        initialized(false),
        chromosome_size(0),
        sense(Sense::MINIMIZE),
        fixed_vars()
{
    if(minimization_factor < 0.0 || minimization_factor > 1.0)
        throw runtime_error("minimization_factor must be in the range [0,1].");

    if(objective_fp_params.phi < 0.0 || objective_fp_params.phi > 1.0)
         throw runtime_error("The parameter phi of the objective feasibility pump must be in the range [0,1].");

    if(objective_fp_params.delta < 0.0 || objective_fp_params.delta > 1.0)
         throw runtime_error("The parameter delta of the objective feasibility pump must be in the range [0,1].");

    model_per_thread.reserve(_num_threads);
    cplex_per_thread.reserve(_num_threads);
    variables_per_thread.reserve(_num_threads);
    binary_variables_per_thread.reserve(_num_threads);
    constraints_per_thread.reserve(_num_threads);
    original_objective_per_thread.reserve(_num_threads);
    fp_objective_per_thread.reserve(_num_threads);
    current_values_per_thread.reserve(_num_threads);
    previous_values_per_thread.reserve(_num_threads);
    rounded_values_per_thread.reserve(_num_threads);
    cuts_per_thread.reserve(_num_threads);

    for(auto &env : environment_per_thread) {
        model_per_thread.emplace_back(env);
        cplex_per_thread.emplace_back(env);
        variables_per_thread.emplace_back(env);
        binary_variables_per_thread.emplace_back(env);
        constraints_per_thread.emplace_back(env);
        original_objective_per_thread.emplace_back(env);
        fp_objective_per_thread.emplace_back(env);
        current_values_per_thread.emplace_back(env);
        previous_values_per_thread.emplace_back(env);
        rounded_values_per_thread.emplace_back(env);
        cuts_per_thread.emplace_back(env);
    }

    #ifndef DEBUG
    for(auto &env : environment_per_thread) {
        env.setOut(env.getNullStream());
        env.setWarning(env.getNullStream());
    }
    #endif

    for(auto &cplex : cplex_per_thread) {
        #ifndef DEBUG
        // It is not enough to set the output to a null stream.
        // We must deactivate all logging manually, specially
        // the simplex logging :-(
        cplex.setParam(IloCplex::Param::MIP::Display, 0);
        cplex.setParam(IloCplex::Param::Tune::Display, 0);
        cplex.setParam(IloCplex::Param::Barrier::Display, 0);
        cplex.setParam(IloCplex::Param::Simplex::Display, 0);
        cplex.setParam(IloCplex::Param::Sifting::Display, 0);
        cplex.setParam(IloCplex::Param::Network::Display, 0);
        cplex.setParam(IloCplex::Param::Conflict::Display, 0);
        cplex.setOut(cplex.getEnv().getNullStream());
        cplex.setWarning(cplex.getEnv().getNullStream());
        #endif
        #ifdef FULLDEBUG
        cplex.setParam(IloCplex::Param::MIP::Display, 5);
        cplex.setParam(IloCplex::Param::Conflict::Display, 2);
        #endif

        cplex.setParam(IloCplex::Param::Threads, 1);
        cplex.setParam(IloCplex::Param::RandomSeed, 2700001);
        //cplex.setParam(IloCplex::Param::TimeLimit, 10);
    }

    #ifdef FULLDEBUG
    dominiqs::gLog().open("run.xml", ".");
    #else
    dominiqs::gLog().open("null", "/dev");
    #endif

    dominiqs::gConfig().set<uint64_t>("Globals", "seed", seed);

    // Warm the generators up
    #ifdef _OPENMP
    #pragma omp parallel for num_threads(this->num_threads)
    #endif
    for(int t = 0; t < num_threads; ++t) {
        auto &rng = rng_per_thread[t];
        for(int i = 0; i < 1000; ++i)
            rng.rand();
    }

    sense = Sense::MINIMIZE;
}

//-----------------------------[ Destructor ]---------------------------------//

FeasibilityPump_Decoder::~FeasibilityPump_Decoder() {
    for(auto &env : environment_per_thread)
        env.end();
}

//----------------------------------------------------------------------------//
// Initialize the decoder
//----------------------------------------------------------------------------//

void FeasibilityPump_Decoder::init() {
    #ifdef DEBUG
    cout << "\n------------------------------------------------------"
         << "\n> Initializing the decoder"
         << "\n\n- Loading models...";
    cout.flush();
    #endif

    if(initialized)
        throw runtime_error("Decoder is not supposed to be initialized twice");


    // Register the functors for the propagation engine.
    // We are using the default values here.
    RankerFactory::getInstance().registerClass<FractionalityRanker>("FRAC");
    dominiqs::TransformersFactory::getInstance().registerClass<PropagatorRounding>("propround");

    // Load the model in each algorithm.
    // TODO: this is very ugly! We must find some way more simple.
    for(size_t i = 0; i < model_per_thread.size(); ++i) {
        cplex_per_thread[i].importModel(model_per_thread[i], instance_file,
                                        original_objective_per_thread[i],
                                        variables_per_thread[i],
                                        constraints_per_thread[i]);

        cplex_per_thread[i].extract(model_per_thread[i]);
        frac_fp_per_thread[i].resize(cplex_per_thread[i].getNcols(), 0.0);
        rounded_fp_per_thread[i].resize(cplex_per_thread[i].getNcols(), 0.0);

        // Setup the propagators
        dominiqs::Env m_env = cplex_per_thread[i].getImpl()->getCplexEnv();
        dominiqs::Prob m_lp = cplex_per_thread[i].getImpl()->getCplexLp();

        frac2int_per_thread[i] =
                dominiqs::SolutionTransformerPtr(dominiqs::TransformersFactory::
                                                 getInstance().create("propround"));
        DOMINIQS_ASSERT(frac2int_per_thread[i]);
        frac2int_per_thread[i]->readConfig();

        dominiqs::Model domModel;
        domModel.extract(m_env, m_lp);
        frac2int_per_thread[i]->init(domModel, true);
    }

    // Now, let tight some variable bounds.
    #ifdef DEBUG
    cout << "\n\n> Variables: " << variables_per_thread[0].getSize()
         << "\n> Constraints: " << constraints_per_thread[0].getSize()
         << "\n\nTighting bounds..."; cout.flush();
    #endif

    #ifndef NO_TIGHTING_BOUNDS
    IloCP cp(environment_per_thread[0]);
    cp.setIntParameter(IloCP::RandomSeed, 2700001);

    #ifdef DEBUG
    cp.setIntParameter(IloCP::LogVerbosity, IloCP::ParameterValues::Verbose);
    #endif

    IloModel model_cp(environment_per_thread[0]);
    model_cp.add(model_per_thread[0]);
    cp.extract(model_cp);
    cp.propagate();

    variables_id_index.reserve(variables_per_thread[0].getSize());

    for(IloInt j = 0; j < variables_per_thread[0].getSize(); ++j) {
        auto &var = variables_per_thread[0][j];
        for(int i = 0; i < num_threads; ++i)
            variables_per_thread[i][j].setBounds(cp.getMin(var), cp.getMax(var));

        variables_id_index[var.getId()] = j;
    }
    #endif

    #ifdef DEBUG
    cout << "\n\nFinding binary vars..."; cout.flush();
    #endif

    // Find the binary variables
    vector<size_t> indices;
    indices.reserve(variables_per_thread[0].getSize());

    for(IloInt i = 0; i < variables_per_thread[0].getSize(); ++i) {
        if(variables_per_thread[0][i].getType() == IloNumVar::Bool) {
            for(auto j = 0; j < num_threads; ++j) {
                binary_variables_per_thread[j].add(variables_per_thread[j][i]);
                current_values_per_thread[j].add(0);
                previous_values_per_thread[j].add(0);
                rounded_values_per_thread[j].add(0.5);
            }

            indices.push_back(i);

            #ifdef FULLDEBUG
            cout << "\n" << variables_per_thread[0][i]; cout.flush();
            #endif
        }

        if(variables_per_thread[0][i].getType() == IloNumVar::Int &&
           (variables_per_thread[0][i].getUB() - variables_per_thread[0][i].getLB() > EPS)) {
            stringstream ss;
            ss << "This method does not work with integer variables: "
               << variables_per_thread[0][i];

            throw runtime_error(ss.str());
        }
    }

    binary_variables_id_index.reserve(binary_variables_per_thread[0].getSize());
    for(IloInt i = 0; i < binary_variables_per_thread[0].getSize(); ++i) {
        binary_variables_id_index[binary_variables_per_thread[0][i].getId()] = i;
    }

    const IloInt NUM_BINARIES = binary_variables_per_thread[0].getSize();

    binary_variables_bounds.reserve(NUM_BINARIES);
    for(IloInt i = 0; i < NUM_BINARIES; ++i)
        binary_variables_bounds.emplace_back(binary_variables_per_thread[0][i].getLB(),
                                             binary_variables_per_thread[0][i].getUB());

    binary_variables_indices.resize(NUM_BINARIES);
    copy(indices.begin(), indices.end(), binary_variables_indices.begin());

    for(auto &v : best_rounding_per_thread)
        v.resize(NUM_BINARIES);

    for(auto &hashstring : hashstring_per_thread)
        hashstring.resize(NUM_BINARIES, '0');

    for(auto &v : sorted_per_thread)
        v.reserve(NUM_BINARIES);

    fixed_vars.resize(NUM_BINARIES, -1);

    #ifdef DEBUG
    cout << "\n\nBuilding models for each thread..."; cout.flush();
    #endif
    // Now, let build the models for each thread and extract them with cplex.
    // Not that the objective function will be build during the optimization.
    // We all presolve the models to get better stability when deal with
    // degenerated objective functions.
    #ifdef _OPENMP
    #pragma omp parallel for num_threads(this->num_threads)
    #endif
    for(int i = 0; i < num_threads; ++i) {
        // Now, let's build the projecting engines
        auto &model_tmp = model_per_thread[i];
        model_tmp.remove(original_objective_per_thread[i]);

        IloExpr obj_expr(environment_per_thread[i]);
        obj_expr = IloSum(binary_variables_per_thread[i]);

        obj_expr += (original_objective_per_thread[i].getSense() == IloObjective::Sense::Maximize)?
                    - original_objective_per_thread[i].getExpr() :
                    original_objective_per_thread[i].getExpr();

        fp_objective_per_thread[i].setExpr(obj_expr);
        fp_objective_per_thread[i].setSense(IloObjective::Sense::Minimize);
        obj_expr.end();

        model_tmp.add(variables_per_thread[i]);
        model_tmp.add(constraints_per_thread[i]);
        model_tmp.add(fp_objective_per_thread[i]);

        relaxer_per_thread[i] = IloConversion(environment_per_thread[i],
                                              variables_per_thread[i],
                                              IloNumVar::Float);
        model_tmp.add(relaxer_per_thread[i]);

        cplex_per_thread[i].extract(model_tmp);

        #ifndef NO_PRESOLVE
        cplex_per_thread[i].presolve(IloCplex::Algorithm::AutoAlg);
        #endif
    }

    #ifdef DEBUG
    cout << "\n\nComputing the norm of objective function coefficients: "; cout.flush();
    #endif

    if(fp_strategy == PumpStrategy::DEFAULT) {
           c_norm = 1.0;
    }
    else {
        c_norm = 0.0;
        for(auto it = original_objective_per_thread[0].getLinearIterator(); it.ok(); ++it)
            c_norm = it.getCoef() * it.getCoef();

        c_norm = sqrt(c_norm);
        if(c_norm < EPS)
            c_norm = EPS;
    }
    #ifdef DEBUG
    cout << setiosflags(ios::fixed) << c_norm << endl;
    #endif

    #ifdef DEBUG
    cout << "\n\nSolving initial relaxation and collection some stats..."; cout.flush();
    #endif

    // To not messy with previous structures, let load the model again
    // modify the variables, and than, solve the relaxation.
    auto env = environment_per_thread[0];
    IloModel model(env);
    IloCplex cplex(env);

    #ifndef DEBUG
    cplex.setParam(IloCplex::Param::MIP::Display, 0);
    cplex.setParam(IloCplex::Param::Tune::Display, 0);
    cplex.setParam(IloCplex::Param::Barrier::Display, 0);
    cplex.setParam(IloCplex::Param::Simplex::Display, 0);
    cplex.setParam(IloCplex::Param::Sifting::Display, 0);
    cplex.setParam(IloCplex::Param::Network::Display, 0);
    cplex.setParam(IloCplex::Param::Conflict::Display, 0);
    cplex.setOut(cplex.getEnv().getNullStream());
    #endif

    model.add(variables_per_thread[0]);
    model.add(constraints_per_thread[0]);
    model.add(original_objective_per_thread[0]);
    model.add(IloConversion(env, variables_per_thread[0],
                            IloNumVar::Float));
    cplex.extract(model);

    boost::timer::cpu_timer local_timer;
    local_timer.start();

    cplex.solve();

    local_timer.stop();
    relaxation_time = local_timer.elapsed();

    IloNumArray values(env);
    cplex.getValues(variables_per_thread[0], values);

    percentage_zeros_initial_relaxation = 0.0;
    full_relaxation_variable_values.reserve(values.getSize());

    for(IloInt i = 0; i < values.getSize(); ++i) {
        full_relaxation_variable_values.push_back(values[i]);

        if(variables_per_thread[0][i].getType() == IloNumVar::Bool) {
            if(values[i] < EPS)
                percentage_zeros_initial_relaxation += 1.0;
            else if(values[i] > 1.0 - EPS)
                percentage_ones_initial_relaxation += 1.0;
        }
    }

    percentage_zeros_initial_relaxation /= NUM_BINARIES;
    percentage_ones_initial_relaxation /= NUM_BINARIES;

    if(var_fixing_type == FixingType::AUTOMATIC) {
        if(percentage_zeros_initial_relaxation > percentage_ones_initial_relaxation)
            var_fixing_type = FixingType::MOST_ZEROS;
        else
            var_fixing_type = FixingType::MOST_ONES;
    }

    values.clear();
    cplex.getDuals(values, constraints_per_thread[0]);

    duals.reserve(values.getSize());
    for(IloInt i = 0; i < values.getSize(); ++i)
        duals.push_back(values[i]);

    values.clear();
    cplex.getSlacks(values, constraints_per_thread[0]);
    slacks.reserve(values.getSize());
    for(IloInt i = 0; i < values.getSize(); ++i)
        slacks.push_back(values[i]);

    // Take some relationship between variables and constraints.
    buildConstraint2VariableMatrix();

    if(variable_fixing_percentage < EPS && variable_fixing_percentage > -EPS)
        determineFixingPercentage();

    // Add one more key to be used as seed in each decoding.
    chromosome_size = NUM_BINARIES + 1;
    initialized = true;

    #ifdef DEBUG
    cout << "\n\n- chromosome_size: " << chromosome_size
         << "\n End of initialization"
         << "\n------------------------------------------------------" << endl;
    #endif
}

//----------------------------------------------------------------------------//
// Build constraint-to-variable matrices
//----------------------------------------------------------------------------//

// Preconditions: most of data structures initialized on init() must be
// available.
void FeasibilityPump_Decoder::buildConstraint2VariableMatrix() {
    #ifdef DEBUG
    cout << "\n------------------------------------------------------"
         << "\n> Building constraint-to-variable matrix"
         << "\n> using ";
    switch(constraint_filtering_type) {
    case ConstraintFilteringType::ALL:
        cout << "all constraints";
        break;
    case ConstraintFilteringType::ONLY_NONZERO_DUALS:
        cout << "constraints with non-zero duals";
        break;
    case ConstraintFilteringType::NONZERO_DUALS_NONZERO_SLACKS:
        cout << "constraints with non-zero duals or zero slacks";
        break;
    }
    cout.flush();
    #endif

    vector<size_t> ctr_sorting(constraints_per_thread[0].getSize());
    for(size_t i = 0; i < ctr_sorting.size(); ++i)
        ctr_sorting[i] = i;

    if(constraint_filtering_type != ConstraintFilteringType::ALL)
        sort(ctr_sorting.begin(), ctr_sorting.end(),
             [&](size_t i, size_t j) {
                return (fabs(duals[i]) > fabs(duals[j])) ||
                       (fabs(duals[i] - duals[j]) < EPS &&
                        fabs(slacks[i]) < fabs(slacks[j]));
             });

    #ifdef FULLDEBUG
    for(size_t i = 0; i < ctr_sorting.size(); ++i)
        cout << "\n> Orig: " << constraints_per_thread[0][i].getName()
             << " | dual: " << duals[i] << " | slack: " << slacks[i]
             << " \t->\tSorted: " << constraints_per_thread[0][ctr_sorting[i]].getName()
             << " | dual: " << duals[ctr_sorting[i]]
             << " | slack: " << slacks[ctr_sorting[i]];
    cout << endl;
    #endif

    // First, we check if we have non-zero duals and change the constraint
    // filtering if it is the case. This means that if we have no non-zero
    // dual values, we must change the filtering to consider the slacks.
    if(constraint_filtering_type == ConstraintFilteringType::ONLY_NONZERO_DUALS &&
       fabs(duals[ctr_sorting[0]]) < EPS) {
        constraint_filtering_type = ConstraintFilteringType::NONZERO_DUALS_NONZERO_SLACKS;

        #ifdef FULLDEBUG
        cout << "\n** No non-zero dual values are found. Switching to slacks!";
        #endif
    }

    // Remove some constraints from the list according to the filtering type
    auto it_cut = ctr_sorting.end();
    if(constraint_filtering_type == ConstraintFilteringType::ONLY_NONZERO_DUALS) {
        it_cut = find_if(ctr_sorting.begin(), ctr_sorting.end(),
                         [&](size_t i) {
                             return fabs(duals[i]) < EPS;
                         });
    }
    else if(constraint_filtering_type == ConstraintFilteringType::NONZERO_DUALS_NONZERO_SLACKS) {
        it_cut = find_if(ctr_sorting.begin(), ctr_sorting.end(),
                         [&](size_t i) {
                             return fabs(duals[i]) < EPS && fabs(slacks[i]) > EPS;
                         });
    }

    #ifdef FULLDEBUG
    cout << "\n\n> Orig. size: " << ctr_sorting.size();
    cout << "\n> Back: "
         << constraints_per_thread[0][ctr_sorting.back()].getName()
         << " | dual: " << duals[ctr_sorting.back()]
         << " | slack: " << slacks[ctr_sorting.back()]
         << endl;
    #endif

    ctr_sorting.erase(it_cut, ctr_sorting.end());

    #ifdef FULLDEBUG
    cout << "\n> Reduced size: " << ctr_sorting.size();
    cout << "\n> Back: "
         << constraints_per_thread[0][ctr_sorting.back()].getName()
         << " | dual: " << duals[ctr_sorting.back()]
         << " | slack: " << slacks[ctr_sorting.back()]
         << endl;
    #endif

    // Now, build the constraint-variable matrix
    constraints_per_variable.clear();
    constraints_per_variable.resize(binary_variables_per_thread[0].getSize());

    for(auto &v : constraints_per_variable)
        v.reserve(constraints_per_thread[0].getSize());

    for(auto ctr_idx : ctr_sorting) {
        auto &ctr = constraints_per_thread[0][ctr_idx];
        for(auto it = ctr.getLinearIterator(); it.ok(); ++it) {
            auto var = it.getVar();
            if(var.getType() == IloNumVar::Bool)
                constraints_per_variable[binary_variables_id_index[var.getId()]]
                                         .push_back(ctr);
        }
    }

    for(auto &v : constraints_per_variable)
        v.shrink_to_fit();

    num_constraints_used = ctr_sorting.size();

    #ifdef DEBUG
    cout << "\n------------------------------------------------------" << endl;
    #endif
}

//----------------------------------------------------------------------------//
// Determine fixing percentage
//----------------------------------------------------------------------------//

void FeasibilityPump_Decoder::determineFixingPercentage() {
    #ifdef DEBUG
    cout << "\n------------------------------------------------------\n"
         << "> Trying to determine the variable fixing percentage "
         << endl;
    #endif

    auto &model = model_per_thread[0];
    auto &binary_variables = binary_variables_per_thread[0];
    auto &cplex = cplex_per_thread[0];
    auto &relaxer = relaxer_per_thread[0];

    // First, we undo the previous variable fixing
    const auto NUM_BINARIES = binary_variables.getSize();

    vector<pair<IloNum, size_t>> to_be_fixed(NUM_BINARIES);

    for(IloInt j = 0; j < NUM_BINARIES; ++j) {
        IloNum value = full_relaxation_variable_values[binary_variables_indices[j]];

        switch(var_fixing_type) {
        case FixingType::MOST_ONES:
            if(value < 0.0)
                value = -value;
            break;

        case FixingType::MOST_ZEROS:
            if(value > 0.0)
                value = -value;
            break;

        default:
            value = fabs(value);
            if(value > 0.5)
                value = 1 - value;
            break;
        }

        to_be_fixed[j] = make_pair(value, j);
    }

    sort(begin(to_be_fixed), end(to_be_fixed), greater<pair<IloNum, size_t>>());

    // Now, we start with the all variables in zero, one, or fractional
    // according to var_fixing_type. If the fixing fails, we reduce the size
    // for the first half and continue doing it until we get a feasible
    // fixing. If the fixing falls to one variable only, we set var_fixing
    // to default values (20% in case of zeros, and 5% in case of ones and
    // fractionals).

    // NOTE: note that both MOST_ZEROS and MOST_FRACTIONALS options would
    // work with "i.first < EPS". Both, for numerical representation reasons,
    // this note work im my current developing environment. So, I decide
    // to keep extra code here.
    if(var_fixing_type == FixingType::MOST_ZEROS) {
        to_be_fixed.erase(find_if(to_be_fixed.begin(), to_be_fixed.end(),
                                  [&](const pair<IloNum, size_t>& i) {
                                      return i.first < 0.0;
                                   }),
                                   to_be_fixed.end());
    }
    else if(var_fixing_type == FixingType::MOST_ONES) {
        to_be_fixed.erase(find_if(to_be_fixed.begin(), to_be_fixed.end(),
                                  [&](const pair<IloNum, size_t>& i) {
                                      return i.first < 1.0;
                                   }),
                                   to_be_fixed.end());
    }
    else {
        to_be_fixed.erase(find_if(to_be_fixed.begin(), to_be_fixed.end(),
                                  [&](const pair<IloNum, size_t>& i) {
                                      return i.first < EPS;
                                   }),
                                   to_be_fixed.end());
    }

    // Now, we try to fix blocks of variables until size the right block size.
    model.remove(relaxer);
    cplex.setParam(IloCplex::Param::Threads, num_threads);
    cplex.setParam(IloCplex::Param::Emphasis::MIP, CPX_MIPEMPHASIS_FEASIBILITY);

    // Save the current bounds
    vector<UpperLowerBounds> old_bounds;
    old_bounds.reserve(NUM_BINARIES);
    for(IloInt i = 0; i < NUM_BINARIES; ++i)
        old_bounds.emplace_back(binary_variables[i].getLB(),
                                binary_variables[i].getUB());

    size_t block_size = to_be_fixed.size();
    bool found_size = false;

    while(!found_size && block_size > 2) {
        #ifdef FULLDEBUG
        cout << "\n-- Block size: " << block_size << endl;
        #endif

        size_t begin = 0;
        while(begin != to_be_fixed.size()) {
            size_t end = begin + block_size;
            if(end > to_be_fixed.size())
                end = to_be_fixed.size();

            for(size_t i = begin; i < end; ++i) {
                auto &var = binary_variables[to_be_fixed[i].second];

                // If the var is already fixed, skip it.
                if(fabs(var.getUB() - var.getLB()) > EPS) {
                    // Check the violations. If the variables appears with value 1.0 in
                    // more than 50% of the chromosomes, fix to 1.0. If not, fix to 0.0.
                    const auto value_to_be_fixed =
                        (full_relaxation_variable_values[binary_variables_indices[to_be_fixed[i].second]] > 0.5)? 1.0 : 0.0;
                    var.setBounds(value_to_be_fixed, value_to_be_fixed);
                }
            }

            // Check if this fixing of this block of variables is feasible.
            // NOTE: Yes, I know that this try/catch construction is horrible
            // slow! But, this is the unique way, AFAIK, to deal with the
            // CPLEX presolver.
            try {
                cplex.presolve(IloCplex::NoAlg);
                #ifdef FULLDEBUG
                cout << "- Success in fixing block " << begin << "-" << end << endl;
                #endif

                found_size = true;
                break;
            }
            catch(IloException& e) {
                #ifdef FULLDEBUG
                cout << "- Fail fixing block " << begin << "-" << end << endl;
                #endif

                // Restore the old bounds and go to the next block.
                for(size_t i = begin; i < end; ++i) {
                    const auto &index = to_be_fixed[i].second;
                    binary_variables[index].setBounds(old_bounds[index].lb,
                                                      old_bounds[index].ub);
                }

                begin = end;
            }
        }

        if(!found_size)
            block_size = round(block_size / 2.0);
    }

    variable_fixing_percentage = block_size / (double) NUM_BINARIES;

    #ifdef FULLDEBUG
    cout << "\n> Block size: " << block_size
         << "\n> variable_fixing_percentage: " << (variable_fixing_percentage * 100)
         << endl;
    #endif

    if(block_size == 2) {
        if(var_fixing_type == FixingType::MOST_ZEROS)
            variable_fixing_percentage = 0.20;
        else
            variable_fixing_percentage = 0.05;

        #ifdef DEBUG
        cout << "> Block size to small. Changing variable_fixing_percentage to "
             << (variable_fixing_percentage * 100)
             << endl;
        #endif
    }

    // Recover original settings
    model.add(relaxer);
    cplex.setParam(IloCplex::Param::Threads, 1);

    for(IloInt i = 0; i < NUM_BINARIES; ++i)
        binary_variables[i].setBounds(old_bounds[i].lb, old_bounds[i].ub);


    #ifdef DEBUG
    cout << "\n------------------------------------------------------" << endl;
    #endif
}

//----------------------------------------------------------------------------//
// Informational methods
//----------------------------------------------------------------------------//

unsigned FeasibilityPump_Decoder::getNumVariables() const {
    if(!initialized)
        throw std::runtime_error("Decoder did not initialized");

    return variables_per_thread[0].getSize();
}

unsigned FeasibilityPump_Decoder::getNumBinaryVariables() const {
    if(!initialized)
        throw std::runtime_error("Decoder did not initialized");

    return binary_variables_per_thread[0].getSize();
}

unsigned FeasibilityPump_Decoder::getNumConstraints() const {
    if(!initialized)
        throw std::runtime_error("Decoder did not initialized");

    return constraints_per_thread[0].getSize();
}

// Return the percentage of binary variables with value zero in the LP initial relaxation.
double FeasibilityPump_Decoder::getZerosPercentageInInitialRelaxation() const {
    return percentage_zeros_initial_relaxation;
}

// Just to implement. Really doesn't make sense in this decoder.
double FeasibilityPump_Decoder::getWorstPrimalValue() const {
    return (sense == Sense::MAXIMIZE? numeric_limits<double>::lowest() :
                                      numeric_limits<double>::max());
}

/// Return the value of the relaxation for binary variables.
/// Note that it is the same size of the chromosome.
vector<Chromosome> FeasibilityPump_Decoder::getRelaxBinaryValues(const int how_many) {
    #ifdef DEBUG
    cout << "\n------------------------------------------------------\n"
         << "> Solving initial relaxation and generating "
         << how_many << " relaxations" << endl;
    cout.flags(ios::fixed);
    #endif

    if(!initialized)
        throw std::runtime_error("Decoder did not initialized");

    auto env = environment_per_thread[0];
    const IloInt num_binaries = binary_variables_per_thread[0].getSize();

    vector<Chromosome> relaxations;
    Chromosome relaxation;
    relaxation.reserve(num_binaries + 1);

    for(IloInt i = 0; i < num_binaries; ++i)
        relaxation.push_back(full_relaxation_variable_values[binary_variables_indices[i]]);

    // The seed.
    int seed = 0;
    relaxation.push_back((++seed) / (double) (how_many + 1));

    relaxations.push_back(Chromosome());
    relaxations.back().swap(relaxation);

    if(how_many == 1)
        return relaxations;

    // Generate other relaxations fixing variables. To not messy with previous
    // structures, let load the model again modify the variables, and then,
    // solve the relaxation.
    IloModel model(env);
    IloCplex cplex(env);

    #ifndef DEBUG
    cplex.setParam(IloCplex::Param::MIP::Display, 0);
    cplex.setParam(IloCplex::Param::Tune::Display, 0);
    cplex.setParam(IloCplex::Param::Barrier::Display, 0);
    cplex.setParam(IloCplex::Param::Simplex::Display, 0);
    cplex.setParam(IloCplex::Param::Sifting::Display, 0);
    cplex.setParam(IloCplex::Param::Network::Display, 0);
    cplex.setParam(IloCplex::Param::Conflict::Display, 0);
    cplex.setOut(cplex.getEnv().getNullStream());
    #endif

    model.add(variables_per_thread[0]);
    model.add(constraints_per_thread[0]);
    model.add(original_objective_per_thread[0]);
    model.add(IloConversion(env, variables_per_thread[0],
                            IloNumVar::Float));
    cplex.extract(model);

    auto &sorted = sorted_per_thread[0];
    sorted.clear();

    for(IloInt i = 0; i < num_binaries; ++i) {
        auto value = relaxations[0][i];
        if(value < 0.5)
            value = 1 - value;

        sorted.emplace_back(value, i);
    }

    sort(sorted.begin(), sorted.end(), less<pair<double, IloInt>>());

    auto it_var = sorted.begin();
    int bound = 0;
    IloNumArray values(env);

    for(int i = 1; (i < how_many) && (it_var != sorted.end()); ++i) {
        binary_variables_per_thread[0][it_var->second].setBounds(bound, bound);
        cplex.solve();
        cplex.getValues(binary_variables_per_thread[0], values);

        relaxation.reserve(num_binaries + 1);
        for(IloInt j = 0; j < values.getSize(); ++j)
            relaxation.push_back(values[j]);

        relaxation.push_back((++seed) / (double) (how_many + 1));

        relaxations.push_back(Chromosome());
        relaxations.back().swap(relaxation);

        bound = 1 - bound;

        if(bound == 0) {
            binary_variables_per_thread[0][it_var->second].setBounds(0, 1);
            ++it_var;
        }
    }

    // Just to ensure the the bounds of binary variables are right.
    for(IloInt i = 0; i < binary_variables_per_thread[0].getSize(); ++i)
        binary_variables_per_thread[0][i].setBounds(0, 1);

    #ifdef DEBUG
    #ifdef FULLDEBUG
    cout << "\n Relaxed binary vars: ";
    for(size_t i = 0; i < relaxations.size(); ++i) {
        cout << "\n\n[" << i << "]: ";
        for(auto &v : relaxations[i])
            cout << setprecision(2) << v << " ";
    }
    #endif
    cout << "\n------------------------------------------------------" << endl;
    #endif

    return relaxations;
}

//----------------------------------------------------------------------------//
// Decode
//----------------------------------------------------------------------------//

double FeasibilityPump_Decoder::decode(Chromosome& chromosome, bool /*writeback*/) {
    #ifdef DEBUG
    cout << "\n------------------------------------------------------\n"
         << "> Decoding chromosome " << endl;

    #ifdef FULLDEBUG
    for(const auto &key : chromosome)
        cout << key << " ";
    cout << endl;
    #endif
    #endif

    if(!initialized)
        throw std::runtime_error("Decoder did not initialized");

    objectiveFeasibilityPump(chromosome, objective_fp_params.phi,
                             objective_fp_params.delta);

    #ifdef DEBUG
    cout << "\n--------------------------------\n"
         << "> Computing infeasibility"
         << endl;
    #endif

    double performance;

    if(fitness_type == FitnessType::CONVEX)
        performance = (minimization_factor * chromosome.feasibility_pump_value) +
                      ((1 - minimization_factor) * chromosome.num_non_integral_vars);
    else
        performance = pow(chromosome.feasibility_pump_value, minimization_factor) *
                      pow(chromosome.num_non_integral_vars, 1 - minimization_factor);

    #ifdef DEBUG
    cout << "\n\n> Num. of non-integral vars: " << chromosome.num_non_integral_vars
         << setiosflags(ios::fixed) << setprecision(2)
         << " (" << (((double)chromosome.num_non_integral_vars / getNumBinaryVariables()) * 100) << "%)"
         << "\n> Delta value: " << chromosome.feasibility_pump_value
         << "\n> Performance: " << performance;
    cout << "\n------------------------------------------------------" << endl;
    #endif

    return performance;
}

//----------------------------------------------------------------------------//
// Analyze and fix vars
//----------------------------------------------------------------------------//

bool FeasibilityPump_Decoder::analyzeAndFixVars(const Population& population,
                                                const unsigned num_chromosomes,
                                                const FixingType fixing,
                                                Chromosome& possible_feasible,
                                                unsigned& num_fixings) {
    #ifdef DEBUG
    cout << "\n------------------------------------------------------\n"
         << "> Analyzing and fixing variables... "
         << "\n\n* Building the histogram from " << num_chromosomes
         << " roundings (chromosomes)"
         << endl;
    #endif

    auto &model = model_per_thread[0];
    auto &binary_variables = binary_variables_per_thread[0];
    auto &cplex = cplex_per_thread[0];
    auto &relaxer = relaxer_per_thread[0];
    auto &original_objective = original_objective_per_thread[0];
    auto &fp_objective = fp_objective_per_thread[0];

    // First, we undo the previous variable fixing
    const auto NUM_BINARIES = binary_variables.getSize();

    for(int i = 0; i < num_threads; ++i)
        for(IloInt j = 0; j < NUM_BINARIES; ++j)
            binary_variables_per_thread[i][j]
                         .setBounds(binary_variables_bounds[j].lb,
                                    binary_variables_bounds[j].ub);

    fill(fixed_vars.begin(), fixed_vars.end(), -1);

    // Now, build the histogram.
    vector<int> histogram(population.getN(), 0);
    vector<UpperLowerBounds> old_bounds(population.getN());

    for(unsigned i = 0; i < num_chromosomes; ++i) {
        auto &chr = population.getChromosome(i);
        for(size_t j = 0; j < population.getN(); ++j)
            histogram[j] += chr.rounded[j];
    }

    vector<pair<float, size_t>> to_be_fixed(NUM_BINARIES);
    for(IloInt j = 0; j < NUM_BINARIES; ++j) {
        float value;

        switch(fixing) {
        case FixingType::MOST_ONES:
            value = histogram[j];
            break;

        case FixingType::MOST_ZEROS:
            value = -histogram[j];
            break;

        default:
            value = histogram[j] / (float) num_chromosomes;
            if(value > 0.5)
                value = 1 - value;
            break;
        }

        to_be_fixed[j] = make_pair(value, j);
    }

    sort(begin(to_be_fixed), end(to_be_fixed), greater<pair<float, size_t>>());

    #ifdef FULLDEBUG
    cout << "> Sorted vars: ";
    for(auto &value_index : to_be_fixed)
        cout << value_index.first << " (" << value_index.second << ") ";
    #endif

    const IloInt num_to_fix = floor(NUM_BINARIES * variable_fixing_percentage);

    #ifdef FULLDEBUG
    cout << "\n> Trying to fix " << num_to_fix << " variables"; cout.flush();
    #endif

    // Remove the objective function and the relaxer.
    model.remove(fp_objective);
    model.remove(relaxer);
    cplex.setParam(IloCplex::Param::Threads, num_threads);

    num_fixings = 0;

//    fixDivideAndConquer(0, num_to_fix, to_be_fixed, histogram,
//                        num_chromosomes / 2.0, old_bounds, num_fixings);
    fixPerBlocks(0, num_to_fix, to_be_fixed, histogram, num_chromosomes / 2.0,
                 old_bounds, num_fixings);

    // Run CPLEX for short time to early infeability detection.
    model.add(original_objective);
    cplex.setParam(IloCplex::Param::TimeLimit, 10.0);

    #ifdef FULLDEBUG
    environment_per_thread[0].setOut(cout);
    cplex.setParam(IloCplex::Param::MIP::Display, 4);
    #endif

    cplex.solve();

    // If infeasible, unfix vars and return fail.
    bool worked = true;
    if(cplex.getStatus() == IloAlgorithm::Infeasible) {
        for(IloInt j = 0; j < NUM_BINARIES; ++j)
            binary_variables[j].setBounds(binary_variables_bounds[j].lb,
                                          binary_variables_bounds[j].ub);
        worked = false;
        #ifdef DEBUG
        cout << "\n* After CPLEX probing: " << cplex.getCplexStatus() << endl;
        #endif
    }

    // If cplex cannot find anything, fix the vars and return success.
    if(cplex.getStatus() == IloAlgorithm::Unknown) { // &&
//       cplex.getCplexStatus() == IloCplex::AbortTimeLim) {

        for(int i = 0; i < num_threads; ++i)
            for(IloInt j = 0; j < NUM_BINARIES; ++j)
                binary_variables_per_thread[i][j]
                             .setBounds(binary_variables[j].getLB(),
                                        binary_variables[j].getUB());

        for(IloInt j = 0; j < NUM_BINARIES; ++j) {
            if(binary_variables[j].getLB() < binary_variables[j].getUB())
                fixed_vars[j] = -1;
            else if(binary_variables[j].getLB() < EPS)
                fixed_vars[j] = 0;
            else
                fixed_vars[j] = 1;
        }

        worked = true;
        #ifdef DEBUG
        cout << "\n* After CPLEX probing: " << cplex.getCplexStatus() << endl;
        #endif
    }

    // If we found a feasible solution, yey!!! Just report it to stop.
    if(cplex.getStatus() == IloAlgorithm::Feasible ||
       cplex.getStatus() == IloAlgorithm::Optimal) {

        cplex.getValues(binary_variables, current_values_per_thread[0]);
        for(IloInt i = 0; i < NUM_BINARIES; ++i) {
            possible_feasible[i] = current_values_per_thread[0][i];
            possible_feasible.rounded[i] = (int) current_values_per_thread[0][i];
        }

        possible_feasible.feasibility_pump_value = 0.0;
        possible_feasible.fractionality = 0.0;
        possible_feasible.num_non_integral_vars = 0;
        worked = true;
        #ifdef DEBUG
        cout << "\n* After CPLEX probing: " << cplex.getStatus() << "!!!" << endl;
        #endif
    }

    // Add back the FP obj. function and the relaxation transformation.
    model.remove(original_objective);
    model.add(fp_objective);
    model.add(relaxer);

    cplex.setParam(IloCplex::Param::TimeLimit, 1e+75);
    cplex.setParam(IloCplex::Param::Threads, 1);

    #ifdef DEBUG
    cplex.setParam(IloCplex::Param::MIP::Display, 0);
    environment_per_thread[0].setOut(environment_per_thread[0].getNullStream());
    cout << "\n--------------------------------\n" << endl;
    #endif
    return worked;
}

//----------------------------------------------------------------------------//
// Fix Divide-and-Conquer
//----------------------------------------------------------------------------//

void FeasibilityPump_Decoder::fixDivideAndConquer(const IloInt begin, const IloInt end,
          const vector<pair<float, size_t>>& to_be_fixed, const vector<int>& histogram,
          const double threshold, vector<UpperLowerBounds>& old_bounds, unsigned& num_fixings) {

    auto &binary_variables = binary_variables_per_thread[0];
    auto &cplex = cplex_per_thread[0];

    #ifdef FULLDEBUG
    static int padding = 1;
    string str_padding(4 * padding, ' ');
    cout << "\n" << str_padding << "Begin: " << begin
         << "\n" << str_padding << "End: " << end;
    for(IloInt i = begin; i < end; ++i)
        cout << "\n" << str_padding
             << to_be_fixed[i].second << ": " << binary_variables[i];
    cout << endl;
    #endif

    for(IloInt i = begin; i < end; ++i) {
        const auto &index = to_be_fixed[i].second;
        auto &var = binary_variables[index];

        // Save original bounds.
        old_bounds[index].lb = var.getLB();
        old_bounds[index].ub = var.getUB();

        // If the var is already fixed, skip it.
        if(fabs(var.getUB() - var.getLB()) > EPS) {
            // Check the violations. If the variables appears with value 1.0 in
            // more than 50% of the chromosomes, fix to 1.0. If not, fix to 0.0.
            const auto value_to_be_fixed =
                (histogram[index] >= threshold)? 1.0 : 0.0;
            var.setBounds(value_to_be_fixed, value_to_be_fixed);
        }
    }

    // Check if this fixing of this block of variables is feasible.
    // NOTE: Yes, I know that this try/catch construction is horrible
    // slow! But, this is the unique way, AFAIK, to deal with the
    // CPLEX presolver.
    bool recurse = false;
    try {
        cplex.presolve(IloCplex::NoAlg);
        #ifdef FULLDEBUG
        cout << ">> Success in fixing block " << begin << "-"
             << end << endl;
        #endif

        num_fixings += end - begin;
    }
    catch(IloException& e) {
        #ifdef FULLDEBUG
        cout << ">> Fail fixing block " << begin << "-" << end << endl;
        #endif

        // Restore the old bounds and recurse.
        for(IloInt i = begin; i < end; ++i) {
            const auto &index = to_be_fixed[i].second;
            binary_variables[index].setBounds(old_bounds[index].lb,
                                              old_bounds[index].ub);
        }
        recurse = true;
    }

    if(begin < end - 1 && recurse)
        fixDivideAndConquer(begin, (begin + end) / 2, to_be_fixed, histogram,
                            threshold, old_bounds, num_fixings);

    if(begin < end - 1 && recurse)
        fixDivideAndConquer((begin + end) / 2, end, to_be_fixed, histogram,
                            threshold, old_bounds, num_fixings);

    // We have just one variable such first fixing failed . So, we
    // try to fix it to the opposite value.
    if(begin == end - 1 && recurse) {
        const auto &index = to_be_fixed[begin].second;
        auto &var = binary_variables[index];

        const auto value_to_be_fixed =
            (histogram[index] >= threshold)? 0.0 : 1.0;

        var.setBounds(value_to_be_fixed, value_to_be_fixed);

        try {
            cplex.presolve(IloCplex::NoAlg);
            #ifdef FULLDEBUG
            cout << ">> Success in fixing variable  " << var
                 << " to the opposite value." << endl;
            #endif
            ++num_fixings;
        }
        catch(IloException& e) {
            #ifdef FULLDEBUG
            cout << ">> Fail in fixing variable  " << var
                 << " to the opposite value." << endl;
            #endif

            // Restore the old bounds and recurse.
            var.setBounds(old_bounds[index].lb, old_bounds[index].ub);
        }
    }
    #ifdef FULLDEBUG
    --padding;
    #endif
}

//----------------------------------------------------------------------------//
// Fix per blocks
//----------------------------------------------------------------------------//

void FeasibilityPump_Decoder::fixPerBlocks(const IloInt begin, const IloInt end,
      const vector<pair<float, size_t>>& to_be_fixed, const vector<int>& histogram,
      const double threshold, vector<UpperLowerBounds>& old_bounds, unsigned& num_fixings) {

    const IloInt BLOCK_SIZE = 8;
    auto &binary_variables = binary_variables_per_thread[0];
    auto &cplex = cplex_per_thread[0];

    #ifdef FULLDEBUG
    cout << "\n Begin: " << begin
         << "\n End: " << end;
    for(IloInt i = begin; i < end; ++i)
        cout << "\n" << to_be_fixed[i].second << ": " << binary_variables[i];
    cout << endl;
    #endif

    auto begin_block = begin;
    while(begin_block != end) {
        auto end_block = begin_block + BLOCK_SIZE;

        if(end_block > end)
            end_block = end;

        for(IloInt i = begin_block; i < end_block; ++i) {
            const auto &index = to_be_fixed[i].second;
            auto &var = binary_variables[index];

            // Save original bounds.
            old_bounds[index].lb = var.getLB();
            old_bounds[index].ub = var.getUB();

            // If the var is already fixed, skip it.
            if(fabs(var.getUB() - var.getLB()) > EPS) {
                // Check the violations. If the variables appears with value 1.0 in
                // more than 50% of the chromosomes, fix to 1.0. If not, fix to 0.0.
                const auto value_to_be_fixed =
                    (histogram[index] >= threshold)? 1.0 : 0.0;
                var.setBounds(value_to_be_fixed, value_to_be_fixed);
            }
        }

        // Check if this fixing of this block of variables is feasible.
        // NOTE: Yes, I know that this try/catch construction is horrible
        // slow! But, this is the unique way, AFAIK, to deal with the
        // CPLEX presolver.
        bool one_by_one = false;
        try {
            cplex.presolve(IloCplex::NoAlg);
            #ifdef FULLDEBUG
            cout << ">> Success in fixing block " << begin_block << "-" << end_block << endl;
            #endif

            num_fixings += end_block - begin_block;
        }
        catch(IloException& e) {
            #ifdef FULLDEBUG
            cout << ">> Fail fixing block " << begin_block << "-" << end_block << endl;
            #endif

            // Restore the old bounds and fix one-by-one
            for(IloInt i = begin_block; i < end_block; ++i) {
                const auto &index = to_be_fixed[i].second;
                binary_variables[index].setBounds(old_bounds[index].lb,
                                                  old_bounds[index].ub);
            }
            one_by_one = true;
        }

        // If the block fixing failed, we try to fix one variable
        // at the time
        if(one_by_one) {
            #ifdef FULLDEBUG
            cout << ">> Fixing one-by-one" << endl;
            #endif

            for(IloInt i = begin_block; i < end_block; ++i) {
                const auto &index = to_be_fixed[i].second;
                auto &var = binary_variables[index];

                // Save original bounds.
                old_bounds[index].lb = var.getLB();
                old_bounds[index].ub = var.getUB();

                // If the var is already fixed, skip it.
                if(fabs(var.getUB() - var.getLB()) > EPS) {
                    auto value_to_be_fixed =
                        (histogram[index] >= threshold)? 1.0 : 0.0;

                    bool fail = true;
                    for(int k = 0; k < 2; ++k) {
                        var.setBounds(value_to_be_fixed, value_to_be_fixed);
                        try {
                            cplex.presolve(IloCplex::NoAlg);
                            #ifdef FULLDEBUG
                            cout << ">> Success in fixing variable  " << var
                                 << " to " << value_to_be_fixed << endl;
                            #endif
                            ++num_fixings;
                            fail = false;
                            break;
                        }
                        catch(IloException& e) {
                            #ifdef FULLDEBUG
                            cout << ">> Fail in fixing variable  " << var
                                 << " to " << value_to_be_fixed << endl;
                            #endif
                            value_to_be_fixed = 1 - value_to_be_fixed;
                        }
                    }

                    // Restore the old bounds and recurse.
                    if(fail)
                        var.setBounds(old_bounds[index].lb, old_bounds[index].ub);
                } // endif
            } // endfor
        } //endif one_by_one

        begin_block = end_block;
    } // endwhile
}

//----------------------------------------------------------------------------//
// Add cuts from roudings
//----------------------------------------------------------------------------//

void FeasibilityPump_Decoder::addCutsFromRoudings(const Population& population,
                                                  const unsigned num_cuts) {
    #ifdef DEBUG
    cout << "\n------------------------------------------------------\n"
         << "> Adding " <<  num_cuts << " cuts from roundings..."
         << endl;
    #endif

    if(num_cuts > population.getP()) {
        stringstream ss;
        ss << "Num. of cuts (" << num_cuts << ") larger than the population ("
           << population.getP() << ")";
        throw runtime_error(ss.str());
    }

    // First, we undo the previous variable fixing
    const auto NUM_BINARIES = binary_variables_per_thread[0].getSize();

    vector<IloExpr> exps_per_thread(num_threads);

    for(unsigned i = 0; i < num_cuts; ++i) {
        const auto &chr = population.getChromosome(i);

        #ifdef FULLDEBUG
        IloExpr tmp(environment_per_thread[0]);
        int accum_tmp = 0;
        for(IloInt j = 0; j < NUM_BINARIES; ++j) {
            if(chr.rounded[j] == 1) {
                tmp += binary_variables_per_thread[0][j];
            }
            else {
                tmp -= binary_variables_per_thread[0][j];
                ++accum_tmp;
            }
        }
        cout << "\n\n- Trying cut " << IloConstraint(tmp <= NUM_BINARIES - 1 - accum_tmp);
        cout.flush();
        tmp.end();
        #endif

        // Hashing the rounding
        size_t hash_value = 0;
        for(size_t k = 0; k < (size_t)NUM_BINARIES; ++k) {
            if(chr.rounded[k] == 1)
                hash_value ^= k + 0x9e3779b9 + (hash_value << 6) + (hash_value >> 2);
        }

        if(rounding_cuts.find(hash_value) != rounding_cuts.end()) {
            #ifdef FULLDEBUG
            cout << "\n- Already taken. Skipped"; cout.flush();
            #endif
            continue;
        }

        for(int t = 0; t < num_threads; ++t)
            exps_per_thread[t] = IloExpr(environment_per_thread[t]);

        int accum = 0;
        for(IloInt j = 0; j < NUM_BINARIES; ++j) {
            if(chr.rounded[j] == 1) {
                for(int t = 0; t < num_threads; ++t)
                    exps_per_thread[t] += binary_variables_per_thread[t][j];
            }
            else {
                for(int t = 0; t < num_threads; ++t)
                    exps_per_thread[t] -= binary_variables_per_thread[t][j];
                ++accum;
            }
        }

        for(int t = 0; t < num_threads; ++t)
            cuts_per_thread[t].add(exps_per_thread[t] <= NUM_BINARIES - 1 - accum);

        rounding_cuts[hash_value] = cuts_per_thread[0][cuts_per_thread[0].getSize() - 1];

        for(auto &expr : exps_per_thread)
            expr.end();
    }

    // Adding the cuts.
    for(int t = 0; t < num_threads; ++t) {
        model_per_thread[t].add(cuts_per_thread[t]);
        cuts_per_thread[t].clear();
    }

    #ifdef DEBUG
    cout << "\n--------------------------------\n" << endl;
    #endif
}

//----------------------------------------------------------------------------//
// Perform MIP Local Search
//----------------------------------------------------------------------------//

// Call when find a feasible solution.
ILOINCUMBENTCALLBACK0(StopWhenFindFeasibleCallback) {
    if(hasIncumbent() || ExecutionStopper::mustStop())
        abort();
}

ILOMIPINFOCALLBACK0(StopCtrlCorTimeCallback) {
    if(ExecutionStopper::mustStop())
        abort();
}

bool FeasibilityPump_Decoder::performMIPLocalSearch(
        const Population& population, const unsigned num_chromosomes,
        const unsigned unfix_level, const double max_time,
        Chromosome& possible_feasible, size_t& num_unfixed_vars) {

    #ifdef DEBUG
    cout << "\n------------------------------------------------------\n"
         << "> Performing MIP local search (time: "
         << setiosflags(ios::fixed)
         << setprecision(2)
         << max_time << ")"
         << endl;
    #endif

    auto &env = environment_per_thread[0];
    auto &cplex = cplex_per_thread[0];
    auto &model = model_per_thread[0];
    auto &binary_variables = binary_variables_per_thread[0];
    auto &constraints = constraints_per_thread[0];
    auto &relaxer = relaxer_per_thread[0];
    auto &original_objective = original_objective_per_thread[0];
    auto &fp_objective = fp_objective_per_thread[0];

    const auto NUM_BINARIES = binary_variables.getSize();
    size_t num_fixed_vars = 0;

    // Now, build the histogram.
    vector<int> histogram(NUM_BINARIES, 0);

    // Indicate if the variable is fixed to 0, 1, or is free (-1).
    vector<int8_t> local_fixed(NUM_BINARIES, -1);

    for(unsigned i = 0; i < num_chromosomes; ++i) {
        auto &chr = population.getChromosome(i);
        for(IloInt j = 0; j < NUM_BINARIES; ++j)
            histogram[j] += chr.rounded[j];
    }

    for(IloInt var_idx = 0; var_idx < NUM_BINARIES; ++var_idx) {
        auto value = histogram[var_idx] / (double) num_chromosomes;
        if(value < (discrepancy_level + EPS) || value > (1- discrepancy_level - EPS)) {
            value = round(value);
            binary_variables[var_idx].setBounds(value, value);
            local_fixed[var_idx] = int8_t(value);
            ++num_fixed_vars;
        }
        else
            binary_variables[var_idx].setBounds(binary_variables_bounds[var_idx].lb,
                                                binary_variables_bounds[var_idx].ub);
    }

    num_unfixed_vars = NUM_BINARIES - num_fixed_vars;

    #ifdef DEBUG
    cout << "\n** After fixing..."
         << "\n** Num of fixed variables: "
         << num_fixed_vars << " / " << NUM_BINARIES
         << " (" << ((100.0 * num_fixed_vars) / NUM_BINARIES)
         << "%)"
         << "\n** Num of free variables: "
         << num_unfixed_vars << " / " << NUM_BINARIES
         << " (" << ((100.0 * num_unfixed_vars) / NUM_BINARIES)
         << "%)\n"
         << endl;
    #endif

    /////////////////////////////////
    // Checking violated constraints.
    /////////////////////////////////

    for(IloInt i = 0; i < constraints.getSize(); ++i) {
        if(ExecutionStopper::mustStop())
            break;

        auto &ctr = constraints[i];
        IloNum fixed_contribution = 0.0;
        IloNum positive_contribution = 0.0;
        IloNum negative_contribution = 0.0;

        size_t hash_value = 0;  // Used to identify cut already taken

        for(auto it = ctr.getLinearIterator(); it.ok(); ++it) {
            auto var = it.getVar();
            auto value = it.getCoef();

            if(var.getType() == IloNumVar::Bool) {
                const auto var_index = binary_variables_id_index[var.getId()];

                if(local_fixed[var_index] == 1) {
                    fixed_contribution += value;
                    hash_value ^= (size_t)var_index + 0x9e3779b9 +
                                  (hash_value << 6) + (hash_value >> 2);
                }
                else
                if(local_fixed[var_index] == -1) {
                    if(value > 0)
                        positive_contribution += value;
                    else
                        negative_contribution += value;
                }
            }
            else {
                if(value > 0)
                    positive_contribution += value;
                else
                    negative_contribution += value;
            }
        }

        // Let's check for violation in each type of constraint
        char constraint_type;

        // <= inequalities
        if(ctr.getLB() == -IloInfinity && ctr.getUB() < IloInfinity) {
            constraint_type = 'l';
        }
        // >= inequalities
        else if(ctr.getLB() > -IloInfinity && ctr.getUB() == IloInfinity) {
            constraint_type = 'g';
        }
        // equalities
        else if(fabs(ctr.getUB() - ctr.getLB()) < EPS) {
            constraint_type = 'e';
        }
        // Oops, this constraint is a range and we will not handle that for now.
        else {
            stringstream ss;
            ss << "performIteratedMIPLocalSearch: found a strange constraint: "
               << ctr;
            throw runtime_error(ss.str());
        }

        // First <= inequalities
        bool violated = (constraint_type == 'l') &&
                        (fixed_contribution + negative_contribution > ctr.getUB());

        // Now >= inequalities
        if(!violated && constraint_type == 'g')
            violated = fixed_contribution + positive_contribution < ctr.getLB();

        // Finally, = equalities
        if(!violated && constraint_type == 'e') {
            auto surplus = fixed_contribution - ctr.getUB();
            violated = surplus < 0? (surplus + positive_contribution < 0) :
                                    (surplus + negative_contribution > 0);
        }

        #ifdef FULLDEBUG
        cout << "\n** " << ctr
             << "\n> ctr.getLB: " << ctr.getLB()
             << "\n> ctr.getUB: " << ctr.getUB()
             << "\n- fixed_contribution: " << fixed_contribution
             << "\n- positive_contribution: " << positive_contribution
             << "\n- negative_contribution: " << negative_contribution
             << "\n violated: " << (violated? "yes" : "no")
             << endl;
        #endif

        if(!violated)
            continue;

        // If violated, create a cutting plane.
        if(rounding_cuts.find(hash_value) == rounding_cuts.end()) {
            IloExpr expr(env);
            int accum = 0;

            for(auto it = ctr.getLinearIterator(); it.ok(); ++it) {
                auto var = it.getVar();
                if(var.getType() != IloNumVar::Bool)
                    continue;

                const auto var_index = binary_variables_id_index[var.getId()];
                if(local_fixed[var_index] == 0) {
                    expr -= var;
                }
                else if(local_fixed[var_index] == 1) {
                    expr += var;
                    ++accum;
                }
            }

            IloConstraint cut(expr <= accum - 1);
            rounding_cuts[hash_value] = cut;
            cplex.addLazyConstraint(cut);

            expr.end();
        }

        /// Unfix the variables of this constraint.
        for(auto it = ctr.getLinearIterator(); it.ok(); ++it) {
            auto var = it.getVar();
            auto coef = it.getCoef();

            // If the violated constraint is <= and the constraint coefficient
            // is < 0, then free the variable only if its current value is 0;
            // if the constraint coefficient is > 0, the free only if the
            // current value is 1, opposite for >= constraints.
            if(var.getType() == IloNumVar::Bool &&
               !(var.getLB() < EPS && var.getUB() > 1 - EPS) &&
               ((constraint_type == 'e') ||
                (local_fixed[binary_variables_id_index[var.getId()]] == 0 &&
                 ((constraint_type == 'l' && coef < 0.0) ||
                  (constraint_type == 'g' && coef > 0.0))
               ))) {
                var.setBounds(0, 1);
                ++num_unfixed_vars;
                local_fixed[binary_variables_id_index[var.getId()]] = -1;
            }
        }
    }

    num_fixed_vars = binary_variables.getSize() - num_unfixed_vars;

    #ifdef DEBUG
    cout << "\n** After constraint violation checking..."
         << "\n** Num of fixed variables: "
         << num_fixed_vars << " / " << NUM_BINARIES
         << " (" << ((100.0 * num_fixed_vars) / NUM_BINARIES)
         << "%)"
         << "\n** Num of free variables: "
         << num_unfixed_vars << " / " << NUM_BINARIES
         << " (" << ((100.0 * num_unfixed_vars) / NUM_BINARIES)
         << "%)\n"
         << endl;
    #endif

    // Now, solve using MIP.
    model.remove(relaxer);
    model.remove(fp_objective);
    model.add(original_objective);

    cplex.setParam(IloCplex::Param::WorkDir, "/tmp");
    cplex.setParam(IloCplex::Param::Threads, num_threads);
    cplex.setParam(IloCplex::Param::Emphasis::MIP, CPX_MIPEMPHASIS_FEASIBILITY);
    cplex.setParam(IloCplex::Param::TimeLimit, max_time);

    #ifdef FULLDEBUG
    env.setOut(cout);
    cplex.setOut(cout);
    cplex.setParam(IloCplex::Param::MIP::Display, 4);
    #endif

    auto stop_when_find_feasible_callback = StopWhenFindFeasibleCallback(env);
    auto stop_ctrl_c_or_time_callback = StopCtrlCorTimeCallback(env);
    cplex.use(stop_when_find_feasible_callback);
    cplex.use(stop_ctrl_c_or_time_callback);

    bool solution_found = false;
    feasible_before_var_unfixing = true;

    cplex.solve();

    #ifdef DEBUG
    cout << "\n** CPLEX status after first fix/unfix: " << cplex.getStatus()
         << " " << cplex.getCplexStatus() << endl;
    #endif

    // If fails, add a cut and unfix more variables.
    if(cplex.getStatus() == IloAlgorithm::Infeasible && !ExecutionStopper::mustStop()) {
        feasible_before_var_unfixing = false;

        // First, we insert a general cut.
        IloExpr expr(env);
        int accum = 0;
        size_t hash_value = 0;

        for(IloInt i = 0; i < NUM_BINARIES; ++i) {
            if(local_fixed[i] == 0) {
                expr -= binary_variables[i];
            }
            else if(local_fixed[i] == 1) {
                expr += binary_variables[i];
                ++accum;
                hash_value ^= (size_t)i + 0x9e3779b9 +
                              (hash_value << 6) + (hash_value >> 2);
            }
        }

        if(rounding_cuts.find(hash_value) == rounding_cuts.end()) {
            IloConstraint cut(expr <= accum - 1);
            rounding_cuts[hash_value] = cut;
            cplex.addLazyConstraint(cut);
        }

        expr.end();

        // Now, we unfix variables.
        vector<IloInt> vars_to_unfix_current;
        vector<IloInt> vars_to_unfix_next;
        unordered_set<IloInt> taken_vars;
        unordered_set<IloInt> taken_constraints;

        vars_to_unfix_current.reserve(NUM_BINARIES / 2);
        vars_to_unfix_next.reserve(NUM_BINARIES / 2);
        taken_vars.reserve(NUM_BINARIES / 2);
        taken_constraints.reserve(constraints.getSize());

        for(IloInt i = 0; i < NUM_BINARIES; ++i) {
            if(local_fixed[i] != -1)
                continue;

            vars_to_unfix_current.push_back(binary_variables[i].getId());
            taken_vars.insert(binary_variables[i].getId());
        }

        for(unsigned iteration = 0; iteration < unfix_level &&
                                    vars_to_unfix_current.size() > 0; ++iteration) {
            #ifdef FULLDEBUG
            cout << "> Iteration " << iteration
                 << " | vars_to_unfix_current: " << vars_to_unfix_current.size()
                 << endl;
            #endif

            vars_to_unfix_next.clear();

            for(auto &var_id : vars_to_unfix_current) {
                for(auto &ctr : constraints_per_variable[binary_variables_id_index[var_id]]) {
                    if(taken_constraints.find(ctr.getId()) != taken_constraints.end())
                        continue;

                    taken_constraints.insert(ctr.getId());

                    for(auto it = ctr.getLinearIterator(); it.ok(); ++it) {
                        if(ExecutionStopper::mustStop())
                            // NOTE: I know, it's horrible but the best solution here.
                            goto after_unfix;

                        auto var = it.getVar();
                        if(taken_vars.find(var.getId()) != taken_vars.end())
                            continue;

                        if(var.getType() == IloNumVar::Bool) {
                            var.setBounds(0, 1);
                            taken_vars.insert(var.getId());
                            vars_to_unfix_next.push_back(var.getId());
                            ++num_unfixed_vars;
                        }
                    }
                }
            }

            vars_to_unfix_current.swap(vars_to_unfix_next);
        }

        after_unfix:

        num_fixed_vars = NUM_BINARIES - num_unfixed_vars;

        #ifdef DEBUG
        cout << "\n** After unfixing..."
             << "\n** Num of fixed variables: "
             << num_fixed_vars << " / " << NUM_BINARIES
             << " (" << ((100.0 * num_fixed_vars) / NUM_BINARIES)
             << "%)"
             << "\n** Num of free variables: "
             << num_unfixed_vars << " / " << NUM_BINARIES
             << " (" << ((100.0 * num_unfixed_vars) / NUM_BINARIES)
             << "%)\n"
             << endl;
        #endif

        // Solve again.
        cplex.solve();

        #ifdef DEBUG
        cout << "\n** CPLEX status after unfix levels: " << cplex.getStatus()
             << " " << cplex.getCplexStatus() << endl;
        #endif
    }

    // If we found a feasible solution, yey!!! Just report it to stop.
    if(cplex.getStatus() == IloAlgorithm::Feasible ||
       cplex.getStatus() == IloAlgorithm::Optimal) {
        #ifdef DEBUG
        cout << "\n%%% Yey, we found a feasible solution!" << endl;
        #endif

        auto &current_values = current_values_per_thread[0];

        cplex.getValues(binary_variables, current_values);

        for(IloInt i = 0; i < NUM_BINARIES; ++i) {
            possible_feasible[i] = current_values[i];
            possible_feasible.rounded[i] = (int) current_values[i];
        }

        possible_feasible.feasibility_pump_value = 0.0;
        possible_feasible.fractionality = 0.0;
        possible_feasible.num_non_integral_vars = 0;

        solution_found = true;
    }
    else {
        feasible_before_var_unfixing = false;
    }

    // Add back the FP obj. function and the relaxation transformation.
    for(IloInt j = 0; j < NUM_BINARIES; ++j) {
        if(fixed_vars[j] == -1)
            binary_variables[j].setBounds(binary_variables_bounds[j].lb,
                                          binary_variables_bounds[j].ub);
        else
            binary_variables[j].setBounds(fixed_vars[j], fixed_vars[j]);
    }

    model.remove(original_objective);
    model.add(fp_objective);
    model.add(relaxer);

    cplex.remove(stop_when_find_feasible_callback);
    cplex.remove(stop_ctrl_c_or_time_callback);

    cplex.setParam(IloCplex::Param::TimeLimit, 1e+75);
    cplex.setParam(IloCplex::Param::Threads, 1);

    #ifdef DEBUG
    cout << "\n--------------------------------\n" << endl;
    #endif
    return solution_found;
}
