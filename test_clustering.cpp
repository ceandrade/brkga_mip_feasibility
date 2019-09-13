/*******************************************************************************
 * test_propagation.cpp: test CPLEX constraint propagation.
 *
 * Author: Carlos Eduardo de Andrade <ce.andrade@gmail.com>
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

#include "plain_ragged_matrix.hpp"
#include "clusterator.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <bitset>

// CPLEX and Boost in dome platforms have a lot of problems with some
// compile flags. So, we just deactivate these flags here.
#include "pragma_diagnostic_ignored_header.hpp"
#include <boost/timer/timer.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/filesystem.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <ilcplex/ilocplex.h>
#include "pragma_diagnostic_ignored_footer.hpp"

using namespace std;
using namespace ce_andrade;

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
        IloModel cplex_model(env);
        IloObjective objective(env);
        IloNumVarArray variables(env);
        IloRangeArray constraints(env);
        IloCplex cplex(env);

        // Import the model from file and extract to algorithm
        cplex.importModel(cplex_model, instance_file, objective,
                          variables, constraints);
        cplex.extract(cplex_model);

        cout << "\n> Num. of variables: " << variables.getSize()
             << "\n> Num. of constraints: " << constraints.getSize()
             << endl;

        boost::timer::cpu_timer timer;

        Clusterator clusterator;
        clusterator.buildIncidenceMatrices(variables, constraints);

        cout << "\n\n Time to build distance matrices: "
             << boost::timer::format(timer.elapsed()) << endl;

        timer.start();

//        auto cluster_tree = clusterator.hierarchicalClustering(
//                                    Clusterator::ClusteringObject::VARIABLE,
//                                    Clusterator::Metric::L1);
//
        auto cluster_tree = clusterator.hierarchicalClustering(
                                    Clusterator::ClusteringObject::VARIABLE,
                                    Clusterator::Metric::WEIGHTED_L2);

//        auto cluster_tree = clusterator.hierarchicalClustering(
//                                    Clusterator::ClusteringObject::VARIABLE,
//                                    Clusterator::Metric::SHARED);

//        cout << "\n\nInitial leaves: ";
//        for(auto &leaf : cluster_tree->current_leaves)
//            cout << "\n-id: " << leaf->original_id
//                 << " height: " << leaf->height
//                 << " dist: " <<  leaf->distance;
//        cout << endl;
//
//        cluster_tree->compactTree(ClusterTree::Compaction::BY_DISTANCE, 0.5);
//
//        cout << "\n\nCompacted leaves: ";
//        for(auto &leaf : cluster_tree->current_leaves)
//            cout << "\n-id: " << leaf->original_id
//                 << " height: " << leaf->height
//                 << " dist: " <<  leaf->distance;
//        cout << endl;
//
//        cout << "\n\n";
//        for(auto &name : cluster_tree->current_leaves.back()->getObjectNames())
//            cout << name << " ";
//
////        cout << *cluster_tree;

        cout << "\n\n Time to clustering: "
             << boost::timer::format(timer.elapsed()) << endl;

//        cout << "\n\n Time to build distance matrices: "
//             << boost::timer::format(timer.elapsed(), 2, "%w") << endl;

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
