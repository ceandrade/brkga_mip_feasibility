/******************************************************************************
 * fix_and_local_search_random.cpp: fixing from random vectors.
 *
 * Author: Carlos Eduardo de Andrade
 *         <carlos.andrade@gatech.edu / ce.andrade@gmail.com>
 *
 * (c) Copyright 2015-2019
 *     Industrial and Systems Engineering, Georgia Institute of Technology
 *     All Rights Reserved.
 *
 * Created on : Ago 11, 2015 by andrade
 * Last update: Ago 12, 2015 by andrade
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
#include "brkga.hpp"
#include "distance_function_base.hpp"
#include "hamming_distance.hpp"
#include "execution_stopper.hpp"

#include <iostream>
#include <fstream>
#include <iomanip>
#include <stdexcept>
#include <algorithm>
#include <iterator>
#include <limits>
#include <utility>
#include <cstdlib>
#include <cmath>
#include <unordered_set>
#include <vector>

#include "pragma_diagnostic_ignored_header.hpp"
#include <boost/timer/timer.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/filesystem.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include "pragma_diagnostic_ignored_footer.hpp"

using namespace std;
using namespace BRKGA_ALG;

const double EPS = 1e-6;

//-------------------------[ Some control constants ]-------------------------//

// Controls stop criteria.
enum class StopRule{GENERATIONS = 'G', TARGET = 'T', IMPROVEMENT = 'I'};

//-------------------------[ Information functions ]--------------------------//

void show_usage_info(const char* exec_name, bool full) {
    cerr << "usage: " << exec_name
         << " <config-file> <seed> <stop-rule> <stop-arg> <max_time>"
         << " <LP-or-MPS-file> <output-dir> <initial-population> <pump-strategy>"
         << " OBJ <fitness-type> <minimization-factor>"
         << " <minimization-factor-decay> <decay-application-offset>"
         << " FP <feas-pump-param: iteration_limit>"
         << " <feas-pump-param: perturb_when_cycling> <feas-pump-param: t>"
         << " <feas-pump-param: rho_lb> <feas-pump-param: rho_ub>"
         << " OFP <obj-feas-pump-param: phi> <obj-feas-pump-param: delta>"
         << " FIXING <var_fixing_percentage> <var_fixing_growth_rate>"
         << " <var_fixing_frequency> <var_fixing_type>"
         << " ROUNDINGCUT <roundcuts_percentage>"
         << " MIPLOCALSEARCH <miplocalsearch-threshold> <miplocalsearch-discrepancy_level>"
         << " <miplocalsearch-unfix-levels> <miplocalsearch-max-time>"
         << " <miplocalsearch-constraint-filtering>"
        #ifdef TUNING
         << " <max_population_size> <elite-percentage> <mutants-percentage> <biasing-rhoe>"
         << " <number-of-populations> <exchange_interval> <num_exchange_indivuduals>"
         << " <reset_interval>"
        #endif
         << endl;

    if(!full) {
        cerr << "\n-- Please, type " << exec_name << " --help for complete parameter description"
             << endl;
        #ifdef TUNING
        cerr << "\n-- THIS IS THE TUNING VERSION!" << endl;
        #endif
    }
    else
    cerr << "\nwhere: "
         << "\n - <config-file>: parameters of BRKGA algorithm."
         << "\n - <seed>: seed for random generator."
         << "\n - <stop-rule> <stop-arg>: stop rule and its arguments where:"
         << "\n\t+ (G)enerations <number_generations>: the algorithm runs until <number_generations>;"
         << "\n\t+ (T)arget <value of expected target>: runs until obtains the target value;"
         << "\n\t+ (I)terations <max generations without improvement>: runs until the solutions don't."
         << "\n - <max-time>: max running time (in seconds). If 0, the algorithm stops on chosen stop rule."
         << "\n - <LP-or-MPS-file>: describes the service wire center (subject locations and demands)."
         << "\n - <output-dir>: folder to save the results. All files will have the <LP-or-MPS-file>"
         << "\n   prefix in their names."
         << "\n - <initial-population>: number of individuals in the initial relaxation."
         << "\n   The first individual is a full relaxation of the model, and the others"
         << "\n   are built fixing binary variables individually:"
         << "\n\t+ < 0: number of relaxation is at most the size of the population;"
         << "\n\t+ == 0: no initial population;"
         << "\n\t+ > 0: neither the given number, or the number of binary variables at most."
         << "\n - <pump-strategy>: the feasibility pump strategy:"
         << "\n\t+ Default: default feasibility pump using only the distance function."
         << "\n\t+ Objective: feasibility pump using a convex combination between the"
         << "\n\t             distance function and the original objective function."

         << "\n\n*** OBJ"
         << "\n - <fitness-type>: defines how the fitness is computed:"
         << "\n\t+ Convex: the convex combination (beta * Delta) + (1 - beta) * zeta"
         << "\n\t  where \"Delta\" is the the distance between a LP feasible and"
         << "\n\t  an integer solution (as in the default feasibility pump), and"
         << "\n\t  \"zeta\" is the measure of infeasibility, usually the number of fractional variables. "
         << "\n\t+ Geometric: the convex combination Delta^beta x zeta^(1 - beta)."
         << "\n - <minimization-factor: beta>: A factor in the range [0,1]. It is used to control the"
         << "\n   direction of the optimization. Note the when beta is 1.0, only"
         << "\n    Delta is used. When it is 0.0, only the measure of infeasibility is used."
         << "\n - <minimization-factor-decay>: it is used to change the direction of the optimization."
         << "\n   Usually, this is done using a geometric decay. If it is equal to 1.0, nothing is changed."
         << "\n - <decay-application-offset>: the number of iterations without improvement before apply"
         << "\n   the decay in minimization factor."

         << "\n\n*** FP"
         << "\n - <feas-pump-param: iteration_limit>: maximum number of iterations without improvement."
         << "\n - <feas-pump-param: perturb_when_cycling>: indicates if a perturbation must be done when FP cycles."
         << "\n\t+ Perturb: does the shaking."
         << "\n\t+ NotPerturb: does not do the shaking."
         << "\n - <feas-pump-param: t>: parameter used to control the weak perturbation in the"
         << "\n   cycling detection."
         << "\n - <feas-pump-param: rho_lb>: Parameter used to control the strong perturbation"
         << "\n   in the cycling detection. This is the lower bound."
         << "\n - <feas-pump-param: rho_ub>: Parameter used to control the strong perturbation"
         << "\n   in the cycling detection. This is the upper bound."

         << "\n\n*** OFP"
         << "\n - <obj-feas-pump-param: phi>: this is the decay factor used to change the"
         << "\n   objective function in the LP phase if using objective feasibility pump."
         << "\n - <obj-feas-pump-param: delta>: this is the minimum difference between two"

         << "\n\n*** FIXING"
         << "\n   iterations. This parameter is used to detect cycling in the objective feasibility pump."
         << "\n - <var_fixing_percentage>: percentage of variables to be fixed. Range [0,1]."
         << "\n   If 0, the fixing percentage is determined automatically using information from LP"
         << "\n   relaxation. if < 0, does not perform fixing."
         << "\n - <var_fixing_growth_rate>: grownth rate on var_fixing_percentage. Range [0,1]."
         << "\n - <var_fixing_frequency>: number of iterations between to variable fixings."
         << "\n - <var_fixing_type>: defines the type of the fixing:"
         << "\n\t+ Ones: fix variables such value is one in the most roundings."
         << "\n\t+ Zeros: fix variables such value is zero in the most roundings."
         << "\n\t+ Fractionals: Fix variables such value is splitted between zeros and ones"
         << "\n\t               among the roundings."
         << "\n\t+ Automatic: let the algorithm decide (using LP relaxation info)."

         << "\n\n*** ROUNDINGCUT"
         << "\n - <roundcuts_percentage>: percentage of the population used to produce"
         << "\n   cuts avoind such (infeasible) roundings. Range [0,1]."

         << "\n\n*** MIPLOCALSEARCH"
         << "\n - <miplocalsearch-threshold>: maximum percentage of fractional variables to"
         << "\n   launch a MIP local search. Range [0,1]."
         << "\n - <miplocalsearch-discrepancy_level>:  Defines the discrepancy level to be used"
         << "\n   when fixing variable during MIP local search. Range [0,1]. For instance, at the"
         << "\n   discrepancy level of 0.05, variables such roundings have the same value in,"
         << "\n   at least, 95% of roudings, will be fixed to this value. Value 0.0 represents"
         << "\n   no tolerance to discrepancy, i.e., all roundings to a variable must be the same."
         << "\n - <miplocalsearch-unfix-levels>: controls the recursion on unfix variables."
         << "\n   If zero, no unfix is performed. If 1, all varaibles that belong to constraints"
         << "\n   with free variables are unfixed. If greater than 2, the unfix is done recursively."
         << "\n - <miplocalsearch-max-time>: time used in the local search. If the time is less than"
         << "\n   or equal to zero, the remaining optimization time is used."
         << "\n - <miplocalsearch-constraint-filtering>: defines which constraints consider during the unfix phase:"
         << "\n\t+ All: consider all constraints (not filtering at all)."
         << "\n\t+ Duals: only constraints such dual values in the relaxation are not zero."
         << "\n\t+ SlacksAndDuals: as in \"Duals\" but also consider constraints with slacl values equal to zero."

         << "\n\n ALL PARAMETERS ARE MANDATORY\n"
         << endl;
}

//--------------------------------[ Main ]------------------------------------//

int main(int argc, char* argv[]) {
    // First read parameters from command line:
    const int num_args = 37;

    if(argc == 2 && string(argv[1]) == string("--help")) {
        show_usage_info(argv[0], true);
        return 0;
    }

    #ifndef TUNING
    if(argc < num_args) {
    #else
    if(argc < num_args + 8) {
    #endif
        show_usage_info(argv[0], false);
        return 64;  // BSD usage error code.
    }

    // Loading parameters from command line
    const char* configFile = argv[1];
    unsigned long seed;
    StopRule stop_rule;
    double stop_arg;
    double max_time;
    const char* instance_file = argv[6];
    const char* output_dir = argv[7];
    int num_init_population;
    FeasibilityPump_Decoder::PumpStrategy pump_strategy;
    FeasibilityPump_Decoder::FitnessType fitness_type;

    double minimization_factor;
    double minimization_factor_decay;
    unsigned decay_application_offset;

    FeasibilityPump_Decoder::FPparams fp_params;
    FeasibilityPump_Decoder::ObjFPparams objective_fp_params;

    double var_fixing_percentage;
    double var_fixing_growth_rate;
    unsigned var_fixing_frequency;
    FeasibilityPump_Decoder::FixingType var_fixing_type;

    double roundcuts_percentage;

    double miplocalsearch_threshold;
    double miplocalsearch_discrepancy_level;
    unsigned miplocalsearch_unfix_levels;
    double miplocalsearch_max_time;
    FeasibilityPump_Decoder::ConstraintFilteringType constraint_filtering;

    try {
         seed = boost::lexical_cast<unsigned long>(argv[2]);
         stop_rule = StopRule(toupper(argv[3][0]));
         stop_arg = boost::lexical_cast<double>(argv[4]);
         max_time = boost::lexical_cast<double>(argv[5]);
         num_init_population = boost::lexical_cast<int>(argv[8]);

         if(toupper(argv[9][0]) == 'O')
             pump_strategy = FeasibilityPump_Decoder::PumpStrategy::OBJECTIVE;
         else
             pump_strategy = FeasibilityPump_Decoder::PumpStrategy::DEFAULT;

         // skip arg 10
         if(toupper(argv[11][0]) == 'C')
             fitness_type = FeasibilityPump_Decoder::FitnessType::CONVEX;
         else
             fitness_type = FeasibilityPump_Decoder::FitnessType::GEOMETRIC;

         minimization_factor = boost::lexical_cast<double>(argv[12]);
         minimization_factor_decay = boost::lexical_cast<double>(argv[13]);
         decay_application_offset = boost::lexical_cast<unsigned>(argv[14]);

         // skip arg 15
         fp_params.iteration_limit = boost::lexical_cast<unsigned>(argv[16]);
         fp_params.perturb_when_cycling = toupper(argv[17][0]) == 'P';
         fp_params.t = boost::lexical_cast<int>(argv[18]);
         fp_params.rho_lb = boost::lexical_cast<double>(argv[19]);
         fp_params.rho_ub = boost::lexical_cast<double>(argv[20]);

         // skip 20
         objective_fp_params.phi = boost::lexical_cast<double>(argv[22]);
         objective_fp_params.delta = boost::lexical_cast<double>(argv[23]);

         // skip 24
         var_fixing_percentage = boost::lexical_cast<double>(argv[25]);
         var_fixing_growth_rate = boost::lexical_cast<double>(argv[26]);
         var_fixing_frequency = boost::lexical_cast<unsigned>(argv[27]);

         switch(toupper(argv[28][0])) {
         case 'O':
             var_fixing_type = FeasibilityPump_Decoder::FixingType::MOST_ONES;
             break;

         case 'Z':
             var_fixing_type = FeasibilityPump_Decoder::FixingType::MOST_ZEROS;
             break;

         case 'F':
             var_fixing_type = FeasibilityPump_Decoder::FixingType::MOST_FRACTIONALS;
             break;

         case 'A':
             var_fixing_type = FeasibilityPump_Decoder::FixingType::AUTOMATIC;
             break;

         default:
             throw std::runtime_error("Cannot define parameter var_fixing_type.");
         }

         // skip 29
         roundcuts_percentage = boost::lexical_cast<double>(argv[30]);

         // Skip 31
         miplocalsearch_threshold = boost::lexical_cast<double>(argv[32]);
         miplocalsearch_discrepancy_level = boost::lexical_cast<double>(argv[33]);
         miplocalsearch_unfix_levels = boost::lexical_cast<unsigned>(argv[34]);
         miplocalsearch_max_time = boost::lexical_cast<double>(argv[35]);

         switch(toupper(argv[36][0])) {
         case 'A':
             constraint_filtering = FeasibilityPump_Decoder::ConstraintFilteringType::ALL;
             break;

         case 'D':
             constraint_filtering = FeasibilityPump_Decoder::ConstraintFilteringType::ONLY_NONZERO_DUALS;
             break;

         case 'S':
             constraint_filtering = FeasibilityPump_Decoder::ConstraintFilteringType::NONZERO_DUALS_NONZERO_SLACKS;
             break;

         default:
             throw std::runtime_error("Cannot define parameter constraint_filtering.");
         }


         //----------------------------
         // Check in input
         //----------------------------

         if(stop_rule != StopRule::GENERATIONS && stop_rule != StopRule::TARGET && stop_rule != StopRule::IMPROVEMENT)
             throw std::runtime_error("Incorrect stop rule.");

         auto check_range = [](const double& var, string name) {
             if(var < 0.0 || var > 1.0)
                  throw std::runtime_error(string("The parameter ") + name +
                                           string(" must be in the range [0,1]."));
         };

         check_range(minimization_factor, "minimization_factor");

         if(minimization_factor_decay < 0.0)
             throw std::runtime_error("minimization_factor_decay must be a non-negative number.");

         check_range(objective_fp_params.phi, "objective_fp_params.phi");
         check_range(objective_fp_params.delta, "objective_fp_params.delta");

         if(var_fixing_percentage > 1.0)
             throw std::runtime_error("var_fixing_percentage less than or equal to 1.0.");

         check_range(var_fixing_growth_rate, "var_fixing_growth_rate");

         if(var_fixing_frequency == 0)
             throw std::runtime_error("The parameter var_fixing_frequency must be greater than 0.");

         check_range(roundcuts_percentage, "roundcuts_percentage");
         check_range(miplocalsearch_threshold, "miplocalsearch_threshold");
         check_range(miplocalsearch_discrepancy_level, "miplocalsearch_discrepancy_level");
    }
    catch(std::exception& e) {
        cerr << "*** Bad arguments. Verify them!"
             << "\n*** " << e.what() << endl;
        return 64; // BSD usage error code
    }

    string instance_name;
    string base_output_filename;
    {
        boost::filesystem::path instance_path(instance_file);
        boost::filesystem::path output_path(output_dir);
        instance_name = instance_path.stem().stem().string();
        output_path /= instance_path.stem().stem();
        base_output_filename = output_path.string();
    }

    string log_filename(base_output_filename);
    log_filename += ".log";
    ofstream log_file;

    #ifndef TUNING
    log_file.open(log_filename.c_str(), ios::trunc);
//    log_file.open("/dev/stdout", ios::app);
    #else
    log_file.open("/dev/null", ios::out|ios::app);
    #endif
    if(!log_file) {
        cerr << "\nImpossible to open the log file " << log_filename << endl;
        return 64;
    }
    log_file.flags(ios::fixed);

    // Parameters read from configuration file:
    double pe;                          // % of elite items in the population
    double pm;                          // % of mutants introduced at each generation
    double rhoe;                        // prob. of inheriting each allele from elite parent
    unsigned num_populations;           // number of independent populations
    unsigned max_population_size;       // maximum size of each population
    unsigned num_threads;               // number of threads in parallel decoding
    unsigned exchange_interval;         // interval at which elite chromosomes are exchanged (0 means no exchange)
    unsigned num_exchange_indivuduals;  // number of elite chromosomes to obtain from each population
    unsigned reset_interval;            // interval at which the populations are reset (0 means no reset)

    // Loading algorithm parameters from config file (code from rtoso).
    ifstream fin(configFile, std::ios::in);
    if(!fin) {
        cerr << "Cannot open configuration file: " << configFile << endl;
        return 66; // BSD file not found error code
    }
    else {
        fin.exceptions(ifstream::eofbit | ifstream::failbit | ifstream::badbit);
    }

    try {
        string line;
        fin >> pe;                          getline(fin, line);
        fin >> pm;                          getline(fin, line);
        fin >> rhoe;                        getline(fin, line);
        fin >> num_populations;             getline(fin, line);
        fin >> max_population_size;         getline(fin, line);
        fin >> num_threads;                 getline(fin, line);
        fin >> exchange_interval;           getline(fin, line);
        fin >> num_exchange_indivuduals;    getline(fin, line);
        fin >> reset_interval;
        fin.close();
    }
    catch(ifstream::failure& e) {
        cerr << "Failure when reading configfile: " << e.what() << endl;
        fin.close();
        return 65;  // BSD file read error code
    }

    //-----------------------------------------//
    // Tuning
    //-----------------------------------------//
    #ifdef TUNING
    max_population_size = boost::lexical_cast<unsigned>(argv[num_args]);
    pe = boost::lexical_cast<double>(argv[num_args + 1]);
    pm = boost::lexical_cast<double>(argv[num_args + 2]);
    rhoe = boost::lexical_cast<double>(argv[num_args + 3]);
    num_populations = boost::lexical_cast<unsigned>(argv[num_args + 4]);
    exchange_interval = boost::lexical_cast<unsigned>(argv[num_args + 5]);
    num_exchange_indivuduals = boost::lexical_cast<unsigned>(argv[num_args + 6]);
    reset_interval = boost::lexical_cast<unsigned>(argv[num_args + 7]);

    double tuning_value = numeric_limits<double>::max();
    #endif
    //-----------------------------------------/

    int return_code = 0;
    try {
        // Used to track the global optimization time.
        ExecutionStopper::init(boost::timer::nanosecond_type(max_time));
        //ExecutionStopper::init(boost::timer::nanosecond_type(max_time * num_threads),
                               //ExecutionStopper::TimeType::CPU);

        // Used to track the time of each component.
        boost::timer::cpu_timer local_timer;

        // Used to track the time through the iterations.
        boost::timer::cpu_timer iteration_timer;

        // Log some information.
        log_file << setiosflags(ios::fixed) << setprecision(2)
                 << "\n------------------------------------------------------\n"
                 << "> Experiment started at " << boost::posix_time::second_clock::local_time()
                 << "\n> Instance: " << instance_file
                 << "\n> Algorithm Parameters"
                 << "\n> Config file: " << configFile
                 << "\n>    + % of Elite: " << pe
                 << "\n>    + % of Mutants: " << pm
                 << "\n>    + Prob. inheritance (rhoe): " << rhoe
                 << "\n>    + # of independent populations: " << num_populations
                 << "\n>    + maximum size of each population: " << max_population_size
                 << "\n>    + # of threads: " << num_threads
                 << "\n>    + interval of chromosome exchange: " << exchange_interval
                 << "\n>    + # of elite chromosome of each population: " << num_exchange_indivuduals
                 << "\n>    + reset interval: " << reset_interval
                 << "\n> Seed: " << seed
                 << "\n> Stop Rule: "
                 << (stop_rule == StopRule::GENERATIONS ? "Generations -> " :
                    (stop_rule == StopRule::TARGET ? "Target -> " : "Improvement -> "))
                 << stop_arg
                 << "\n> Size of initial population: " << num_init_population
                 << (num_init_population < 0? " (size of population)" : "")
                 << "\n> Max Time: " << max_time;

        log_file << "\n> Pump strategy: ";
        if(pump_strategy == FeasibilityPump_Decoder::PumpStrategy::OBJECTIVE)
            log_file << "objective feas. pump";
        else
            log_file << "default feas. pump";

        log_file << "\n> Fitness type: ";
        if(fitness_type == FeasibilityPump_Decoder::FitnessType::CONVEX)
            log_file << "convex combination";
        else
            log_file << "geometric combination";

        log_file << "\n> Minimization factor: " << minimization_factor
                 << "\n> Minimization factor decay: " << minimization_factor_decay
                 << "\n> Decay application offset: " << decay_application_offset
                 << "\n> Feas. pump params: "
                 << "\n>\t- iteration limit: " << fp_params.iteration_limit
                 << "\n>\t- perturb when cycling: "
                 << (fp_params.perturb_when_cycling? "perturb" : "not perturb")
                 << "\n>\t- t: " << fp_params.t
                 << "\n>\t- rho_lb: " << fp_params.rho_lb
                 << "\n>\t- rho_ub: " << fp_params.rho_ub
                 << "\n> Obj. feas. pump params:"
                 << "\n>\t- phi: " << objective_fp_params.phi
                 << "\n>\t- delta: " << objective_fp_params.delta
                 << "\n> Fixing parameters:";

        if(var_fixing_percentage < 0.0)
            log_file << " no variable fixing";
        else {
            log_file << "\n>\t- var_fixing_percentage: " << var_fixing_percentage
                     << (var_fixing_percentage < EPS? " (automatic)" : "")
                     << "\n>\t- var_fixing_growth_rate: " << var_fixing_growth_rate
                     << "\n>\t- var_fixing_frequency: " << var_fixing_frequency
                     << "\n>\t- var_fixing_type: ";

            switch(var_fixing_type) {
            case FeasibilityPump_Decoder::FixingType::MOST_ONES:
                log_file << "fixing most ones";
                break;
            case FeasibilityPump_Decoder::FixingType::MOST_ZEROS:
                log_file << "fixing most zeros";
                break;
            case FeasibilityPump_Decoder::FixingType::MOST_FRACTIONALS:
                log_file << "fixing most fractionals";
                break;
            default:
                log_file << "automatic detection";
                break;
            }
        }

        log_file << "\n> Rounding cuts percentage: " << (roundcuts_percentage * 100)
                 << "\n> Mip local search:"
                 << "\n>\t- threshold: " << miplocalsearch_threshold
                 << "\n>\t- discrepancy_level: " << miplocalsearch_discrepancy_level
                 << "\n>\t- unfix_levels: " << miplocalsearch_unfix_levels
                 << "\n>\t- max_time: " << miplocalsearch_max_time
                 << "\n>\t- constraint_filtering: ";

        switch(constraint_filtering) {
        case FeasibilityPump_Decoder::ConstraintFilteringType::ALL:
            log_file << "all constraints (not filtering)";
            break;
        case FeasibilityPump_Decoder::ConstraintFilteringType::ONLY_NONZERO_DUALS:
            log_file << "constraint with non-zero dual";
            break;
        case FeasibilityPump_Decoder::ConstraintFilteringType::NONZERO_DUALS_NONZERO_SLACKS:
            log_file << "constraint with non-zero dual or zero slacks";
            break;
        }

        log_file << endl;

        ////////////////////////////////////////////
        // Load instance
        ////////////////////////////////////////////

        FeasibilityPump_Decoder decoder(instance_file, num_threads, seed,
                                        pump_strategy,
                                        fitness_type,
                                        minimization_factor,
                                        minimization_factor_decay,
                                        fp_params,
                                        objective_fp_params,
                                        var_fixing_percentage,
                                        var_fixing_growth_rate,
                                        var_fixing_type,
                                        constraint_filtering,
                                        miplocalsearch_discrepancy_level);

        if(roundcuts_percentage > 0.0)
            decoder.rounding_cuts.reserve(10000);

        log_file << "\n\n-----------------------------"
                 << "\n>>>> Initializing the decoder..." << endl;

        ExecutionStopper::timerStart();
        local_timer.start();
        decoder.init();
        //decoder.setAlleleThreshold(decoder.getZerosPercentageInInitialRelaxation());

        local_timer.stop();
        ExecutionStopper::timerStop();
        boost::timer::cpu_times preprocessing_time(local_timer.elapsed());

        double initial_variable_fixing_percentage = decoder.variable_fixing_percentage;

        log_file << "\n- Num. of variables: " << decoder.getNumVariables()
                 << "\n- Num. of binaries: " << decoder.getNumBinaryVariables()
                 << "\n- Num. of constraints: " << decoder.getNumConstraints()
                 << "\n- Sense: "
                 << (decoder.getSense() == FeasibilityPump_Decoder::Sense::MAXIMIZE?
                     "Maximization" : "Minimization")
                 << "\n- % of variables with value zero in the relaxation: "
                 << (decoder.percentage_zeros_initial_relaxation * 100.0)
                 << "\n- % of variables with value one in the relaxation: "
                 << (decoder.percentage_ones_initial_relaxation * 100.0)
                 << "\n- Variable fixing type: ";

        switch(decoder.var_fixing_type) {
        case FeasibilityPump_Decoder::FixingType::MOST_ONES:
            log_file << "fixing most ones";
            break;
        case FeasibilityPump_Decoder::FixingType::MOST_ZEROS:
            log_file << "fixing most zeros";
            break;
        case FeasibilityPump_Decoder::FixingType::MOST_FRACTIONALS:
            log_file << "fixing most fractionals";
            break;
        default:
            log_file << "automatic detection";
            break;
        }

        log_file << "\n- Variable fixing percentage: " << (decoder.variable_fixing_percentage * 100.0)
                 << "\n- Constraint filtering: ";

        switch(decoder.constraint_filtering_type) {
        case FeasibilityPump_Decoder::ConstraintFilteringType::ALL:
            log_file << "all constraints";
            break;
        case FeasibilityPump_Decoder::ConstraintFilteringType::ONLY_NONZERO_DUALS:
            log_file << "constraints with non-zero duals";
            break;
        case FeasibilityPump_Decoder::ConstraintFilteringType::NONZERO_DUALS_NONZERO_SLACKS:
            log_file << "constraints with non-zero duals or zero slacks";
            break;
        }

        log_file << "\n- Number of constraints to be used on unfixing: " << decoder.num_constraints_used
                 << " (" << ((double)decoder.num_constraints_used / decoder.getNumConstraints() * 100.0) << "%)"
                 << "\n- Relaxation time: " << boost::timer::format(decoder.relaxation_time)
                 << "- Decoder init. time: " << boost::timer::format(preprocessing_time);

        log_file.flush();

        ////////////////////////////////////////////
        // Star optimization
        ////////////////////////////////////////////

        MTRand rng(seed);
        // Warm the generator up
        for(int i = 0; i < 1000; ++i) rng.rand();

        const unsigned population_size = max_population_size;

        // Setting the initial population.
        log_file << "\n\n-----------------------------"
                 << "\n>>>> Creating initial population..." << endl;

        if(num_init_population < 0)
            num_init_population = population_size;

        vector<Chromosome> initial_chromosomes;

        ExecutionStopper::timerResume();
        local_timer.start();
        boost::timer::cpu_times relaxations_time(local_timer.elapsed());

        // Optimization info.
        double best_fitness = 0.0; //numeric_limits<double>::max();
        Chromosome best_chr(decoder.getChromosomeSize(), 0.0);
        best_chr.feasibility_pump_value = 0;
        best_chr.fractionality = 0;
        best_chr.num_non_integral_vars = 0;
        best_chr.num_iterations = 0;

        bool feasible = false;
        bool feasible_from_fixing = false;
        bool feasible_from_local_search = false;

        unsigned last_update_iteration = 0;
        unsigned update_offset = 0;
        unsigned large_offset = 0;
        unsigned num_improvements = 0;
        unsigned num_fixings = 0;
        unsigned num_successful_fixings = 0;
        unsigned num_local_searchs = 0;
        unsigned change_perf_meas_counter = 0;
        unsigned num_best_random = 0;
        unsigned num_best_offspring_rr = 0;
        unsigned num_best_offspring_or = 0;
        unsigned num_best_offspring_oo = 0;
        double heterogeneity = 1.0;

        boost::timer::cpu_times last_update_time;
        boost::timer::cpu_times decoding_time;
        boost::timer::cpu_times fixing_time;
        boost::timer::cpu_times local_search_time;
        decoding_time.clear();
        fixing_time.clear();
        local_search_time.clear();

        unordered_set<size_t> elite_chromosomes_hashes;
        elite_chromosomes_hashes.reserve(unsigned(population_size * pe));

        vector<size_t> num_unfixed_vars_per_call;
        num_unfixed_vars_per_call.reserve(20);

        unsigned actual_num_fixings = 0;

        // For control the optimization
        unsigned iteration = 1;

        log_file << "\n\n-----------------------------"
                 << "\n>>>> Optimizing..."
                 << "\n> Lines starting with % represent the iteration and "
                 << "the heterogeneity of the elite population\n\n"
                 << "Iteration | PerformanceValue | FPValue | Fractionality | "
                 << "NumNonIntegralVars | NumNonIntegralVarsPerc | "
                 << "CurrMinFactor | ChrType | "
                 << "DecTimeCPU | DecTimeWall | "
                 << "CurrentTimeCPU | CurrentTimeWall"
                 << endl;

        // This file will contains some information about
        // each elite chromosome give an iteration.
        string pop_statistics_filename(base_output_filename);
        pop_statistics_filename += "_pop_statistics.dat";
        ofstream pop_statistics;

        #ifndef TUNING
        pop_statistics.open(pop_statistics_filename.c_str(), ios::trunc);
        #else
        solution_file.open("/dev/null");
        #endif
        if(!pop_statistics) {
            throw std::runtime_error(string("Impossible to open population statistics file: ") + pop_statistics_filename);
        }

        pop_statistics << "Iteration Chromosome NonIntegralVars Fractionality FPIterations FPValue"
                       << endl;

        ExecutionStopper::timerResume();
        iteration_timer.start();

        //////////////////////////////////////////////////////
        // Main loop
        //////////////////////////////////////////////////////

        const size_t NUM_ROUNDINGS = 30;
        const auto NUM_BINARIES = decoder.getNumBinaryVariables();

        // The chromossomes.
        Population population(NUM_BINARIES, NUM_ROUNDINGS);

        // Keep the random values.
        vector<IloNumArray> random_values;

        // Keep the rounded values.
        vector<IloNumArray> rounded_values;

        random_values.reserve(NUM_ROUNDINGS);
        rounded_values.reserve(NUM_ROUNDINGS);
        for(size_t i = 0; i < NUM_ROUNDINGS; ++i) {
            random_values.emplace_back(decoder.environment_per_thread[0]);
            rounded_values.emplace_back(decoder.environment_per_thread[0]);
        }

        for(auto &v : random_values)
            v.setSize(NUM_BINARIES);

        for(auto &v : rounded_values)
            v.setSize(NUM_BINARIES);

        while(true) {
            // Run one iteration
            ExecutionStopper::timerResume();
            iteration_timer.resume();
            local_timer.start();

            for(size_t i = 0; i < NUM_ROUNDINGS; ++i) {
                for(IloInt j = 0; j < NUM_BINARIES; ++j)
                    random_values[i][j] = rng.rand();
            }

            #ifdef _OPENMP
            #pragma omp parallel for num_threads(num_threads) schedule(static, 1)
            #endif
            for(size_t i = 0; i < NUM_ROUNDINGS; ++i) {
                //decoder.roundingWithConstraintPropagation(random_values[i],
                                                          //rounded_values[i]);

                auto &chr = population.getChromosome(i);
                for(size_t j = 0; j < NUM_BINARIES; ++j) {
                    chr[j] = random_values[i][j];
                    chr.rounded[j] = rounded_values[i][j];
                }

                decoder.decode(chr);
            }

            local_timer.stop();
            iteration_timer.stop();
            ExecutionStopper::timerStop();

            // Skip the first and the last iteration on gathering decoding time.
            if(iteration > 1 && !ExecutionStopper::mustStop()) {
                decoding_time.wall += local_timer.elapsed().wall;
                decoding_time.user += local_timer.elapsed().user;
                decoding_time.system += local_timer.elapsed().system;
            }

            //////////////////////////////////////////////////////
            // Compute some statistics over the elite individuals
            //////////////////////////////////////////////////////

            const auto &pop = population;
            for(unsigned i = 0; i < pop.getP(); ++i) {
                const auto &chromosome = pop.getChromosome(i);

                pop_statistics << iteration << " "
                               << i << " "
                               << chromosome.num_non_integral_vars << " "
                               << chromosome.fractionality << " "
                               << chromosome.num_iterations << " "
                               << chromosome.feasibility_pump_value
                               << "\n";

                // Hashing the rounding
                size_t hash_value = 0;
                for(size_t k = 0; k < (size_t)decoder.getNumBinaryVariables(); ++k) {
                    if(chromosome.rounded[k] == 1)
                        hash_value ^= k + 0x9e3779b9 + (hash_value << 6) + (hash_value >> 2);
                }

                elite_chromosomes_hashes.insert(hash_value);
            }
            pop_statistics.flush();

            heterogeneity = (100.0 * elite_chromosomes_hashes.size()) / pop.getP();

            log_file << "% " << iteration
                     << setiosflags(ios::fixed) << setprecision(2)
                     << " " << heterogeneity
                     << endl;

            elite_chromosomes_hashes.clear();

            //////////////////////////////////////////////////////
            // Stopping controls
            //////////////////////////////////////////////////////

            unsigned iterWithoutImprovement = iteration - last_update_iteration;
            bool run = true;

            // Time to stop?
            if(ExecutionStopper::mustStop() && !(max_time - EPS < 0.0))
                run = false;

            if(feasible || !run) break; //... the main loop.

            //////////////////////////////////////////////////////
            // MIP local search
            //////////////////////////////////////////////////////

            ExecutionStopper::timerResume();
            iteration_timer.resume();
            local_timer.start();

            // Check if the number of fractional variables is low enough
            // to launch a CPLEX MIP optimization.
            log_file << iteration << " --- Launching full MIP search on "
                     << best_chr.num_non_integral_vars << " vars ("
                     << (best_chr.num_non_integral_vars / (double) decoder.getNumBinaryVariables() * 100)
                     << "%): ";
            log_file.flush();

            bool mipsearch_time_changed = false;
            if(miplocalsearch_max_time < EPS) {
                const auto t = iteration_timer.elapsed();
                miplocalsearch_max_time = max_time - ((t.user + t.system) /
                                                      (1e9 * num_threads));
                mipsearch_time_changed = true;
            }

            ++num_local_searchs;

            size_t num_unfixed_vars = 0;

            if(decoder.performIteratedMIPLocalSearch(population,
                         population.getP(), miplocalsearch_unfix_levels,
                         miplocalsearch_max_time, best_chr, num_unfixed_vars)) {

                 feasible = feasible_from_local_search = true;
                 log_file << "feasible solution found. ("
                          << boost::timer::format(local_timer.elapsed(), 2, "%w")
                          << " segs)" << endl;

                 num_unfixed_vars_per_call.push_back(num_unfixed_vars);
                 break; // main loop.
             }
             else
                 log_file << "no feasible solution found. ("
                          << boost::timer::format(local_timer.elapsed(), 2, "%w")
                          << " segs)" << endl;

            num_unfixed_vars_per_call.push_back(num_unfixed_vars);

            if(mipsearch_time_changed)
                miplocalsearch_max_time = 0.0;

            local_timer.stop();
            iteration_timer.stop();
            ExecutionStopper::timerStop();

            local_search_time.wall += local_timer.elapsed().wall;
            local_search_time.user += local_timer.elapsed().user;
            local_search_time.system += local_timer.elapsed().system;

            ++iteration;
        }

        //////////////////////////////////////////////////////
        // End of main loop
        //////////////////////////////////////////////////////

        ExecutionStopper::timerStop();
        iteration_timer.stop();
        boost::timer::cpu_times elapsed_time(iteration_timer.elapsed());

        const double proportional_cpu_usage_general =
                ((double)elapsed_time.wall < EPS) ? 0.0 :
                ((elapsed_time.user + elapsed_time.system) / (double)elapsed_time.wall) / num_threads;

        const double proportional_cpu_usage_decoding =
                ((double)decoding_time.wall < EPS) ? 0.0 :
                ((decoding_time.user + decoding_time.system) / (double)decoding_time.wall) / num_threads;

        const double proportional_cpu_usage_fixing =
                ((double)fixing_time.wall < EPS) ? 0.0 :
                ((fixing_time.user + fixing_time.system) / (double)fixing_time.wall) / num_threads;

        const double proportional_cpu_usage_local_search =
                ((double)local_search_time.wall < EPS) ? 0.0 :
                ((local_search_time.user + local_search_time.system) / (double)local_search_time.wall) / num_threads;

        boost::timer::cpu_times decoding_time_avg = decoding_time;

        decoding_time_avg.wall /= iteration;
        decoding_time_avg.user /= iteration;
        decoding_time_avg.system /= iteration;

        boost::timer::cpu_times fixing_time_avg = fixing_time;

        if(num_fixings > 0) {
            fixing_time_avg.wall /= num_fixings;
            fixing_time_avg.user /= num_fixings;
            fixing_time_avg.system /= num_fixings;
        }

        boost::timer::cpu_times local_search_time_avg = local_search_time;

        if(num_local_searchs > 0) {
            local_search_time_avg.wall /= num_local_searchs;
            local_search_time_avg.user /= num_local_searchs;
            local_search_time_avg.system /= num_local_searchs;
        }

        unsigned solved_lps = 0;
        for(auto &v : decoder.solved_lps_per_thread)
            solved_lps += v;

        const double solved_lps_per_decoding = solved_lps /
                (population_size  + (population_size * (1.0 - pe) * (iteration - 1)));

        double avg_num_unfixed_vars_per_call = 0.0;
        size_t last_num_unfixed_vars = 0;
        if(num_unfixed_vars_per_call.size() > 0) {
            for(auto &v : num_unfixed_vars_per_call)
                avg_num_unfixed_vars_per_call += v;
            avg_num_unfixed_vars_per_call /= num_unfixed_vars_per_call.size();
            last_num_unfixed_vars = num_unfixed_vars_per_call.back();
        }

        double final_variable_fixing_percentage = decoder.variable_fixing_percentage;

        log_file << "\n- Optimization time: " << boost::timer::format(elapsed_time)
                 << "- Decoding time: " << boost::timer::format(decoding_time)
                 << "- Avg. decoding time: " << boost::timer::format(decoding_time_avg)
                 << "- Fixing time: " << boost::timer::format(fixing_time)
                 << "- Avg. fixing time: " << boost::timer::format(fixing_time_avg)
                 << "- MIP local search time: " << boost::timer::format(local_search_time)
                 << "- Avg. MIP local search time: " << boost::timer::format(local_search_time_avg)
                 << "- Iterations: " << iteration
                 << "\n- Improvements: " << num_improvements
                 << "\n- Num. fixings trials: " << num_fixings
                 << "\n- Num. successful fixings: " << num_successful_fixings
                 << "\n- Initial variable fixing percentage: " << (initial_variable_fixing_percentage * 100)
                 << "\n- Final variable fixing percentage: " << (final_variable_fixing_percentage * 100)
                 << "\n- Feasible from fixing: " << (feasible_from_fixing? "yes" : "no")
                 << "\n- Final/actual num. of fixings: " << actual_num_fixings
                 << " (" << (100.0 * actual_num_fixings / decoder.getNumBinaryVariables()) << "%)"
                 << "\n- Num. MIP local search: " << num_local_searchs
                 << "\n- Feasible from local search: " << (feasible_from_local_search? "yes" : "no")
                 << "\n- Feasible before var. unfixing: " << (decoder.feasible_before_var_unfixing? "yes" : "no")
                 << "\n- Num. of constraints used during unfixing: " << decoder.num_constraints_used
                 << " (" << (100.0 * decoder.num_constraints_used / decoder.getNumConstraints()) << "%)"
                 << "\n- Avg. num. unfixed vars in local search: " << avg_num_unfixed_vars_per_call
                 << " (" << (100.0 * avg_num_unfixed_vars_per_call / decoder.getNumBinaryVariables()) << "%)"
                 << "\n- Num. unfixed vars in the last call: " << last_num_unfixed_vars
                 << " (" << (100.0 * last_num_unfixed_vars / decoder.getNumBinaryVariables()) << "%)"
                 << "\n- Solved LPs: " << solved_lps
                 << "\n- Solved LPs per decoding: " << solved_lps_per_decoding
                 << "\n- Rounding cuts: " << decoder.rounding_cuts.size()
                 << "\n- Viability: " << (feasible? "feasible" : "infeasible")
                 << "\n- Fractionality: " << best_chr.fractionality
                 << "\n- NumNonIntegralVars: " << best_chr.num_non_integral_vars
                 << "\n- NumNonIntegralVarsPerc: " << setprecision(2)
                 << (100.0 * best_chr.num_non_integral_vars / decoder.getNumBinaryVariables())
                 << endl;

        #ifdef TUNING
        int multiplier = 0;
        for(; max_time > 1.0; max_time /= 10.0, ++multiplier);

        tuning_value = ((best_chr.num_non_integral_vars * pow(10.0, (multiplier + 2))) +
                        (elapsed_time.wall / 1e9));
        #endif

        log_file << "\n> Best solution: "
                 << (feasible? "feasible" : "infeasible")
                 << " (value form feasibility pump: " << best_fitness << ")"
                 << endl;

        string solution_filename(base_output_filename);
        solution_filename += ".sol";
        ofstream solution_file;

        #ifndef TUNING
        solution_file.open(solution_filename.c_str(), ios::trunc);
        #else
        solution_file.open("/dev/null");
        #endif
        if(!solution_file) {
            log_file << "\nImpossible to open the solution file " << solution_filename << endl;
        }
        else {
            solution_file.flags(ios::fixed);
            for(IloInt i = 0; i < decoder.binary_variables_per_thread[0].getSize(); ++i) {
                if(best_chr[i] > EPS || best_chr[i] < -EPS)
                    solution_file << decoder.binary_variables_per_thread[0][i]
                             << ": " << best_chr[i] << "\n";
            }
            solution_file.close();
        }

        //--------------------------------------------------------------
        // If feasible, let's check if the solution is feasible in fact.
        //--------------------------------------------------------------

        if(feasible) {
            IloEnv env;
            IloModel model(env);
            IloCplex cplex(env);
            IloObjective objective(env);
            IloNumVarArray variables(env);
            IloRangeArray constraints(env);

            cplex.setParam(IloCplex::Param::MIP::Display, 0);
            env.setOut(env.getNullStream());
            cplex.setOut(env.getNullStream());
            cplex.setWarning(env.getNullStream());

            cplex.importModel(model, instance_file, objective, variables, constraints);
            cplex.extract(model);

            size_t j = 0;
            for(IloInt i = 0; i < variables.getSize(); ++i) {
                auto var = variables[i];
                if(var.getType() == IloNumVar::Bool) {
                    var.setBounds(best_chr.rounded[j], best_chr.rounded[j]);
                    ++j;
                }
            }

            cplex.solve();

            if(cplex.getStatus() == IloAlgorithm::Feasible ||
               cplex.getStatus() == IloAlgorithm::Optimal) {
                best_fitness = cplex.getObjValue();
            }
            else {
                log_file << "\n!!!!! CPLEX status: "
                         << cplex.getStatus() << "\n"
                         << "\n!!!! Solution should be feasible but it is not."
                         << "\nMaybe the CPLEX algorithm reach a weird condition."
                         << " Please, use the solution checker (test_solution)"
                         << " to assert this condition. In the most cases,"
                         << " this message is a false-negative."
                         << endl;
            }
        }

        log_file << "\n-----------------------------\n\n"
                 ///////////////////////
                 // BRKGA Stats
                 ///////////////////////
                 << "Instance & NumVars & NumBinaries & NumConstraints & "
                 << "Seed & Threads & MaxTime & Iters & Improvements & "
                 << "LUI & LUTCpu & LUTWall & LUTProp & UO & LO & "
                 << "ChrType & Random & OS_RR & OS_OR & OS_OO & "
                 << "DecodingTimeCPU & DecodingTimeWall & DecodingTimeProp & "
                 << "DecodingAvgTimeCPU & DecodingAvgTimeWall & DecodingAvgTimeProp & "
                 << "PreprocessingTimeCpu & PreprocessingTimeWall & "
                 << "RelaxationsTimeCpu & RelaxationsTimeWall & "
                 << "TotalTimeCpu & TotalTimeWall & TotalTimeProp & "
                 ///////////////////////
                 // MIP stats
                 ///////////////////////
                 << "FPPerturbation & "
                 << "NumFixings & NumFixingsSuccess & "
                 << "FixingTimeCPU & FixingTimeWall & FixingTimeWallProp & "
                 << "FixingAvgTimeCPU & FixingAvgTimeWall & FixingAvgTimeWallProp & "
                 << "FixingAutomatic & FixingType & InitialVarFixingPerc & "
                 << "FinalVarFixingPerc & ActualNumFixing & ActualFixingPerc & "
                 << "NumMIPLocalSearch & "
                 << "MIPLocalSearchTimeCPU & MIPLocalSearchTimeWall & MIPLocalSearchTimeProp & "
                 << "MIPLocalSearchAvgTimeCPU & MIPLocalSearchAvgTimeWall & MIPLocalSearchAvgTimeProp & "
                 << "DiscrepancyLevel & UnfixLevel & ConstraintFiltering & "
                 << "ConstraintsUsedUnfixing & ConstraintsUsedUnfixingPerc & "
                 << "AvgNumUnfixedVarsMIPLS & AvgNumUnfixedVarsMIPLSPerc & "
                 << "LastNumUnfixedVarsMIPLS & LastNumUnfixedVarsMIPLSPerc & "
                 << "FeasibleBeforeVarUnfixing & "
                 << "NumInitialRelax & SolvedLPs & SolvedLPsPerDecoding & "
                 << "NumRoundingCuts & Viability & FeasFromFixing  & "
                 << "FeasFromLocalSearch & Value & FPValue & "
                 << "Fractionality & NumNonIntegralVars & NumNonIntegralVarsPerc"
                 << endl;

        log_file
            << instance_name << " & "
            << decoder.getNumVariables() << " & "
            << decoder.getNumBinaryVariables() << " & "
            << decoder.getNumConstraints() << " & "
            << seed << " & "
            << num_threads << " & "
            << setprecision(0)
            << max_time << " & "

            ///////////////////////
            // BRKGA stats
            ///////////////////////

            << setprecision(2)
            << iteration << " & "
            << num_improvements << " & "
            << last_update_iteration << " & "
            << boost::timer::format(last_update_time, 2, "%u") << " & "
            << boost::timer::format(last_update_time, 2, "%w") << " & "
            << (last_update_time.wall / 1e9 * proportional_cpu_usage_general) << " & "
            << update_offset << " & "
            << large_offset << " & ";

        switch(best_chr.type) {
        case Chromosome::ChromosomeType::RANDOM:
            log_file << "R & ";
            break;

        case Chromosome::ChromosomeType::OS_OO:
            log_file << "OF-OO & ";
            break;

        case Chromosome::ChromosomeType::OS_OR:
            log_file << "OF-OR & ";
            break;

        case Chromosome::ChromosomeType::OS_RR:
            log_file << "OF-RR & ";
            break;
        }

        log_file
            << num_best_random << " & "
            << num_best_offspring_rr << " & "
            << num_best_offspring_or << " & "
            << num_best_offspring_oo << " & "
            << boost::timer::format(decoding_time, 2, "%u") << " & "
            << boost::timer::format(decoding_time, 2, "%w") << " & "
            << (decoding_time.wall / 1e9 * proportional_cpu_usage_decoding) << " & "
            << boost::timer::format(decoding_time_avg, 2, "%u") << " & "
            << boost::timer::format(decoding_time_avg, 2, "%w") << " & "
            << (decoding_time_avg.wall / 1e9 * proportional_cpu_usage_decoding) << " & "
            << boost::timer::format(preprocessing_time, 2, "%u") << " & "
            << boost::timer::format(preprocessing_time, 2, "%w") << " & "
            << boost::timer::format(relaxations_time, 2, "%u") << " & "
            << boost::timer::format(relaxations_time, 2, "%w") << " & "
            << boost::timer::format(elapsed_time, 2, "%u") << " & "
            << boost::timer::format(elapsed_time, 2, "%w") << " & "
            << setiosflags(ios::fixed)
            << setprecision(2)
            << (elapsed_time.wall / 1e9 * proportional_cpu_usage_general) << " & "

            ///////////////////////
            // MIP stats
            ///////////////////////

            << (fp_params.perturb_when_cycling? "yes & " : "no & ")
            << setiosflags(ios::fixed)
            << num_fixings << " & "
            << num_successful_fixings << " & "
            << boost::timer::format(fixing_time, 2, "%u") << " & "
            << boost::timer::format(fixing_time, 2, "%w") << " & "
            << (fixing_time.wall / 1e9 * proportional_cpu_usage_fixing) << " & "
            << boost::timer::format(fixing_time_avg, 2, "%u") << " & "
            << boost::timer::format(fixing_time_avg, 2, "%w") << " & "
            << (fixing_time_avg.wall / 1e9 * proportional_cpu_usage_fixing) << " & "
            << (var_fixing_percentage < EPS? "yes & " : "no & ");

        switch(decoder.var_fixing_type) {
        case FeasibilityPump_Decoder::FixingType::MOST_ONES:
            log_file << "ones & ";
            break;
        case FeasibilityPump_Decoder::FixingType::MOST_ZEROS:
            log_file << "zeros & ";
            break;
        case FeasibilityPump_Decoder::FixingType::MOST_FRACTIONALS:
            log_file << "fracs & ";
            break;
        case FeasibilityPump_Decoder::FixingType::AUTOMATIC:
            log_file << "auto & ";
            break;
        }

        log_file
            << (initial_variable_fixing_percentage * 100) << " & "
            << (final_variable_fixing_percentage * 100) << " & "
            << actual_num_fixings  << " & "
            << (100.0 * actual_num_fixings / decoder.getNumBinaryVariables())  << " & "
            << num_local_searchs << " & "
            << boost::timer::format(local_search_time, 2, "%w") << " & "
            << boost::timer::format(local_search_time, 2, "%u") << " & "
            << (local_search_time.wall / 1e9 * proportional_cpu_usage_local_search) << " & "
            << boost::timer::format(local_search_time_avg, 2, "%w") << " & "
            << boost::timer::format(local_search_time_avg, 2, "%u") << " & "
            << (local_search_time_avg.wall / 1e9 * proportional_cpu_usage_local_search) << " & "
            << miplocalsearch_discrepancy_level << " & "
            << miplocalsearch_unfix_levels << " & ";

        switch(decoder.constraint_filtering_type) {
        case FeasibilityPump_Decoder::ConstraintFilteringType::ALL:
            log_file << "all & ";
            break;
        case FeasibilityPump_Decoder::ConstraintFilteringType::ONLY_NONZERO_DUALS:
            log_file << "duals & ";
            break;
        case FeasibilityPump_Decoder::ConstraintFilteringType::NONZERO_DUALS_NONZERO_SLACKS:
            log_file << "slacks & ";
            break;
        }

        log_file
            << decoder.num_constraints_used << " & "
            << ((double)decoder.num_constraints_used / decoder.getNumConstraints() * 100.0) << " & "
            << avg_num_unfixed_vars_per_call << " & "
            << (avg_num_unfixed_vars_per_call / decoder.getNumBinaryVariables() * 100) << " & "
            << last_num_unfixed_vars << " & "
            << (100.0 * last_num_unfixed_vars / decoder.getNumBinaryVariables()) << " & "
            << (decoder.feasible_before_var_unfixing? "yes" : "no") << " & "
            << num_init_population << " & "
            << solved_lps << " & "
            << solved_lps_per_decoding << " & "
            << decoder.rounding_cuts.size() << " & "
            << (feasible? "feasible & " : "infeasible & ")
            << (feasible_from_fixing? "yes & " : "no & ")
            << (feasible_from_local_search? "yes & " : "no & ")
            << best_fitness << " & "
            << best_chr.feasibility_pump_value << " & "
            << best_chr.fractionality << " & "
            << best_chr.num_non_integral_vars << " & "
            << setprecision(2)
            << (((double)best_chr.num_non_integral_vars /
                 decoder.getNumBinaryVariables()) * 100);

        log_file.close();
        pop_statistics.close();
    }
    catch(IloException& e) {
        stringstream message;
        message << "\n***********************************************************"
                << "\n*** Exception Occurred: " << e
                << "\n***********************************************************"
                << endl;
        log_file << message.str();
        log_file.close();

        cerr << message.str();
        return_code = 70; // BSD software internal error code
    }
    // If somethig goes wrong....
    catch(std::exception& e) {
        stringstream message;
        message << "\n***********************************************************"
                << "\n*** Exception Occurred: " << e.what()
                << "\n***********************************************************"
                << endl;

        if(!log_file)
            log_file.open(log_filename.c_str(), ios::out|ios::app);

        log_file << message.str();
        log_file.close();

        cerr << message.str();
        return_code = 70; // BSD software internal error code
    }

    #ifdef TUNING
    cout << setiosflags(ios::fixed) << tuning_value; cout.flush();
    #endif

    return return_code;
}
