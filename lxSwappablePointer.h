/*
// ====================================================================================
//	Library for hot-swappable smart pointer.
//
//	- Zero overhead using the pointer, work the same as any pointer.
//	- Memory overhead for each smart pointer of 3 pointers instead of 1.
//	 (thus increase cache miss probability on big classes with a lot of smart pointer)
//	- Overhead when assigning a new value to the smart pointer, O(1) algorithm.
//
//  Usage :
//	- Create a manager instance which control registrations.
//	- Modify your classes to be swappable, no need to modify inheritance.
//	- Use the hotswap_ptr<...> template for your member and use them as you would usually use pointer.
//
//  Note :
//  - If a swappable class is destroyed, pointer stays as invalid pointer.
//	  Policy in implementation could force a purge to clean all references to NULL pointer.
//  - If the manager is destroyed before all swappable classes are destroyed crash can occur.
//	  Again policy in implementation could decide.
//
//	The choice should be given to the user and do not implement default complex policy.
//  Safer policy should indeed be implemented later on, but flag should be setup by the user
//  to execute it or not (bigger CPU usage)
// ====================================================================================

Sample Usage here :
	#include "lxSwappablePointer.h"			// 0. Add the include
	using namespace lx;						// Short cut for names.

	//
	// A class you want instances to be hot-swappable.
	//
	class MyClass {							// Step
		MAKESWAPPABLE(MyClass)				// 1. Put class as Swappable as the TOP of the class (else your visibility could be messed up)
											//
	public:									// Step
		MyClass(SwappableManager* mgr);		// 2. Modify constructor OR pass manager in a second phase constructor (ie init func)
											//    The reason here for passing a Manager as argument is usage in multithreaded
											//	  Environment, library could also use a singleton with a proper criticalsection / atomic usage.
	};

	MyClass::MyClass(SwappableManager* mgr)	// Step
	:_trackMe(this,mgr)						// 3. Do not forget this little initialization.
	{
	}

	//
	// Now some classes using a hot-swappable pointer.
	//
	class SomeOwner {
	public:
		hotswap_ptr<MyClass> myClass;

		void someSetter(MyClass* obj) {
			myClass = obj;
		}
	};

	//
	// We are done !
	// Now perform the swapping call from any instances of the hotswap_ptr containing the pointer.
	//
*/

#ifndef LX_SWAPPABLE_H
#define LX_SWAPPABLE_H

// WHY DO I NEED FOR PORTABILITY FUCK SAKE to have a size_t and a fucking long build time include ?
#include <cstddef>

namespace lx {

class Swappable;

/*  ====================================================================================
	Manager tracking all the swappable objects.
	User has to provide memory, no allocation is performed by the system.
	==================================================================================== */
class SwappableManager {
public:
	/*	Function for the client to know how much memory is needed in advance before
		doing calling the init(...) function */
	static
	int	 getAllocSize	(int SwappableMaxCount);

	/*	Setup buffer used by the manager to track our instances.
		- Set the buffer used for tracking, size of the buffer given.
		- Set the buffer size to be sure.
		- Maximum number of instances tracked. Maximum is 0xFFFFFF

		Return true if successful, false if memory was not big enough.			*/
	bool init			(void* alignPtr_buffer, int bufferSize, int SwappableMaxCount);

	/*	Just a clean interface for future extension.
		Manager should NEVER be destroyed before anything else.
		(May be do assert here to check that somebody is still in the room...)	*/
	void release		() { }

private:

	//
	//
	// PRIVATE SECTION
	//
	//

	friend class Swappable;
	template<class U> friend class hotswap_ptr;

	/* Structure used inside each smart pointer as a link list item.			*/
	struct SwappableInstance {
		SwappableInstance()
		:ptr	(0)
		,next	(0)
		,prev	(0) {}

		/** One can note that the same pointer is duplicate all over the link list,
			but if we stored the pointer once, we would need another pointer
			to the beginning of the list, or a pointer to the swappable object
			or handle to the manager : creating much more overhead with same
			memory cost anyway. */
		const void* ptr;			// Real Pointer to instance of swappable object.
		SwappableInstance* next;	// Link list for next item with same pointer.
		SwappableInstance* prev;	// Link list for previous item with same pointer.
	};

	/*	Internal arrays and associated allocator info.
		Uses double link-list using arrays index on 24 bit.						*/
	struct SLOTLIST {
		// 6 Byte per entry.
		unsigned short	m_prev16;
		unsigned short	m_next16;
		unsigned char	m_prev8;
		unsigned char	m_next8;
	};

	/*	Information stored for each entry inside the manager					*/
	struct ITEM {
		Swappable*			m_item;					// Pointer to the registered swappable.
		SwappableInstance*	m_linkList;				// Pointer to the link list of references.
	};

	/* All array and variable for the manager									*/
	ITEM*				m_arrayList;				// List of registered swappable object.
	SLOTLIST*			m_allocList;				// Link list of registered swappable and free slot.
	unsigned int		m_freeSwappable;			// Number of available free swappable object.
	unsigned int		m_totalSwappable;			// Total number of swappable object we can register.
	unsigned int		m_usedIdxSwappable;			// Head to list of registered swappable object.
	unsigned int		m_freeIdxSwappable;			// Head to list of freely available object.

	/* Internal null constant for array index link list							*/
	static const unsigned int	NULL_IDX	= 0x00FFFFFF;	// 24 bit null
	static const unsigned short	NULL_IDX16	= 0xFFFF;		// 16 bit part null
	static const unsigned char	NULL_IDX8	= 0xFF;			//  8 bit part null

	/* Remove swappable entry													*/
	void freeSwappable		(unsigned int handle);

	/* Allocate swappable entry													*/
	unsigned int
         allocateSwappable	(Swappable* pTracker);

	/* Connect a reference at the beginning of the references link list			*/
	inline
	void addListStart		(SwappableInstance* wrapper, unsigned int handle) {
		SwappableInstance* prevHead = m_arrayList[handle].m_linkList;
		if (prevHead) {
			prevHead->prev = wrapper;
		}
		wrapper->next = prevHead;
		wrapper->prev = 0;

		m_arrayList[handle].m_linkList = wrapper;
	}

	/* Remove a reference at the beginning of the references link list			*/
	inline
	void removeListStart	(SwappableInstance* wrapper, unsigned int handle) {
		// Remove just first item and put new one.
		m_arrayList[handle].m_linkList = wrapper->next;
	}

	/* Remove a reference at the beginning of the references link list			*/
	void replaceObject		(Swappable* oldInstance, Swappable* newInstance);
};

/*  ====================================================================================
	  Member object to add to a swappable object.
	  It links the handle in the manager
	==================================================================================== */
class Swappable {
	template<class U> friend class hotswap_ptr;
	friend class SwappableManager;
public:
	/* Swappable stores pointer to the manager and reference to the original object.
	   It will receive a allocated handle in exchange */
	Swappable(void* obj, SwappableManager* mgr)
	{
		m_owner			= obj;
		m_mgr			= mgr;
		registerObject(this);
	}

	/* When Swappable is destroyed, ie when a swappable class dies (because it is a member)
	   Call the manager to unregister the pointer */
	~Swappable() {
		unregisterObject(this);
	}

	inline
	void _SwappableReset	(SwappableManager::SwappableInstance* wrapper) {
		//
		// Remove item from link list
		//

		if (wrapper->prev == 0) {
			// Remove from the beginning of the link list.
			this->m_mgr->removeListStart(wrapper, m_handle);
		} else {
			// Remove from the middle place of the link list.
			wrapper->prev->next = wrapper->next;
		}

		if (wrapper->next) {
			wrapper->next->prev = wrapper->prev;
		}
	}

	inline
	void _SwappableWrite	(SwappableManager::SwappableInstance* wrapper) {
		// Add item to link list
		m_mgr->addListStart(wrapper, m_handle);
	}
private:

	//
	//
	// PRIVATE SECTION
	//
	//

	// Tracker registration
	void registerObject		(Swappable* tracker);
	void unregisterObject	(Swappable* tracker);

	SwappableManager*	m_mgr;
	void*				m_owner;
	unsigned int		m_handle;

	// Force object to be a member or alloc on stack only.
	void *operator	new		( size_t );
	void operator	delete	( void*  );
	void *operator	new[]	( size_t );
	void operator	delete[]( void*	 );
};


// Public OR friend, so macros is public.
#define MAKESWAPPABLE(className)	\
public:\
	lx::Swappable _trackMe;\
private:\


/*  ====================================================================================
		Smart pointer like template, no overhead when using the pointer.
    ====================================================================================*/
template < typename T >
class hotswap_ptr {
	friend class Swappable;
private:
	SwappableManager::SwappableInstance instance;

	// Force object to be a member or alloc on stack only.
	void *operator	new		( size_t );
	void operator	delete	( void*  );
	void *operator	new[]	( size_t );
	void operator	delete[]( void*	 );

	void update(const T* ptr) {
		// Optimize updates
		if (ptr != instance.ptr) {
			if (instance.ptr) {
				((T*)instance.ptr)->_trackMe._SwappableReset(&instance);
			}

			instance.ptr = (const void*)ptr;

			if (ptr) {
				((T*)instance.ptr)->_trackMe._SwappableWrite(&instance);
			}
		}
	}
public:
	hotswap_ptr()
	{
	}

	hotswap_ptr(T* pValue)
	{
		if (pValue) {
			instance.ptr = pValue;
			pValue->_trackMe._SwappableWrite(&instance, (void*)pValue);
		}
	}

	~hotswap_ptr()
	{
		((T*)instance.ptr)->_trackMe._SwappableReset(&instance);
	}

	T& operator* ()
	{
		return *((T*)(instance.ptr));
	}

	T* operator-> ()
	{
		return (T*)(instance.ptr);
	}

	hotswap_ptr<T>& operator = (const hotswap_ptr<T>& sp)
	{
		if (this != &sp) {
			update(sp.instance.ptr);
		}
		return *this;
	}

	hotswap_ptr<T>& operator = (const T* obj)
	{
		if (this->instance.ptr != obj) {
			update(obj);
		}
		return *this;
	}

	hotswap_ptr<T>& operator = (T* obj)
	{
		if (this->instance.ptr != obj) {
			update(obj);
		}
		return *this;
	}

	// Support for user defined (void*) NULL macro in some cases.
	/*
		Not supported for now, we do not want people to stick any pointer by mistake and be "OK".
		Prefer to have a nice compiler error about the type.
		So, what happends with nullptr Cx11 isnt very nice either.

		The problem is that I do NOT want to support exception in my library.
		The code should be portable and fast for embedded systems.

	hotswap_ptr<T>& operator = (void* obj)
	{
		if (this->instance.ptr != obj) {
			update(obj);
		}
		return *this;
	}
	*/

	// Support for NULL, it can't be helped.
	hotswap_ptr<T>& operator = (int obj)
	{
		if (obj == 0) {
			if (this->instance.ptr != (void*)0) {
				update(0);
			}
		}
		return *this;
	}

	/* Hotswap from any place all user of the same pointer.
	   Return false if current object is NULL or if new object is NULL.*/
	bool hotSwapTo(T* obj) {
		if (this->instance.ptr && obj) {
			T* a = (T*)this->instance.ptr;
			a->_trackMe.m_mgr->replaceObject(&a->_trackMe, &obj->_trackMe);
			return true;
		} else {
			//
			return false;
		}
	}
};

};

#endif
