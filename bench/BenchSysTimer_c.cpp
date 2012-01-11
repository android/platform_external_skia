#include "BenchSysTimer_c.h"

//Time
#include <time.h>

void BenchSysTimer::startWall() {
    this->fStartWall = time(NULL);
}
void BenchSysTimer::startCpu() {
    this->fStartCpu = clock();
}

double BenchSysTimer::endCpu() {
    clock_t end_cpu = clock();
    return (end_cpu - this->fStartCpu) / CLOCKS_PER_SEC * 1000.0;
}
double BenchSysTimer::endWall() {
    time_t end_wall = time(NULL);
    return difftime(end_wall, this->fStartWall) * 1000.0;
}
