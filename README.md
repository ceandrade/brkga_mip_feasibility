BRKGA for MIP feasibility
===============================================================================

We present a new framework for finding feasible solutions to mixed integer
programs (MIP). We use the feasibility pump heuristic coupled to a biased
random-key genetic algorithm (BRKGA). The feasibility pump heuristic attempts
to find a feasible solution to a MIP by first rounding a solution to the linear
programming (LP) relaxation to an integer (but not necessarily feasible)
solution and then projecting it to a feasible solution to the LP relaxation.
The BRKGA is able to build a pool of projected and rounded but not necessarily
feasible solutions and to combine them using information from previous
projections. This information is also used to fix variables during the process
to speed up the solution of the LP relaxations, and to reduce the problem size
in enumeration phases. Experimental results show that this approach is able to
find feasible solutions for instances where the original feasibility pump or a
commercial mixed integer programming solver often fail.


Dependencies
-------------------------------------------------------------------------------

- [IBM ILOG CPLEX 12.6.1 or greater](https://www.ibm.com/products/ilog-cplex-optimization-studio). 
  Other solvers can also be used. However, changes in the codes are needed 
  to adequate their API;

- [Eigen library](http://eigen.tuxfamily.org): a 2015 version is included in 
  this package. Please, see proper licenses when using Eigen;

- [The C Clustering Library](http://bonsai.hgc.jp/~mdehoon/software/cluster):
  also included in this package. See lincense for proper use;

- [Boost libraries](https://www.boost.org): common large set of libraries for
  C++. Not included since is system dependent. See documentation how to 
  install in your system.

- [GNU G++ >= 5.2](https://gcc.gnu.org). [Clang](https://clang.llvm.org) 
  should work too but changes are needed on the makefiles;

- [libxml2](http://www.xmlsoft.org/): this is a FP2.0 dependency.  Once
  installed, set the include path for this lib on "FP2/Makefile", lines 45 and
  46. Note that each operating system has a way to install and deal with
  libxml. On GNU/Linux distributions, to install this lib should be an easy
  task. On Mac OSX, you can use MacPorts or Homebrew. I haven't tried this code
  on Windows;

- [Doxygen](http://doxygen.nl): optional for generating documentation.


Configuration
-------------------------------------------------------------------------------

You must set your headers and libs paths according to your system installation.
Such configurations vary from system to system, in a way that we cannot
describe all scenarios here. On the main `Makefile`, you can find Section "Lib
and include definitions" from line 160.  There are multiple paths to be set
there (headers and libs), including CPLEX config from line 200. The code does
not compile either does link if such configurations are not set correctly.

Note that this code was tested with CPLEX 12.6.2. It should work with newer
CPLEX versions since the API didn't change too much, AFAIK, but pay attention
that the ABI changed on Mac OSX on CPLEX 12.8. So, you may need to make more
changes to the code link correctly on Mac OSX.

The original Feasibility Pump code (on folder `FP2`) also depends on CPLEX, and
the paths must be set accordingly. On folder `FP2/make`, edit file
`pcfisch4b.mk` and set `CPLEX_DIR` and `CPLEX_LIBDIR` according to your system.
If you find some problem ou doubts about FP2.0 code, please, contact the
authors directly, since I cannot support their code.


Usage
-------------------------------------------------------------------------------

`brkga-feasibility-pump` has a massive set of parameters! For historical and
experimental reasons, all parameters must be given in the command-line, which
can be a little bit tedious and error-prone. We understand that it will be
better to read all default parameters from a file, and only change the ones
given in the command-line. We let this feature to the TODO part. For access to
all parameters, type `brkga-feasibility-pump --help`.

The parameters are divided into 7 sections:

* BRKGA section:
 - <config-file>: parameters of BRKGA algorithm.
 - <seed>: seed for random generator.
 - <stop-rule> <stop-arg>: stop rule and its arguments where:
	+ (G)enerations <number_generations>: the algorithm runs until <number_generations>;
	+ (T)arget <value of expected target>: runs until obtains the target value;
	+ (I)terations <max generations without improvement>: runs until the solutions don't.
 - <max-time>: max running time (in seconds). If 0, the algorithm stops on chosen stop rule.
 - <LP-or-MPS-file>: describes the service wire center (subject locations and demands).
 - <output-dir>: folder to save the results. All files will have the <LP-or-MPS-file>
   prefix in their names.
 - <initial-population>: number of individuals in the initial relaxation.
   The first individual is a full relaxation of the model, and the others
   are built fixing binary variables individually:
	+ < 0: number of relaxation is at most the size of the population;
	+ == 0: no initial population;
	+ > 0: neither the given number, or the number of binary variables at most.
 - <pump-strategy>: the feasibility pump strategy:
	+ Default: default feasibility pump using only the distance function.
	+ Objective: feasibility pump using a convex combination between the
	             distance function and the original objective function.

* OBJ - Objective-function parameters:
 - <fitness-type>: defines how the fitness is computed:
	+ Convex: the convex combination (beta * Delta) + (1 - beta) * zeta
	  where "Delta" is the the distance between a LP feasible and
	  an integer solution (as in the default feasibility pump), and
	  "zeta" is the measure of infeasibility, usually the number of fractional variables. 
	+ Geometric: the convex combination Delta^beta x zeta^(1 - beta).
 - <minimization-factor: beta>: A factor in the range [0,1]. It is used to control the
   direction of the optimization. Note the when beta is 1.0, only
    Delta is used. When it is 0.0, only the measure of infeasibility is used.
 - <minimization-factor-decay>: it is used to change the direction of the optimization.
   Usually, this is done using a geometric decay. If it is equal to 1.0, nothing is changed.
 - <decay-application-offset>: the number of iterations without improvement before apply
   the decay in minimization factor.

* FP - feasibility pump
 - <feas-pump-param: iteration_limit>: maximum number of iterations without improvement.
 - <feas-pump-param: perturb_when_cycling>: indicates if a perturbation must be done when FP cycles.
	+ Perturb: does the shaking.
	+ NotPerturb: does not do the shaking.
 - <feas-pump-param: t>: parameter used to control the weak perturbation in the
   cycling detection.
 - <feas-pump-param: rho_lb>: Parameter used to control the strong perturbation
   in the cycling detection. This is the lower bound.
 - <feas-pump-param: rho_ub>: Parameter used to control the strong perturbation
   in the cycling detection. This is the upper bound.

* OFP - Objective-function feasibility pump:
 - <obj-feas-pump-param: phi>: this is the decay factor used to change the
   objective function in the LP phase if using objective feasibility pump.
 - <obj-feas-pump-param: delta>: this is the minimum difference between two

* FIXING - paramaters for variable fixing
   iterations. This parameter is used to detect cycling in the objective feasibility pump.
 - <var_fixing_percentage>: percentage of variables to be fixed. Range [0,1].
   If 0, the fixing percentage is determined automatically using information from LP
   relaxation. if < 0, does not perform fixing.
 - <var_fixing_growth_rate>: grownth rate on var_fixing_percentage. Range [0,1].
 - <var_fixing_frequency>: number of iterations between to variable fixings.
 - <var_fixing_type>: defines the type of the fixing:
	+ Ones: fix variables such value is one in the most roundings.
	+ Zeros: fix variables such value is zero in the most roundings.
	+ Fractionals: Fix variables such value is splitted between zeros and ones
	               among the roundings.
	+ Automatic: let the algorithm decide (using LP relaxation info).

* ROUNDINGCUT - parameters to control the rouding
 - <roundcuts_percentage>: percentage of the population used to produce
   cuts avoind such (infeasible) roundings. Range [0,1].

* MIPLOCALSEARCH - parameters to control the MIP phase
 - <miplocalsearch-threshold>: maximum percentage of fractional variables to
   launch a MIP local search. Range [0,1].
 - <miplocalsearch-discrepancy_level>:  Defines the discrepancy level to be used
   when fixing variable during MIP local search. Range [0,1]. For instance, at the
   discrepancy level of 0.05, variables such roundings have the same value in,
   at least, 95% of roudings, will be fixed to this value. Value 0.0 represents
   no tolerance to discrepancy, i.e., all roundings to a variable must be the same.
 - <miplocalsearch-unfix-levels>: controls the recursion on unfix variables.
   If zero, no unfix is performed. If 1, all varaibles that belong to constraints
   with free variables are unfixed. If greater than 2, the unfix is done recursively.
 - <miplocalsearch-max-time>: time used in the local search. If the time is less than
   or equal to zero, the remaining optimization time is used.
 - <miplocalsearch-constraint-filtering>: defines which constraints consider during the unfix phase:
	+ All: consider all constraints (not filtering at all).
	+ Duals: only constraints such dual values in the relaxation are not zero.
	+ SlacksAndDuals: as in "Duals" but also consider constraints with slacl values equal to zero.
