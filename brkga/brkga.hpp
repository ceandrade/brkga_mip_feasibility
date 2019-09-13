/*******************************************************************************
 * brkga.hpp: Biased Random-Key Genetic Algorithm
 *
 * Author: Carlos Eduardo de Andrade
 *         <ce.andrade@gmail.com / andrade@ic.unicamp.br>
 *
 * (c) Copyright 2015-2019
 *     Institute of Computing, University of Campinas.
 *     All Rights Reserved.
 *
 *  Created on : Jan 06, 2015 by andrade
 *  Last update: Jun 25, 2015 by andrade
 *
 *  This software is based on the work of Rodrigo Franco Toso, available at
 *  https://github.com/rfrancotoso/brkgaAPI. The most parts of the evolution
 *  code are from the original work with some refactoring. The new stuff is:
 *  - setInitialPopulation();
 *  - initialize();
 *
 *  Created on : Jun 22, 2010 by rtoso
 *      Authors: Rodrigo Franco Toso <rtoso@cs.rutgers.edu>
 *               Carlos Eduardo de Andrade <ce.andrade@gmail.com>
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
 * POSSIBILITY OF SUCH DAMAGE.
 ******************************************************************************/

#ifndef BRKGA_BRKGA_HPP_
#define BRKGA_BRKGA_HPP_

#include <iostream>
using std::cout;
using std::endl;

#include <omp.h>
#include <vector>
#include <deque>
#include <utility>
#include <set>
#include <algorithm>
#include <exception>
#include <stdexcept>
#include <limits>
#include <ctime>
#include <sys/time.h>

#include "population.hpp"

/**
 * This namespace contains all stuff related to BRKGA
 * Relink.
 */
namespace BRKGA_ALG {

/** This class encapsulates a Biased Random-key Genetic Algorithm with K
 * independent Populations stored in two vectors of Population, current and
 * previous. It supports multi-threading via OpenMP, and implements the
 * following key methods:
 *
 * - BRKGA() constructor: initializes the populations with parameters
 *   described below.
 *
 * - evolve() operator: evolve each Population following the BRKGA
 *   methodology. This method supports OpenMP to evolve up to K independent
 *   Populations in parallel. Please note that double Decoder::decode(...) MUST
 *   be thread-safe.
 *
 * Required hyperparameters:
 * - n: number of genes in each chromosome
 * - p: number of elements in each population
 * - pe: pct of elite items into each population
 * - pm: pct of mutants introduced at each generation into the population
 * - rhoe: probability that an offspring inherits the allele of its elite
 *   parent
 *
 * Optional parameters:
 * - K: number of independent Populations
 * - MAX_THREADS: number of threads to perform parallel decoding -- WARNING:
 *   Decoder::decode() MUST be thread-safe!
 *
 * Required templates are:
 *
 * RNG: random number generator that implements the methods below.
 *
 * - RNG(unsigned long seed) to initialize a new RNG with 'seed'
 * - double rand() to return a double precision random deviate in range [0,1)
 * - unsigned long randInt() to return a >= 32-bit unsigned random deviate
 *   in range [0,2^32-1)
 * - unsigned long randInt(N) to return a unsigned random deviate in range
 *   [0, N] with N < 2^32
 *
 * Decoder: problem-specific decoder that implements any of the decode methods
 * outlined below. When compiling and linking BRKGA with -fopenmp (i.e.,
 * with multithreading support via OpenMP), the method must be thread-safe.
 * - double decode(const vector<double>& chromosome) const, if you don't want
 *   to change chromosomes inside the framework, or
 * - double decode(vector<double>& chromosome) const, if you'd like to
 *   update a chromosome
 *
 * \author Carlos Eduardo de Andrade <ce.andrade@gmail.com>
 * \author Rodrigo Franco Toso <rtoso@cs.rutgers.edu>
 * \date 2015
 */
template<class Decoder, class RNG>
class BRKGA {
public:
    /** \name Enumerations */
    //@{
    /// Specifies objective as minimization or maximization.
    enum class Sense {
        MINIMIZE = false,  ///< Minimization
        MAXIMIZE = true    ///< Maximization
    };
    //@}

public:
    /**
     * Default constructor
     *
     * \param n number of genes in each chromosome
     * \param p number of elements in each population
     * \param pe percentage of elite items into each population
     * \param pm percentage of mutants introduced at each generation into the population
     * \param rhoe probability that an offspring inherits the allele of its elite parent
     * \param K number of independent Populations
     * \param MAX_THREADS number of threads to perform parallel decoding
     *        WARNING: Decoder::decode() MUST be thread-safe; safe if implemented as
     *        double Decoder::decode(Chromosome& chromosome) const
     * \param cut_point define a intermediate point of the chromosome. In the
     *        left side of the cut point, the alleles should be values between
     *        left_lb and left_ub. In the right side of the cut point, the
     *        alleles should be values between right_lb and right_ub. NOTE: this
     *        is completely dependent of the allele type. If you modify the
     *        allele type, you must modify all points where the random number
     *        generator #rng is called in this class. WE DO NOT PROVIDE AN
     *        AUTOMATIC WAY TO DO THAT. Perhaps, in the future, we may implement
     *        this using traits
     * \param left_lb the minimum number to be generate randomly in the left side of the cut point
     * \param left_ub the maximum number to be generate randomly in the left side of the cut point
     * \param right_lb the minimum number to be generate randomly in the right side of the cut point
     * \param right_ub the maximum number to be generate randomly in the right side of the cut point
     */
    BRKGA(unsigned n, unsigned p, double pe, double pm, double rhoe,
          Decoder& refDecoder, RNG& refRNG, unsigned K = 1,
          Sense sense = Sense::MINIMIZE,
          unsigned MAX_THREADS = 1,
          Allele left_lb = std::numeric_limits< Allele >::min(),
          Allele left_ub = std::numeric_limits< Allele >::max(),
          unsigned cut_point = 0,
          Allele right_lb = std::numeric_limits< Allele >::min(),
          Allele right_ub = std::numeric_limits< Allele >::max());

    /**
     * Destructor
     */
    ~BRKGA();

    /**
     * Set individuals to initial population (only one population in case of multiple ones).
     * \param chromosomes a set of individuals described as double vectors
     *        between 0 and 1.
     */
    void setInitialPopulation(const std::vector< Chromosome >& chromosomes);


    /**
     * Initialize the populations and others parameters of the algorithm. If a
     * initial population is supplied, this method completes the remain
     * individuals, if they not exist.
     * \param true_init indicates a true initialization or other from reset phase.
     */
    void initialize(bool true_init = true);

    /**
     * Resets all populations with brand new keys
     * \param partial_reset if true indicates partial reset considering the initial population.
     */
    void reset(bool partial_reset = false);

    /**
     * Evolve the current populations following the guidelines of BRKGAs
     * \param generations number of generations (must be even and nonzero)
     */
    void evolve(unsigned generations = 1);

    /**
     * Exchange elite-solutions between the populations
     * \param M number of elite chromosomes to select from each population
     */
    void exchangeElite(unsigned M);

    /**
     * Returns the current population
     */
    const Population& getCurrentPopulation(unsigned k = 0) const;

    /**
     * Returns a reference to the chromosome with best fitness so far among
     * all populations
     */
    const Chromosome& getBestChromosome() const;

    /**
     * Returns the best fitness found so far among all populations
     */
    double getBestFitness() const;

private:
    /** \name Hyperparameters */
    //@{
    const unsigned n;   ///< Number of genes in the chromosome
    const unsigned p;   ///< Number of elements in the population
    const unsigned pe;  ///< Number of elite items in the population
    const unsigned pm;  ///< Number of mutants introduced at each generation into the population
    const double rhoe;  ///< Probability that an offspring inherits the allele of its elite parent
    //@}

    /** \name Templates */
    //@{
    RNG& refRNG;            ///< Reference to the random number generator
    Decoder& refDecoder;    ///< Reference to the problem-dependent Decoder
    //@}

    /** \name Parallel populations parameters */
    //@{
    const unsigned K;               ///< Number of independent parallel populations
    const unsigned MAX_THREADS;     ///< Number of threads for parallel decoding
    //@}

    /** \name Parameters to limit the allele generation */
    //@{
    const Allele left_lb;     ///< Mininum allele value on left section
    const Allele left_ub;     ///< Maximum allele value on left section
    const unsigned cut_point;             ///< Separate the chromosome in two sections
    Allele right_lb;          ///< Mininum allele value on right section
    Allele right_ub;          ///< Maximum allele value on right section
    //@}

    /** \name Data */
    //@{
    std::vector< Population* > previous;    ///< Previous populations
    std::vector< Population* > current;     ///< Current populations
    bool initialPopulation;                 ///< Indicate if a initial population is set
    bool initialized;                       ///< Indicate if the algorithm was proper initialized
    bool reset_phase;                       ///< Indicate if the algorithm have been reset
    bool maximize;                          ///< Indicate if is maximization or minimization
    //@}

    /** Local methods */
    //@{
    /**
     * Evolve the current population to the next
     * \param curr current population
     * \param next next population
     */
    void evolution(Population& curr, Population& next);

    //bool isRepeated(const Chromosome& chrA, const Chromosome& chrB) const;

    /**
     * Returns true if a1 is better than a2. This method depends on the
     * type of the alleles and the optimization sense. The Allele type
     * must implement the less operator "<". In most cases, Allele is a double
     * and we do not have to mind about the operator "<".
     * \param a1 allele value
     * \param a2 allele value
     */
    bool betterThan(Allele a1, Allele a2) const;
    //@}
};

template<class Decoder, class RNG>
BRKGA<Decoder, RNG>::BRKGA(unsigned _n, unsigned _p, double _pe, double _pm, double _rhoe,
        Decoder& decoder, RNG& rng, unsigned _K, Sense sense, unsigned MAX, Allele _left_lb,
        Allele _left_ub, unsigned _cut_point,
        Allele _right_lb, Allele _right_ub):
        n(_n), p(_p),
        pe(unsigned(_pe * p)), pm(unsigned(_pm * p)), rhoe(_rhoe),
        refRNG(rng), refDecoder(decoder), K(_K), MAX_THREADS(MAX),
        left_lb(_left_lb), left_ub(_left_ub), cut_point(_cut_point),
        right_lb(_right_lb), right_ub(_right_ub), previous(K, 0),
        current(K, 0), initialPopulation(false), initialized(false),
        reset_phase(false),
        maximize(sense == Sense::MAXIMIZE)
{
    // Error check:
    using std::range_error;
    if(n == 0) { throw range_error("Chromosome size equals zero."); }
    if(p == 0) { throw range_error("Population size equals zero."); }
    if(pe == 0) { throw range_error("Elite-set size equals zero."); }
    if(pe > p) { throw range_error("Elite-set size greater than population size (pe > p)."); }
    if(pm > p) { throw range_error("Mutant-set size (pm) greater than population size (p)."); }
    if(pe + pm > p) { throw range_error("elite + mutant sets greater than population size (p)."); }
    if(K == 0) { throw range_error("Number of parallel populations cannot be zero."); }

    if(cut_point == 0) {
        right_lb = left_lb;
        right_ub = left_ub;
    }
}

template<class Decoder, class RNG>
BRKGA<Decoder, RNG>::~BRKGA() {
    for(unsigned i = 0; i < K; ++i) { delete current[i]; delete previous[i]; }
}

template<class Decoder, class RNG>
inline bool BRKGA<Decoder, RNG>::betterThan(Allele a1, Allele a2) const {
    return !(a1 < a2) != !(maximize);
}

template<class Decoder, class RNG>
const Population& BRKGA<Decoder, RNG>::getCurrentPopulation(unsigned k) const {
    return (*current[k]);
}

template<class Decoder, class RNG>
double BRKGA<Decoder, RNG>::getBestFitness() const {
    double best = current[0]->fitness[0].first;
    for(unsigned i = 1; i < K; ++i) {
        if(betterThan(current[i]->fitness[0].first, best)) { best = current[i]->fitness[0].first; }
    }

    return best;
}

template<class Decoder, class RNG>
const Chromosome& BRKGA<Decoder, RNG>::getBestChromosome() const {
    unsigned bestK = 0;
    for(unsigned i = 1; i < K; ++i)
        if(betterThan(current[i]->getBestFitness(), current[bestK]->getBestFitness()) ) { bestK = i; }
    return current[bestK]->getChromosome(0);    // The top one :-)
}

template<class Decoder, class RNG>
void BRKGA<Decoder, RNG>::reset(bool partial_reset) {
    if(!initialized) {
        throw std::runtime_error("The algorithm hasn't been initialized. Don't forget to call initialize() method");
    }
    reset_phase = true;
    initialize(partial_reset);
}

template<class Decoder, class RNG>
void BRKGA<Decoder, RNG>::evolve(unsigned generations) {
    if(!initialized) {
        throw std::runtime_error("The algorithm hasn't been initialized. Don't forget to call initialize() method");
    }

    if(generations == 0) { throw std::range_error("Cannot evolve for 0 generations."); }

    for(unsigned i = 0; i < generations; ++i) {
        for(unsigned j = 0; j < K; ++j) {
            evolution(*current[j], *previous[j]);   // First evolve the population (curr, next)
            std::swap(current[j], previous[j]);     // Update (prev = curr; curr = prev == next)
        }
    }
}

template<class Decoder, class RNG>
void BRKGA<Decoder, RNG>::exchangeElite(unsigned M) {
    if(M == 0 || M >= p) { throw std::range_error("M cannot be zero or >= p."); }

    #ifdef _OPENMP
        #pragma omp parallel for num_threads(MAX_THREADS)
    #endif
    for(int i = 0; i < int(K); ++i) {
        // Population i will receive some elite members from each Population j below:
        unsigned dest = p - 1;  // Last chromosome of i (will be overwritten below)
        for(unsigned j = 0; j < K; ++j) {
            if(j == unsigned(i)) { continue; }

            // Copy the M best of Population j into Population i:
            for(unsigned m = 0; m < M; ++m) {
                // Copy the m-th best of Population j into the 'dest'-th position of Population i:
                const Chromosome& bestOfJ = current[j]->getChromosome(m);

                std::copy(bestOfJ.begin(), bestOfJ.end(), current[i]->getChromosome(dest).begin());
                current[i]->fitness[dest].first = current[j]->fitness[m].first;

                --dest;
            }
        }
    }

    // PARALLEL REGION: re-sort each population since they were modified
    #ifdef _OPENMP
        #pragma omp parallel for num_threads(MAX_THREADS)
    #endif
    for(int i = 0; i < int(K); ++i) { current[i]->sortFitness(maximize); }
}

template<class Decoder, class RNG>
void BRKGA<Decoder, RNG>::setInitialPopulation(const std::vector< Chromosome >& chromosomes) {
//    if(initialPopulation) {
//        throw std::runtime_error("You cannot set initial population twice!");
//    }

    if(initialPopulation)
        delete current[0];

    current[0] = new Population(n, chromosomes.size());
    unsigned i = 0;

    for(std::vector< Chromosome >::const_iterator it_chrom = chromosomes.begin();
        it_chrom != chromosomes.end(); ++it_chrom, ++i) {

        if(it_chrom->size() != n) {
            throw std::runtime_error("Error on setting initial population: number of genes isn't equal!");
        }
        std::copy(it_chrom->begin(), it_chrom->end(), current[0]->population[i].begin());
    }
    initialPopulation = true;
}

template<class Decoder, class RNG>
void BRKGA<Decoder, RNG>::initialize(bool true_init) {
    unsigned start = 0;

    // Verify the initial population and complete or prune it!
    if(initialPopulation && true_init) {
        if(current[0]->population.size() < p) {
            Population* pop = current[0];
            Chromosome chromosome(n);
            unsigned last_chromosome = pop->population.size();

            pop->population.resize(p);
            pop->fitness.resize(p);

            for(; last_chromosome < p; ++last_chromosome) {
                for(unsigned k = 0; k < n; ++k) {
                    //TODO: fix this.
                    chromosome[k] = Allele(refRNG.rand());         // for doubles
                    //chromosome[k] = refRNG.randInt(n, unsigned(-(n + 1)));   // for ints
                    //if(k < cut_point)
                    //    chromosome[k] = refRNG.randInt(left_lb, left_ub);
                    //else
                    //    chromosome[k] = refRNG.randInt(right_lb, right_ub);
                }
                pop->population[last_chromosome] = chromosome;
            }
        }
        // Prune some additional chromosomes
        else if(current[0]->population.size() > p) {
            current[0]->population.resize(p);
            current[0]->fitness.resize(p);
        }
        start = 1;
    }

    // Initialize each chromosome of the current population
    for(; start < K; ++start) {
        // Allocate:
        if(!reset_phase)
            current[start] = new Population(n, p);

        for(unsigned j = 0; j < p; ++j) {
            for(unsigned k = 0; k < n; ++k) {
                //TODO: fix this.
                (*current[start])(j, k) = Allele(refRNG.rand());
                //(*current[start])(j, k) = refRNG.randInt(n, unsigned(-(n + 1)));
                //if(k < cut_point)
                //    (*current[start])(j, k) = refRNG.randInt(left_lb, left_ub);
                //else
                //    (*current[start])(j, k) = refRNG.randInt(right_lb, right_ub);
            }
        }
    }

    // Initialize and decode each chromosome of the current population, then copy to previous:
    for(unsigned i = 0; i < K; ++i) {
        // Decode:
        #ifdef _OPENMP
            #pragma omp parallel for num_threads(MAX_THREADS) schedule(static,1)
        #endif
        for(int j = 0; j < int(p); ++j) {
            current[i]->setFitness(j, refDecoder.decode((*current[i])(j)) );
        }

        // Sort:
        current[i]->sortFitness(maximize);

        // Then just copy to previous:
        if(!reset_phase)
            previous[i] = new Population(*current[i]);

        if(reset_phase && true_init) {
            delete previous[i];
            previous[i] = new Population(*current[i]);
        }
    }

    initialized = true;
}

template<class Decoder, class RNG>
inline void BRKGA<Decoder, RNG>::evolution(Population& curr, Population& next) {
    // We now will set every chromosome of 'current', iterating with 'i':
    unsigned i = 0; // Iterate chromosome by chromosome
    unsigned j = 0; // Iterate allele by allele

    // 2. The 'pe' best chromosomes are maintained, so we just copy these into 'current':
    while(i < pe) {
//        for(j = 0 ; j < n; ++j) { next(i,j) = curr(curr.fitness[i].second, j); }
        next.population[i] = curr.population[curr.fitness[i].second];

        next.fitness[i].first = curr.fitness[i].first;
        next.fitness[i].second = i;
        ++i;
    }

    // 3. We'll mate 'p - pe - pm' pairs; initially, i = pe, so we need to iterate until i < p - pm:
    while(i < p - pm) {
        // Select an elite parent:
        const unsigned eliteParent = (refRNG.randInt(pe - 1));

        // Select a non-elite parent:
        const unsigned noneliteParent = pe + (refRNG.randInt(p - pe - 1));

        // Mate:
        for(j = 0; j < n; ++j) {
            const unsigned sourceParent = ((refRNG.rand() < rhoe) ? eliteParent : noneliteParent);

            next(i, j) = curr(curr.fitness[sourceParent].second, j);

            //next(i, j) = (refRNG.rand() < rhoe) ? curr(curr.fitness[eliteParent].second, j) :
            //                                    curr(curr.fitness[noneliteParent].second, j);
        }

        typedef Chromosome::ChromosomeType LocalChrType;
        LocalChrType type = LocalChrType::OS_OR;

        if(curr.population[eliteParent].type == LocalChrType::RANDOM &&
           curr.population[noneliteParent].type == LocalChrType::RANDOM)
            type = LocalChrType::OS_RR;
        else
        if(curr.population[eliteParent].type != LocalChrType::RANDOM &&
           curr.population[noneliteParent].type != LocalChrType::RANDOM)
            type = LocalChrType::OS_OO;

        next.setType(i, type);
        ++i;
    }

    // We'll introduce 'pm' mutants:
    while(i < p) {
        //TODO: fix this.
        for(j = 0; j < n; ++j) {
            next(i, j) = Allele(refRNG.rand());
            //next(i, j) = refRNG.randInt(n, unsigned(-(n + 1)));
            //if(j < cut_point)
            //    next(i, j) = refRNG.randInt(left_lb, left_ub);
            //else
            //    next(i, j) = refRNG.randInt(right_lb, right_ub);
        }

        next.setType(i, Chromosome::ChromosomeType::RANDOM);
        ++i;
    }

    // Time to compute fitness, in parallel:
    #ifdef _OPENMP
        #pragma omp parallel for num_threads(MAX_THREADS) schedule(static,1)
    #endif
    for(int ii = int(pe); ii < int(p); ++ii) {
        next.setFitness(ii, refDecoder.decode(next.population[ii]) );
    }

    // Now we must sort 'current' by fitness, since things might have changed:
    next.sortFitness(maximize);
}
} // end namespace BRKGA_ALG

#endif // BRKGA_BRKGA_HPP_
