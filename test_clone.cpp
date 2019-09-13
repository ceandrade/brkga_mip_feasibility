/*******************************************************************************
 * test_propagation.cpp: test CPLEX constraint propagation.
 *
 * Author: Carlos Eduardo de Andrade <ce.andrade@gmail.com>
 *
 * (c) Copyright 2015-2019
 *     Industrial and Systems Engineering, Georgia Institute of Technology
 *     All Rights Reserved.
 *
 *  Created on : Apr 20, 2015 by andrade
 *  Last update: Apr 20, 2015 by andrade
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

// CPLEX has a lot of problems with these flags.
#include "pragma_diagnostic_ignored_header.hpp"
#include <boost/timer/timer.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/filesystem.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <ilcplex/ilocplex.h>
#include <ilcp/cp.h>
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

    // CPLEX environment
    IloEnv env;

    int return_code = 0;
    try {
        //----------------------------------------
        // CPLEX objects
        //----------------------------------------
        IloModel model1(env), model2(env);
        IloObjective obj1(env), obj2(env);
        IloNumVarArray vars1(env), vars2(env);
        IloRangeArray rngs1(env), rngs2(env);

        IloCplex cplex1(env), cplex2(env);

        cplex1.setParam(IloCplex::Param::Preprocessing::Presolve, false);
        cplex2.setParam(IloCplex::Param::Preprocessing::Presolve, false);

        cplex1.importModel(model1, instance_file, obj1,
                           vars1, rngs1);

        model1.add(IloConversion(env, vars1, IloNumVar::Float));
        model2 = model1.getClone();

        cout << "\n\n** " << model1
             << "\n\n** " << model2
             << endl;

        cplex1.extract(model1);
        cplex2.extract(model2);

        for(IloModel::Iterator it(model2); it.ok(); ++it) {
           if((*it).isVariable())
               vars2.add((*it).asVariable());
        }

        cout << "\n- vars1[0]: " << vars1[0]
             << "\n- vars2[0]: " << vars2[0]
             << endl;

        vars2[0].setBounds(1.0, 1.0);

        cout << "\n- vars1[0]: " << vars1[0]
             << "\n- vars2[0]: " << vars2[0]
             << endl;

        cplex1.solve();
        cplex2.solve();

        cout << "\n- cplex1 value: " << cplex1.getObjValue()
             << "\n- cplex2 value: " << cplex2.getObjValue()
             << endl;

       cplex1.exportModel("ze1.lp");
       cplex2.exportModel("ze2.lp");

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
