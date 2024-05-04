/*
 *
 */

#ifndef UHBOOK_CXX
#define UHBOOK_CXX

#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <exception>
#include <limits>
#include <complex>

class UHnBook {
public:
	virtual ~UHnBook(){};
	void SetTitle(std::string &title) {m_title = title;};
	void SetTitle(const char *ctitle) {m_title = std::string(ctitle);};
	std::string& GetTitle() {return m_title;};

	void SetXLabel(std::string &label) {m_x_label = label;};
	void SetXLabel(const char *clabel) {m_x_label = std::string(clabel);};
	std::string& GetXLabel() {return m_x_label;};
	void SetYLabel(std::string &label) {m_y_label = label;};
	void SetYLabel(const char *clabel) {m_y_label = std::string(clabel);};
	std::string& GetYLabel() {return m_y_label;};

	int GetEntries() {return m_entry;};
protected:
	std::string m_title;
	std::string m_x_label;
	std::string m_y_label;
	int m_entry = 0;
private:
};

class UH1Book : public UHnBook {
public:
	UH1Book();	
	UH1Book(const char *, int, double, double);	
	UH1Book(std::string &, int, double, double);	
	virtual ~UH1Book();
	void Fill(double, double);
	void Fill(int val, double weight = 1.0) {
		Fill(static_cast<double>(val)
		+ (std::numeric_limits<double>::epsilon() * 128),
		weight);};
	void Reset();

#if 0
	void SetTitle(std::string &title) {m_title = title;};
	void SetTitle(char *ctitle) {m_title = std::string(ctitle);};
	std::string& GetTitle() {return m_title;};

	void SetXLabel(std::string &label) {m_x_label = label;};
	void SetXLabel(char *clabel) {m_x_label = std::string(clabel);};
	std::string& GetXLabel() {return m_x_label;};
	void SetYLabel(std::string &label) {m_y_label = label;};
	void SetYLabel(char *clabel) {m_y_label = std::string(clabel);};
	std::string& GetYLabel() {return m_y_label;};

	int GetEntries() {return m_entry;};
#endif

	void SetNBins(int bins) {if (m_entry == 0) {m_x_bins.resize(bins);}};
	int GetNBins() {return m_x_bins.size();};
	int GetBins() {return m_x_bins.size();};
	void SetMinimum(double min) {if (m_entry == 0) {m_x_min = min;}};
	double GetMinimum() {return m_x_min;};
	void SetMaximum(double max) {if (m_entry == 0) {m_x_max = max;}};
	double GetMaximum() {return m_x_max;};

	//void SetBinContent(int ibin, double val) {m_x_bins[ibin] = val;};
	//double GetBinContent(int ibin) {return m_x_bins[ibin];};
	void SetBinContent(int, double);
	double GetBinContent(int);
	std::vector<double>& GetBinContents() {return m_x_bins;};

	int GetOverflows() {return m_of;};
	int GetUnderflows() {return m_uf;};

	UH1Book& Add(UH1Book &);
	UH1Book operator +(UH1Book &);
	UH1Book& Subtract(UH1Book &);
	UH1Book operator -(UH1Book &);
	UH1Book& Multiply(UH1Book &);
	UH1Book operator *(UH1Book &);
	UH1Book& Divide(UH1Book &);
	UH1Book operator /(UH1Book &h);

	void Print();
	void Draw();
protected:
private:
	void Initialize(std::string &, int);
	void Initialize();

	std::vector<double> m_x_bins;
	double m_x_min;
	double m_x_max;

#if 0
	std::string m_title;
	std::string m_x_label;
	std::string m_y_label;

	int m_entry = 0;
#endif
	int m_uf = 0;
	int m_of = 0;
};


UH1Book::UH1Book()
{
	return;
}

UH1Book::UH1Book(const char *title, int bins, double min, double max)
	: m_x_min(min), m_x_max(max)
{
	std::string stitle(title);
	Initialize(stitle, bins);

	return;
}

UH1Book::UH1Book(std::string &title, int bins, double min, double max)
	: m_x_min(min), m_x_max(max)
{
	Initialize(title, bins);

	return;
}

void UH1Book::Initialize(std::string &title, int bins = 0)
{
	m_title = title;
	m_x_bins.clear();
	m_x_bins.resize(bins);
	m_entry = 0;
	m_uf = 0;
	m_of = 0;

	return;
}

void UH1Book::Initialize()
{
	m_x_bins.clear();
	m_x_bins.resize(0);
	m_entry = 0;
	m_uf = 0;
	m_of = 0;

	return;
}

UH1Book::~UH1Book()
{
	return;
}

void UH1Book::Reset()
{
	//for (auto &i : m_x_bins )  i = 0.0;
	std::fill(m_x_bins.begin(), m_x_bins.end(), 0.0);
	m_entry = 0;
	m_uf = 0;
	m_of = 0;

	return;
};

void UH1Book::Fill(double val, double weight = 1.0)
{
	if (val < m_x_min) m_uf++;
	if (val >= m_x_max) m_of++;
	if ((val >= m_x_min) && (val < m_x_max)) {
		int index = static_cast<int>(
			(val - m_x_min) / (m_x_max - m_x_min) * m_x_bins.size());
		m_x_bins[index] += weight;
		m_entry++;
	}

	return;
}

double UH1Book::GetBinContent(int ibin)
{
	int nbins_x = static_cast<int>(m_x_bins.size());
	if (ibin < 1)       return m_uf;
	if (ibin > nbins_x) return m_of;
	return m_x_bins[ibin - 1];
}

void UH1Book::SetBinContent(int ibin, double val)
{
	int nbins_x = static_cast<int>(m_x_bins.size());
	if (ibin < 1)       m_uf = val;
	if (ibin > nbins_x) m_of = val;
	if ((ibin >= 1) && (ibin <= nbins_x)) m_x_bins[ibin - 1] = val;
}

UH1Book& UH1Book::Add(UH1Book &h)
{
	constexpr double EPS = std::numeric_limits<double>::epsilon();
	if ((static_cast<size_t>(h.GetNBins()) == m_x_bins.size())
		&& (std::abs(h.GetMinimum() - m_x_min) < EPS)
		&& (std::abs(h.GetMaximum() - m_x_max) < EPS)) {
		for (size_t i = 0 ; i < m_x_bins.size() ; i++) {
			m_x_bins[i] += h.m_x_bins[i];
		}

		#if 0
		m_entry += h.GetEntries();
		m_uf += h.GetOverflows();
		m_of += h.GetUnderflows();
		#else
		m_entry += h.m_entry;
		m_uf += h.m_uf;
		m_of += h.m_of;
		#endif

	} else {
		//throw std::runtime_error("Diffrent size histograms");
	}

	return *this;
}

UH1Book UH1Book::operator +(UH1Book &h)
{
	UH1Book hh(*this);

	#if 0
	constexpr double EPS = std::numeric_limits<double>::epsilon();
	if ((static_cast<size_t>(h.GetNBins()) == m_x_bins.size())
		&& (std::abs(h.GetMinimum() - m_x_min) < EPS)
		&& (std::abs(h.GetMaximum() - m_x_max) < EPS)) {
		for (size_t i = 0 ; i < m_x_bins.size() ; i++) {
			hh.m_x_bins[i] += h.m_x_bins[i];
		}
		hh.m_entry += h.m_entry;
		hh.m_uf += h.m_uf;
		hh.m_of += h.m_of;
	} else {
		//throw std::runtime_error("Diffrent size histograms");
	}
	#else
	hh.Add(h);
	#endif

	return hh;
}


UH1Book& UH1Book::Subtract(UH1Book &h)
{
	constexpr double EPS = std::numeric_limits<double>::epsilon();
	if ((static_cast<size_t>(h.GetNBins()) == m_x_bins.size())
		&& (std::abs(h.GetMinimum() - m_x_min) < EPS)
		&& (std::abs(h.GetMaximum() - m_x_max) < EPS)) {
		for (size_t i = 0 ; i < m_x_bins.size() ; i++) {
			m_x_bins[i] -= h.m_x_bins[i];
		}
		m_entry -= h.m_entry;
		m_uf -= h.m_uf;
		m_of -= h.m_of;
	} else {
		//throw std::runtime_error("Diffrent size histograms");
	}

	return *this;
}

UH1Book UH1Book::operator -(UH1Book &h)
{
	UH1Book hh(*this);

	#if 0
	constexpr double EPS = std::numeric_limits<double>::epsilon();
	if ((static_cast<size_t>(h.GetNBins()) == m_x_bins.size())
		&& (std::abs(h.GetMinimum() - m_x_min) < EPS)
		&& (std::abs(h.GetMaximum() - m_x_max) < EPS)) {
		for (size_t i = 0 ; i < m_x_bins.size() ; i++) {
			hh.m_x_bins[i] -= h.m_x_bins[i];
		}
		hh.m_entry -= h.m_entry;
		hh.m_uf -= h.m_uf;
		hh.m_of -= h.m_of;
	} else {
		//throw std::runtime_error("Diffrent size histograms");
	}
	#else
	hh.Subtract(h);
	#endif

	return hh;
}


UH1Book& UH1Book::Multiply(UH1Book &h)
{
	constexpr double EPS = std::numeric_limits<double>::epsilon();
	if ((static_cast<size_t>(h.GetNBins()) == m_x_bins.size())
		&& (std::abs(h.GetMinimum() - m_x_min) < EPS)
		&& (std::abs(h.GetMaximum() - m_x_max) < EPS)) {
		for (size_t i = 0 ; i < m_x_bins.size() ; i++) {
			m_x_bins[i] = m_x_bins[i] * h.m_x_bins[i];
		}
	}

	return *this;
}

UH1Book UH1Book::operator *(UH1Book &h)
{
	UH1Book hh(*this);

	#if 0
	constexpr double EPS = std::numeric_limits<double>::epsilon();
	if ((static_cast<size_t>(h.GetNBins()) == m_x_bins.size())
		&& (std::abs(h.GetMinimum() - m_x_min) < EPS)
		&& (std::abs(h.GetMaximum() - m_x_max) < EPS)) {
		for (size_t i = 0 ; i < m_x_bins.size() ; i++) {
			hh.m_x_bins[i] = m_x_bins[i] * h.m_x_bins[i];
		}
	}
	#else
	hh.Multiply(h);
	#endif

	return hh;
}

UH1Book& UH1Book::Divide(UH1Book &h)
{
	constexpr double EPS = std::numeric_limits<double>::epsilon();
	if ((static_cast<size_t>(h.GetNBins()) == m_x_bins.size())
		&& (std::abs(h.GetMinimum() - m_x_min) < EPS)
		&& (std::abs(h.GetMaximum() - m_x_max) < EPS)) {
		for (size_t i = 0 ; i < m_x_bins.size() ; i++) {
			if (std::abs(h.m_x_bins[i]) > EPS) {
				m_x_bins[i] = m_x_bins[i] / h.m_x_bins[i];
			} else {
				m_x_bins[i] = 0.0;
			}
		}
	}

	return *this;
}

UH1Book UH1Book::operator /(UH1Book &h)
{
	UH1Book hh(*this);

	#if 0
	constexpr double EPS = std::numeric_limits<double>::epsilon();
	if ((static_cast<size_t>(h.GetNBins()) == m_x_bins.size())
		&& (std::abs(h.GetMinimum() - m_x_min) < EPS)
		&& (std::abs(h.GetMaximum() - m_x_max) < EPS)) {
		for (size_t i = 0 ; i < m_x_bins.size() ; i++) {
			if (std::abs(h.GetBinContent(i)) > EPS) {
				hh.m_x_bins[i] = m_x_bins[i] / h.m_x_bins[i];
			} else {
				hh.m_x_bins[i] = 0.0;
			}
		}
	}
	#else
	hh.Divide(h);
	#endif

	return hh;
}

void UH1Book::Print()
{
	std::cout << "Title: " << m_title << std::endl;
	std::cout << "Entry:	  " << m_entry << std::endl;
	std::cout << "Over flow:  " << m_of << std::endl;
	std::cout << "Under flow: " << m_uf << std::endl;

	return;
}


#include <sys/ioctl.h>
#include <unistd.h>

size_t get_terminal_width()
{
	size_t line_length = 0;
	struct winsize ws;
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) != -1 ) {
		printf("terminal_width  =%d\n", ws.ws_col);
		printf("terminal_height =%d\n", ws.ws_row);
		if ((ws.ws_col > 0) && (ws.ws_col == (size_t)ws.ws_col)) {
			line_length = ws.ws_col;
		}
	}
	return line_length;
}

void UH1Book::Draw()
{
	int tlen = 100;
	int hlen = tlen - 8 - 8;

	double vmax = 0;
	for (size_t i = 0 ; i < m_x_bins.size() ; i++) {
		if (vmax < m_x_bins[i]) vmax = m_x_bins[i];
	}

	for (size_t i = 0 ; i < m_x_bins.size() ; i++) {
		int dnum;
		if (vmax < hlen) {
			dnum = static_cast<int>(m_x_bins[i]);
		} else {
			dnum = m_x_bins[i] / vmax * hlen;
		}
		double xindex = ((m_x_max - m_x_min) / m_x_bins.size() * i) + m_x_min;
		std::cout
			<< std::scientific << std::setprecision(1)
			<< xindex << ":"
			<< m_x_bins[i] << "|";
		for (int j = 0 ; j < dnum ; j++) std::cout << "#";
		std::cout << std::endl;
	}

	std::cout << std::defaultfloat;

	return;
}


class UH2Book : public UHnBook {
public:
	UH2Book();
	UH2Book(const char *, int, double, double, int, double, double);	
	UH2Book(std::string &, int, double, double, int, double, double);	
	virtual ~UH2Book();
	void Fill(double, double, double);
	void Fill(int xval, int yval, double weight = 1.0) {
		Fill(static_cast<double>(xval)
			+ (std::numeric_limits<double>::epsilon() * 128),
			static_cast<double>(yval)
			+ (std::numeric_limits<double>::epsilon() * 128),
			weight);};
	void Reset();

	void SetNbins(int xbins, int ybins) {
		if (m_entry == 0) {
			m_bins.resize(xbins);
			for (auto &i : m_bins) i.resize(ybins);
		}
	};
	int GetNBinsX() {return m_bins.size();};
	int GetNBinsY() {return m_bins[0].size();};

	void SetMinimumX(double min) {if (m_entry == 0) {m_x_min = min;}};
	double GetMinimumX() {return m_x_min;};
	void SetMinimumY(double min) {if (m_entry == 0) {m_y_min = min;}};
	double GetMinimumY() {return m_y_min;};
	void SetMaximumX(double max) {if (m_entry == 0) {m_x_max = max;}};
	double GetMaximumX() {return m_x_max;};
	void SetMaximumY(double max) {if (m_entry == 0) {m_y_max = max;}};
	double GetMaximumY() {return m_y_max;};

	//void SetBinContent(int xbin, int ybin, double val) {m_bins[xbin][ybin] = val;};
	//double GetBinContent(int xbin, int ybin) {return m_bins[xbin][ybin];};
	void SetBinContent(int, int, double);
	double GetBinContent(int, int);
	std::vector< std::vector<double> >& GetBinContents() {return m_bins;};

	int GetOverUnderflows(int x, int y) {
		if ((x >= 0) && (x < 3) && (y >= 0) && (y < 3)) {
			return m_ouflows[x][y];
		} else {
			return 0;
		}
	};

	UH2Book& Add(UH2Book &);
	UH2Book operator +(UH2Book &);
	UH2Book& Subtract(UH2Book &);
	UH2Book operator -(UH2Book &);
	UH2Book& Multiply(UH2Book &);
	UH2Book operator *(UH2Book &);
	UH2Book& Divide(UH2Book &);
	UH2Book operator /(UH2Book &);

	void Print();
	void Draw();
protected:
private:
	void Initialize(std::string &, int, int);
	void Initialize();

	std::vector< std::vector<double> > m_bins;
	double m_x_min;
	double m_x_max;
	double m_y_min;
	double m_y_max;

	//std::array<std::array<int, 3>, 3> m_ouflows = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}};
	std::array<std::array<int, 3>, 3> m_ouflows;

};

UH2Book::UH2Book()
{
	return;
}

UH2Book::UH2Book(const char *title,
	int xbins, double xmin, double xmax,
	int ybins, double ymin, double ymax)
	: m_x_min(xmin), m_x_max(xmax), m_y_min(ymin), m_y_max(ymax)
{
	std::string stitle(title);
	Initialize(stitle, xbins, ybins);

	return;
}

UH2Book::UH2Book(std::string &title,
	int xbins, double xmin, double xmax,
	int ybins, double ymin, double ymax)
	: m_x_min(xmin), m_x_max(xmax), m_y_min(ymin), m_y_max(ymax)
{
	Initialize(title, xbins, ybins);

	return;
}

void UH2Book::Initialize(std::string &title, int xbins = 0, int ybins = 0)
{
	m_title = title;
	m_bins.clear();
	m_bins.resize(xbins);
	for (auto &i : m_bins) i.resize(ybins);
	m_entry = 0;
	for (auto &i : m_ouflows) std::fill(i.begin(), i.end(), 0);

	return;
}

void UH2Book::Initialize()
{
	m_bins.clear();
	m_bins.resize(0);
	m_entry = 0;
	for (auto &i : m_ouflows) std::fill(i.begin(), i.end(), 0);

	return;
}

UH2Book::~UH2Book()
{
	return;
}

void UH2Book::Reset()
{
	for (auto &i : m_bins) std::fill(i.begin(), i.end(), 0.0);
	m_entry = 0;
	for (auto &i : m_ouflows) std::fill(i.begin(), i.end(), 0);
	
	return;
};

void UH2Book::Fill(double xval, double yval, double weight = 1.0)
{
	if ((xval <  m_x_min) && (yval <  m_y_min)) m_ouflows[0][0]++;
	if ((xval >= m_x_min) && (xval <  m_x_max) && (yval <  m_y_min)) m_ouflows[1][0]++;
	if ((xval >= m_x_max) && (yval <  m_y_min)) m_ouflows[2][0]++;

	if ((xval <  m_x_min) && (yval >= m_y_min) && (yval <  m_y_max)) m_ouflows[0][1]++;
	if ((xval >= m_x_max) && (yval >= m_y_min) && (yval <  m_y_max)) m_ouflows[2][1]++;

	if ((xval <  m_x_min) && (yval >= m_y_max)) m_ouflows[0][2]++;
	if ((xval >= m_x_max) && (xval <  m_x_max) && (yval >= m_y_max)) m_ouflows[1][2]++;
	if ((xval >= m_x_max) && (yval >= m_y_max)) m_ouflows[2][2]++;

	if (	   (xval >= m_x_min) && (xval < m_x_max)
		&& (yval >= m_y_min) && (yval < m_x_max)) {
		int ix = static_cast<int>(
			(xval - m_x_min) / (m_x_max - m_x_min) * m_bins.size());
		int iy = static_cast<int>(
			(yval - m_y_min) / (m_y_max - m_y_min) * m_bins[0].size());
		m_bins[ix][iy] += weight;
		m_entry++;
	} else {
	}

	return;
}

double UH2Book::GetBinContent(int xbin, int ybin)
{
	int nbins_x = static_cast<int>(m_bins.size());
	int nbins_y = static_cast<int>(m_bins[0].size());
	if ((xbin < 1)                && (ybin < 1)       ) return m_ouflows[0][0];
	if ((xbin < 1) && (ybin >= 1) && (ybin <= nbins_y)) return m_ouflows[0][1];
	if ((xbin < 1)                && (ybin >  nbins_y)) return m_ouflows[0][2];

	if ((xbin >= 1) && (xbin <= nbins_x) && (ybin < 1)) return m_ouflows[1][0];
	if ((xbin >= 1) && (xbin <= nbins_x) && (ybin > nbins_y)) return m_ouflows[1][2];

	if ((xbin > nbins_x) && (ybin < 1)                ) return m_ouflows[2][0];
	if ((xbin > nbins_x) && (ybin >= 1) && (ybin <= nbins_y)) return m_ouflows[2][1];
	if ((xbin > nbins_x) && (ybin > nbins_y)          ) return m_ouflows[2][2];

	return m_bins[xbin - 1][ybin - 1];
}

void UH2Book::SetBinContent(int xbin, int ybin, double val)
{
	int nbins_x = static_cast<int>(m_bins.size());
	int nbins_y = static_cast<int>(m_bins[0].size());

	if ((xbin < 1)                && (ybin < 1)       ) m_ouflows[0][0] = val;
	if ((xbin < 1) && (ybin >= 1) && (ybin <= nbins_y)) m_ouflows[0][1] = val;
	if ((xbin < 1)                && (ybin >  nbins_y)) m_ouflows[0][2] = val;

	if ((xbin >= 1) && (xbin <= nbins_x) && (ybin < 1)) m_ouflows[1][0] = val;
	if ((xbin >= 1) && (xbin <= nbins_x) && (ybin > nbins_y)) m_ouflows[1][2] = val;

	if ((xbin > nbins_x) && (ybin < 1)                ) m_ouflows[2][0] = val;
	if ((xbin > nbins_x) && (ybin >= 1) && (ybin <= nbins_y)) m_ouflows[2][1] = val;
	if ((xbin > nbins_x) && (ybin > nbins_y)          ) m_ouflows[2][2] = val;

	if ((xbin >= 1) && (xbin <= nbins_x) && (ybin >= 1) && (ybin <= nbins_y)) {
		m_bins[xbin - 1][ybin - 1] = val;
	}
}

UH2Book& UH2Book::Add(UH2Book &h)
{
	constexpr double EPS = std::numeric_limits<double>::epsilon();
	if (	   (static_cast<size_t>(h.GetNBinsX()) == m_bins.size())
		&& (std::abs(h.GetMinimumX() - m_x_min) < EPS)
		&& (std::abs(h.GetMaximumX() - m_x_max) < EPS)
		&& (static_cast<size_t>(h.GetNBinsY()) == m_bins[0].size())
		&& (std::abs(h.GetMinimumY() - m_y_min) < EPS)
		&& (std::abs(h.GetMaximumY() - m_y_max) < EPS)
		) {

		for (size_t i = 0 ; i < m_bins.size() ; i++) {
			for (size_t j = 0 ; j < m_bins[i].size() ; j++) {
				m_bins[i][j] += h.m_bins[i][j];
			}
		}

		m_entry += h.GetEntries();
		for (int i = 0 ; i < 3 ; i++) {
			for (int j = 0 ; j < 3 ; j++) {
				m_ouflows[i][j] += h.m_ouflows[i][j];
			}
		}

	} else {
		//throw std::runtime_error("Diffrent size histograms");
	}

	return *this;
}

UH2Book UH2Book::operator +(UH2Book &h)
{
	UH2Book hh(*this);

	#if 0
	constexpr double EPS = std::numeric_limits<double>::epsilon();
	if (	   (static_cast<size_t>(h.GetNBinsX()) == m_bins.size())
		&& (std::abs(h.GetMinimumX() - m_x_min) < EPS)
		&& (std::abs(h.GetMaximumX() - m_x_max) < EPS)
		&& (static_cast<size_t>(h.GetNBinsY()) == m_bins[0].size())
		&& (std::abs(h.GetMinimumY() - m_y_min) < EPS)
		&& (std::abs(h.GetMaximumY() - m_y_max) < EPS)
		) {

		for (size_t i = 0 ; i < m_bins.size() ; i++) {
			for (size_t j = 0 ; j < m_bins[i].size() ; j++) {
				hh.m_bins[i][j] += h.m_bins[i][j];
			}
		}

		hh.m_entry += h.GetEntries();
		for (int i = 0 ; i < 3 ; i++) {
			for (int j = 0 ; j < 3 ; j++) {
				hh.m_ouflows[i][j] += h.m_ouflows[i][j];
			}
		}

	} else {
		//throw std::runtime_error("Diffrent size histograms");
	}
	#else
	hh.Add(h);
	#endif

	return hh;
}

UH2Book& UH2Book::Subtract(UH2Book &h)
{
	constexpr double EPS = std::numeric_limits<double>::epsilon();
	if (	   (static_cast<size_t>(h.GetNBinsX()) == m_bins.size())
		&& (std::abs(h.GetMinimumX() - m_x_min) < EPS)
		&& (std::abs(h.GetMaximumX() - m_x_max) < EPS)
		&& (static_cast<size_t>(h.GetNBinsY()) == m_bins[0].size())
		&& (std::abs(h.GetMinimumY() - m_y_min) < EPS)
		&& (std::abs(h.GetMaximumY() - m_y_max) < EPS)
		) {

		for (size_t i = 0 ; i < m_bins.size() ; i++) {
			for (size_t j = 0 ; j < m_bins[i].size() ; j++) {
				m_bins[i][j] -= h.m_bins[i][j];
			}
		}

		m_entry += h.GetEntries();
		for (int i = 0 ; i < 3 ; i++) {
			for (int j = 0 ; j < 3 ; j++) {
				m_ouflows[i][j] -= h.m_ouflows[i][j];
			}
		}

	} else {
		//throw std::runtime_error("Diffrent size histograms");
	}

	return *this;
}

UH2Book UH2Book::operator -(UH2Book &h)
{
	UH2Book hh(*this);

	#if 0
	constexpr double EPS = std::numeric_limits<double>::epsilon();
	if (	   (static_cast<size_t>(h.GetNBinsX()) == m_bins.size())
		&& (std::abs(h.GetMinimumX() - m_x_min) < EPS)
		&& (std::abs(h.GetMaximumX() - m_x_max) < EPS)
		&& (static_cast<size_t>(h.GetNBinsY()) == m_bins[0].size())
		&& (std::abs(h.GetMinimumY() - m_y_min) < EPS)
		&& (std::abs(h.GetMaximumY() - m_y_max) < EPS)
		) {

		for (size_t i = 0 ; i < m_bins.size() ; i++) {
			for (size_t j = 0 ; j < m_bins[i].size() ; j++) {
				hh.m_bins[i][j] -= h.m_bins[i][j];
			}
		}

		hh.m_entry -= h.GetEntries();
		for (int i = 0 ; i < 3 ; i++) {
			for (int j = 0 ; j < 3 ; j++) {
				hh.m_ouflows[i][j] -= h.m_ouflows[i][j];
			}
		}

	} else {
		//throw std::runtime_error("Diffrent size histograms");
	}
	#else
	hh.Subtract(h);
	#endif

	return hh;
}

UH2Book& UH2Book::Multiply(UH2Book &h)
{
	constexpr double EPS = std::numeric_limits<double>::epsilon();
	if (	   (static_cast<size_t>(h.GetNBinsX()) == m_bins.size())
		&& (std::abs(h.GetMinimumX() - m_x_min) < EPS)
		&& (std::abs(h.GetMaximumX() - m_x_max) < EPS)
		&& (static_cast<size_t>(h.GetNBinsY()) == m_bins[0].size())
		&& (std::abs(h.GetMinimumY() - m_y_min) < EPS)
		&& (std::abs(h.GetMaximumY() - m_y_max) < EPS)
		) {

		for (size_t i = 0 ; i < m_bins.size() ; i++) {
			for (size_t j = 0 ; j < m_bins[i].size() ; j++) {
				m_bins[i][j] = m_bins[i][j] * h.m_bins[i][j];
			}
		}
	}

	return *this;
}

UH2Book UH2Book::operator *(UH2Book &h)
{
	UH2Book hh(*this);

	#if 0
	constexpr double EPS = std::numeric_limits<double>::epsilon();
	if (	   (static_cast<size_t>(h.GetNBinsX()) == m_bins.size())
		&& (std::abs(h.GetMinimumX() - m_x_min) < EPS)
		&& (std::abs(h.GetMaximumX() - m_x_max) < EPS)
		&& (static_cast<size_t>(h.GetNBinsY()) == m_bins[0].size())
		&& (std::abs(h.GetMinimumY() - m_y_min) < EPS)
		&& (std::abs(h.GetMaximumY() - m_y_max) < EPS)
		) {

		for (size_t i = 0 ; i < m_bins.size() ; i++) {
			for (size_t j = 0 ; j < m_bins[i].size() ; j++) {
				hh.m_bins[i][j] = m_bins[i][j] * h.m_bins[i][j];
			}
		}
	}
	#else
	hh.Multiply(h);
	#endif

	return hh;
}

UH2Book& UH2Book::Divide(UH2Book &h)
{
	constexpr double EPS = std::numeric_limits<double>::epsilon();
	if (	   (static_cast<size_t>(h.GetNBinsX()) == m_bins.size())
		&& (std::abs(h.GetMinimumX() - m_x_min) < EPS)
		&& (std::abs(h.GetMaximumX() - m_x_max) < EPS)
		&& (static_cast<size_t>(h.GetNBinsY()) == m_bins[0].size())
		&& (std::abs(h.GetMinimumY() - m_y_min) < EPS)
		&& (std::abs(h.GetMaximumY() - m_y_max) < EPS)
		) {

		for (size_t i = 0 ; i < m_bins.size() ; i++) {
			for (size_t j = 0 ; j < m_bins[i].size() ; j++) {
				if (std::abs(h.m_bins[i][j]) > EPS) {
					m_bins[i][j] = m_bins[i][j] / h.m_bins[i][j];
				} else {
					m_bins[i][j] = 0;
				}
			}
		}
	}

	return *this;
}

UH2Book UH2Book::operator /(UH2Book &h)
{
	UH2Book hh(*this);

	#if 0
	constexpr double EPS = std::numeric_limits<double>::epsilon();
	if (	   (static_cast<size_t>(h.GetNBinsX()) == m_bins.size())
		&& (std::abs(h.GetMinimumX() - m_x_min) < EPS)
		&& (std::abs(h.GetMaximumX() - m_x_max) < EPS)
		&& (static_cast<size_t>(h.GetNBinsY()) == m_bins[0].size())
		&& (std::abs(h.GetMinimumY() - m_y_min) < EPS)
		&& (std::abs(h.GetMaximumY() - m_y_max) < EPS)
		) {

		for (size_t i = 0 ; i < m_bins.size() ; i++) {
			for (size_t j = 0 ; j < m_bins[i].size() ; j++) {
				if (std::abs(h.GetBinContent(i, j)) > EPS) {
					hh.m_bins[i][j] = m_bins[i][j] / h.m_bins[i][j];
				} else {
					hh.m_bins[i][j] = 0;
				}
			}
		}
	}
	#else
	hh.Divide(h);
	#endif

	return hh;
}


void UH2Book::Print()
{
	std::cout << "Title: " << m_title << std::endl;
	std::cout << "Entry: " << m_entry << std::endl;
	std::cout << "Over/Under flow: "
		<< std::setw(6) << m_ouflows[0][2] << " "
		<< std::setw(6) << m_ouflows[1][2] << " "
		<< std::setw(6) << m_ouflows[2][2] << std::endl;
	std::cout << "               : "
		<< std::setw(6) << m_ouflows[0][1] << " "
		<< std::setw(6) << m_entry << " "
		<< std::setw(6) << m_ouflows[2][1] << std::endl;
	std::cout << "               : "
		<< std::setw(6) << m_ouflows[0][0] << " "
		<< std::setw(6) << m_ouflows[1][0] << " "
		<< std::setw(6) << m_ouflows[2][0] << std::endl;

	return;
}

void UH2Book::Draw()
{
	const int ngrade = 8;
	std::array<char, ngrade + 1> dispchar = {
		' ', '.', '-', '+',
		'x', '*', '@', '#', '#'};

	double vmax = m_bins[0][0];
	double vmin = m_bins[0][0];
	for (auto &i : m_bins) {
		for (auto &j : i) {
			if (vmax < j) vmax = j;
			if (vmin > j) vmin = j;
		}
	}
	
	constexpr double EPS = std::numeric_limits<double>::epsilon();
	if (std::abs(vmax) < EPS) vmax = 1.0;

	for (size_t i = 0 ; i < m_bins.size() ; i++) {
		double xindex = ((m_x_max - m_x_min) / m_bins.size() * i) + m_x_min;
		std::cout << std::scientific << std::setprecision(1)
			<< xindex << "|";
		for (size_t j = 0 ; j < m_bins[i].size() ; j++) {
			char v[2] = {0, 0};
			int dnum = static_cast<int>(((m_bins[i][j] - vmin) / (vmax - vmin)) * ngrade);
			v[0] = dispchar[dnum];
			std::cout << v;
		}
		std::cout << std::endl;
	}

	std::cout << std::fixed << std::defaultfloat;

	return;
}

#endif // UHBOOK_CXX


#ifdef TEST_MAIN
#include <random>
#include <sstream>


std::string Slowdashify(UH1Book& hist) {
	std::ostringstream os;

	os << "{" << std::endl;
	os << "	\"bins\": { \"min\": " << hist.GetMinimum()
		<< ", \"max\": " << hist.GetMaximum()
		<< " }," << std::endl;
	os << "	\"counts\": [ ";
	for (int i = 0; i < hist.GetNBins(); i++) {
		os << (i==0 ? "" : ", ") << hist.GetBinContent(i);
	}
	os << " ]" << std::endl;
	os << "}" << std::endl;

	return os.str();
}

int main(int argc, char* argv[])
{
	int nentry = 100;
	for (int i = 1 ; i < argc ; i++) {
		std::istringstream iss(argv[i]);
		iss >> nentry;
	}


	UH1Book h1("Hello", 30, 0.0, 200.0);

	//std::string title("Hello");
	//UH1Book h1(title, 100, 2.0, 500.0);
	//std::cout << "#D " << h1.getMin() << " " << h1.getMax()
	//	<< " " << h1.getBins() << std::endl;

	std::random_device rnd;
	std::mt19937 engine(rnd());
	std::normal_distribution<> dist(100.0, 20.0);
	for (int i = 0 ; i < nentry ; i++) {
		double val = dist(engine);
		h1.Fill(val);
	}

	h1.Print();
	h1.Draw();

	h1.Reset();
	for (int i = 0 ; i < nentry ; i++) {
		double val = dist(engine);
		h1.Fill(val);
	}
	h1.Print();
	h1.Draw();

	std::vector<double>& hcont = h1.GetBinContents();

	std::cout << "Nbins: " << h1.GetNBins() << std::endl;
	std::cout << "Contents:";
	for (auto &i : hcont) std::cout << " " << i;
	std::cout << std::endl;
	std::cout << "Contents:";
	for (int i = 0 ; i < h1.GetNBins() + 2 ; i++) std::cout << " " << i << ":" << h1.GetBinContent(i);
	std::cout << std::endl;


	hcont[5] = 20.0;
	h1.Print();
	h1.Draw();

	UH1Book h1a("Hello 2", 30, 0.0, 200.0);
	for (int i = 0 ; i < nentry ; i++) {
		double val = dist(engine);
		h1a.Fill(val);
	}

	int val = 101;
	h1a.Fill(val);
	
	UH1Book h1b = h1a + h1 + h1a;
	h1b.SetTitle("Add");
	h1b.Print();
	h1b.Draw();

	h1.Print();
	h1.Draw();

	UH1Book h1xx("Bining", 50, 0.0, 50.0);
	for (int i = 0 ; i < 1000 ; i++) {
		h1xx.Fill(i % 50);
	}
	h1xx.Print();
	h1xx.Draw();

	UH1Book h1c = h1b - h1a;
	h1c.SetTitle("Subtract");
	h1c.Print();
	h1c.Draw();

	UH1Book h1d = h1b * h1a;
	h1d.SetTitle("Multiply");
	h1d.Print();
	h1d.Draw();

	UH1Book h1e = h1b / h1a;
	h1e.SetTitle("Divice");
	h1e.Print();
	h1e.Draw();


	UH2Book h2("Hello 2D", 40, 0.0, 200.0, 40, 0.0, 200.0);
	for (int i = 0 ; i < nentry * nentry ; i++) {
		double xval = dist(engine);
		double yval = dist(engine);
		h2.Fill(xval, yval);
	}
	h2.Print();
	h2.Draw();

	UH2Book h2a("Hello 2D", 40, 0.0, 200.0, 40, 0.0, 200.0);
	for (int i = 0 ; i < nentry * nentry ; i++) {
		double xval = dist(engine);
		double yval = dist(engine);
		h2a.Fill(xval, yval);
	}

	UH2Book h2b = h2 + h2a;
	h2b.SetTitle("2D + 2D");
	h2b.Print();
	h2b.Draw();

	UH2Book h2c = h2b - h2a;
	h2c.SetTitle("2D - 2D");
	h2c.Print();
	h2c.Draw();

	UH2Book h2d = h2b * h2a;
	h2d.SetTitle("2D * 2D");
	h2d.Print();
	h2d.Draw();

	UH2Book h2e = h2b / h2a;
	h2e.SetTitle("2D / 2D");
	h2e.Print();
	h2e.Draw();

	std::cout << Slowdashify(h1) << std::endl;

	return 0;
}
#endif
