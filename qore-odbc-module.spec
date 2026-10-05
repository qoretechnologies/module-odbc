# Copyright (C) 2026 Qore Technologies, s.r.o.
# SPDX-License-Identifier: MIT
# Use the pinned source epoch for RPM headers and installed file timestamps.
%global source_date_epoch_from_changelog 1
%global use_source_date_epoch_as_buildtime 1
%if v"%{rpmversion}" >= v"4.20"
%global build_mtime_policy clamp_to_source_date_epoch
%else
%global clamp_mtime_to_source_date_epoch 1
%endif
%bcond_without tests
%bcond_without docs
Name: qore-odbc-module
Version: 2.0.0
Release: 1%{?dist}
Summary: ODBC database driver for Qore
License: MIT
URL: https://github.com/qoretechnologies/module-odbc
Source0: %{name}-%{version}.tar.xz
BuildRequires: cmake >= 3.5
BuildRequires: make
BuildRequires: gcc-c++
BuildRequires: unixODBC-devel
BuildRequires: qore-devel >= 3.0.0~
BuildRequires: qore-rpm-macros >= 3.0.0~
%if %{with tests}
BuildRequires: python3
BuildRequires: postgresql-server
%if 0%{?suse_version}
BuildRequires: psqlODBC
%else
BuildRequires: postgresql-odbc
%endif
%endif
%if %{with docs}
BuildRequires: doxygen
%if 0%{?suse_version}
BuildRequires: util-linux
%else
BuildRequires: util-linux-core
%endif
%endif

%description
Database access through ODBC, with prepared statements, array binding,
transactions and explicit parameter types. Install the ODBC driver for your
database separately and configure its connection string or data source name.

%if %{with docs}
%package doc
Summary: ODBC module reference documentation and examples
BuildArch: noarch
%description doc
API reference and database test examples for Qore's ODBC driver.
%endif

%prep
%autosetup
%build
%{?set_build_flags}
. %{_rpmconfigdir}/qore/module-env.sh
qore_set_source_prefix_maps "%{qore_debug_source_dir}"
cmake -S . -B build -G 'Unix Makefiles' \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS_RELEASE=-DNDEBUG \
  -DCMAKE_INSTALL_PREFIX=%{_prefix} \
  -DCMAKE_SKIP_RPATH=ON -DCMAKE_IGNORE_PREFIX_PATH=/usr/local \
  -DQore_DIR=%{_libdir}/cmake/Qore -DQORE_EXECUTABLE=/usr/bin/qore \
  -DQORE_QPP_EXECUTABLE=/usr/bin/qpp \
  -DCMAKE_DISABLE_FIND_PACKAGE_Doxygen=%{!?with_docs:ON}%{?with_docs:OFF}
cmake --build build -- %{?_smp_mflags}
%if %{with docs}
printf "\nWARN_AS_ERROR = FAIL_ON_WARNINGS\n" >> build/Doxyfile
cmake --build build --target docs -- %{?_smp_mflags}
%endif
%install
DESTDIR=%{buildroot} cmake --install build
chmod 755 %{buildroot}%{_libdir}/qore-modules/odbc-api-*.qmod
%if %{with docs}
install -d %{buildroot}%{_docdir}/%{name}-doc
cp -a build/docs/odbc/html %{buildroot}%{_docdir}/%{name}-doc/
install -d %{buildroot}%{_docdir}/%{name}-doc/examples/test
install -m644 test/*.qtest %{buildroot}%{_docdir}/%{name}-doc/examples/test/
hardlink -t -O %{buildroot}%{_docdir}/%{name}-doc
%endif
%check
%if %{with tests}
. %{_rpmconfigdir}/qore/module-env.sh
python3 -B -W error rpm/test_postgres_fixture.py -v
%if %{with docs}
python3 -B -W error test/test_docs.py build -v
%endif
export QORE_ODBC_BINARY_MODULE="$PWD/build/odbc-api-$(/usr/bin/qore --latest-module-api).qmod"
rpm/test-postgres test --native
%endif
%files
%license LICENSE.txt
%doc README.md
%{_libdir}/qore-modules/odbc-api-*.qmod
%dir %{_datadir}/qore/metadata/odbc
%{_datadir}/qore/metadata/odbc/*.meta.json
%if %{with docs}
%files doc
%license LICENSE.txt
%doc %{_docdir}/%{name}-doc/
%endif
%changelog
* Sun Oct 04 2026 David Nichols <david@qore.org> - 2.0.0-1
- Align the upcoming module, documentation, and package version at 2.0.0.

* Thu Oct 01 2026 David Nichols <david@qore.org> - 1.2.0-1
- Package the ODBC driver, metadata and complete public API documentation.
- Run real PostgreSQL database tests in a private offline cluster.
