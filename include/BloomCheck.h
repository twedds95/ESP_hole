#ifndef BLOOMCHECK_H
#define BLOOMCHECK_H

#include <Arduino.h>

bool bloomCheck(const char *domain);
void setupBloom();
void handleTimedBloomMsg();

#endif //BLOOMCHECK_H