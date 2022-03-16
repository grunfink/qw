#!/bin/sh

# Configuration shell script

PROJ=qw
EXE_NAME=qw

# gets program version
VERSION=`cut -f2 -d\" VERSION`

# default installation prefix
PREFIX=/usr/local

# installation directory for documents
DOCDIR=""

# store command line args for configuring the libraries
CONF_ARGS="$*"

# driver
DRIVER=""

# more object files
MORE_OBJS=""

MINGW32_PREFIX=i686-w64-mingw32

WITHOUT_CURSES=1

# parse arguments
while [ $# -gt 0 ] ; do

    case $1 in
    --without-curses)   WITHOUT_CURSES=1 ;;
    --with-large-file)  WITH_LARGE_FILE=1 ;;
    --help)             CONFIG_HELP=1 ;;

    --mingw32-prefix=*)     MINGW32_PREFIX=`echo $1 | sed -e 's/--mingw32-prefix=//'`
                            ;;

    --mingw32)          CC=${MINGW32_PREFIX}-gcc
                        WINDRES=${MINGW32_PREFIX}-windres
                        AR=${MINGW32_PREFIX}-ar
                        LD=${MINGW32_PREFIX}-ld
                        CPP=${MINGW32_PREFIX}-g++
                        ;;

    --prefix)           PREFIX=$2 ; shift ;;
    --prefix=*)         PREFIX=`echo $1 | sed -e 's/--prefix=//'` ;;

    --docdir)           DOCDIR=$2 ; shift ;;
    --docdir=*)         DOCDIR=`echo $1 | sed -e 's/--docdir=//'` ;;

    esac

    shift
done

if [ "$CONFIG_HELP" = "1" ] ; then

    echo "Available options:"
    echo "--without-curses      Disable curses (text) interface detection."
    echo "--with-large-file     Include Large File support (>2GB)."
    echo "--prefix=PREFIX       Installation prefix ($PREFIX)."
    echo "--docdir=DOCDIR       Instalation directory for documentation."
    echo "--mingw32             Build using the mingw32 compiler."
    
    echo
    echo "Environment variables:"
    echo "CC                    C Compiler."
    echo "AR                    Library Archiver."
    echo "CFLAGS                Additional arguments for the C Compiler."
    
    exit 1
fi

echo "Configuring ${PROJ}..."

echo "/* automatically created by config.sh - do not modify */" > config.h
echo "# automatically created by config.sh - do not modify" > makefile.opts
> config.ldflags
> config.cflags
> .config.log

# set compiler
if [ "$CC" = "" ] ; then
    CC=cc
    # if CC is unset, try if gcc is available
    which gcc > /dev/null 2>&1

    if [ $? = 0 ] ; then
        CC=gcc
    fi
fi

echo "CC=$CC" >> makefile.opts

# Add CFLAGS to CC
CC="$CC $CFLAGS"

# set archiver
if [ "$AR" = "" ] ; then
    AR=ar
fi

echo "AR=$AR" >> makefile.opts

# add version
cat VERSION >> config.h

# add installation prefix
echo "#define CONFOPT_PREFIX \"$PREFIX\"" >> config.h

if [ "$WITH_LARGE_FILE" = 1 ] ; then
    echo "#define _FILE_OFFSET_BITS 64" >> config.h
fi

#########################################################

# configuration directives

# CFLAGS
if [ -z "$CFLAGS" ] ; then
    CFLAGS="-g -Wall"
fi

echo -n "Testing if C compiler supports ${CFLAGS}... "
echo "int main(int argc, char *argv[]) { return 0; }" > .tmp.c

$CC .tmp.c -o .tmp.o 2>> .config.log

if [ $? = 0 ] ; then
    echo "OK"
else
    echo "No; resetting to defaults"
    CFLAGS=""
fi

echo "CFLAGS=$CFLAGS" >> makefile.opts

# Add CFLAGS to CC
CC="$CC $CFLAGS"


# Win32
echo -n "Testing for windows... "
if [ "$WITHOUT_WINDOWS" = "1" ] ; then
    echo "Disabled by user"
else
    echo "#include <windows.h>" > .tmp.c
    echo "#include <commctrl.h>" >> .tmp.c
    echo "int CALLBACK WinMain(HINSTANCE h, HINSTANCE p, LPSTR c, int m)" >> .tmp.c
    echo "{ return 0; }" >> .tmp.c

    TMP_LDFLAGS="-lws2_32"
    $CC .tmp.c $TMP_LDFLAGS -o .tmp.o 2>> .config.log

    if [ $? = 0 ] ; then
        echo "-mwindows -lcomctl32" >> config.ldflags
        echo "OK"
        WITH_WINDOWS=1
        DRIVER="windows"
        MORE_OBJS="qw_res.o"
        EXE_NAME="qw.exe"
        echo "WINDRES=$WINDRES" >> makefile.opts
        echo $TMP_LDFLAGS >> config.ldflags
    else
        echo "No"
    fi
fi


# test for curses / ncurses library
echo -n "Testing for ncursesw... "

if [ "$DRIVER" != "" ] || [ "$WITHOUT_CURSES" = "1" ] ; then
    echo "Disabled"
else
    echo "#include <ncurses.h>" > .tmp.c
    echo "int main(void) { initscr(); endwin(); return 0; }" >> .tmp.c

    TMP_CFLAGS="-I/usr/local/include"
    TMP_LDFLAGS="-L/usr/local/lib -lncursesw"

    $CC $TMP_CFLAGS .tmp.c $TMP_LDFLAGS -o .tmp.o 2>> .config.log
    if [ $? = 0 ] ; then
        echo $TMP_CFLAGS >> config.cflags
        echo $TMP_LDFLAGS >> config.ldflags
        echo "OK (ncursesw)"
        DRIVER="curses"
        WITHOUT_ANSI=1
    else
        # retry with ncursesw/ncurses.h
        echo "#include <ncursesw/ncurses.h>" > .tmp.c
        echo "int main(void) { initscr(); endwin(); return 0; }" >> .tmp.c

        $CC $TMP_CFLAGS .tmp.c $TMP_LDFLAGS -o .tmp.o 2>> .config.log

        if [ $? = 0 ] ; then
            echo "#define CONFOPT_CURSES 1" >> config.h
            echo "#define CONFOPT_NCURSESW_NCURSES 1" >> config.h
            echo $TMP_CFLAGS >> config.cflags
            echo $TMP_LDFLAGS >> config.ldflags
            echo "OK (ncursesw with ncursesw/ncurses.h)"
            DRIVER="curses"
            WITHOUT_ANSI=1
        else
            echo "No"
            WITHOUT_CURSES=1
        fi
    fi
fi

# ANSI
echo -n "Testing for ANSI terminal support... "

if [ "$DRIVER" != "" ] || [ "$WITHOUT_ANSI" = "1" ] ; then
    echo "Disabled"
else
    rm -f .tmp.c
    echo "#include <stdio.h>" >> .tmp.c
    echo "#include <termios.h>" >> .tmp.c
    echo "#include <unistd.h>" >> .tmp.c
    echo "#include <sys/select.h>" >> .tmp.c
    echo "#include <signal.h>" >> .tmp.c
    echo "int main(void) { struct termios o; tcgetattr(0, &o); return 0; }" >> .tmp.c

    TMP_CFLAGS=""
    TMP_LDFLAGS=""

    $CC $TMP_CFLAGS .tmp.c $TMP_LDFLAGS -o .tmp.o 2>> .config.log
    if [ $? = 0 ] ; then
        echo "#define CONFOPT_ANSI 1" >> config.h
        echo $TMP_CFLAGS >> config.cflags
        echo $TMP_LDFLAGS >> config.ldflags
        echo "OK"
        DRIVER="ansi"
    else
        echo "No"
        WITHOUT_ANSI=1
    fi
fi


#########################################################

# final setup

echo "PROJ=$PROJ" >> makefile.opts
echo "EXE_NAME=$EXE_NAME" >> makefile.opts
echo "DOCS=$DOCS" >> makefile.opts
echo "VERSION=$VERSION" >> makefile.opts
echo "PREFIX=\$(DESTDIR)$PREFIX" >> makefile.opts
echo "DOCDIR=\$(DESTDIR)$DOCDIR" >> makefile.opts
echo "DRIVER_OBJ=qw_drv_${DRIVER}.o" >> makefile.opts
echo "MORE_OBJS=$MORE_OBJS" >> makefile.opts
echo "CONF_ARGS=$CONF_ARGS" >> makefile.opts
echo >> makefile.opts

cat makefile.opts makefile.in > Makefile

if [ -f makefile.depend ] ; then
    cat makefile.depend >> Makefile
fi

#########################################################

# cleanup

rm -f .tmp.c .tmp.o

if [ "$DRIVER" = "" ] ; then

    echo
    echo "*ERROR* No usable drivers (interfaces) found"
    echo "See the README file for the available options."

    exit 1
fi

echo
echo "Configured driver:" $DRIVER
echo
echo Type 'make' to build ${PROJ}.

exit 0
