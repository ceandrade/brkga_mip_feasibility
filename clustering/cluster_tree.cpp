/******************************************************************************
 * cluster_tree.cpp: Implementation for ClusterNode and ClusterTree classes.
 *
 * Author: Carlos Eduardo de Andrade
 *         <carlos.andrade@gatech.edu / ce.andrade@gmail.com>
 *
 * (c) Copyright 2015-2019
 *     Industrial and Systems Engineering, Georgia Institute of Technology
 *     All Rights Reserved.
 *
 *  Created on : May 26, 2015 by andrade
 *  Last update: Jun 23, 2015 by andrade
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
#include <stdexcept>
#include <limits>
#include <queue>
#include <algorithm>

using namespace std;

namespace ce_andrade {

/******************************************************************************
 * ClusterNode methods
 ******************************************************************************/

//------------------[ Default Constructor and Destructor ]--------------------//

ClusterNode::ClusterNode():
        original_id(numeric_limits<decltype(original_id)>::min()),
        height(numeric_limits<decltype(height)>::min()),
        distance(numeric_limits<decltype(distance)>::lowest()),
        parent(),
        left(nullptr),
        right(nullptr),
        object_names()
{}

ClusterNode::~ClusterNode() {}

//--------------------------[ Get object names ]------------------------------//

const std::deque<std::string>& ClusterNode::getObjectNames() {
    // If the names are empty, find the names in the subtrees.
    if(object_names.empty()) {
        auto &left_names = left->getObjectNames();
        copy(begin(left_names), end(left_names), back_inserter(object_names));

        auto &right_names = right->getObjectNames();
        copy(begin(right_names), end(right_names), back_inserter(object_names));
    }

    return object_names;
}

//---------------------[ Print the node and its subtree ]---------------------//

void ClusterNode::print(std::ostream &os, int padding) {
    string str_padding(4 * padding, ' ');
    os << "\n" << str_padding;
    os << "+ " << original_id
        << " (height: " << height
        << ", dist: " << distance
        << ")";
    os << "\n" << str_padding << "| Items: ";
    for(auto &name : object_names)
        cout << name << " ";

    ++padding;

    if(left != nullptr)
        left->print(os, padding);

    if(right != nullptr)
        right->print(os, padding);
}

/******************************************************************************
 * ClusterTree methods
 ******************************************************************************/

//------------------[ Default Constructor and Destructor ]--------------------//

ClusterTree::ClusterTree(std::vector<CClusteringLibNode> &_plain_tree,
                         const std::vector<std::string> &object_names):
        root(nullptr),
        original_leaves(),
        current_leaves(),
        plain_tree(),
        minimum_distance(numeric_limits<double>::max()),
        maximum_distance(numeric_limits<double>::lowest()),
        average_distance(0.0),
        median_distance(0.0),
        height(0)
{
    #ifdef DEBUG
    cout << "\n--------------------------------\n"
         << "> Building a cluster tree..."
         << endl;
    #endif

    // First, take the ownership of the data.
    plain_tree.swap(_plain_tree);

    original_leaves.reserve(object_names.size());
    current_leaves.reserve(object_names.size());

    for(int i = 0; i < (int)object_names.size(); ++i) {
        shared_ptr<ClusterNode> node(new ClusterNode());
        node->original_id = i;
        node->object_names.push_back(object_names[i]);
        original_leaves.push_back(node);
        current_leaves.push_back(node);
    }

    vector<shared_ptr<ClusterNode>> intermediate_nodes(plain_tree.size(), nullptr);

    // Inner nodes have IDs using negative number. So, we need do some
    // offsetting to get the right position in intermediate_nodes.
    // Please, note the minus signals along this block.
    int id_count = -1;
    for(auto &plain_node : plain_tree) {
        #ifdef FULLDEBUG
        cout << "\n Creating node " << id_count << " >"
             << " left: " << plain_node.left
             << " right: " << plain_node.right
             << " distance: " << plain_node.distance;
        cout.flush();
        #endif

        shared_ptr<ClusterNode> node(new ClusterNode());
        node->original_id = id_count;
        intermediate_nodes[-id_count - 1] = node;

        if(plain_node.left > -1)
            node->left = original_leaves[plain_node.left];
        else
            node->left = intermediate_nodes[-plain_node.left - 1];

        if(plain_node.right > -1)
            node->right = original_leaves[plain_node.right];
        else
            node->right = intermediate_nodes[-plain_node.right - 1];

        node->left->parent = node;
        node->right->parent = node;

        node->distance = plain_node.distance;

        --id_count;
    }

    // Usually, the last node of intermediate_nodes is the root node. But,
    // to make sure, let's traverse the tree bottom-up from a leaf to the root.
    weak_ptr<ClusterNode> lookup_node(original_leaves.front());
    while(!lookup_node.lock()->parent.expired())
        lookup_node = lookup_node.lock()->parent;

    root = lookup_node.lock();

    // Now, we adjust the heights and compute some statistics.
    root->height = 0;
    average_distance = root->distance;

    vector<double> distances;
    distances.reserve(plain_tree.size());

    queue<shared_ptr<ClusterNode>> queue;
    queue.push(root);
    while(!queue.empty()) {
        auto node = queue.front();
        queue.pop();

        height = max(height, node->height);

        if(node->left != nullptr || node->right != nullptr) {
            minimum_distance = std::min(minimum_distance, node->distance);
            maximum_distance = std::max(maximum_distance, node->distance);
            average_distance += node->distance;
            distances.push_back(node->distance);
        }

        if(node->left != nullptr) {
            node->left->height = node->height + 1;
            queue.push(node->left);
        }

        if(node->right != nullptr) {
            node->right->height = node->height + 1;
            queue.push(node->right);
        }
    }

    // height = last_level + 1
    ++height;

    average_distance /= distances.size() + 1;

    nth_element(begin(distances), begin(distances) + (distances.size() / 2), end(distances));
    median_distance = *(begin(distances) + (distances.size() / 2));
}

ClusterTree::~ClusterTree() {}

//---------------------------[ Compact tree ]---------------------------------//

void ClusterTree::compactTree(const Compaction type, const double value) {
    queue<shared_ptr<ClusterNode>> queue;
    queue.push(root);
    current_leaves.clear();

    while(!queue.empty()) {
        auto node = queue.front();
        queue.pop();

        if((type == Compaction::BY_LEVEL && node->height < (decltype(node->height))value) ||
           (type == Compaction::BY_DISTANCE && node->distance > value)) {

            if(node->left != nullptr)
                queue.push(node->left);
            if(node->right != nullptr)
                queue.push(node->right);
        }
        else
            current_leaves.push_back(node);
    }
}

//-----------------------------[ Operator <<  ]-------------------------------//

std::ostream& operator<<(std::ostream &os, const ClusterTree &tree) {
    os << "Height: " << tree.height
       << "\nDistances:"
       << "\n- minimum: " << tree.minimum_distance
       << "\n- maximum: " << tree.maximum_distance
       << "\n- average: " << tree.average_distance
       << "\n- median: " << tree.median_distance
       << "\n";

//    if(tree.root != nullptr)
//        tree.root->print(os);
    return os;
}

//---------------------------[ Save to Graphviz ]-----------------------------//

void ClusterTree::saveToGraphviz(const string filename) {
    const string PADDING(4, ' ');
    ofstream dot_file(filename, ios::trunc);

    if(!dot_file)
        throw runtime_error(string("Graphviz file cannot be opened: ") + filename);

    // Header.
    dot_file << "/* To better visualization use \"dot -Kfdp -s100.0  -Tpdf\" */\n"
             << "digraph \"0\" {\n";

    // First, we describe the leaves
    for(auto &leaf : original_leaves) {
        dot_file << PADDING
                 << leaf->original_id
                 << " [label=\"" << leaf->getObjectNames().front() << "\","
                 << " style=\"filled\", fillcolor=\"#C4C400\"];\n";
    }

    queue<shared_ptr<ClusterNode>> queue;
    queue.push(root);

    while(!queue.empty()) {
        auto node = queue.front();
        queue.pop();

        if(node->left != nullptr) {
            dot_file << PADDING
                     << node->original_id << " -> " << node->left->original_id
                     << ";\n";
            queue.push(node->left);
        }

        if(node->right != nullptr) {
            dot_file << PADDING
                     << node->original_id << " -> " << node->right->original_id
                     << ";\n";
            queue.push(node->right);
        }
    }

    dot_file << "}\n";
    dot_file.close();
}

} //endnamaspace
