#ifndef FilterProf_h
#define FilterProf_h

// Per-TimeFrame stage timing shared by LogicFilter and LogicFilterGpu.
//
//   index    MakeBlockMap + BuildHBF
//   trigger  Exec / ExecBatch (mark + scan; on the GPU it includes the copies)
//   post     MarkFlagSending
//   build    assembling outParts, before anything is sent
//
// Every stage starts after Receive() has returned and ends before the
// console output, so neither the wait for data nor printing is counted.

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

class FilterProf
{
public:
	using Clock = std::chrono::steady_clock;

	static double Us(Clock::time_point a, Clock::time_point b)
	{
		return std::chrono::duration<double, std::micro>(b - a).count();
	}

	void Add(double index, double trigger, double post, double build)
	{
		fIndex.push_back(static_cast<float>(index));
		fTrigger.push_back(static_cast<float>(trigger));
		fPost.push_back(static_cast<float>(post));
		fBuild.push_back(static_cast<float>(build));
	}

	// Means over the TimeFrames since the previous call.
	void PrintWindow()
	{
		const size_t n = fTrigger.size();
		if (n <= fWindowStart) return;
		std::cout << "#Prof n=" << (n - fWindowStart) << std::fixed << std::setprecision(1)
			<< " index=" << Mean(fIndex, fWindowStart)
			<< " trigger=" << Mean(fTrigger, fWindowStart)
			<< " post=" << Mean(fPost, fWindowStart)
			<< " build=" << Mean(fBuild, fWindowStart)
			<< " us/TF" << std::defaultfloat << std::endl;
		fWindowStart = n;
	}

	// One line per run, read by the signal-count sweep.
	void PrintSummary(const std::string& name, const std::string& backend, int nsignal)
	{
		const size_t n = fTrigger.size();
		std::cout << "#ProfSum device=" << name << " backend=" << backend
			<< " nsig=" << nsignal << " ntf=" << n;
		if (n > 0) {
			std::vector<float> proc(n);
			for (size_t i = 0 ; i < n ; i++) proc[i] = fIndex[i] + fTrigger[i] + fPost[i];
			std::cout << std::fixed << std::setprecision(2)
				<< " index_med=" << Quantile(fIndex, 0.5)
				<< " trigger_med=" << Quantile(fTrigger, 0.5)
				<< " trigger_mean=" << Mean(fTrigger, 0)
				<< " trigger_p90=" << Quantile(fTrigger, 0.9)
				<< " post_med=" << Quantile(fPost, 0.5)
				<< " build_med=" << Quantile(fBuild, 0.5)
				<< " proc_med=" << Quantile(proc, 0.5)
				<< " proc_mean=" << Mean(proc, 0)
				<< std::defaultfloat;
		}
		std::cout << " (us/TF)" << std::endl;
	}

	void Clear()
	{
		fIndex.clear();
		fTrigger.clear();
		fPost.clear();
		fBuild.clear();
		fWindowStart = 0;
	}

private:
	static double Mean(const std::vector<float>& v, size_t from)
	{
		if (v.size() <= from) return 0;
		double s = 0;
		for (size_t i = from ; i < v.size() ; i++) s += v[i];
		return s / static_cast<double>(v.size() - from);
	}

	static double Quantile(std::vector<float> v, double q)
	{
		if (v.empty()) return 0;
		const size_t k = static_cast<size_t>(q * static_cast<double>(v.size() - 1));
		std::nth_element(v.begin(), v.begin() + k, v.end());
		return v[k];
	}

	std::vector<float> fIndex;
	std::vector<float> fTrigger;
	std::vector<float> fPost;
	std::vector<float> fBuild;
	size_t fWindowStart = 0;
};

#endif
