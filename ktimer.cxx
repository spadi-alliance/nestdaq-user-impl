/*
 * A Kittchen Timer
 *
 */
#ifndef KTIMER_H
#define KTIMER_H

#include <chrono>

class KTimer
{
public:
	KTimer(){};
	KTimer(int t){
		SetDuration(t);
		f_start = std::chrono::system_clock::now();
	};
	virtual ~KTimer(){};
	void SetDuration(int t){
		f_duration = std::chrono::milliseconds(t);
		f_start = std::chrono::system_clock::now();
	};
	void Reset(){
		f_start = std::chrono::system_clock::now();
	};
	bool Check(){
		auto now = std::chrono::system_clock::now();
		auto elapse = (now - f_start);
		if (elapse > f_duration) {
			Reset();
			return true;
		} else {return false;}
	};
	bool TryCheck(){
		auto now = std::chrono::system_clock::now();
		auto elapse = (now - f_start);
		if (elapse > f_duration) {
			return true;
		} else {
			return false;
		}
	};
protected:
private:
	std::chrono::system_clock::time_point f_start;
	std::chrono::system_clock::duration f_duration;
};
#endif

#ifdef TEST_MAIN_KTIMER
#include <iostream>
#include <unistd.h>
int main(int argc, char* argv[])
{
	KTimer kt;

	kt.SetDuration(100);
	while (true) {
		if (kt.Check()) {
			std::cout << "x" << std::flush;
			//break;
		} else {
			std::cout << ".";
		}
		usleep(1000);
	}
	return 0;
}
#endif

#if 0
start = std::chrono::system_clock::now();

static std::chrono::system_clock::time_point last;
std::cout << " Elapsed time:"
<< std::chrono::duration_cast<std::chrono::microseconds>
(now - last).count();
last = now;
std::cout << std::endl;

std::chrono::milliseconds ms( 321 );
// std::chrono::duration< int > md = ms;
std::chrono::duration< int, std::milli > md = ms;
#endif
