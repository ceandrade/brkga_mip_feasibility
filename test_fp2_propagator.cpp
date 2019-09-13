/*******************************************************************************
 * test_propagation.cpp: test CPLEX constraint propagation.
 *
 * Author: Carlos Eduardo de Andrade <ce.andrade@gmail.com>
 *
 * (c) Copyright 2015-2019
 *     Industrial and Systems Engineering, Georgia Institute of Technology
 *     All Rights Reserved.
 *
 *  Created on : May 06, 2015 by andrade
 *  Last update: May 06, 2015 by andrade
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
#include <iomanip>
#include <stdexcept>
#include <bitset>
#include <cstdlib>

#include "mtrand.hpp"


// CPLEX has a lot of problems with these flags.
#include "pragma_diagnostic_ignored_header.hpp"
#include <boost/timer/timer.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/filesystem.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <ilcplex/ilocplex.h>
#include <ilcp/cp.h>

#include "cpxutils.h"
#include "fp_interface.h"
#include "transformers.h"
#include "logger.h"

#include "pragma_diagnostic_ignored_footer.hpp"

using namespace std;
const double EPS = 1e-6;

ILOSTLBEGIN

//--------------------------------[ Main ]------------------------------------//

int main(int argc, char* argv[]) {
    // First read parameters from command line:
    if(argc < 2) {
        cerr << "usage: " << argv[0]
             << " <LP-or-MPS-file>"
             << endl;
        return 64;  // BSD usage error code.
    }

    // Loading parameters from command line
    const char* instance_file = argv[1];

    const int seed = 0;
    MTRand rng(seed);

    // CPLEX environment
    IloEnv env;
    int return_code = 0;
    try {


        //----------------------------------------
        // CPLEX objects
        //----------------------------------------
        IloModel cplex_model(env);
        IloObjective objective(env);
        IloNumVarArray variables(env);
        IloBoolVarArray binary_variables(env);
        IloRangeArray ranges(env);
        IloCplex cplex(env);

        cplex.setParam(IloCplex::Param::MIP::Display, 3);
        cplex.setParam(IloCplex::Param::RandomSeed, 2700001);
        cplex.setParam(IloCplex::Param::Threads, 1);
        cplex.setParam(IloCplex::Param::Preprocessing::Presolve, false);

        // Import the model from file and extract to algorithm
        cplex.importModel(cplex_model, instance_file, objective,
                          variables, ranges);
        cplex.extract(cplex_model);

        dominiqs::gLog().open("null", "/dev");

        dominiqs::Env m_env = cplex.getImpl()->getCplexEnv();
        dominiqs::Prob m_lp = cplex.getImpl()->getCplexLp();

        RankerFactory::getInstance().registerClass<FractionalityRanker>("FRAC");
        dominiqs::TransformersFactory::getInstance().registerClass<PropagatorRounding>("propround");

        dominiqs::SolutionTransformerPtr frac2int =
                dominiqs::SolutionTransformerPtr(dominiqs::TransformersFactory::
                                                 getInstance().create("propround"));
        DOMINIQS_ASSERT( frac2int );
        frac2int->readConfig();

        dominiqs::Model domModel;
        domModel.extract(m_env, m_lp);
        frac2int->init(domModel, true);

        vector<double> fracs(cplex.getNcols(), 0.0);
        vector<double> ints(cplex.getNcols(), 0.0);

        cout << "\n\n*** ";
        for(auto &k : fracs) {
            k = rng.rand();
            cout << setiosflags(std::ios::fixed) << setprecision(2)
                 << k << " ";
        }
        cout << endl;

        frac2int->apply(fracs, ints);

        cout << "\n\n*** ";
        srand(0);
        for(auto &k : ints) {
            cout << setiosflags(std::ios::fixed) << setprecision(2)
                 << k << " ";
        }
        cout << endl;
    }
    catch(IloException& e) {
        cerr << "\n***********************************************************"
             << "\n*** Exception Occurred: " << e
             << "\n***********************************************************"
             << endl;
        return_code = -1;
    }
    catch(std::exception& e) {
        cerr << "\n***********************************************************"
             << "\n****  Exception Occurred: " << e.what()
             << "\n***********************************************************"
             << endl;
        return_code = -1;
    }

    // We must not forget to finalize the cplex environment.
    env.end();
    return return_code;
}
