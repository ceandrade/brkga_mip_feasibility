/******************************************************************************
 * cluster_tree.hpp: Interface for ClusterNode and ClusterTree classes.
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

#ifndef CLUSTER_TREE_HPP_
#define CLUSTER_TREE_HPP_

#include <iostream>
#include <deque>
#include <vector>
#include <memory>

namespace ce_andrade {

// Forward declaration.
class ClusterTree;

/**
 * \brief ClusterNode
 *
 * \author Carlos Eduardo de Andrade <ce.andrade@gmail.com>
 * \date 2015
 *
 * This class represents a node in the clusterization tree.
 */
class ClusterNode {
    friend class ClusterTree;

    public:
        /** \name Constructor and Destructor */
        //@{
        /// Default Constructor.
        ClusterNode();

        /// Destructor.
        ~ClusterNode();
        //@}

        /** \name Informational methods */
        //@{
        /// Returns the name of objects under the subtrees.
        /// \return a list with the names.
        const std::deque<std::string>& getObjectNames();

        /** \brief Print the instance on ostream output.
         * \param os the output object
         * \param padding used to adjust the padding in the print.
         */
        void print(std::ostream &os, int padding = 0);
        //@}

    public:
        /** \name Data
         * We decided to give free access to internal data to speed up a little
         * bit the utilization of this data. PLEASE, USE THEM WITH CARE!
         */
        //@{
        /// ID of the node from C clustegin library. For leaves with just
        /// one element, the ID is mininum integer.
        int original_id;

        /// The height in the tree. The root node is
        int height;

        /// The distance between the two items or subnodes that were joined.
        double distance;

        /// The parent node. If nullptr is there is the root node.
        std::weak_ptr<ClusterNode> parent;

        /// The left child. If nullptr is there is no such child.
        std::shared_ptr<ClusterNode> left;

        /// The right child. If nullptr is there is no such child.
        std::shared_ptr<ClusterNode> right;
        //@}

    protected:
        /** Internal data */
        //@{
        /// Describes all objects (variables or constraints names) under
        /// this node. It is built in lazy fashion. So, this is the reason
        /// to no be public.
        std::deque<std::string> object_names;
        //@}
};

/**
 * \brief ClusterNode
 *
 * \author Carlos Eduardo de Andrade <ce.andrade@gmail.com>
 * \date 2015
 *
 * This class represents a node in the clusterization tree.
 */
class ClusterTree {
    public:
        /** \name Enumerations */
        //@{
        /// Defines the type of compaction used to create new leaves.
        enum class Compaction {
            BY_LEVEL,       ///< Using the level information.
            BY_DISTANCE     ///< Using the distance information.
        };
       //@}

    public:
        /** Type definitions */
        //@{
        /// This struct represents a node from the clustering algorithms
        /// of C Clustering library. Please, use with caution.
        struct CClusteringLibNode {
            int left;
            int right;
            double distance;

            CClusteringLibNode(int _left, int _right, double _distance):
                left(_left),
                right(_right),
                distance(_distance) {}
        };
        //@}

    public:
        /** \name Constructor and Destructor */
        //@{
        /** \brief Default Constructor.
         * \param plain_tree from the clustering algorithm. This class takes
         *        the ownership of the data swapping the vectors.
         * \param object_names the names of variables or constraints.
         */
        ClusterTree(std::vector<CClusteringLibNode> &plain_tree,
                    const std::vector<std::string> &object_names);

        /// Destructor.
        ~ClusterTree();
        //@}

        /** \name Utilities */
        //@{
        /** \brief This method collapses nodes resulting in new leaves.
         * This method DOES NOT CHANGE THE TREE. It only changes
         * ClusterTree::current_leaves. The tree is yet full accessible through
         * ClusterTree::root and ClusterTree::original_leaves.
         *
         * \param type define the type of compaction. If BY_LEVEL, nodes with
         * level higher than or equal to the "value" parameter are compacted
         * and became new leaves. If BY_DISTANCE, nodes whose the distance
         * between their subtrees are at most min_dist are compacted.
         *
         * \param value the value used in the compaction.
         */
        void compactTree(const Compaction type, const double value);
        //@}

        /** \name Informational methods */
        //@{
        /** \brief Print the instance on std::ostream output.
         * \param os the output object
         * \param tree the tree.
         */
        friend std::ostream& operator<<(std::ostream &os, const ClusterTree &tree);

        /** \brief Create a Graphviz file from the tree.
         * \param filename the file to save the tree.
         */
        void saveToGraphviz(const std::string filename);
        //@}

    public:
        /** \name Data
         * We decided to give free access to internal data to speed up a little
         * bit the utilization of this data. PLEASE, USE THEM WITH CARE!
         */
        //@{
        /// The root node of the tree.
        std::shared_ptr<ClusterNode> root;

        /// The original leaves from the clusterization.
        std::vector<std::shared_ptr<ClusterNode>> original_leaves;

        /// The current leaves after compression(s).
        std::vector<std::shared_ptr<ClusterNode>> current_leaves;

        /// Holds the results of clustering algorithm.
        std::vector<CClusteringLibNode> plain_tree;

        /// Minimum distance between the nodes. It also includes
        /// the distance between intermediate nodes.
        double minimum_distance;

        /// Maximum distance between the nodes. It also includes
        /// the distance between intermediate nodes.
        double maximum_distance;

        /// Average distance between the nodes. It also includes
        /// the distance between intermediate nodes.
        double average_distance;

        /// The mediam of the distance between the nodes. It also includes
        /// the distance between intermediate nodes.
        double median_distance;

        /// Tree height.
        int height;
        //@}
};

} //endnamaspace

#endif // CLUSTER_TREE_HPP_
