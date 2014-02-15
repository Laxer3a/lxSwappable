#include "lxSwappablePointer.h"
using namespace lx;

class Sample {
	MAKESWAPPABLE(Sample)
public:
	Sample(SwappableManager* mgr)
	:_trackMe(this,mgr)
	{
	}
};

int main(int argc, char* argv[])
{
	hotswap_ptr<Sample> g_helloSwappable;
	hotswap_ptr<Sample> g_helloSwappable2;
	hotswap_ptr<Sample> g_helloSwappable3;

	SwappableManager* pMgr = new SwappableManager();
	int size = SwappableManager::getAllocSize(5000);
	pMgr->init(new unsigned char[size], size, 5000);
	Sample* sample = new Sample(pMgr);

	g_helloSwappable	= sample;
	g_helloSwappable2	= sample;
	g_helloSwappable3	= sample;

	g_helloSwappable2	= 0;

	g_helloSwappable	= 0;

	g_helloSwappable3.hotSwapTo(new Sample(pMgr));

	/*
	MyClass aClass(NULL);
	SomeOwner bClass;

	TRACK_ASSIGN(bClass.myClass, &aClass);
	*/

	return 0;
}

