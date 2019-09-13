/******************************************************************************
 * test_decoder.cpp: Test the decoder.
 *
 * Author: Carlos Eduardo de Andrade <ce.andrade@gmail.com>
 *
 * (c) Copyright 2015-2019
 *     Industrial and Systems Engineering, Georgia Institute of Technology
 *     All Rights Reserved.
 *
 *  Created on : Dec 10, 2014 by andrade
 *  Last update: Ago 05, 2015 by andrade
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
 *****************************************************************************/

#include "feasibility_pump_decoder.hpp"
#include "mtrand.hpp"
#include "population.hpp"

#include <iostream>
#include <fstream>
#include <iomanip>
#include <stdexcept>
#include <algorithm>
#include <vector>
#include <set>
#include <functional>
#include <boost/timer/timer.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string.hpp>

#include "pragma_diagnostic_ignored_header.hpp"
#include <cstring>
#include <ilcplex/ilocplex.h>
#include "pragma_diagnostic_ignored_footer.hpp"

using boost::timer::cpu_timer;
using boost::timer::cpu_times;

using namespace std;

//-------------------------------[ Main ]------------------------------------//

int main(int argc, char* argv[]) {
    if(argc < 4) {
        cerr << "Usage: "<< argv[0] << " <seed> <num-threads> <LP-or-MPS-file>" << endl;
        return 64;
    }

    try {
        const unsigned seed = boost::lexical_cast<unsigned>(argv[1]);
        const unsigned num_threads = boost::lexical_cast<int>(argv[2]);
        const char *instance_file = argv[3];

        cout.flags(ios::fixed);
        //cout << "\n** seed: " << seed
             //<< "\n** num_threads: " << num_threads
             //<< endl;

        MTRand rng(seed);

        FeasibilityPump_Decoder decoder(instance_file, num_threads, seed,
                                        FeasibilityPump_Decoder::PumpStrategy::OBJECTIVE,
                                        FeasibilityPump_Decoder::FitnessType::CONVEX,
                                        1.0, 1.0,
                                        {25, true, 20, -0.3, 0.7},
                                        {0.9, 0.0005},
                                        0.15, 0.05,
                                        FeasibilityPump_Decoder::FixingType::AUTOMATIC,
                                        FeasibilityPump_Decoder::ConstraintFilteringType::ALL);
        decoder.init();

//        Chromosome chr1;
//        chr1.resize(decoder.getChromosomeSize());
//
//        for(size_t j = 0; j < chr1.size(); ++j) {
//            chr1.rounded[j] = floor(rng.rand() + 0.5);
//            chr1[j] = chr1.rounded[j];
//        }
//
//        chr1.back() = 0.3346327643;
//
//        auto chr2 = chr1;
//
//        for(size_t i = 0; i < chr1.size(); ++i)
//            if(fabs(chr1[i] - chr2[i]) > 1e-10)
//                cout << "\n!!! " << chr1[i]  << " != " << chr2[i];
//
//        cout << "\n- pop.size: " << chr1.size();
//        cout << "\n- chr.seed: " << chr1.back();
//        cout.flush();
//
//        decoder.decode(chr1);
//
//        cout << "\n\n* num_non_integral_vars: " << chr1.num_non_integral_vars
//             << "\n* fractionality: " << chr1.fractionality
//             << "\n* feasibility_pump_value: " << chr1.feasibility_pump_value
//             << endl;
//
//        decoder.decode(chr2);
//
//        cout << "\n\n* num_non_integral_vars: " << chr2.num_non_integral_vars
//             << "\n* fractionality: " << chr2.fractionality
//             << "\n* feasibility_pump_value: " << chr2.feasibility_pump_value
//             << endl;
//
//        for(size_t i = 0; i < chr1.size(); ++i)
//            if(fabs(chr1[i] - chr2[i]) > 1e-10)
//                cout << "\n!!! " << chr1[i]  << " != " << chr2[i];
//
        const size_t NUM_CHRS = 4;
        Population pop(decoder.getChromosomeSize(), NUM_CHRS);
        for(size_t i = 0; i < NUM_CHRS; ++i)
            pop.setFitness(i, i * 10);
        pop.sortFitness(false);

        for(size_t i = 0; i < NUM_CHRS; ++i) {
            auto &chr = pop.getChromosome(i);
            for(size_t j = 0; j < chr.size(); ++j) {
                chr.rounded[j] = floor(rng.rand() + 0.2);
                chr[j] = chr.rounded[j];
//                chr[j] = chr.rounded[j] = (i < j? 1.0 : 0.0);   // Ordem direta
//                chr[j] = chr.rounded[j] = (i < j? 0.0 : 1.0);   // Ordem reversa
            }
        }


        #ifdef _OPENMP
        #pragma omp parallel for num_threads(num_threads) schedule(static, 1)
        #endif
        for(size_t i = 0; i < NUM_CHRS; ++i)
            decoder.decode(pop.getChromosome(i));

//
//        auto &ze1 = pop.getChromosome(0);
//        ze1[0] = 1; ze1[1] = 0; ze1[2] = 0; ze1[3] = 1;
//        ze1.rounded[0] = 1; ze1.rounded[1] = 0; ze1.rounded[2] = 0; ze1.rounded[3] = 1;
//
//        auto &ze2 = pop.getChromosome(1);
//        ze2[0] = 1; ze2[2] = 1; ze2[2] = 1; ze2[3] = 1;
//        ze2.rounded[0] = 1; ze2.rounded[1] = 1; ze2.rounded[2] = 1; ze2.rounded[3] = 1;

//        Chromosome* chr = &pop.getChromosome(0);
//        chr->rounded[0] = 1.0;
//        chr->rounded[1] = 0.0;
//        chr->rounded[2] = 1.0;
//        chr->rounded[3] = 1.0;
//        chr->rounded[4] = 0.0;
//        chr->rounded[5] = 1.0;
//
//        chr = &pop.getChromosome(1);
//        chr->rounded[0] = 0.0;
//        chr->rounded[1] = 0.0;
//        chr->rounded[2] = 1.0;
//        chr->rounded[3] = 1.0;
//        chr->rounded[4] = 1.0;
//        chr->rounded[5] = 1.0;
//
//        chr = &pop.getChromosome(2);
//        chr->rounded[0] = 0.0;
//        chr->rounded[1] = 1.0;
//        chr->rounded[2] = 1.0;
//        chr->rounded[3] = 1.0;
//        chr->rounded[4] = 1.0;
//        chr->rounded[5] = 1.0;
//
//        chr = &pop.getChromosome(3);
//        chr->rounded[0] = 0.0;
//        chr->rounded[1] = 1.0;
//        chr->rounded[2] = 1.0;
//        chr->rounded[3] = 1.0;
//        chr->rounded[4] = 1.0;
//        chr->rounded[5] = 1.0;
//
//        chr = &pop.getChromosome(4);
//        chr->rounded[0] = 0.0;
//        chr->rounded[1] = 0.0;
//        chr->rounded[2] = 1.0;
//        chr->rounded[3] = 1.0;
//        chr->rounded[4] = 1.0;
//        chr->rounded[5] = 1.0;

        auto chr = pop.getChromosome(0);
        unsigned actual_num_fixings = 0;

//        decoder.analyzeAndFixVars(pop, pop.getP(),
//                                  FeasibilityPump_Decoder::FixingType::MOST_ZEROS,
//                                  chr, actual_num_fixings);
//
//        cout << "\n- actual_num_fixings: "
//                << (actual_num_fixings / (double) decoder.getNumVariables() * 100)
//                << endl;

        size_t num_unfixed_vars = 0;
        decoder.discrepancy_level = 0.0;

        decoder.performIteratedMIPLocalSearch(pop, pop.getP(), 1, 60.0, chr, num_unfixed_vars);

//        cout << "\n\n";
//        for(IloInt i = 0; i < decoder.binary_variables_per_thread[0].getSize(); ++i) {
//            if(chr[i] > 1e-6 || chr[i] < -1e-6)
//                cout << decoder.binary_variables_per_thread[0][i]
//                     << ": " << chr[i] << "\n";
//        }


////        decoder.addCutsFromRoudings(pop, 6);
////
////        for(size_t i = 0; i < NUM_CHRS; ++i) {
////            auto &chr = pop.getChromosome(i);
////            for(size_t j = 0; j < chr.size(); ++j) {
////                chr.rounded[j] = floor(rng.rand() + 0.5);
////                chr[j] = chr.rounded[j];
////            }
////        }
////
////        decoder.addCutsFromRoudings(pop, 20);
    }
    catch(IloException& e) {
        cout << "\n***********************************************************"
             << "\n*** Exception Occurred: " << e
             << "\n***********************************************************"
             << endl;
    }
    catch(exception& e) {
        cout << "\n***********************************************************"
             << "\n****  Exception Occurred: " << e.what()
             << "\n***********************************************************"
            << endl;
    }

    return 0;
}
