/*******************************************************************************
 * test_final_local_search.cpp: Main Process for MIP.
 *
 * Author: Carlos Eduardo de Andrade <ce.andrade@gmail.com>
 *
 * (c) Copyright 2015-2019
 *     Industrial and Systems Engineering, Georgia Institute of Technology
 *     All Rights Reserved.
 *
 *  Created on : Jun 26, 2015 by andrade
 *  Last update: Jun 26, 2015 by andrade
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

#include <iostream>
#include <fstream>
#include <string>
#include <stdexcept>
#include <unordered_set>
#include <unordered_map>
#include <queue>

// CPLEX has a lot of problems with these flags.
#include "pragma_diagnostic_ignored_header.hpp"
#include <cstring>
#include <ilcplex/ilocplex.h>
#include <boost/timer/timer.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include "pragma_diagnostic_ignored_footer.hpp"

using namespace std;

const double EPS = 1e-6;

ILOSTLBEGIN
//--------------------------------[ Main ]------------------------------------//

int main(int argc, char* argv[]) {
    // First read parameters from command line:
    if(argc < 4) {
        cerr << "usage: " << argv[0]
             << " <unfix-depth> <MPS-LP-file> <solution-files...>"
             << "\n\n ALL PARAMETERS IN CURLY BRACKETS ARE MANDATORY\n"
             << endl;
        return 64;  // BSD usage error code.
    }

    // Loading parameters from command line
    const char* instance_file = argv[2];

    int return_code = 0;
    try {
        FeasibilityPump_Decoder decoder(instance_file, 1, 270001,
                                        FeasibilityPump_Decoder::PumpStrategy::DEFAULT,
                                        FeasibilityPump_Decoder::FitnessType::GEOMETRIC,
                                        0.0, 0.0,
                                        {25, true, 20, -0.3, 0.7},
                                        {0.9, 0.0005},
                                        0.1, 0.05);
        decoder.init();

        auto &env = decoder.environment_per_thread[0];
        auto &cplex = decoder.cplex_per_thread[0];
        auto &model = decoder.model_per_thread[0];
        auto &variables = decoder.variables_per_thread[0];
        auto &binary_variables = decoder.binary_variables_per_thread[0];
        auto &constraints = decoder.constraints_per_thread[0];
        auto &relaxer = decoder.relaxer_per_thread[0];
        auto &original_objective = decoder.original_objective_per_thread[0];
        auto &fp_objective = decoder.fp_objective_per_thread[0];
        auto &binary_variables_id_index = decoder.binary_variables_id_index;
        auto &current_values = decoder.current_values_per_thread[0];

        cout << "\n>> binary_variables: " << binary_variables.getSize() << endl;

        // Map the variables' names to the variables themselves.
        unordered_map<string, IloNumVar> name_var;
        name_var.reserve(variables.getSize());

        for(IloInt i = 0; i < variables.getSize(); ++i) {
            name_var[string(variables[i].getName())] = variables[i];
//            cout << "\n" << variables[i].getName();
        }

        //////////////////////////////////////////////////////////////
        // First, let try to solve a IP fixing all no-frac. bin. vars
        //////////////////////////////////////////////////////////////

        vector<vector<double>> solution_values(argc - 3);
        for(auto &v : solution_values)
            v.resize(binary_variables.getSize(), 0.0);

        // Open solution files and load the solutions.
        for(int i = 3; i < argc; ++i) {
            ifstream solution(argv[i]);
            if(!solution)
                throw runtime_error(string("It is impossible to open file ") + string(argv[i]));

            cout << "\n> Loading " << argv[i]; cout.flush();

            string line;
            string name;
            double value;
            while(getline(solution, line)) {
                name = line.substr(0, line.find("["));
                value = std::stod(line.substr(line.find(":") + 1));
                solution_values[i - 3][binary_variables_id_index[name_var[name].getId()]] = value;
            }
            solution.close();
        }

        model.remove(fp_objective);
        model.add(original_objective);

        env.setOut(cout);
        cplex.setOut(cout);
        cplex.setParam(IloCplex::Param::MIP::Display, 4);
//        cplex.setParam(IloCplex::Param::Tune::Display, 2);
//        cplex.setParam(IloCplex::Param::Barrier::Display, 2);
//        cplex.setParam(IloCplex::Param::Simplex::Display, 2);
//        cplex.setParam(IloCplex::Param::Sifting::Display, 2);
//        cplex.setParam(IloCplex::Param::Network::Display, 2);
//        cplex.setParam(IloCplex::Param::Conflict::Display, 2);
        cplex.setParam(IloCplex::Param::Threads, 3);
        //cplex.setParam(IloCplex::Param::Emphasis::MIP, CPX_MIPEMPHASIS_FEASIBILITY);
        cplex.setParam(IloCplex::Param::TimeLimit, 120);

        cplex.solve();
        cout << "\n\n>> Status: " << cplex.getStatus() << " " << cplex.getCplexStatus();

        IloNumArray duals(env), slacks(env);
        cplex.getDuals(duals, constraints);
        cplex.getSlacks(slacks, constraints);



//        cplex.getValues(binary_variables, current_values);
//        for(IloInt i = 0; i < binary_variables.getSize(); ++i) {
//            if(current_values[i] > EPS && current_values[i] < 1 - EPS)
//                cout << binary_variables[i] << " : " << current_values[i] << "\n";
//        }
//
//        cout
//             << "\n- " << name_var[string("z_(2,11),4")]
//             << " : " << cplex.getValue(name_var[string("z_(2,11),4")])
//             << "\n- " << name_var[string("f_(1,11),4")]
//             << " : " << cplex.getValue(name_var[string("f_(1,11),4")])
//             << "\n- " << name_var[string("z_(0,20),4")]
//             << " : " << cplex.getValue(name_var[string("z_(0,20),4")])
//             << "\n- " << name_var[string("f_(0,20),4")]
//             << " : " << cplex.getValue(name_var[string("f_(0,20),4")]);
//
//        string ze("supplyOnVessel_4,");
//        for(int i = 0; i < 20; ++i) {
//            cout << "\n- " << name_var[ze + to_string(i)]
//                 << " : " << cplex.getValue(name_var[ze + to_string(i)]);
//        }

        //////////////////////////////////
        // Relax neighbor variables
        //////////////////////////////////

        // Now, fix only variable such value is the same in all solutions
        vector<IloInt> different_variables;
        different_variables.reserve(binary_variables.getSize() / 2);

        for(IloInt var_idx = 0; var_idx < binary_variables.getSize(); ++var_idx) {
            bool same_value = true;
            double value = solution_values[0][var_idx];

            for(size_t i = 1; i < solution_values.size(); ++i)
                same_value &= fabs(value - solution_values[i][var_idx]) < EPS;

            if(same_value) {
                binary_variables[var_idx].setBounds(value, value);
//                cout << "\n- Fixing " << binary_variables[var_idx] << " to " << value;
            }
            else {
                different_variables.push_back(var_idx);
//                cout << "\n- Not fixing " << binary_variables[var_idx];
            }
        }

        cout << "\n\n** Fixed "
             << (binary_variables.getSize() - different_variables.size()) << endl;


        vector<IloInt> vars_to_unfix_current, vars_to_unfix_next;
        vars_to_unfix_current.reserve(binary_variables.getSize());
        vars_to_unfix_next.reserve(binary_variables.getSize());

        unordered_set<IloInt> taken_vars;
        unordered_set<IloInt> taken_constraints;

        for(auto &v : different_variables) {
            vars_to_unfix_current.push_back(binary_variables[v].getId());
            taken_vars.insert(binary_variables[v].getId());
        }

        vector<vector<IloRange>> constraints_per_variable;
        constraints_per_variable.resize(binary_variables.getSize());

        for(auto &v : constraints_per_variable)
            v.reserve(cplex.getNNZs() / variables.getSize());

        for(IloInt i = 0; i < constraints.getSize(); ++i) {
            auto &ctr = constraints[i];
            for(auto it = ctr.getLinearIterator(); it.ok(); ++it) {
                auto var = it.getVar();
                if(var.getType() == IloNumVar::Bool) {
                    constraints_per_variable[binary_variables_id_index[var.getId()]]
                                             .push_back(ctr);
                }
            }
        }

        const int DEEP = atoi(argv[1]);

        cout << "\n";
        for(int iteration = 0; iteration < DEEP; ++iteration) {
            cout << "> Iteration " << iteration
                 << " | vars_to_unfix_current: " << vars_to_unfix_current.size()
                 << endl;

            vars_to_unfix_next.clear();

            for(auto &var_id : vars_to_unfix_current) {
                for(auto &ctr : constraints_per_variable[binary_variables_id_index[var_id]]) {
                    if(taken_constraints.find(ctr.getId()) != taken_constraints.end())
                        continue;

                    taken_constraints.insert(ctr.getId());

                    for(auto it = ctr.getLinearIterator(); it.ok(); ++it) {
                        auto var = it.getVar();
                        if(taken_vars.find(var.getId()) != taken_vars.end())
                            continue;

                        if(var.getType() == IloNumVar::Bool) {
                            var.setBounds(0, 1);
                            taken_vars.insert(var.getId());
                            vars_to_unfix_next.push_back(var.getId());
                        }
                    }
                }
            }

            vars_to_unfix_current.swap(vars_to_unfix_next);
        }

        cout << "\n\n>>>> Unfix vars: " << taken_vars.size() << endl << endl;

        ////////////////////////////////////
        //// Do surrogate constraints
        ////////////////////////////////////

        //class ContraintIndicators {
            //public:
                //IloNum dual;
                //IloNum slack;
                //IloRange contraint;

                //ContraintIndicators(IloNum _dual, IloNum _slack, IloRange _ctr):
                    //dual(_dual), slack(_slack), contraint(_ctr) {}

                //bool operator>(const ContraintIndicators &ci) const {
                    //return (fabs(this->dual) > fabs(ci.dual)) ||
                           //(fabs(this->dual - ci.dual) < EPS &&
                            //fabs(this->slack) < fabs(ci.slack));
                //}
        //};

        //vector<ContraintIndicators> ctr_sorting;
        //ctr_sorting.reserve(constraints.getSize());

        //for(IloInt i = 0; i < constraints.getSize(); ++i)
            //ctr_sorting.emplace_back(duals[i], slacks[i], constraints[i]);

        //sort(ctr_sorting.begin(), ctr_sorting.end(),
             //[](const ContraintIndicators &c1, const ContraintIndicators &c2) {
                 //return c1 > c2;
             //});

        //IloExpr surrogate_expr(env);
        //int signal;
        //IloNum rhs = 0.0;
        //int num_removed_ctrs = 0;

        //for(IloInt i = 0; i < constraints.getSize(); ++i) {
////            cout << "\n Ctr " << ctr_sorting[i].contraint.getName()
////                 << " | dual: " << ctr_sorting[i].dual
////                 << " | slack: " << ctr_sorting[i].slack;

            //if(fabs(ctr_sorting[i].dual) < EPS) {
                //auto &ctr = ctr_sorting[i].contraint;

                //const auto LB = ctr.getLB();
                //const auto UB = ctr.getUB();

                //signal = 0;
                //if(LB == -IloInfinity && UB != IloInfinity)
                    //signal = 1;
                //else
                    //if(LB != -IloInfinity && UB == IloInfinity)
                        //signal = -1;
                //// In case of both bounds, the variables nullify themselves
                //// in the sum in the surrorate constraint. So, it is not
                //// necessary to add them up.

                //if(signal != 0) {
                    //for(auto it = ctr.getLinearIterator(); it.ok(); ++it)
                        //surrogate_expr += signal * it.getCoef() * it.getVar();

                    //if(signal == 1)
                        //rhs += UB;
                    //else
                        //rhs -= LB;
                //}
                //else  // this is the case when the constraint has both LB and UB.
                    //rhs += UB - LB;

                //model.remove(ctr);
                //++num_removed_ctrs;
            //}
        //}

        //auto surrogate_ctr = surrogate_expr <= rhs;
        //surrogate_ctr.normalize();

        //cout << "\n\n >> Num ctrs: " << constraints.getSize()
             //<< " | num_removed_ctrs: " << num_removed_ctrs << endl << endl;

////        model.add(surrogate_ctr);
        //surrogate_expr.end();
        //surrogate_ctr.end();

        model.remove(relaxer);

        cplex.solve();

        cout << "\n\n>> Status: " << cplex.getStatus() << " " << cplex.getCplexStatus();
    }
    catch(IloException& e) {
        cout << "\n***********************************************************"
             << "\n*** Exception Occurred: " << e
             << "\n***********************************************************"
             << endl;
        return_code = -1;
    }
    catch(std::exception& e) {
        cout << "\n***********************************************************"
             << "\n****  Exception Occurred: " << e.what()
             << "\n***********************************************************"
             << endl;
        return_code = -1;
    }
    return return_code;
}
