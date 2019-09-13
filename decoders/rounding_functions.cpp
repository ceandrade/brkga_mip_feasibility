/******************************************************************************
 * rounding_functions.cpp: Implementation roundings functions used on
 * the feasibility pump.
 *
 * Author: Carlos Eduardo de Andrade
 *         <carlos.andrade@gatech.edu / ce.andrade@gmail.com>
 *
 * (c) Copyright 2015-2019
 *     Industrial and Systems Engineering, Georgia Institute of Technology
 *     All Rights Reserved.
 *
 *  Created on : Apr 21, 2015 by andrade
 *  Last update: Jun 19, 2015 by andrade
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

#include "feasibility_pump_decoder.hpp"

#include "pragma_diagnostic_ignored_header.hpp"
#include "transformers.h"
#include "pragma_diagnostic_ignored_footer.hpp"

#ifdef DEBUG
#include <iostream>
#include <iomanip>
#endif

#include <algorithm>
#include <cmath>
#include <omp.h>

using namespace std;
ILOSTLBEGIN

//----------------------------------------------------------------------------//
// Simple rounding

void FeasibilityPump_Decoder::simpleRounding(const IloNumArray &in,
                                             IloNumArray &out) {
    for(IloInt i = 0; i < in.getSize(); ++i)
        out[i] = std::round(in[i]);
}

//----------------------------------------------------------------------------//
// Rounding with constraint propagation

void FeasibilityPump_Decoder::roundingWithConstraintPropagation(
                                    const IloNumArray &in, IloNumArray &out) {
    #ifdef DEBUG
    cout << "\n--------------------------------\n"
         << "> Rounding with constraint propagation "
         << endl;
    #endif

    #ifdef _OPENMP
    vector<double> &frac = frac_fp_per_thread[omp_get_thread_num()];
    vector<double> &rounded = rounded_fp_per_thread[omp_get_thread_num()];
    dominiqs::SolutionTransformerPtr &frac2int = frac2int_per_thread[omp_get_thread_num()];
    #else
    vector<double> &frac = frac_fp_per_thread[0];
    vector<double> &rounded = rounded_fp_per_thread[0];
    dominiqs::SolutionTransformerPtr &frac2int = frac2int_per_thread[0];
    #endif

    // Copy the fractional values for binary vars.
    for(IloInt i = 0; i < in.getSize(); ++i)
        frac[binary_variables_indices[i]] = in[i];

    // Round.
    frac2int->apply(frac, rounded);

    // Copy back.
    for(IloInt i = 0; i < in.getSize(); ++i)
        out[i] = rounded[binary_variables_indices[i]];

    #ifdef FULLDEBUG
    cout << "Frac Rounded";
    for(size_t i = 0; i < rounded.size(); ++i)
        cout << "\n" << setiosflags(ios::fixed) << setprecision(2)
             << frac[i] << " - " << rounded[i];
    cout << endl;
    #endif
}
