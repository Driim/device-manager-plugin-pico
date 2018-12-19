Name:       device-manager-plugin-exynos8890
Summary:    Device manager plugin exynos8890
Version:    0.0.1
Release:    0
Group:      System/Hardware Adaptation
License:    Apache-2.0
Source0:    %{name}-%{version}.tar.gz
Source1:    %{name}.manifest
Requires(post): /sbin/ldconfig
Requires(postun): /sbin/ldconfig
BuildRequires:  cmake
BuildRequires:  pkgconfig(dlog)
BuildRequires:  pkgconfig(hwcommon)
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  pkgconfig(libudev)
BuildRequires:  pkgconfig(libusbgx)

%description
Device manager plugin exynos 8890


%prep
%setup -q
cp %{SOURCE1} .

%build
%cmake .

make %{?jobs:-j%jobs}

%install
%make_install


%post
/sbin/ldconfig

%postun
/sbin/ldconfig

%files
%{_libdir}/hw/*.so
%manifest %{name}.manifest
%license LICENSE
