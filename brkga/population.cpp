/******************************************************************************
 * population.cpp: Implementation for Population class.
 *
 * Author: Carlos Eduardo de Andrade <ce.andrade@gmail.com>
 *
 * (c) Copyright 2015-2019
 *     Institute of Computing, University of Campinas.
 *     All Rights Reserved.
 *
 *  Created on : Jan 30, 2014 by andrade
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

#include <algorithm>
#include <exception>
#include <stdexcept>

#include "population.hpp"

namespace BRKGA_ALG {

Population::Population(const Population& pop) :
		population(pop.population),
		fitness(pop.fitness)
{}

Population::Population(const unsigned n, const unsigned p) :
		population(p, Chromosome(n, 0.0)), fitness(p)
{
	if(p == 0)
	    throw std::range_error("Population size p cannot be zero.");

	if(n == 0)
	    throw std::range_error("Chromosome size n cannot be zero.");
}

Population::~Population() {}

unsigned Population::getN() const {
	return population[0].size();
}

unsigned Population::getP() const {
	return population.size();
}

double Population::getBestFitness() const {
	return getFitness(0);
}

double Population::getFitness(unsigned i) const {
	return fitness[i].first;
}

const Chromosome& Population::getChromosome(unsigned i) const {
	return population[fitness[i].second];
}

Chromosome& Population::getChromosome(unsigned i) {
	return population[fitness[i].second];
}

void Population::setFitness(unsigned i, double f) {
	fitness[i].first = f;
	fitness[i].second = i;
}

Allele Population::operator()(unsigned chromosome, unsigned allele) const {
	return population[chromosome][allele];
}

Allele& Population::operator()(unsigned chromosome, unsigned allele) {
	return population[chromosome][allele];
}

Chromosome& Population::operator()(unsigned chromosome) {
	return population[chromosome];
}

void Population::sortFitness(bool maximize) {
    if(maximize)
        std::sort(fitness.begin(), fitness.end(), std::greater<std::pair<Allele, unsigned>>());
    else
        std::sort(fitness.begin(), fitness.end(), std::less<std::pair<Allele, unsigned>>());
}

void Population::setType(unsigned chromosome, Chromosome::ChromosomeType type) {
    population[chromosome].type = type;
}
} // end namespace BRKGA_ALG
