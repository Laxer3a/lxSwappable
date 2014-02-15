#include "lxSwappablePointer.h"

namespace lx {

void SwappableManager::freeSwappable(unsigned int handle) {
	SLOTLIST* freeEntry = &m_allocList[handle];
	unsigned int next = (unsigned int)(freeEntry->m_next16 | (freeEntry->m_next8 << 16));
	unsigned int prev = (unsigned int)(freeEntry->m_prev16 | (freeEntry->m_prev8 << 16));

	//
	// Use update
	//
	if (next != (unsigned int)NULL_IDX) {
		m_allocList[next].m_prev16 = (unsigned short) prev;
		m_allocList[next].m_prev8  = (unsigned char )(prev >> 16);
	}

	if (prev != (unsigned int)NULL_IDX) {
		m_allocList[prev].m_next16 = (unsigned short) next;
		m_allocList[prev].m_next8  = (unsigned char )(next >> 16);
	} else {
		m_usedIdxSwappable = next;
	}

	//
	// Delete update
	//
	freeEntry->m_next16 = (unsigned short) m_freeIdxSwappable;
	freeEntry->m_next8  = (unsigned char )(m_freeIdxSwappable>>16);

	m_freeIdxSwappable = handle;
	m_freeSwappable++;
}

unsigned int SwappableManager::allocateSwappable(Swappable* pTracker) {
	unsigned int oldFree = m_freeIdxSwappable;
	if (oldFree != (unsigned int)NULL_IDX) {
		//
		// Update free list.
		//
		SLOTLIST* newEntry = &m_allocList[oldFree];
		m_freeIdxSwappable = (unsigned int)(newEntry->m_next16 | (newEntry->m_next8 << 16));
		newEntry->m_next16 = (unsigned short) m_usedIdxSwappable;
		newEntry->m_next8  = (unsigned char )(m_usedIdxSwappable>>16);
		newEntry->m_prev16 = NULL_IDX16;
		newEntry->m_prev8  = NULL_IDX8;

		// No need to update LEFT of next free item -> m_connection[free].m_prev = NULL_ID;
		if (m_usedIdxSwappable != (unsigned int)NULL_IDX) {
			m_allocList[m_usedIdxSwappable].m_prev16 = (unsigned short) oldFree;
			m_allocList[m_usedIdxSwappable].m_prev8  = (unsigned char )(oldFree>>16);
		}

		m_usedIdxSwappable = oldFree;
		m_arrayList[oldFree].m_item		= pTracker;
		m_arrayList[oldFree].m_linkList	= 0;
		m_freeSwappable--;

		return oldFree;
	} else {

	}
	return ((unsigned int)-1);
}

void SwappableManager::replaceObject	(Swappable* oldInstance, Swappable* newInstance) {
	unsigned int handleOld = oldInstance->m_handle;
	SwappableInstance* pStart    = m_arrayList[handleOld].m_linkList;
	SwappableInstance* pInstance = pStart;
	SwappableInstance* pPrev     = 0;

	// Patch the memory with the new link list.
	while (pInstance) {
		pInstance->ptr = newInstance->m_owner;
		pPrev = pInstance;
		pInstance = pInstance->next;
	}

	if (pPrev) {
		// TODO
	}

	// Move the link list to new instance link list.
}

/*static*/
int SwappableManager::getAllocSize(int SwappableMaxCount) {
	unsigned int bufferSizeTrackList		= SwappableMaxCount * sizeof(ITEM);
	unsigned int bufferSizeTrackListAlloc	= SwappableMaxCount * sizeof(SLOTLIST);
	return (int)(bufferSizeTrackList + bufferSizeTrackListAlloc);
}

bool SwappableManager::init(void* alignPtr_buffer, int bufferSize, int SwappableMaxCount) {
	// 1. Array of Swappable Instance.
	unsigned int bufferSizeTrackList		= SwappableMaxCount * sizeof(ITEM);
	unsigned int bufferSizeTrackListAlloc	= SwappableMaxCount * sizeof(SLOTLIST);

	// 2. If user give space is enough
	if ((unsigned int)bufferSize >= (bufferSizeTrackList + bufferSizeTrackListAlloc)) {
		unsigned char* ptr	= (unsigned char*)alignPtr_buffer;
		// List of Swappable
		m_arrayList			= (ITEM*)ptr;
		// Link list items
		m_allocList			= (SLOTLIST*)&m_arrayList[SwappableMaxCount];

		//
		// Internal allocator double link list setup.
		//
		m_freeSwappable		= (unsigned int)SwappableMaxCount;
		m_totalSwappable	= m_freeSwappable;

		m_usedIdxSwappable	= NULL_IDX;
		m_freeIdxSwappable	= 0;

		int n;
		for (n=0; n < (int)m_freeSwappable; n++) {
			int idx = n + 1;
			m_allocList[n].m_next16	= (unsigned short)idx;
			m_allocList[n].m_next8	= (unsigned char)(idx>>16);

			idx = n - 1;
			m_allocList[n].m_prev16	= (unsigned short)idx;
			m_allocList[n].m_prev8	= (unsigned char)(idx>>16);
		}
		m_allocList[n].m_next16	= NULL_IDX16;
		m_allocList[n].m_next8	= NULL_IDX8;

		return true;
	} else {
		return false;
	}
}



void Swappable::registerObject	(Swappable* tracker) {
	int handle = (int)m_mgr->allocateSwappable(tracker);
	if (handle >= 0) {
		tracker->m_handle = (unsigned int)handle;
	}
}

void Swappable::unregisterObject(Swappable* tracker) {
	// Free the handle
	m_mgr->freeSwappable(tracker->m_handle);
}

} // End namespace lx
