/******************************************************************************
 * execution_stopper.cpp: Implementation for ExecutionStopper class.
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

#include "execution_stopper.hpp"
#include <numeric>
#include <exception>
#include <string>
#include <limits>

#include <iostream>

//------------------[ Default Constructor and Destructor ]--------------------//

ExecutionStopper::ExecutionStopper():
    max_time(std::numeric_limits<decltype(max_time)>::max()),
    time_type(TimeType::WALL),
    timer(),
    previousHandler(nullptr),
    initialized(false),
    stopsign(false)
{}

ExecutionStopper::~ExecutionStopper() {}

//------------------[ Default Constructor and Destructor ]--------------------//

ExecutionStopper& ExecutionStopper::instance() {
    static ExecutionStopper inst;
    return inst;
}

//-----------------------------[ Timer methods ]------------------------------//

void ExecutionStopper::timerStart() {
    instance().timer.start();
}

void ExecutionStopper::timerStop() {
    instance().timer.stop();
}

void ExecutionStopper::timerResume() {
    instance().timer.resume();
}

boost::timer::cpu_times ExecutionStopper::elapsed() {
    return instance().timer.elapsed();
}

//----------------------------[ Initialization ]------------------------------//

void ExecutionStopper::init(boost::timer::nanosecond_type _max_time, TimeType type) {
    auto &inst = instance();
    inst.previousHandler = signal(SIGINT, userSignalBreak);
    inst.max_time = _max_time * 1e9;
    inst.time_type = type;
    inst.initialized = true;
}

//----------------------[ Check if it is time to stop ]-----------------------//

bool ExecutionStopper::mustStop() {
    auto &inst = instance();
    auto current_time = (inst.time_type == TimeType::WALL? inst.timer.elapsed().wall :
                         inst.timer.elapsed().user + inst.timer.elapsed().system);
    return (current_time > inst.max_time) || inst.stopsign;
}

//----------------------------[ Ctrl-C handler ]------------------------------//

void ExecutionStopper::userSignalBreak(int /*signum*/) {
    std::cerr << "\n\n> Ctrl-C detected. Aborting execution. "
              << "Type Ctrl-C once more for exit immediately" << std::endl;

    instance().stopsign = true;
    signal(SIGINT, instance().previousHandler);
}
