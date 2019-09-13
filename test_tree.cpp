/*******************************************************************************
 * test_propagation.cpp: test CPLEX constraint propagation.
 *
 * Author: Carlos Eduardo de Andrade <ce.andrade@gmail.com>
 *
 * (c) Copyright 2015-2019
 *     Industrial and Systems Engineering, Georgia Institute of Technology
 *     All Rights Reserved.
 *
 *  Created on : May 26, 2015 by andrade
 *  Last update: May 27, 2015 by andrade
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

#include "cluster_tree.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <bitset>
#include <vector>
#include <memory>

using namespace std;
using namespace ce_andrade;

const double EPS = 1e-6;
//ILOSTLBEGIN

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

    int return_code = 0;
    try {
        vector<shared_ptr<ClusterNode>> nodes(5, nullptr);

        for(size_t i = 0; i < nodes.size(); ++i) {
            nodes[i] = std::make_shared<ClusterNode>();
            nodes[i]->original_id = i;
        }

        auto root = nodes[4];
        root->left = nodes[3];
        root->right = nodes[2];

        nodes[3]->left = nodes[0];
        nodes[3]->right = nodes[1];

        nodes[0]->object_names.push_back("0");
        nodes[0]->object_names.push_back("10");
        nodes[0]->object_names.push_back("20");

        nodes[1]->object_names.push_back("1");
        nodes[2]->object_names.push_back("2");

        root->print(cout);

        nodes[4]->getObjectNames();
        root->print(cout);

        nodes[2]->getObjectNames();
        root->print(cout);
    }
    catch(std::exception& e) {
        cerr << "\n***********************************************************"
             << "\n****  Exception Occurred: " << e.what()
             << "\n***********************************************************"
             << endl;
        return_code = -1;
    }

    return return_code;
}
