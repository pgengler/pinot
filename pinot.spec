%define name	pinot
%define version	0.0.1
%define release	1

Summary	: Nano editor clone with enhancements
Name		: %{name}
Version		: %{version}
Release		: %{release}
License		: GPL
Group		: Applications/Editors
URL		: http://github.com/pgengler
Source		: http://github.com/pgengler
BuildRoot	: %{_tmppath}/%{name}-%{version}-root
BuildRequires	: autoconf, automake, gettext-devel, ncurses-devel

%description
pinot is a small and friendly text editor.  It aims to emulate the
nano text editor while also offering a few enhancements.

%prep
%setup -q

%build
%configure --enable-all
make

%install
rm -rf %{buildroot}
make DESTDIR="%{buildroot}" install

%files
%defattr(-,root,root)
%doc AUTHORS BUGS COPYING ChangeLog INSTALL NEWS README THANKS TODO doc/faq.html doc/pinotrc.sample
%{_bindir}/*
%{_mandir}/man*/*
%{_mandir}/fr/man*/*
%{_infodir}/pinot.info*
%{_datadir}/locale/*/LC_MESSAGES/pinot.mo
%{_datadir}/pinot/*
