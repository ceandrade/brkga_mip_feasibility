/******************************************************************************
 * plain_ragged_matrix.hpp: Interface for a plain ragged matrix.
 *
 * Author: Carlos Eduardo de Andrade
 *         <carlos.andrade@gatech.edu / ce.andrade@gmail.com>
 *
 * (c) Copyright 2015-2019
 *     Industrial and Systems Engineering, Georgia Institute of Technology
 *     All Rights Reserved.
 *
 *  Created on : May 24, 2015 by andrade
 *  Last update: Jun 17, 2015 by andrade
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

#ifndef PLAIN_RAGGED_MATRIX_HPP_
#define PLAIN_RAGGED_MATRIX_HPP_

#include <iostream>
#include <new>
#include <algorithm>

namespace ce_andrade {

/**
 * \brief Plain Ragged Matrix
 *
 * \author Carlos Eduardo de Andrade <ce.andrade@gmail.com>
 * \date 2015
 *
 * This class represents a plain ragged matrix used in the clustering
 * algorithm. This is bottom-left triangular symmetric matrix.
 *
 * \note
 *  -# We DO NOT CHECK types or use traits in this template;
 *  -# DataType must have a simple constructor;
 *  -# IndexType must be a countable type admitting initialization from zero,
 *     pre-increment operator, and less_than operator. Usually, IndexType
 *     will be a integer type.
 */
template <typename DataType, typename IndexType>
class PlainRaggedMatrix {
    public:
        /** \name Constructor and Destructor */
        //@{
        /// Default constructor.
        PlainRaggedMatrix(): size(0), data(nullptr) {}

        /** \brief Build constructor.
         * \param _size the matrix's dimension.
         */
        explicit PlainRaggedMatrix(const IndexType _size): size(0), data(nullptr) {
            resize(_size);
        }

        /** \brief Copy constructor
         * \param other the matrix to be copied.
         */
        PlainRaggedMatrix(const PlainRaggedMatrix& other): size(0), data(nullptr) {
            resize(other.size);
            for(IndexType i = 0; i < size; ++i)
                std::copy(other.data[i], other.data[i] + (i + 1), data[i]);
        }

        /** \brief Destructor. */
        ~PlainRaggedMatrix() {
            deallocate();
        }
        //@}

        /** \name Re-dimensioning */
        //@{
        /** Resize the matrix.
         * \param _size the new dimensions.
         */
        void resize(const IndexType _size) {
            deallocate();
            data = new DataType*[_size];

            for(IndexType i = 0; i < _size; ++i) {
                data[i] = new DataType[i + 1];
                std::fill_n(data[i], i + 1, DataType());
            }
            size = _size;
        }
        //@}

        /** \name Accessors */
        //@{
        /// Access the matrix element-wise. Note the m(x,y) = m(y,x).
        /// \param row the row to be access.
        /// \param col the column to be access.
        DataType& operator()(const IndexType row, const IndexType col) {
            if(col < row)
                return data[row][col];
            else
                return data[col][row];
        }

        /// Return a pointer to the raw data stored. Note that we may change
        /// the values stored there but not the pointer themselves. THIS IS
        /// A DANGEROUS METHOD. DO NOT MODIFY THE POINTERS (modify the data
        /// is OK). USE WITH VERY CARE.
        inline DataType** rawData() const {
            return data;
        }

        /// Returns the number of rows.
        inline IndexType getSize() const {
            return size;
        }
        //@}

    private:
        /// No assignment operators.
        PlainRaggedMatrix& operator=(const PlainRaggedMatrix&) = delete;

    protected:
        /** The data */
        //@{
        IndexType size;     ///< The dimension size.
        DataType** data;    ///< The data.
        //@}

        /** Helper methods */
        //@{
        /// Deallocate data.
        void deallocate() {
            if(data == nullptr)
                return;

            for(IndexType i = 0; i < size; ++i)
                delete[] data[i];

            delete[] data;
        }
        //@}

    public:
        /// Print the instance on ostream output.
        friend std::ostream& operator<<(std::ostream &os, const PlainRaggedMatrix &matrix) {
            for(IndexType i = 0; i < matrix.size; ++i) {
                for(IndexType j = 0; j < i + 1; ++j)
                    os << matrix.data[i][j] << " ";
                os << "\n";
            }
            return os;
        }
};

} //endnamaspace

#endif //PLAIN_RAGGED_MATRIX_HPP_
