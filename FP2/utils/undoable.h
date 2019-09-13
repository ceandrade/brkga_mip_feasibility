/**
 * @file undoable.h
 * @brief Base class for undoable changes
 *
 * @author Domenico Salvagnin dominiqs@gmail.com
 * 2011
 */

#ifndef UNDOABLE_H
#define UNDOABLE_H

#include "managed_object.h"

namespace dominiqs {

/**
 * Interface for undoable changes
 */

class Undoable : private ManagedObject<Undoable>
{
public:
	virtual void redo() = 0;
	virtual void undo() = 0;
};

} // namespace dominiqs

#endif /* UNDOABLE_H */
