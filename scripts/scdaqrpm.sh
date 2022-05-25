#!/bin/bash -e
BUILD_ARCH=x86_64
SCRIPTDIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RHEL_MAJOR=$(cat /etc/redhat-release | cut -d' ' -f4 | cut -d'.' -f1)

echo "Checking if necessary build packages are installed..."
if [ "$RHEL_MAJOR" = "7" ]; then
  if ! rpm -q rpm-build; then
    echo "\nPlease install missing packages."
    exit 1
  fi
else
  if ! rpm -q gcc redhat-rpm-config rpm-build zlib-devel; then
    echo ""
    echo "Please install missing packages."
    exit 1
  fi
fi
echo "..Build dependencies OK"
echo ""

cd $SCRIPTDIR/..
BASEDIR=$PWD
PARAMCACHE="paramcache"
NLINES=1
ASK="1"

if [ -n "$1" ]; then
  if [ "$1" = "--batch" ]; then
    ASK="0"
  fi

  if [ "$1" = "-b" ]; then
    ASK="0"
  fi

  if [ $ASK = "0" ]; then #check $2 if found
    if [ -n "$2" ]; then
      PARAMCACHE=${2##*/}
    fi
  fi
  if [ $ASK = "1" ]; then #take $1
    PARAMCACHE=${1##*/}
  fi
fi

echo "Using cache file $PARAMCACHE"

if [ -f $SCRIPTDIR/$PARAMCACHE ]; then
  readarray lines <$SCRIPTDIR/$PARAMCACHE
  for ((i = 0; i < ${NLINES}; i++)); do
    lines[$i]=$(echo -n ${lines[$i]} | tr -d "\n")
  done
else
  for ((i = 0; i < ${NLINES}; i++)); do
    lines[$i]=""
  done
fi

if [ $ASK = "1" ]; then

  echo "This is the scdaq build script. It will now ask for several configuration parameters."
  echo "Use -b cmdline parameter to build from cache without waiting for input"
  echo "   ... press any key to continue ..."
  read readin

  echo "Dummy parameter, this will be used to modify/obtain build parameters from cache (press enter for \"${lines[0]}\"):"
  readin=""
  read readin
  if [ ${#readin} != "0" ]; then
    lines[0]=$readin
  fi

fi #ask

#update cache file
if [ -f $SCRIPTDIR/$PARAMCACHE ]; then
  rm -rf -f $SCRIPTDIR/$PARAMCACHE
fi
for ((i = 0; i < ${NLINES}; i++)); do
  echo ${lines[$i]} >>$SCRIPTDIR/$PARAMCACHE
done

PACKAGENAME="scdaq"

# set the RPM build architecture
#BUILD_ARCH=$(uname -i)      # "i386" for SLC4, "x86_64" for SLC5

cd $SCRIPTDIR/..
BASEDIR=$PWD

# create a build area
echo "removing old build area"
rm -rf /tmp/$PACKAGENAME-build-tmp
echo "creating new build area"
mkdir /tmp/$PACKAGENAME-build-tmp
cd /tmp/$PACKAGENAME-build-tmp
#mkdir BUILD
#mkdir RPMS
TOPDIR=$PWD
echo "working in $PWD"
#ls

# we are done here, write the specs and make the fu***** rpm
cat >scoutdaq.spec <<EOF
Name: $PACKAGENAME$pkgsuffix
Version: 0.1.0
Release: 0%{?dist}
Summary: scouting daq
License: gpl
Group: DAQ
Packager: meschi
Source: none
%define _tmppath $TOPDIR/scdaq-build
BuildRoot: %{_tmppath}
BuildArch: $BUILD_ARCH
AutoReqProv: no
Provides:/opt/scdaq

Requires: tbb boost-thread libcurl
Obsoletes: scdaq

%description
scouting daq 

%prep

echo "PIPPPPPPPPO"
echo $RPM_SOURCE_DIR
echo "PIPPOOOOOOOOO"
echo $BASEDIR
cp -R $BASEDIR/src SOURCES/

%build
echo $RPM_SOURCE_DIR
cd SOURCES
pwd
make

%install
echo $RPM_SOURCE_DIR
rm -rf \$RPM_BUILD_ROOT
mkdir -p \$RPM_BUILD_ROOT
%__install -d "%{buildroot}/var/cache/scdaq"
%__install -d "%{buildroot}/var/log/scdaq"
%__install -d "%{buildroot}/var/log/scdaq/pid"
%__install -d "%{buildroot}/opt/scdaq"
%__install -d "%{buildroot}/opt/scdaq/init.d"
%__install -d "%{buildroot}/opt/scdaq/bin"


cd \$RPM_BUILD_ROOT
echo "Creating directories"
mkdir -p opt/scdaq
mkdir -p etc/scdaq
mkdir -p etc/logrotate.d
mkdir -p usr/lib/systemd/system
#mkdir -p %{buildroot}/usr/lib/systemd/system 
mkdir -p etc/init.d
#mkdir -p %{buildroot}/opt/scdaq/init.d

echo "Copying files to their destination"
cp $BASEDIR/init.d/runSCdaq.service      usr/lib/systemd/system/runSCdaq.service
cp $BASEDIR/init.d/scoutboardResetServer.service usr/lib/systemd/system/scoutboardResetServer.service
cp -R $BASEDIR/*                    opt/scdaq
echo "PIPPOOOOOOOOOOO"
pwd
cp $TOPDIR/RPMBUILD/BUILD/SOURCES/scdaq opt/scdaq/bin/
cp -R $BASEDIR/etc/scdaq/scdaq.conf        etc/scdaq/
#cp -R $BASEDIR/etc/logrotate.d/scdaq etc/logrotate.d/
#rm -rf opt/hltd/init.d

#touch opt/scdaq/scratch/new-version

echo "Deleting unnecessary files"
rm -rf opt/hltd/{bin,rpm,lib}
rm -rf opt/hltd/scripts/paramcache*
rm -rf opt/hltd/scripts/*rpm.sh
#rm -rf opt/hltd/scripts/postinstall.sh


%post
#/opt/scdaq/postinstall.sh
systemctl daemon-reload

%files
%dir %attr(777, -, -) /var/cache/scdaq
%dir %attr(777, -, -) /var/log/scdaq
%dir %attr(777, -, -) /var/log/scdaq/pid
%defattr(-, root, root, -)
/opt/scdaq/
%config(noreplace) /etc/scdaq/scdaq.conf
#/etc/logrotate.d/scdaq
%attr( 644 ,root, root) /usr/lib/systemd/system/runSCdaq.service
%attr( 644 ,root, root) /usr/lib/systemd/system/scoutboardResetServer.service
#%attr( 755 ,root, root) /opt/scdaq/init.d/
#%attr( 755 ,root, root) /opt/scdaq/init.d/


%preun
if [ \$1 == 0 ]; then
  /usr/bin/systemctl stop runSCdaq || true
  /usr/bin/systemctl disable runSCdaq || true
  /usr/bin/systemctl stop scoutboardResetServer || true
  /usr/bin/systemctl disable scoutboardResetServer || true
fi
EOF
mkdir -p RPMBUILD/{RPMS/{noarch},SPECS,BUILD,SOURCES,SRPMS}
rpmbuild --define "_topdir $(pwd)/RPMBUILD" -bb scoutdaq.spec
#rm -rf patch-cmssw-tmp
