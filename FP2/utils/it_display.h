/**
 * @file it_display.h
 * @brief Iteration Display Class
 *
 * @author Domenico Salvagnin dominiqs@gmail.com
 * Copyright 2009-2012 Domenico Salvagnin
 */

#ifndef IT_DISPLAY_H
#define IT_DISPLAY_H

#include <iostream>
#include <iomanip>
#include <string>
#include <map>
#include <boost/shared_ptr.hpp>

namespace dominiqs {

/**
 * Class for printing table-like iteration logs to the terminal
 */

class IterationDisplay
{
public:
	class Fmt
	{
	public:
	    virtual ~Fmt() {}
		virtual std::ostream& print(std::ostream& out, int width) { return out; }
	};

	typedef boost::shared_ptr<Fmt> FmtPtr;

	template<typename T> class SimpleFmt : public Fmt
	{
	public:
		SimpleFmt(const T& v) : value(v) {}
		SimpleFmt* clone() const { return new SimpleFmt(*this); }
		std::ostream& print(std::ostream& out, int width)
		{
			out << std::setw(width) << value;
			return out;
		}
	protected:
		T value;
	};
public:
	IterationDisplay() : headerInterval(100), iterationInterval(10) {}
	bool addColumn(const std::string& name, int p = 0, int w = 10, bool v = true, const std::string& d = "-");
	void removeColumn(const std::string& name);
	void setVisible(const std::string& name, bool visible);
	void printHeader(std::ostream& out);
	bool needHeader(int k) const { return ((k % headerInterval) == 0); }
	void resetIteration();
	void markIteration() { marked = true; }
	bool needPrint(int k) const { return (marked || ((k % iterationInterval) == 0)); }
	void set(const std::string& name, FmtPtr data);
	void printIteration(std::ostream& out);
	int headerInterval;
	int iterationInterval;
protected:
	class Column
	{
	public:
		Column(const std::string& n, int p, int w, bool v, const std::string& d) : name(n), priority(p), width(w), visible(v), defValue(d) {}
		std::string name;
		int priority;
		int width;
		bool visible;
		std::string defValue;
	};
	typedef std::map<int, Column*> ColumnMap;
	ColumnMap columns;
	typedef std::map<std::string, FmtPtr> ItMap;
	ItMap current;
	bool marked;
};

template<> class IterationDisplay::SimpleFmt<double> : public IterationDisplay::Fmt
{
public:
	SimpleFmt(const double& v, int p = 2) : value(v), precision(p) {}
	SimpleFmt* clone() const { return new SimpleFmt(*this); }
	std::ostream& print(std::ostream& out, int width)
	{
		out << std::setiosflags(std::ios::fixed) << std::setprecision(precision) << std::setw(width) << value;
		return out;
	}
protected:
	double value;
	int precision;
};

#define IT_DISPLAY_SIMPLE_FMT(type, expr) IterationDisplay::FmtPtr(new dominiqs::IterationDisplay::SimpleFmt<type>(expr))
#define IT_DISPLAY_SIMPLE_FLOAT_FMT(type, expr, prec) IterationDisplay::FmtPtr(new dominiqs::IterationDisplay::SimpleFmt<type>(expr, prec))

} // namespace dominiqs

#endif /* IT_DISPLAY_H */
