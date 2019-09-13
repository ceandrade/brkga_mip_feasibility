/**
 * @file fp_interface.h
 * @brief Solution Transformers (a.k.a. rounders) Interface
 *
 * @author Domenico Salvagnin <dominiqs at gmail dot com>
 * 2008
 */

#ifndef FP_INTERFACE_H
#define FP_INTERFACE_H

#include <vector>
#include <boost/shared_ptr.hpp>

#include <utils/singleton.h>
#include <utils/factory.h>
#include <utils/xmlconfig.h>
#include <utils/chrono.h>
#include <cpxutils/cpxutils.h>
#include <cpxutils/model.h>

namespace dominiqs {

/**
 * Solution Transformer interface
 * Base class for frac->int (i.e. rounding) transformations
 */

class SolutionTransformer
{
public:
	virtual ~SolutionTransformer() {}
	virtual void readConfig() {}
	/**
	 * Read needed information (if any) about the problem (@param pinfo)
	 */
	virtual void init(const dominiqs::Model& model, bool ignoreGeneralInt = true) {}
	virtual void ignoreGeneralIntegers(bool flag) {}
	/**
	 * Trasform the vector given as input @param in and store the result in @param out
	 */
	virtual void apply(const std::vector<double>& in, std::vector<double>& out) = 0;
	virtual void newIncumbent(const std::vector<double>& x, double objval) {}
	/**
	 *
	 */
	virtual void clear() {}
};

typedef boost::shared_ptr<SolutionTransformer> SolutionTransformerPtr;

typedef SingletonHolder< Factory<SolutionTransformer, std::string> > TransformersFactory;

#ifdef LIBFP_STATIC
// manual registration
void registerTransformers();
#endif // LIBFP_STATIC

} // namespace dominiqs

#endif /* FP_INTERFACE_H */
