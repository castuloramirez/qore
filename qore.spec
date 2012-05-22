%define qore_ver 0.8.4
%define module_dir %{_libdir}/qore-modules
%define module_ver_dir %{module_dir}/%{qore_ver}

%if 0%{?sles_version}

%define dist .sles%{?sles_version}

%else
%if 0%{?suse_version}

# get *suse release major version
%define os_maj %(echo %suse_version|rev|cut -b3-|rev)
# get *suse release minor version without trailing zeros
%define os_min %(echo %suse_version|rev|cut -b-2|rev|sed s/0*$//)

%if %suse_version > 1010
%define dist .opensuse%{os_maj}_%{os_min}
%else
%define dist .suse%{os_maj}_%{os_min}
%endif

%endif
%endif

# see if we can determine the distribution type
%if 0%{!?dist:1}
%define rh_dist %(if [ -f /etc/redhat-release ];then cat /etc/redhat-release|sed "s/[^0-9.]*//"|cut -f1 -d.;fi)
%if 0%{?rh_dist}
%define dist .rhel%{rh_dist}
%else

%define dist .unknown
%endif
%endif

Summary: Qore Programming Language
Name: qore
Version: %{qore_ver}
Release: 1%{dist}
License: LGPL or GPL
Group: Development/Languages/Other
URL: http://www.qoretechnologies.com/qore
Source: http://prdownloads.sourceforge.net/qore/qore-%{version}.tar.bz2
#Source0: %{name}-%{version}.tar.bz2
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root
Requires: /usr/bin/env
BuildRequires: gcc-c++
BuildRequires: flex >= 2.5.31
BuildRequires: bison
BuildRequires: openssl-devel
BuildRequires: pcre-devel
BuildRequires: zlib-devel
BuildRequires: doxygen
%if 0%{?suse_version}
BuildRequires: pkg-config
%if 0%{?sles_version} && %{?sles_version} <= 10
BuildRequires: bzip2
%else
BuildRequires: libbz2-devel
%endif
%else
BuildRequires: pkgconfig
BuildRequires: bzip2-devel
%endif

%description
Qore is a scripting language supporting threading and embedded logic, designed
for applying a flexible scripting-based approach to enterprise interface
development but is also useful as a general purpose language.

%if 0%{?suse_version}
%debug_package
%endif

%package -n libqore5
Summary: The libraries for the qore runtime and qore clients
Group: Development/Languages/Other
Provides: qore-module-api-0.12
Provides: qore-module-api-0.11
Provides: qore-module-api-0.10
Provides: qore-module-api-0.9
Provides: qore-module-api-0.8
Provides: qore-module-api-0.7
Provides: qore-module-api-0.6
Provides: qore-module-api-0.5

%description -n libqore5
Qore is a scripting language supporting threading and embedded logic, designed
for applying a flexible scripting-based approach to enterprise interface
development but is also useful as a general purpose language.

This package provides the qore library required for all clients using qore
functionality.

%files -n libqore5
%defattr(-,root,root,-)
%{_libdir}/libqore.so.5.7.0
%{_libdir}/libqore.so.5
%doc COPYING.LGPL COPYING.GPL README README-LICENSE README-MODULES RELEASE-NOTES ChangeLog AUTHORS WHATISQORE

%post -n libqore5
ldconfig %{_libdir}

%postun -n libqore5
ldconfig %{_libdir}

%package doc
Summary: API documentation, programming language reference, and Qore example programs
Group: Development/Languages/Other

%description doc
Qore is a scripting language supporting threading and embedded logic, designed
for applying a flexible scripting-based approach to enterprise interface
development but is also useful as a general purpose language.

This package provides the HTML documentation for the Qore programming language
and also for user modules delivered with Qore and also example programs.

%files doc
%defattr(-,root,root,-)
%doc docs/lang/html docs/modules/*/html examples/

%package devel
Summary: The header files needed to compile programs using the qore library
Group: Development/Languages/Other
Requires: libqore5 = %{version}-%{release}

%description devel
Qore is a scripting language supporting threading and embedded logic, designed
for applying a flexible scripting-based approach to enterprise interface
development but is also useful as a general purpose language.

This package provides header files needed to compile client programs using the
Qore library.

%files devel
%defattr(-,root,root,-)
/usr/bin/qpp
%{_libdir}/libqore.so
%{_libdir}/pkgconfig/qore.pc
%{_prefix}/include/*

%package devel-doc
Summary: C++ API documentation for the qore library
Group: Development/Languages/Other
Requires: libqore5 = %{version}-%{release}

%description devel-doc
Qore is a scripting language supporting threading and embedded logic, designed
for applying a flexible scripting-based approach to enterprise interface
development but is also useful as a general purpose language.

This package provides HTML documentation for the C++ API for the Qore library.

%files devel-doc
%doc docs/library/html

%prep
%setup -q
mv $RPM_BUILD_DIR/%{name}-%{version}/test $RPM_BUILD_DIR/%{name}-%{version}/examples
mkdir docs/modules

%ifarch x86_64 ppc64 x390x
c64=--enable-64bit
%endif
# need to configure with /usr as prefix as this will be used to derive the module directory
./configure RPM_OPT_FLAGS="$RPM_OPT_FLAGS" --prefix=/usr --disable-debug --disable-static $c64

%build
%{__make}

%install
mkdir -p $RPM_BUILD_ROOT/usr/bin
mkdir -p $RPM_BUILD_ROOT/%{module_dir}
mkdir -p $RPM_BUILD_ROOT/%{module_dir}/${qore_ver}
mkdir -p $RPM_BUILD_ROOT/usr/man/man1
make install prefix=%{_prefix} DESTDIR=$RPM_BUILD_ROOT
rm $RPM_BUILD_ROOT/%{_libdir}/libqore.la

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)
/usr/bin/qore
%{module_ver_dir}
%if 0%{?rh_dist}
%if %{?rh_dist} <= 5
/usr/man/man1/qore.1.*
%endif
%else
/usr/share/man/man1/qore.1.*
%else
%if 0%{?mdkversion}
/usr/share/man/man1/qore.1.*
%endif
%endif

%changelog
* Tue May 22 2012 David Nichols <david@qore.org> 0.8.4
- updated for new doxygen-based documentation, added devel-doc pkg for API docs
- updated package descriptions

* Thu Oct 20 2011 David Nichols <david@qore.org> 0.8.4
- updated to 0.8.4

* Fri Oct 07 2011 Petr Vanek <petr.vanek@qoretechnologies.com> 0.8.3
- pkg-config

* Sun Mar 6 2011 David Nichols <david@qore.org>
- updated to 0.8.3

* Sun Dec 26 2010 David Nichols <david@qore.org>
- updated to 0.8.2

* Tue Jun 15 2010 David Nichols <david@qore.org>
- updated to 0.8.1

* Wed Nov 18 2009 David Nichols <david_nichols@users.sourceforge.net>
- updated to 0.8.0

* Fri Nov 6 2009 David Nichols <david_nichols@users.sourceforge.net>
- updated to 0.7.7

* Mon Jul 13 2009 David Nichols <david_nichols@users.sourceforge.net>
- updated to 0.7.6

* Mon Jun 22 2009 David Nichols <david_nichols@users.sourceforge.net>
- updated to 0.7.5

* Wed Mar 4 2009 David Nichols <david_nichols@users.sourceforge.net>
- updated to 0.7.4

* Wed Dec 3 2008 David Nichols <david_nichols@users.sourceforge.net>
- updated to 0.7.3

* Wed Nov 26 2008 David Nichols <david_nichols@users.sourceforge.net>
- made libqore* the default name for lib package, removed la file

* Sun Nov 23 2008 David Nichols <david_nichols@users.sourceforge.net>
- updated to 0.7.2

* Tue Oct 7 2008 David Nichols <david_nichols@users.sourceforge.net>
- released 0.7.0

* Thu Sep 4 2008 David Nichols <david_nichols@users.sourceforge.net>
- removed all modules as they are now independent projects

* Tue Sep 2 2008 David Nichols <david_nichols@users.sourceforge.net>
- fixed dist tag for suse distributions
- updated for new module directory, added qore-module-api-* capability

* Thu Jun 12 2008 David Nichols <david_nichols@users.sourceforge.net>
- added new modules

* Tue Oct 22 2007 David Nichols <david_nichols@users.sourceforge.net>
- updated spec file with corrections from suse open build service

* Tue Jul 17 2007 David Nichols <david_nichols@users.sourceforge.net>
- updated library version to 3.1.0

* Sat Jul 14 2007 David Nichols <david_nichols@users.sourceforge.net>
- copied improvements from opensuse rpm and updated based on rpmlint output
- updated version to 0.7.0

* Thu Jun 14 2007 David Nichols <david_nichols@users.sourceforge.net>
- fixed spec file to support more architectures

* Wed Jun 13 2007 David Nichols <david_nichols@users.sourceforge.net>
- removed tibae module from spec file due to compiler requiremenets (g++-32)
- added pgsql module

* Tue Feb 20 2007 David Nichols <david_nichols@users.sourceforge.net>
- updated to libqore.so.3.0.0

* Tue Feb 11 2007 David Nichols <david_nichols@users.sourceforge.net>
- updated to 0.6.2 and libqore 1.1

* Tue Jan 30 2007 David Nichols <david_nichols@users.sourceforge.net>
- added tuxedo module

* Fri Jan 5 2007 David Nichols <david_nichols@users.sourceforge.net>
- updated libqore so version to 1.0.0

* Sat Nov 18 2006 David Nichols <david_nichols@users.sourceforge.net>
- updated descriptions
- changes to make spec file more release-agnostic (use of the dist tag in release)

* Thu Dec 7 2005 David Nichols <david_nichols@users.sourceforge.net>
- Initial rpm build

