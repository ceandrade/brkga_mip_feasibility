/******************************************************************************
 * execution_stopper.hpp: Interface for ExecutionStopper class.
 *
 * Author: Carlos Eduardo de Andrade
 *         <carlos.andrade@gatech.edu / ce.andrade@gmail.com>
 *
 * (c) Copyright 2015-2019
 *     Industrial and Systems Engineering, Georgia Institute of Technology
 *     All Rights Reserved.
 *
 *  Created on : May 19, 2015 by andrade
 *  Last update: Ago 11, 2015 by andrade
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

#ifndef EXECUTIONSTOPPER_HPP_
#define EXECUTIONSTOPPER_HPP_

#include <boost/timer/timer.hpp>
#include <signal.h>

/**
 * \brief ExecutionStopper class.
 *
 * \author Carlos Eduardo de Andrade <ce.andrade@gmail.com>
 * \date 2015
 *
 * This class is a singleton used during the algorithm execution to stop
 * by time or user interference. It holds a timer that be controlled from
 * external calls. It also overwrite the Ctrl-C signal handling to allow a
 * softer finish of the algorithm.
 */
class ExecutionStopper {
    public:
        /** \name Enumerations */
        //@{
        /// Defines with time use to stop.
        enum class TimeType{
            WALL,   ///< This is the regular wall clock time.
            CPU     ///< This is the CPU/user time.
        };
        //@}

    public:
        /** \name Public interface */
        //@{
        /** Initialize the singleton. Must be call before any other method.
         * \param max_time the maximum time in seconds.
         * \param type of the time to be considered.
         */
        static void init(boost::timer::nanosecond_type max_time,
                         TimeType type = TimeType::WALL);

        /// Start the timer.
        static void timerStart();

        /// Stop the timer.
        static void timerStop();

        /// Resume the timer.
        static void timerResume();

        /// Indicates if we must stop.
        static bool mustStop();

        /// Returns the elapsed time.
        static boost::timer::cpu_times elapsed();
        //@}

    private:
        /** \name Private methods to avoid creation and copy */
        ExecutionStopper();
        ExecutionStopper(const ExecutionStopper&) = delete;
        ~ExecutionStopper();
        ExecutionStopper& operator=(const ExecutionStopper&) = delete;
        //@}

    protected:
        /** \name Internals */
        //@{
        /// Get a reference for an instance.
        static ExecutionStopper& instance();
        //@}

        /** \name Ctr-C handler */
        //@{
        /// Function used to emit the STOP signal.
        static void userSignalBreak(int signum);
        //@}

        /** \name Data members */
        //@{
        ///
        /// The maximum time.
        boost::timer::nanosecond_type max_time;

        /// The type of the time to be used to stop.
        TimeType time_type;

        /// The timer
        boost::timer::cpu_timer timer;

        /// Holds a pointer to the previous Ctrl-C handler.
        void (*previousHandler)(int);

        /// Indicates if the initialization method was called.
        bool initialized;

        /// Indicates if a STOP signal was emitted.
        bool stopsign;
        //@}
};

#endif //EXECUTIONSTOPPER_HPP_
