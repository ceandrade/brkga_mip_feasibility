/******************************************************************************
 * brkga_decoder.hpp: Interface for BRKGA Decoder class.
 *
 * Author: Carlos Eduardo de Andrade
 *         <ce.andrade@gmail.com / andrade@ic.unicamp.br>
 *
 * (c) Copyright 2015-2019
 *     Institute of Computing, University of Campinas.
 *     All Rights Reserved.
 *
 *  Created on : Jun 24, 2011 by andrade
 *  Last update: Feb 11, 2015 by andrade
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
 *****************************************************************************/

#ifndef BRKGA_ALG_BRKGA_DECODER_HPP_
#define BRKGA_ALG_BRKGA_DECODER_HPP_

#include "chromosome.hpp"

namespace BRKGA_ALG {
/**
 * \brief BRKGA Decoder Interface.
 * \author Carlos Eduardo de Andrade <ce.andrade@gmail.com>
 * \date 2015
 *
 * This class is a mandatory interface for a chromosome decoder to
 * BRKGA algorithm.
 */
class BRKGA_Decoder {
    public:
        /** \brief This function decodes a chromosome to a solution for
         *         a problem and returns its fitness.
         * \param chromosome a vector of Allele type representing a solution.
         * \param writeback indicates if the chromosome must be rewritten
         * \return a double with fitness value.
         */
        virtual double decode(Chromosome& chromosome, bool writeback = true) = 0;

    public:
        virtual ~BRKGA_Decoder() {}
};
} // end namespace BRKGA_ALG

#endif // BRKGA_BRKGA_DECODER_HPP_
