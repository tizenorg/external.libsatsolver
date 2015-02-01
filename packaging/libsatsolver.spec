%define run_testsuite 0

Name:           libsatsolver
Version:        0.17.0
Release:        2
License:        BSD 3-clause (or similar)
Url:            https://gitorious.org/opensuse/sat-solver
Source:         satsolver-%{version}.tar.bz2
Patch1:		satsolver-evrcmp.patch
Patch2:		0002-Add-armv7tnhl-and-armv7thl-support.patch

Group:          Development/Libraries/C and C++
BuildRequires:  db4-devel
BuildRequires: perl-devel 
BuildRequires:  fdupes
BuildRequires:  expat-devel
BuildRequires:  cmake gcc-c++ rpm-devel
BuildRequires:  zlib-devel
BuildRequires:  doxygen
# the testsuite uses the check framework
BuildRequires:  check-devel
Summary:        A new approach to package dependency solving

%description
A new approach to package dependency solving

%package devel
License:        BSD 3-clause (or similar)
Summary:        A new approach to package dependency solving
Group:          Development/Libraries/C and C++
Requires:       satsolver-tools = %version
Requires:       rpm-devel

%description devel
Development files for satsolver, a new approach to package dependency solving

%package -n satsolver-tools
License:        BSD 3-clause (or similar)
Summary:        A new approach to package dependency solving
Group:          Development/Libraries/C and C++
Obsoletes:      libsatsolver <= 0.0.15
Provides:       libsatsolver = %{version}-%{release}
Requires:       /bin/cp
Requires:       /bin/gzip
Requires:       /usr/bin/bzip2

%description -n satsolver-tools
A new approach to package dependency solving.

%package demo
License:        BSD 3-clause (or similar)
Summary:        Applications demoing the satsolver library
Group:          System/Management
Requires:       curl
Requires:       gnupg2

%description demo
Applications demoing the satsolver library.


%prep
%setup -q -n satsolver-%{version}
%patch1 -p1
%patch2 -p1

%build
export CFLAGS="$RPM_OPT_FLAGS"
export CXXFLAGS="$CFLAGS"
cmake   $CMAKE_FLAGS \
	-DCMAKE_INSTALL_PREFIX=/usr \
	-DPYTHON_SITEDIR=%{py_sitedir} \
	-DLIB=%{_lib} \
        -DFEDORA=1 \
	-DCMAKE_VERBOSE_MAKEFILE=TRUE \
	-DCMAKE_BUILD_TYPE=Release \
	-DCMAKE_SKIP_RPATH=1 

make %{?jobs:-j %jobs}
%if 0%{?run_testsuite}
  ln -s . build
  ctest .
%endif

%install
make DESTDIR=$RPM_BUILD_ROOT install
# we want to leave the .a file untouched
export NO_BRP_STRIP_DEBUG=true

%clean
rm -rf "$RPM_BUILD_ROOT"

%files -n satsolver-tools
%defattr(-,root,root)
%doc LICENSE*
%exclude /usr/bin/deptestomatic
%exclude /usr/bin/helix2solv
%exclude /usr/bin/solv
/usr/bin/*

%files devel
%defattr(-,root,root)
%_libdir/libsatsolver.a
%_libdir/libsatsolverext.a
#%_libdir/libappsatsolver.a
%dir /usr/include/satsolver
/usr/include/satsolver/*
/usr/bin/deptestomatic
/usr/bin/helix2solv

%files demo
%defattr(-,root,root)
/usr/bin/solv

