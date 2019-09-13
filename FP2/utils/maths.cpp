/**
 * @file maths.cpp
 * @brief Mathematical utilities Source
 *
 * @author Domenico Salvagnin dominiqs@gmail.com
 * 2008
 */

#include "maths.h"
#include "floats.h"
#include <cstring>
// #include <emmintrin.h>

static const int DEF_GATHER_SIZE = 1024;

namespace dominiqs {

SparseVector::SparseVector(const SparseVector& other)
{
	if (other.length)
	{
		scoped_ptr_sse2<int> tmp_idx(static_cast<int*>(mallocSSE2(other.length * sizeof(int))));
		DOMINIQS_ASSERT( tmp_idx.get() );
		std::memcpy(tmp_idx.get(), other.m_idx, other.length * sizeof(int));
		scoped_ptr_sse2<double> tmp_coef(static_cast<double*>(mallocSSE2(other.length * sizeof(double))));
		DOMINIQS_ASSERT( tmp_coef.get() );
		std::memcpy(tmp_coef.get(), other.m_coef, other.length * sizeof(double));
		// nothing can throw now
		m_idx = tmp_idx.release();
		m_coef = tmp_coef.release();
		length = other.length;
		alloc = other.length;
	}
	else
	{
		m_idx = 0;
		m_coef = 0;
		length = 0;
		alloc = 0;
	}
}

SparseVector& SparseVector::operator=(const SparseVector& other)
{
	if (this != &other)
	{
		SparseVector temp(other);
		swap(temp);
	}
	return *this;
}

void SparseVector::resize(size_type newSize)
{
	if (newSize)
	{
		reserve(newSize);
		length = newSize;
	}
	else
	{
		freeSSE2(m_idx);
		m_idx = 0;
		freeSSE2(m_coef);
		m_coef = 0;
		length = 0;
		alloc = 0;
	}
}

void SparseVector::reserve(size_type n)
{
	if (n > alloc)
	{
		int* oldidx = m_idx;
		m_idx = static_cast<int*>(mallocSSE2(n * sizeof(int)));
		std::memcpy(m_idx, oldidx, length * sizeof(int));
		freeSSE2(oldidx);
		double* oldcoef = m_coef;
		m_coef = static_cast<double*>(mallocSSE2(n * sizeof(double)));
		std::memcpy(m_coef, oldcoef, length * sizeof(double));
		freeSSE2(oldcoef);
		alloc = n;
	}
}

void SparseVector::copy(const int* idx, const double* coef, size_type cnt)
{
	resize(cnt);
	DOMINIQS_ASSERT( length == cnt );
	std::memcpy(m_idx, idx, cnt * sizeof(int));
	std::memcpy(m_coef, coef, cnt * sizeof(double));
}

bool SparseVector::operator==(const SparseVector& rhs) const
{
	if (size() != rhs.size()) return false;
	for (size_type i = 0; i < size(); i++)
	{
		if (m_idx[i] != rhs.m_idx[i]) return false;
		if (different(m_coef[i], rhs.m_coef[i])) return false;
	}
	return true;
}

void SparseVector::gather(const double* in, int n, double eps)
{
	clear();
	int fastN = std::min(n, DEF_GATHER_SIZE);
	reserve(fastN);
	for (int i = 0; i < fastN; ++i) if (isNotNull(in[i], eps)) push_unsafe(i, in[i]);
	for (int i = fastN; i < n; ++i) if (isNotNull(in[i], eps)) push(i, in[i]);
}

void SparseVector::scatter(double* out, int n, bool reset)
{
	if (reset) std::memset((void*)out, 0, n * sizeof(double));
	SparseVector::size_type cnt = size();
	for (SparseVector::size_type i = 0; i < cnt; ++i) out[m_idx[i]] = m_coef[i];
}

void SparseVector::unscatter(double* out)
{
	SparseVector::size_type cnt = size();
	for (SparseVector::size_type i = 0; i < cnt; ++i) out[m_idx[i]] = 0.0;
}

double Constraint::violation(const double* x) const
{
	double slack = rhs - dotProduct(row, x);
	if (sense == 'L') return -slack;
	if (sense == 'G') return slack;
	return fabs(slack);
}

void accumulate(double* v, const double* w, int n, double lambda)
{
	register int i;
	for (i = 0; i < n; ++i) v[i] += (lambda * w[i]);
}

void accumulate(double* v, const int* wIdx, const double* wCoef, int n, double lambda)
{
	register int i;
	for (i = 0; i < n; ++i) v[wIdx[i]] += (lambda * wCoef[i]);
}

void scale(double* v, int n, double lambda)
{
	register int i;
	for (i = 0; i < n; ++i) v[i] *= lambda;
}

double dotProduct(const double* x, const double* y, int n)
{
#if 0
	register int cnt = n;
	cnt = cnt >> 2;
	register int i;
	__m128d a1,a2;
	__m128d b1,b2;
	__m128d r = _mm_setzero_pd();
	for (i = cnt; i > 0 ; --i)
	{
		a1 = _mm_loadu_pd(x);
		a2 = _mm_loadu_pd(x + 2);
		b1 = _mm_loadu_pd(y);
		a1 = _mm_mul_pd(a1, b1);
		b2 = _mm_loadu_pd(y + 2);
		a2 = _mm_mul_pd(a2, b2);
		r = _mm_add_pd(r, a1);
		r = _mm_add_pd(r, a2);
		x += 4;
		y += 4;
	}
	double ret, res1, res2;
	_mm_storeh_pd(&res1, r);
	_mm_storel_pd(&res2, r);
	ret = res1 + res2;
	for (i = 0; i < (n - 4*cnt); ++i) ret += (x[i] * y[i]);
	return ret;
#endif // 0
	double ans = 0.0;
	register int i;
	if ( n >= 8 )
	{
		for ( i = 0; i < ( n >> 3 ); ++i, x += 8, y += 8 )
			ans += x[0] * y[0] + x[1] * y[1] +
				x[2] * y[2] + x[3] * y[3] +
				x[4] * y[4] + x[5] * y[5] +
				x[6] * y[6] + x[7] * y[7];
		n -= i << 3;
	}
	for ( i = 0; i < n; ++i ) ans += (x[i] * y[i]);
	return ans;
}

double dotProduct(const int* idx, const double* x, int n, const double* y)
{
#if 0
	register int cnt = n;
	cnt = cnt >> 1;
	register int i;
	__m128d a;
	__m128d b;
	__m128d r = _mm_setzero_pd();
	for (i = cnt; i > 0 ; --i)
	{
		a = _mm_loadu_pd(x);
		b = _mm_set_pd(y[idx[1]], y[idx[0]]);
		a = _mm_mul_pd(a, b);
		r = _mm_add_pd(r, a);
		idx += 2;
		x += 2;
	}
	double ret, res1, res2;
	_mm_storeh_pd(&res1, r);
	_mm_storel_pd(&res2, r);
	ret = res1 + res2;
	if (n % 2) ret += (x[0] * y[idx[0]]);
	return ret;
#endif // 0
	double ans = 0.0;
	register int i;
	for (i = 0; i < n; i++) ans += (x[i] * y[idx[i]]);
	return ans;
}

bool disjoint(const double* x, const double* y, int n)
{
	register int cnt = n;
	cnt = cnt >> 1;
	register int i;
	register double p1;
	register double p2;
	for (i = cnt; i > 0 ; --i)
	{
		p1 = fabs(x[0] * y[0]);
		p2 = fabs(x[1] * y[1]);
		if (isPositive(p1 + p2)) return false;
		x += 2;
		y += 2;
	}
	if (n % 2) return (isPositive(fabs(x[0] * y[0])));
	return true;
}

double euclidianNorm(const double* v, int n)
{
	double ans = 0.0;
	register int i;
	for (i = 0; i < n; i++) ans += (v[i] * v[i]);
	return sqrt(ans);
}

double euclidianDistance(const double* a1, const double* a2, int n)
{
	double ans = 0.0;
	register int i;
	for (i = 0; i < n; i++) ans += ((a1[i] - a2[i]) * (a1[i] - a2[i]));
	return sqrt(ans);
}

int lexComp(const double* s1, const double* s2, int n)
{
	for ( int i = 0; i < n; i++ ) {
		if ( lessThan(s1[i], s2[i]) ) return -1;
		if ( greaterThan(s1[i], s2[i]) ) return 1;
	}
	return 0;
}

} // namespace dominiqs
