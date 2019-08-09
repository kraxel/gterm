Name:         gterm
License:      GPLv2+
Version:      1.0
Release:      1%{?dist}
Summary:      terminal application
Source:       %{name}-%{version}.tar.gz

BuildRequires: gcc binutils
BuildRequires: meson ninja-build
BuildRequires: pkgconfig(glib-2.0)
BuildRequires: pkgconfig(gtk+-3.0)
BuildRequires: pkgconfig(vte-2.91)

%description
Terminal application, based on gtk3 and vte.  The plan
is to have a modern terminal for xterm fans.

%prep
%setup -q

%build
export CFLAGS="%{optflags}"
meson --prefix=%{_prefix} build-rpm
ninja-build -C build-rpm

%install
export DESTDIR=%{buildroot}
ninja-build -C build-rpm install

%files
%doc README.md COPYING gterm.conf.example
%{_bindir}/gterm
%{_bindir}/gload
%{_mandir}/man1/gterm.1*
%{_mandir}/man1/gload.1*
%{_prefix}/share/applications/gterm.desktop
