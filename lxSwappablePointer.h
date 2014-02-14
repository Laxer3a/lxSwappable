#ifndef LX_SWAPPABLE_H
#define LX_SWAPPABLE_H

namespace lx {
/*
// ====================================================================================
//	Library for hot-swappable smart pointer.
//	- Zero overhead using the pointer, work the same as any pointer.
//	- Memory overhead for each smart pointer of 3 pointers instead of 1.
//	 (thus increase cache miss probability on big classes with a lot of smart pointer)
//	- Overhead when assigning a new value to the smart pointer, O(1) algorithm.
//
//  Usage
//	- Create a manager instance which control registrations.
//	- Modify your classes to be swappable, no need to modify inheritance.
//	- Use the hotswap_ptr<...> template for your member and use them as you would usually use pointer.
// ====================================================================================
*/
/* 
Sample Usage here :
	#include "LX_SwappablePointer.h"		// 0. Add the include
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

class Swappable;

struct SwappableInstance {
	SwappableInstance()
	:ptr	(0)
	,next	(0)
	,prev	(0) {}

	const void* ptr;
	SwappableInstance* next;
	SwappableInstance* prev;
};

/*  
	====================================================================================
	Manager tracking all the swappable objects.
	User has to provide memory, no allocation is performed by the system. 
	====================================================================================
*/
class SwappableManager {
	friend class Swappable;
private:
	/////////////////////////////////////////////////////////////////////
	// Internal arrays and associated allocator info.
	// Uses double link-list using arrays index on 24 bit.
	/////////////////////////////////////////////////////////////////////

	struct SLOTLIST {
		// 6 Byte per entry.
		unsigned short	m_prev16;
		unsigned short	m_next16;
		unsigned char	m_prev8;
		unsigned char	m_next8;
	};

	struct ITEM {
		Swappable*			m_item;
		SwappableInstance*	m_linkList;
	};

	ITEM*			m_arrayList;
	SLOTLIST*		m_allocList;
	unsigned int	m_freeSwappable;
	unsigned int	m_totalSwappable;
	unsigned int	m_usedIdxSwappable;
	unsigned int	m_freeIdxSwappable;

	/////////////////////////////////////////////////////////////////////
	// Internal null constant for array index link list
	/////////////////////////////////////////////////////////////////////
	static const int	NULL_IDX	= 0x00FFFFFF;
	static const int	NULL_IDX16	= 0xFFFF;
	static const int	NULL_IDX8	= 0xFF;

private:

	//
	// Internal implementation of the manager
	// - Remove swappable entry
	// - Allocate swappable entry
	// - Add and remove a user of the same instance on the list.
	//

	void freeSwappable(unsigned int handle);

	int allocateSwappable(Swappable* pTracker);

	inline
	void addListStart(SwappableInstance* wrapper, unsigned int handle) {
		SwappableInstance* prevHead = m_arrayList[handle].m_linkList;
		if (prevHead) {
			prevHead->prev = wrapper;
		}
		wrapper->next = prevHead;
		wrapper->prev = 0;

		m_arrayList[handle].m_linkList = wrapper;
	}

	inline
	void removeListStart(SwappableInstance* wrapper, unsigned int handle) {
		// Remove just first item and put new one.
		m_arrayList[handle].m_linkList = wrapper->next;
	}

	void replaceObject	(Swappable* oldInstance, Swappable* newInstance);
public:
	/*	Function for the client to know how much memory is needed in advance before
		doing calling the init(...) function */
	static int getAllocSize(int SwappableMaxCount);
	
	/*	Setup buffer used by the manager to track our instances.
		- Set the buffer used for tracking, size of the buffer given.
		- Set the buffer size to be sure.
		- Maximum number of instances tracked. Maximum is 0xFFFFFF
		
		Return true if successfull, false if memory was not big enough.*/ 		
	bool init(void* alignPtr_buffer, int bufferSize, int SwappableMaxCount);

	/** 
		Just a clean interface for future extension.
		Manager should NEVER be destroyed before anything else.
	 */
	// May be do assert here to check that somebody is still in the room...
	void release() { }
};

/** Just add a Swappable member to any of your classes, can handle 3 user without any allocation */
class Swappable {
	friend class SwappableManager;

	/** Pass a buffer allocated outside, responsability to free it to the library user.
	 */
public:
	Swappable(void* obj, SwappableManager* mgr)
	{
		m_owner			= obj;
		m_mgr			= mgr;
		registerObject(this);
	}

	~Swappable() {
		unregisterObject(this);
	}

	inline
	void _SwappableReset	(SwappableInstance* wrapper) {
		//
		// Remove item from link list
		//

		if (wrapper->prev == 0) {
			this->m_mgr->removeListStart(wrapper, m_handle);
		} else {
			// Reconnection previous and next.
			wrapper->prev->next = wrapper->next;
		}

		if (wrapper->next) {
			wrapper->next->prev = wrapper->prev;
		}
	}

	inline
	void _SwappableWrite	(SwappableInstance* wrapper, void* obj) {
		//
		// Add item to link list
		//
		m_mgr->addListStart(wrapper, m_handle);
	}
private:
	// Tracker registration
	void registerObject	(Swappable* tracker);
	void unregisterObject(Swappable* tracker);

	// 6x Pointers + 32 bit int.
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


//
// Smart pointer like macro, no overhead when using the pointer.
//
template < typename T >
class hotswap_ptr {
	friend class Swappable;
private:
	SwappableInstance instance;

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
				((T*)instance.ptr)->_trackMe._SwappableWrite(&instance, (void*)ptr);
			}
		}
	}
public:
	hotswap_ptr()
	{
	}

	hotswap_ptr(T* pValue) : pData(pValue)
	{
		instance.ptr = pValue;
		pValue->_trackMe._SwappableWrite(&instance, (void*)pValue);
	}

	~hotswap_ptr()
	{
		((T*)instance.ptr)->_trackMe._SwappableWrite(&instance, (void*)NULL);
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
	
	/* Hotswap from any place all user of the same pointer.
	   Return false if current object is NULL or if new object is NULL.*/
	bool hotSwapTo(T* obj) {
		if (this->instance.ptr && obj) {
			T* a = (T*)this->instance.ptr;
			a->_trackMe->m_mgr->replace(a->_trackMe, obj->_trackMe);
		} else {
			// 
			return false;
		}
	}
};

};

#endif
