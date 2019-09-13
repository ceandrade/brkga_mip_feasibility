/**
 * @file history.h
 * @brief State Managemente/Undo System Base Classes
 *
 * @author Domenico Salvagnin dominiqs@gmail.com
 * 2008
 */

#ifndef HISTORY_H
#define HISTORY_H

#include <boost/intrusive_ptr.hpp>

#include <utils/managed_object.h>

/**
 * @brief State Management Base Class
 *
 * This class provides the simple interface dump & restore
 * used for state managament.
 * It is a reference counted (smart) object
 */

class State : private dominiqs::ManagedObject<State>
{
public:
	/**
	 * @brief copy some state inside the object
	 *
	 * Derived classes are responsible for providing a link to the object we are
	 * storing the state for
	 */
	virtual void dump() = 0;
	/**
	 * @brief restore the state of the object
	 *
	 * Derived classes are responsible for providing a link to the object we are
	 * restoring the state to
	 */
	virtual void restore() =0;
	// virtual destructor
	virtual ~State() {}
};

// typedef boost::intrusive_ptr<State> StatePtr;
typedef State* StatePtr;

/**
 * @brief Undoable Action Base Class
 *
 * This class provides the simple interface mark & undo
 * It is a reference counted (smart) object
 */

class Undoable : private dominiqs::ManagedObject<Undoable>
{
public:
	/**
	 * @brief mark current state as the one to return to in case of undo
	 */
	virtual void mark() = 0;
	/**
	 * @brief undo to the last marked state
	 */
	virtual void undo() =0;
};

typedef boost::intrusive_ptr<Undoable> UndoablePtr;

#endif /* HISTORY_H */
