
#ifndef STRIP_EFFICIENCY_H
#define STRIP_EFFICIENCY_H

#include "analysis.h"
#include "aligner.h"
#include <TFile.h>
#include <TH1D.h>
#include <TH2D.h>
#include <TGraph2D.h>
#include <fstream>

class StripEfficiency : public core::Analysis
{
public:
        StripEfficiency();
	virtual ~StripEfficiency();

	virtual void init(const po::variables_map& vm);
	virtual std::string getUsage(const std::string& argv0) const;
	virtual std::string getHelp(const std::string& argv0) const;

private:
	void alignInit();
        bool alignRun(const core::TrackStreamReader::event_t& track_event,
	           const core::BaseSensorStreamReader::event_t& mpa_event);
	void alignFinish();
	void analyzeRunInit();
        bool analyze(const core::TrackStreamReader::event_t& track_event,
	             const core::BaseSensorStreamReader::event_t& mpa_event);
	void analyzeFinish();

	std::map<int, core::Aligner> _aligner;
	TFile* _file;
	bool _forceAlignment;
	double _nSigmaCut;
};

#endif//STRIP_EFFICIENCY_H