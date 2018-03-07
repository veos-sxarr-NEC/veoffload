//
// /opt/nec/ve/bin/ncc -shared -fpic -pthread -o libvesleep.so libvesleep.c
//
#include <stdio.h>
#include <unistd.h>

int do_sleep(int secs)
{
	
	printf("VE: sleeping %d seconds\n", secs);
	sleep(secs);
	printf("VE: finished sleeping.\n");
	return secs;
}
