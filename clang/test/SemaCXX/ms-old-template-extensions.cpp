// RUN: %clang_cc1 -triple i686-pc-windows-msvc -fms-extensions -fms-compatibility -fms-compatibility-version=12.00 -fsyntax-only -verify=old %s
// RUN: %clang_cc1 -triple i686-pc-windows-msvc -fms-extensions -fms-compatibility -fms-compatibility-version=13.10 -fsyntax-only -verify=new %s

// Visual C++ 6.0 and before accept two things that the C++ standard does
// not, and their C++ library headers (Dinkumware, 1998) use both.

// A default template argument in the declaration and again in the
// definition, as in <iosfwd> and <streambuf>.
template <class T> struct char_traits {};
template <class E, class Tr = char_traits<E> > // old-note {{previous default template argument defined here}} new-note {{previous default template argument defined here}}
class basic_streambuf;
template <class E, class Tr = char_traits<E> > // old-warning {{template parameter redefines default argument; Visual C++ 6.0 and before accept this}} new-error {{template parameter redefines default argument}}
class basic_streambuf {
public:
  int x;
};
basic_streambuf<char> sb;

// An explicit specialization without 'template<>', as in <limits>.
template <class T> struct numeric_limits {
  static int digits() { return 0; }
};
struct numeric_limits<int> { // old-warning {{template specialization without 'template<>'; Visual C++ 6.0 and before accept this}} new-error {{template specialization requires 'template<>'}}
  static int digits() { return 31; }
};
int d = numeric_limits<int>::digits();
