
#include "aligner.h"
#include "functions.h"

#include <cassert>
#include <fstream>
#include <cmath>
#include <TH1D.h>
#include <TCanvas.h>
#include <TImage.h>
#include <TF1.h>
#include <TFitResult.h>

using namespace core;


Aligner::Aligner() :
 _nsigma(1.0), _alignX(nullptr), _alignY(nullptr),
 _calculated(false), _offset(0, 0, 0), _cuts(0, 0)
{
}

TH1D* Aligner::getHistX() const
{
	return _alignX;
}
TH1D* Aligner::getHistY() const
{
	return _alignY;
}

void Aligner::initHistograms(const std::string& xname, const std::string& yname)
{
	assert(_alignX == nullptr);
	assert(_alignY == nullptr);
	_alignX = new TH1D(xname.c_str(), "Alignment Correlation on X axis", 1000, -5, 5);
	_alignY = new TH1D(yname.c_str(), "Alignment Correlation on Y axis", 250, -5, 5);
}

void Aligner::writeHistograms()
{
	if(_alignX && _alignY) {
		_alignX->Write();
		_alignY->Write();
	}
}

void Aligner::writeHistogramImage(const std::string& filename)
{
	if(!_alignX || !_alignY)
		return;
	std::ostringstream info;
	auto canvas = new TCanvas("alignmentCanvas", "", 400, 600);
	canvas->Divide(1, 2);
	canvas->cd(1);
	_alignX->Draw();

	canvas->cd(2);
	_alignY->Draw();
	auto img = TImage::Create();
	img->FromPad(canvas);
	img->WriteImage(filename.c_str());
	delete img;
}

void Aligner::calculateAlignment()
{
	if(_calculated)
		return;
	assert(_alignX);
	assert(_alignY);
	_calculated = true;
	auto xalign = alignGaussian(_alignX, 0.5, 0.1);
	auto yalign = alignPlateau(_alignY, 1, 0.05);	
	_offset = { xalign(0), (yalign(1) + yalign(0))/2, 0.0 };
	_cuts = { xalign(1), (yalign(1) - yalign(0))/2 };
}

void Aligner::Fill(const double& xdiff, const double& ydiff)
{
	assert(_alignX);
	assert(_alignY);
	_alignX->Fill(xdiff);
	_alignY->Fill(ydiff);
}

Eigen::Vector3d Aligner::getOffset() const
{
	assert(_calculated);
	return _offset;
}

Eigen::Vector2d Aligner::getCuts() const
{
	assert(_calculated);
	return _cuts;
}

bool Aligner::pointsCorrelated(const Eigen::Vector2d& a, const Eigen::Vector2d& b) const
{
	assert(_calculated);
	Eigen::Array2d diff{(a - b).array().abs()};
	return diff(0) < _cuts(0)*_nsigma &&
	       diff(1) < _cuts(1);
}

bool Aligner::pointsCorrelatedX(const double& a, const double& b) const
{
	assert(_calculated);
	return std::abs(a-b) < _cuts(0)*_nsigma;
}

bool Aligner::pointsCorrelatedY(const double& a, const double& b) const
{
	assert(_calculated);
	return std::abs(a-b) < _cuts(1);
}

void Aligner::saveAlignmentData(const std::string& filename) const
{
	std::ofstream of(filename);
	of << _offset(0) << " "
	   << _offset(1) << " "
	   << _offset(2) << " "
	   << _cuts(0) << " "
	   << _cuts(1) << "\n";
	of.close();
}

bool Aligner::loadAlignmentData(const std::string& filename)
{
	std::cout << "Alignment data filename: " << filename << std::endl;
	std::ifstream fin(filename);
	if(fin.fail()) {
		return false;
	}
	double x, y, z, sigma, dist;
	fin >> x >> y >> z >> sigma >> dist;
	_offset = { x, y, z };
	_cuts = { sigma, dist };
	_calculated = true;
	return true;
}

bool Aligner::rebinIfNeccessary(TH1D* cor, const double& nrms, const double& binratio)
{
	auto maxBin = cor->GetMaximumBin();
	auto mean = cor->GetBinLowEdge(maxBin);
	auto rms = cor->GetRMS();
	if(cor->GetEntries() * binratio * 2 < cor->GetNbinsX()) {
		/*std::cout << "REBIN!\n"
		          << "Num entries: " << cor->GetEntries() << "\n"
		          << "Entry threshold: " << cor->GetEntries() * binratio * 2 << "\n"
			  << "Num X bins: " << cor->GetNbinsX() << "\n";*/
		cor->Rebin(cor->GetNbinsX() / (cor->GetEntries() * binratio));
		// std::cout << "New reduced bin number: " << cor->GetNbinsX() << std::endl;

//		mean += cor->GetMean();
//		mean /= 2;
		return true;
	}
	return false;
}

Eigen::Vector2d Aligner::alignPlateau(TH1D* cor, const double& nrms, const double& binratio)
{
	auto maxBin = cor->GetMaximumBin();
	auto mean = cor->GetBinLowEdge(maxBin);
	auto rms = cor->GetRMS();
	auto rebinned = rebinIfNeccessary(cor, nrms, binratio);
	auto piecewise = new TF1("piecewise", symmetric_plateau_function, mean-rms*nrms, mean+rms*nrms, 5);
	piecewise->SetParameter(0, mean-rms);
	piecewise->SetParameter(1, mean+rms);
	piecewise->SetParameter(2, cor->GetMaximum());
	piecewise->SetParameter(3, 0.1);
	piecewise->SetParameter(4, cor->GetMaximum());
	auto result = cor->Fit(piecewise, "RMS+", "", mean-rms*nrms, mean+rms*nrms);
	return {
		result->Parameter(0) - result->Parameter(3),
		result->Parameter(1) + result->Parameter(3)
	};
}

Eigen::Vector2d Aligner::alignGaussian(TH1D* cor, const double& nrms, const double& binratio)
{
	auto maxBin = cor->GetMaximumBin();
	auto mean = cor->GetBinLowEdge(maxBin);
	auto rms = cor->GetRMS();
	auto rebinned = rebinIfNeccessary(cor, nrms, binratio);
	auto result = cor->Fit("gaus", "RMS+", "", mean-rms*nrms, mean+rms*nrms);
	return {
		result->Parameter(1),
		result->Parameter(2),
	};
}

