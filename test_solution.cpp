/*******************************************************************************
 * main_mip.cpp: Main Process for MIP.
 *
 * Author: Carlos Eduardo de Andrade <ce.andrade@gmail.com>
 *
 * (c) Copyright 2015-2019
 *     Industrial and Systems Engineering, Georgia Institute of Technology
 *     All Rights Reserved.
 *
 *  Created on : Jun 19, 2015 by andrade
 *  Last update: Jun 19, 2015 by andrade
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

#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

// CPLEX has a lot of problems with these flags.
#include "pragma_diagnostic_ignored_header.hpp"
#include <cstring>
#include <ilcplex/ilocplex.h>
#include "pragma_diagnostic_ignored_footer.hpp"

using namespace std;

const double EPS = 1e-6;

ILOSTLBEGIN
//--------------------------------[ Main ]------------------------------------//

int main(int argc, char* argv[]) {
    // First read parameters from command line:
    if(argc < 3) {
        cerr << "usage: " << argv[0]
             << " <MPS-LP-file> <solution-file>"
             << "\n\n ALL PARAMETERS IN CURLY BRACKETS ARE MANDATORY\n"
             << endl;
        return 64;  // BSD usage error code.
    }

    // Loading parameters from command line
    const char* instance_file = argv[1];
    const char* solution_file = argv[2];

    // CPLEX environment
    IloEnv env;

    int return_code = 0;
    try {

        //----------------------------------------
        // CPLEX objects
        //----------------------------------------
        IloModel model(env);
        IloCplex cplex(env);
        IloObjective objective(env);
        IloNumVarArray variables(env);
        IloRangeArray constraints(env);

        unordered_map<string, IloNumVar> name_to_var;

        cplex.setParam(IloCplex::Param::MIP::Display, 4);

        cplex.importModel(model, instance_file, objective, variables, constraints);
        cplex.extract(model);

        for(IloInt i = 0; i < variables.getSize(); ++i) {
            auto &var = variables[i];
            name_to_var[var.getName()] = var;
        }

        // Open solution file, load the solution, and fix the variables.
        ifstream solution(solution_file);
        if(!solution) {
            stringstream ss;
            ss << "It is impossible to open file " << solution_file;
            throw runtime_error(ss.str());
        }

        string line;
        while(getline(solution, line)) {
            string::size_type pos = line.find("[");
            name_to_var[line.substr(0, pos)].setBounds(1.0, 1.0);
        }

        solution.close();


        cplex.solve();
        cout << cplex.getStatus() << endl;

        if(cplex.getStatus() == IloAlgorithm::Status::Feasible ||
           cplex.getStatus() == IloAlgorithm::Status::Optimal) {
//            relative_gap = cplex.getMIPRelativeGap();
//            best_solution_value = cplex.getObjValue();

            // Write the CPLEX best solution.
            //string cplex_solution_name(base_output_filename);
            //cplex_solution_name += ".sol";
            //cplex.writeSolution(cplex_solution_name.c_str());
        }

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

    // We must not forget to finalize the cplex environment.
    env.end();

    return return_code;
}
