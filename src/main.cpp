#include "SchedulerTest.h"

#include <cstdlib>

int main()
{
	return zonejobscheduler::tests::runAllTests()
		? EXIT_SUCCESS
		: EXIT_FAILURE;
}
