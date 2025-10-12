#include "utils.h"

namespace utils {

unsigned long ints2long(unsigned int a, unsigned int b) 
{
	unsigned long c = (((unsigned long) a) << 32) + b;
	return c;
}

}