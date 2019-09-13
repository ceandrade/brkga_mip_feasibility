/*******************************************************************************
 * chromosome.hpp: Interface for Chromosome class.
 *
 * Author: Carlos Eduardo de Andrade <ce.andrade@gmail.com>
 *
 * (c) Copyright 2015-2019
 *     Institute of Computing, University of Campinas.
 *     Industrial and Systems Engineering, Georgia Institute of Technology
 *
 *     All Rights Reserved.
 *
 *  Created on : Jan 30, 2014 by andrade
 *  Last update: Jun 02, 2015 by andrade
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

#ifndef BRKGA_ALG_CHROMOSOME_HPP_
#define BRKGA_ALG_CHROMOSOME_HPP_

#include <vector>
#include <limits>

namespace BRKGA_ALG {
/**
 * \brief Allele type.
 * \author Carlos Eduardo de Andrade <ce.andrade@gmail.com>
 * \date 2015
 *
 * NOTE: If the type is changed, we MUST modified the random number generator
 * in the BRKGA code. We DO NOT recommend you do that unless you are
 * sure what you are doing.
 */
typedef double Allele;

/**
 * \brief Interface for Chromosome class.
 *
 * This class represents a chromosome to be used in the BRKGA.
 * This is a simple STD vector<> class with additional data. All methods
 * and algorithms that can be used with std::vector must work with this class.
 *
 * \author Carlos Eduardo de Andrade <ce.andrade@gmail.com>
 * \date 2015
 */
class Chromosome: public std::vector<Allele> {
    public:
        /** Enumerations */
        //@{
        /// Type of chromosome during the optimization.
        enum class ChromosomeType {
            RANDOM,    ///< Indicates random/mutant individual.
            OS_RR,     ///< Indicates offspring generated from two random individuals.
            OS_OR,     ///< Indicates offspring generated from one random individual and one offspring.
            OS_OO,     ///< Indicates offspring generated from two other offspring.
        };
        //@}

        /** \name Constructors */
        //@{
        /// Default constructor.
        Chromosome() :
            std::vector<Allele>(), type(ChromosomeType::RANDOM),
            feasibility_pump_value(std::numeric_limits<double>::max()),
            fractionality(std::numeric_limits<double>::max()),
            num_non_integral_vars(std::numeric_limits<unsigned>::max()),
            num_iterations(std::numeric_limits<unsigned>::max()),
            rounded()
            {}

        /**
         * Initialize the chromosome with given size.
         * \param _size sie of the chromosome
         * \param value the value to fill the chromosome
         * \param _type type of the chromosome.
         */
        Chromosome(unsigned _size, Allele value = 0, ChromosomeType _type = ChromosomeType::RANDOM) :
            std::vector<Allele>(_size, value), type(_type),
            feasibility_pump_value(std::numeric_limits<double>::max()),
            fractionality(std::numeric_limits<double>::max()),
            num_non_integral_vars(std::numeric_limits<unsigned>::max()),
            num_iterations(std::numeric_limits<unsigned>::max()),
            rounded(_size, 0)
            {}
        //@}

        /** \name Overload methods */
        //@{
        void resize(size_type __sz) {
            std::vector<Allele>::resize(__sz);
            rounded.resize(__sz, 0);
        }

        void resize(size_type __sz, const_reference __x) {
            std::vector<Allele>::resize(__sz, __x);
            rounded.resize(__sz, 0);
        }

        void reserve(size_type __n) {
            std::vector<Allele>::reserve(__n);
            rounded.resize(__n, 0);
        }

        void shrink_to_fit() noexcept {
            std::vector<Allele>::shrink_to_fit();
            rounded.shrink_to_fit();
        }

        void swap(Chromosome& __x) noexcept {
            std::vector<Allele>::swap(__x);
            rounded.swap(__x.rounded);
        }
        //@}

    public:
        ChromosomeType type;
        double feasibility_pump_value;
        double fractionality;
        unsigned num_non_integral_vars;
        unsigned num_iterations;
        std::vector<int> rounded;
};
} // end namespace BRKGA_ALG

#endif // BRKGA_ALG_CHROMOSOME_HPP_
