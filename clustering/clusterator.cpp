/******************************************************************************
 * clusterator.cpp: Implementation for FeasibilityPump decoder.
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

#include "clusterator.hpp"

#include <iostream>
#include <fstream>
#include <set>
#include <vector>
#include <unordered_map>
#include <algorithm>

#include "c_clustering_library/cluster.h"

using namespace std;

namespace ce_andrade {

const double EPS = 1e-10;

//------------------[ Default Constructor and Destructor ]--------------------//

Clusterator::Clusterator():
        weighted_incidence_matrix(),
        incidence_matrix(),
        variables_distance(),
        constraints_distance(),
        metric_distance(),
        num_vars(0),
        num_ctrs(0),
        variable_names(),
        constraint_names()
{
    Eigen::initParallel();
}

Clusterator::~Clusterator() {}

//-----------------------[ Build distance matrices ]--------------------------//

void Clusterator::buildIncidenceMatrices(const IloNumVarArray &variables,
               const IloRangeArray &constraints, string output_file_preffix) {
    #ifdef DEBUG
    cout << "\n--------------------------------\n"
         << "> Building distance matrices..."
         << "\n+ Allocating memory for the matrices..."
         << endl;
    #endif

    num_vars = variables.getSize();
    num_ctrs = constraints.getSize();

    weighted_incidence_matrix = Eigen::MatrixXd::Zero(num_vars, num_ctrs);
    incidence_matrix = Eigen::MatrixXd::Zero(num_vars, num_ctrs);
    variables_distance.resize(num_vars);
    constraints_distance.resize(num_ctrs);

    #ifdef DEBUG
    cout << "\n> Mapping variable IDs..." << endl;
    #endif

    // Maps the CPLEX ID to the index in the variables' and constraints' array.
    unordered_map<IloInt, IloInt> variables_id_index;
    unordered_map<IloInt, IloInt> constraints_id_index;

    variable_names.reserve(variables.getSize());
    for(IloInt i = 0; i < variables.getSize(); ++i) {
        variables_id_index[variables[i].getId()] = i;
        variable_names.emplace_back(variables[i].getName());
    }

    constraint_names.reserve(constraints.getSize());
    for(IloInt i = 0; i < constraints.getSize(); ++i) {
        constraints_id_index[constraints[i].getId()] = i;
        constraint_names.emplace_back(constraints[i].getName());
    }

    #ifdef DEBUG
    cout << "> Building the matrices..." << endl;
    #endif

    vector<IloInt> vars_in_ctr;
    vars_in_ctr.reserve(variables.getSize());

    vector<set<IloInt>> var_per_constraint;
    var_per_constraint.resize(num_ctrs);

    double max_ctr_sharing = 0.0;

    // First, build the weighted incidence matrix.
    for(IloInt i = 0; i < num_ctrs; ++i) {
        auto rhs = constraints[i].getUB();
        if(rhs == IloInfinity)
            rhs = constraints[i].getLB();

        for(auto it = constraints[i].getLinearIterator(); it.ok(); ++it) {
            weighted_incidence_matrix(variables_id_index[it.getVar().getId()], i) = it.getCoef();
            vars_in_ctr.push_back(it.getVar().getId());
            var_per_constraint[i].insert(it.getVar().getId());
        }

        // Sometimes, the constraint is just empty. So, we just skip.
        if(vars_in_ctr.empty())
            continue;

        // Build the distance_matrix
        for(auto it1 = vars_in_ctr.begin(); it1 != vars_in_ctr.end() - 1; ++it1) {
            for(auto it2 = it1 + 1; it2 != vars_in_ctr.end(); ++it2) {
                auto &value = variables_distance(variables_id_index[*it1],
                                                 variables_id_index[*it2]);
                value += 1.0;
                if(max_ctr_sharing < value)
                    max_ctr_sharing = value;
            }
        }
        vars_in_ctr.clear();
    }

    // Building the incidence matrix
    for(IloInt i = 0; i < num_vars; ++i)
        for(IloInt j = 0; j < num_ctrs; ++j)
            if(weighted_incidence_matrix(i, j) > EPS)
                incidence_matrix(i, j) = 1.0;

    #ifdef FULLDEBUG
    cout << "\n- incidence_matrix:\n"
         << incidence_matrix
         << endl;

    cout << "\n- weighted_incidence_matrix:\n"
         << weighted_incidence_matrix
         << endl;
    #endif
    #ifdef DEBUG
    cout << "\n> Normalizing variables_distance (max_sharing = "
         << max_ctr_sharing << ")" << "..." << endl;
    #endif

    // Remember, skip the diagonal.
    for(IloInt i = 1; i < num_vars; ++i)
        for(IloInt j = 0; j < i; ++j)
            variables_distance(i,j) = 1.0 - (variables_distance(i,j) / max_ctr_sharing);

    #ifdef FULLDEBUG
    cout << "\n- variables_distance:\n"
         << variables_distance
         << endl;
    #endif
    #ifdef DEBUG
    cout << "\n> Building constraints_distance..." << endl;
    #endif

    // Let's do in the old way (oh yes, this is STL adapted)
    double max_var_sharing = 0.0;
    for(IloInt i = 0; i < num_ctrs - 1; ++i) {
        for(IloInt j = i + 1; j < num_ctrs; ++j) {
            auto first1 = var_per_constraint[i].begin();
            auto first2 = var_per_constraint[j].begin();
            const auto last1 = var_per_constraint[i].end();
            const auto last2 = var_per_constraint[j].end();

            IloInt num_vars_shared = 0;
            while(first1 != last1 && first2 != last2) {
                if(*first1 < *first2) ++first1;
                else if(*first2 < *first1) ++first2;
                else {
                    ++num_vars_shared; ++first1; ++first2;
                }
            }

            constraints_distance(i,j) = num_vars_shared;
            if(max_var_sharing < (double)num_vars_shared)
                max_var_sharing = num_vars_shared;
        }
    }

    // Remember, skip the diagonal.
    for(IloInt i = 1; i < num_ctrs; ++i)
        for(IloInt j = 0; j < i; ++j)
            constraints_distance(i,j) = 1.0 - (constraints_distance(i,j) / max_var_sharing);

    #ifdef FULLDEBUG
    cout << "\n- constraints_distance:\n"
         << constraints_distance
         << endl;
    #endif

    #ifndef TUNING
    if(output_file_preffix.empty())
        return;

    string incidence_matrix_name(output_file_preffix + "_incidence_matrix.dat");
    string weighted_incidence_matrix_name(output_file_preffix + "_weighted_incidence_matrix.dat");
    string variables_distance_name(output_file_preffix + "_variables_distance.dat");

    // Write the weighted incidence matrix
    #ifdef DEBUG
    cout << "> Writing " << weighted_incidence_matrix_name << "..." << endl;
    #endif

    fstream weighted_incidence_matrix_file(weighted_incidence_matrix_name.c_str(), ios::out);
    if(!weighted_incidence_matrix_file)
        throw runtime_error(string("Cannot open file") + weighted_incidence_matrix_name);

    weighted_incidence_matrix_file << variables.getSize() << "/" << constraints.getSize() << " ";
    for(IloInt i = 0; i < constraints.getSize(); ++i)
        weighted_incidence_matrix_file << constraints[i].getName() << " ";
    weighted_incidence_matrix_file << "\n";

    for(IloInt i = 0; i < variables.getSize(); ++i) {
        weighted_incidence_matrix_file << variables[i].getName() << " ";

        // This works but give us a lot of undesirable white spaces
        //weighted_incidence_matrix_file << weighted_incidence_matrix.row(i) << "\n";

        // So, let do it in the traditional way
        auto row = weighted_incidence_matrix.row(i);
        for(IloInt j = 0; j < constraints.getSize(); ++j)
            weighted_incidence_matrix_file << row(j) << " ";

        weighted_incidence_matrix_file << "\n";
    }

    weighted_incidence_matrix_file.flush();
    weighted_incidence_matrix_file.close();

    // Write the incidence matrix
    #ifdef DEBUG
    cout << "> Writing " << incidence_matrix_name << "..." << endl;
    #endif

    fstream incidence_matrix_file(incidence_matrix_name.c_str(), ios::out);
    if(!incidence_matrix_file)
        throw runtime_error(string("Cannot open file") + incidence_matrix_name);

    incidence_matrix_file << variables.getSize() << "/" << constraints.getSize() << " ";
    for(IloInt i = 0; i < constraints.getSize(); ++i)
        incidence_matrix_file << constraints[i].getName() << " ";
    incidence_matrix_file << "\n";

    for(IloInt i = 0; i < variables.getSize(); ++i) {
        incidence_matrix_file << variables[i].getName() << " ";

        auto row = weighted_incidence_matrix.row(i);
        for(IloInt j = 0; j < constraints.getSize(); ++j)
            incidence_matrix_file << (row(j) < EPS? 0.0 : 1.0) << " ";

        incidence_matrix_file  << "\n";
    }

    incidence_matrix_file.flush();
    incidence_matrix_file.close();

    // Write the incidence matrix
    #ifdef DEBUG
    cout << "> Writing " << variables_distance_name << "..." << endl;
    #endif

    fstream variables_distance_file(variables_distance_name.c_str(), ios::out);
    if(!variables_distance_file)
        throw runtime_error(string("Cannot open file") + variables_distance_name);

    variables_distance_file << variables.getSize() << "/" << variables.getSize() << " ";
    for(long i = 0; i < variables.getSize(); ++i)
        variables_distance_file << variables[i].getName() << " ";
    variables_distance_file << "\n";

    for(long i = 0; i < variables.getSize(); ++i) {
        variables_distance_file << variables[i].getName() << " ";
        for(long j = 0; j < variables.getSize(); ++j)
            variables_distance_file << variables_distance(i, j) << " ";
        variables_distance_file << "\n";
    }

    variables_distance_file.flush();
    variables_distance_file.close();
    #endif

    #ifdef DEBUG
    cout << "--------------------------------\n" << endl;
    #endif
}

//-----------------------[ Hierarchical clustering ]--------------------------//

std::shared_ptr<ClusterTree> Clusterator::hierarchicalClustering(
                            const ClusteringObject type, const Metric metric) {
    #ifdef DEBUG
    cout << "\n--------------------------------\n"
         << "> Performing hierarchical clustering..."
         << endl;
    #endif

    // Transpose the incidence matrix in case of constraint clustering.
    if(type == ClusteringObject::CONSTRAINT) {
        if(metric == Metric::L1 || metric == Metric::L2)
            incidence_matrix.transposeInPlace();
        else
            weighted_incidence_matrix.transposeInPlace();
    }

    #ifdef DEBUG
    switch(metric) {
    case Clusterator::Metric::L1:
        cout << "> Computing L1...";
        break;
    case Clusterator::Metric::L2:
        cout << "> Computing L2...";
        break;
    case Clusterator::Metric::WEIGHTED_L1:
        cout << "> Computing weighted L1...";
        break;
    case Clusterator::Metric::WEIGHTED_L2:
        cout << "> Computing weighted L2...";
        break;
    default:
        break;
    }
    cout << endl;
    #endif

    switch(metric) {
    case Clusterator::Metric::L1:
        computeL1Distance(incidence_matrix);
        break;

    case Clusterator::Metric::L2:
        computeL2Distance(incidence_matrix);
        break;

    case Clusterator::Metric::WEIGHTED_L1:
        computeL1Distance(weighted_incidence_matrix);
        break;

    case Clusterator::Metric::WEIGHTED_L2:
        computeL2Distance(weighted_incidence_matrix);
        break;

    default:
        break;
    }

    #ifdef DEBUG
    cout << "> Clustering..." << endl;
    #endif

    IncidenceMatrix *inc_matrix;
    DistanceMatrix *dist_matrix;

    if(metric == Metric::L1 || metric == Metric::L2)
        inc_matrix = &incidence_matrix;
    else
        inc_matrix = &weighted_incidence_matrix;

    if(metric == Metric::SHARED) {
        if(type == ClusteringObject::VARIABLE)
            dist_matrix = &variables_distance;
        else
            dist_matrix = &constraints_distance;
    }
    else
        dist_matrix = &metric_distance;

    Node* plain_tree = treecluster((int)inc_matrix->rows(), (int)inc_matrix->cols(),
                                   NULL, NULL, NULL, 0, 'e', 's', dist_matrix->rawData());

    if(plain_tree == NULL)
        throw std::runtime_error("Fail in clustering process.");

    const int TREESIZE = inc_matrix->rows() - 1;

    vector<ClusterTree::CClusteringLibNode> formatted_data;
    formatted_data.reserve(TREESIZE);

    for(int i = 0; i < TREESIZE; ++i) {
        auto &node = plain_tree[i];
        formatted_data.emplace_back(node.left, node.right, node.distance);
    }

    delete[] plain_tree;

    std::shared_ptr<ClusterTree> tree(
            new ClusterTree(formatted_data,
                            (type == ClusteringObject::VARIABLE? variable_names :
                                                                constraint_names)));
    #ifdef FULLDEBUG
    cout << "\n\n>> Tree:\n" << *tree << endl;
    #endif

    // Come back to the regular matrix.
    if(type == ClusteringObject::CONSTRAINT) {
        if(metric == Metric::L1 || metric == Metric::L2)
            incidence_matrix.transposeInPlace();
        else
            weighted_incidence_matrix.transposeInPlace();
    }

    #ifdef DEBUG
    cout << "--------------------------------\n" << endl;
    #endif

    return tree;
}

//-----------------------------[ L1 distance ]--------------------------------//

void Clusterator::computeL1Distance(const IncidenceMatrix &matrix) {
    typedef decltype(matrix.rows()) IndexType;
    const auto N = matrix.rows();
    metric_distance.resize(N);

    // This is not the best but helps a little. We must find an algebraic
    // form as in L2 to used the Eigen parallel machinery.
    for(IndexType i = 0; i < N - 1; ++i) {
        const auto &row = matrix.row(i);

        #ifdef _OPENMP
        #pragma omp parallel for shared(row)
        #endif
        for(IndexType j = i + 1; j < N; ++j) {
            metric_distance(i, j) = (row - matrix.row(j)).lpNorm<1>();
        }
    }
}

//-----------------------------[ L2 distance ]--------------------------------//

void Clusterator::computeL2Distance(const IncidenceMatrix &matrix) {
    typedef decltype(matrix.rows()) IndexType;

    // First, compute the distances.
    Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic> XX, D;
    const auto N = matrix.rows();

    XX.resize(N, 1);
    D.resize(N, N);

    XX = matrix.array().square().rowwise().sum();
    D.noalias() = XX.replicate(1, N) + XX.transpose().replicate(N, 1);
    D -= 2 * matrix * matrix.transpose();
    D = D.cwiseSqrt();

    // After that, copy to the final destination.
    metric_distance.resize(N);

    for(IndexType i = 0; i < D.rows(); ++i)
        for(IndexType j = 0; j < i + 1; ++j)
            metric_distance(i, j) = D(i, j);
}

} //endnamaspace
