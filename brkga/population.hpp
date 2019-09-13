/******************************************************************************
 * population.hpp: Interface for Population class.
 *
 * Author: Carlos Eduardo de Andrade <ce.andrade@gmail.com>
 *
 * (c) Copyright 2015-2019
 *     Institute of Computing, University of Campinas.
 *     All Rights Reserved.
 *
 *  Created on : Jun 21, 2010 by rtoso
 *  Last update: Feb 11, 2015 by andrade

 *  This software is based on the work of Rodrigo Franco Toso, available at
 *  https://github.com/rfrancotoso/brkgaAPI. The most parts of the code are
 *  from the original work with minor refactoring.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE
 *****************************************************************************/

#ifndef BRKGA_ALG_POPULATION_HPP_
#define BRKGA_ALG_POPULATION_HPP_

#include "chromosome.hpp"

#include <vector>
#include <utility>
#include <stdint.h>

namespace BRKGA_ALG {
/** \brief Encapsulates a population of chromosomes represented by a vector of
 * doubles
 *
 * This software is based on the work of Rodrigo Franco Toso, available at
 * https://github.com/rfrancotoso/brkgaAPI. The most parts of the code are
 * from the original work with minor refactoring.
 *
 * Original note from rtoso:
 * Encapsulates a population of chromosomes represented by a vector of doubles.
 * We don't decode nor deal with random numbers here; instead, we provide
 * private support methods to set the fitness of a specific chromosome as well
 * as access methods to each allele. Note that the BRKGA class must have access
 * to such methods and thus is a friend.
 *
 * \author Carlos Eduardo de Andrade <ce.andrade@gmail.com>
 * \author Rodrigo Franco Toso <rtoso@cs.rutgers.edu>
 * \date 2015
 */
class Population {
    template<class Decoder, class RNG>
    friend class BRKGA;

public:
    /** \name Default constructors and destructor */
    //@{
    /// Copy constructor.
    Population(const Population& other);

    /** Default constructor
     * \param n size of chromosome
     * \param p size of population
     */
    Population(unsigned n, unsigned p);

    /// Destructor
    ~Population();
    //@}

    /** Simple access methods */
    //@{
    /// Return the size of each chromosome
    unsigned getN() const;

    /// Return the size of the population
    unsigned getP() const;

    /// Direct access to allele j of chromosome i
    Allele operator()(unsigned i, unsigned j) const;
    //@}

    /** Special access methods
     *
     * These methods REQUIRE fitness to be sorted, and thus a call to
     * sortFitness() beforehand.
     */
    //@{
    /// Returns the best fitness in this population
    double getBestFitness() const;

    /// Returns the fitness of chromosome i
    double getFitness(unsigned i) const;

    /// Returns i-th best chromosome
    const Chromosome& getChromosome(unsigned i) const;
    //@}

    /** Set the type of chromosome
     * \param chromosome index of chromosome
     * \param type type of chromosome
     */
    void setType(unsigned chromosome, Chromosome::ChromosomeType type);

public:
//private:
    /// Population as vectors of probabilities
    std::vector<Chromosome> population;

    /// Fitness (double) of a each chromosome
    std::vector<std::pair<double, unsigned>> fitness;

    /** Sorts 'fitness' by its first parameter by predicate template class.
     * \param maximize if true, sort in non-increasing order.
     */
    void sortFitness(bool maximize);

    /** Sets the fitness of chromosome
     * \param i index of chromosome
     * \param f value
     */
    void setFitness(unsigned i, double f);

    /// Returns a chromosome
    Chromosome& getChromosome(unsigned i);

    /// Direct access to allele j of chromosome i
    Allele& operator()(unsigned i, unsigned j);

    /// Direct access to chromosome i
    Chromosome& operator()(unsigned i);
};
} // end namespace BRKGA_ALG

#endif //BRKGA_ALG_POPULATION_HPP_
