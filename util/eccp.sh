#!/bin/sh
# Run the EDG C++ front end into the system cc to compile C++.
# Interface and command-line options are similar to CC.
# CPFE may be set to the executable to use for the C++ front end.
# If CPFE is not set, "cpfe" is used.
#
# Predefined preprocessing variables.
#
defines="-Dsparc -Dunix -Dsun"
#
# Directory where the C++ include files are to be found.
#
INCLDIR=/edg/cpfe/include
#
# Directory where the C include files are to be found.
#
CINCLDIR=/edg/cfe/usr/include
#
# Directory where libC.a is to be found.
#
LIBDIR=${ECCP_LIBDIR-/edg/cpfe/lib}
#
# "patch" executable
#
PATCH=$OWCDIR/sun4/bin/patch
#
# "munch" executable
#
MUNCH=/edg/bin/edg_munch
#
# Flag indicating whether to use "patch" or "munch" for static initialization.
#
patch_mode=1
#
error=0
#
# When set to 1, run front end and cc producing a .o file.
#
cc_only=0
#
# When set to 1, run front end producing a .int.c file.
#
fe_only=0
#
# Name of the executable after linking.
#
executable=a.out
#
# A list of .c files to compile, separated by blanks.
#
cfiles=
more_than_one_c_file=0
#
# A list of .o files to link.  Note that as .c files are successfully compiled,
# the are added to this list.
#
ofiles=
#
# A list of .o files to be removed after linking.  These files are the ones
# that were added to the ofiles list by compiling them.
#
rofiles=
#
# A list of -l and -L options and archive files to be passed to the link step.
#
loptions=
Loptions=
lfiles=
#
# Were any library or object files specified on the command line?
#
any_l_or_o_files=0
#
# A list of options to pass to front end.
#
feoptions=$defines
#
# Symbolic debug output for cc.
#
ccsdb=
keep_int_file=0
#
# Flag indicating that C is being compiled instead of C++
#
cmode=0
#
# Flag indicating that the standard include directory should be added.
#
std_incl=1
#
# Flag indicated that a instantiation mode was specified
#
instantiation_mode_specified=0
#
# Go through every argument, identify it, and add it a list if appropriate.
#
while [ -n "$1" ]
do
  case $1 in
    -O)
#     Allow anachronisms
      feoptions=$feoptions" -O";
      shift;
      ;;
    -b)
#     cfront compatibility
      feoptions=$feoptions" -b";
      shift;
      ;;
    -d)
#     Debug information.
      shift;
      feoptions=$feoptions" -d"$1;
      shift;
      fe_only=1;
      ;;
    -d*)
      feoptions=$feoptions" "$1;
      shift;
      fe_only=1;
      ;;
    -e)
#     Set error limit.
      shift;
      feoptions=$feoptions" -e"$1;
      shift;
      ;;
    -e*)
      feoptions=$feoptions" "$1;
      shift;
      ;;
    -S)
#     Run front end only.
      fe_only=1;
      shift;
      ;;
    -n)
#     Run front end only, suppress output of .int.c file.
      fe_only=1;
      feoptions=$feoptions" -n";
      shift;
      ;;
    -N)
#     Suppress IL lowering.  This will ultimately suppress output of a .int.c
#     file.
      fe_only=1;
      feoptions=$feoptions" -N";
      shift;
      ;;
    -C)
#     Keep comments in preprocessing output.
      feoptions=$feoptions" -C";
      shift;
      ;;
    -c)
#     Run front end and cc producing a .o file.
      cc_only=1;
      shift;
      ;;
    -o)
#     Explicitly name the executable.
      shift;
      executable=$1;
      shift;
      ;;
    -w)
#     Suppress warnings.
      feoptions=$feoptions" -w";
      shift;
      ;;
    -r)
#     Enable remarks.
      feoptions=$feoptions" -r";
      shift;
      ;;
    -A|-a)
#     Strict ANSI mode.
      feoptions=$feoptions" $1";
      shift;
      ;;
    -K)
#     cpp compatible mode.
      feoptions=$feoptions" -K";
      cmode=1;
      shift;
      ;;
    -s)
#     Signed chars.
      feoptions=$feoptions" -s";
      shift;
      ;;
    -u)
#     Unsigned chars.
      feoptions=$feoptions" -u";
      shift;
      ;;
    -V)
#     Suppress virtual table definition if no non-inline, pure virtual
#     function exists.
      feoptions=$feoptions" -V";
      shift;
      ;;
    -v)
#     Verbose mode; display version of front end.
      feoptions=$feoptions" -v";
      shift;
      ;;
    -E)
#     Preprocessor only.
      fe_only=1;
      feoptions=$feoptions" -E";
      shift;
      ;;
    -P)
#     Preprocessor only.
      fe_only=1;
      feoptions=$feoptions" -P";
      shift;
      ;;
    -M)
#     Generate makefile dependency lines.
      fe_only=1;
      feoptions=$feoptions" -M";
      shift;
      ;;
    -H)
#     Generate names of include files used.
      fe_only=1;
      feoptions=$feoptions" -H";
      shift;
      ;;
    -I)
#     Collect a list of -I options.
      shift;
      feoptions=$feoptions" -I"$1;
      shift;
      ;;
    -I*)
      feoptions=$feoptions" "$1;
      shift;
      ;;
    -h)
#     Suppress standard include directory.
      std_incl=0;
      shift;
      ;;
    -X)
#     Collect a list of -X options.
      shift;
      feoptions=$feoptions" -X"$1;
      shift;
      ;;
    -X*)
      feoptions=$feoptions" "$1;
      shift;
      ;;
    -i)
#     Collect a list of -i options.
      shift;
      feoptions=$feoptions" -i"$1;
      shift;
      ;;
    -i*)
      feoptions=$feoptions" "$1;
      shift;
      ;;
    -D)
#     Collect a list of -D options.
      shift;
      feoptions=$feoptions" -D"$1;
      shift;
      ;;
    -D*)
      feoptions=$feoptions" "$1;
      shift;
      ;;
    -U)
#     Collect a list of -U options.
      shift;
      feoptions=$feoptions" -U"$1;
      shift;
      ;;
    -U*)
      feoptions=$feoptions" "$1;
      shift;
      ;;
    -L)
#     Collect a list of -L options to pass to the linker.
      shift;
      Loptions=$Loptions" -L"$1;
      shift;
      ;;
    -L*)
#     Collect a list of -L options to pass to the linker.
      Loptions=$Loptions" "$1
      shift;
      ;;
    -m)
#     Process C instead of C++.
      feoptions=$feoptions" "$1;
      cmode=1;
      shift;
      ;;
    -l*)
#     Collect a list of -l options to pass to the linker.
      loptions=$loptions" "$1
      shift;
      ;;
    -gn)
#     Enable debugging but don't keep the .int.c file.
      ccsdb=-gx;
      shift;
      ;;
    -g*)
      ccsdb=-gx;
      keep_int_file=1;
      shift;
      ;;
    -munch)
#     Use "munch" for handling static constructors and destructors
      patch_mode=0
      shift
      ;;
    -patch)
#     Use "patch" for handling static constructors and destructors
      patch_mode=1
      shift
      ;;
    -target)
#     SunOS 4.n option, as in "-target sun4" -- ignored.
      shift;
      shift;
      ;;
    -t)
#     Template instantiation mode
      shift;
      fe_options=$feoptions" "$1;
      instantiation_mode_specified=1
      shift;
      ;;
    -t*)
#     Template instantiation mode
      feoptions=$feoptions" "$1;
      instantiation_mode_specified=1
      shift;
      ;;
    -sun*)
#     SunOS 4.n option, as in "-sun4" -- ignored.
      shift;
      ;;
    -*)
      echo "Unknown option: $1";
      error=1;
      shift;
      ;;
    *\.a)
#     Collect a list of library archive names (.a) files.
      lfiles=$lfiles" "$1
      any_l_or_o_files=1
      shift;
      ;;
    *\.c)
#     Collect a list of .c files.
      if [ "$cfiles" ]; then more_than_one_c_file=1; fi;
      cfiles=$cfiles" "$1;
      shift;
      ;;
    *\.C)
#     Collect a list of .C files.
      if [ "$cfiles" ]; then more_than_one_c_file=1; fi;
      cfiles=$cfiles" "$1;
      shift;
      ;;
    *\.o)
#     Collect a list of .o files.
      ofiles=$ofiles" "$1;
      any_l_or_o_files=1
      shift;
      ;;
    *)
      echo "eccp: Unrecognizable argument.";
      shift;
      error=1;
      ;;
  esac
done

if [ $error -eq 1 ]
then
  exit 1
fi
#
# Add the proper include directory.
#
if [ $std_incl -eq 1 ]
then
  if [ $cmode -eq 1 ]
  then
#   Compiling C code.
    feoptions=$feoptions" -I"$CINCLDIR;
  else
#   Compiling C++ code.
    feoptions=$feoptions" -I"$INCLDIR;
  fi
fi
#
# If only one source file was specified, and we are compiling and
# linking (i.e., we know everything for this compilation is in a single
# file) then use the "instantiate used" option.
#
if [ $cmode -eq 0 -a $more_than_one_c_file -eq 0 -a $cc_only -eq 0 -a	\
     $fe_only -eq 0 -a $any_l_or_o_files -eq 0 -a \
     $instantiation_mode_specified -eq 0 ] ; then
  feoptions=$feoptions" -tused"
fi
#
# Run through the list of .c files and compile.
#
any_errors=0
max_status=0
for cfile in $cfiles
do
  case $cfile in
    *\.c)
    basefile=`basename $cfile .c`
      ;;
    *\.C)
    basefile=`basename $cfile .C`
      ;;
  esac
  if [ $more_than_one_c_file -ne 0 ]
  then
    echo $cfile: 1>$2
  fi
  if [ -z "$CPFE" ]
  then
    cpfe $feoptions $cfile
  else
    $CPFE $feoptions $cfile
  fi
  status=$?
#
# If front end successfully compiled the file, pass it to cc.
#
  if [ $status -gt 1 ]
  then
    if [ $status -gt $max_status ]
    then
	max_status=$status
    fi
    any_errors=1
  else
#
#   Execute cc unless explicitly told not to.
#
    if [ $fe_only -ne 1 ]
    then
      cc $ccsdb -c -temp=/usr/tmp $basefile.int.c
      status=$?
      if [ $status -ne 0 ]
      then
        if [ $status -gt $max_status ]
        then
	  max_status=$status
        fi
	any_errors=1
      else
#
#       Add resulting .o file to the list of files to be linked.
#
	ofiles=$ofiles" "$basefile.o
	mv $basefile.int.o $basefile.o
#
#       Add the file to the list of .o files to be removed later.
#
	rofiles=$rofiles" "$basefile.o
	if [ $keep_int_file -eq 0 ]
	then
          rm $basefile.int.c
	fi
      fi
    fi
  fi
done
if [ $any_errors -eq 0 ]
then
#  No compilation errors from front end or cc on any of the files.
  status=0
  if [ $fe_only -ne 1 ]
  then
    if [ $cc_only -ne 1 ]
    then
#     Save the link command in a variable so it can be done again in the
#     "munch" step below.
#     Note:  -lC is missing from this command and is supplied later.
      link_command="cc  $ccsdb $Loptions -L$LIBDIR -o $executable \
                       $ofiles $lfiles $loptions -lstd"
      $link_command -lC
      status=$?
      if [ $status = 0 -a $cmode -eq 0 ]
      then
#       Do processing to handle calling static constructors and destructors.
        if [ $patch_mode = 1 ] ; then
#         Do "patch" processing.
          chmod 664 $executable
          $PATCH $executable
          status=$?
          if [ $status = 0 ]
          then
            chmod 775 $executable
          fi
        else
#
#         Do "munch" processing:
#            1. Run munch on executable to produce C file
#            2. Compile C file
#            3. Re-link with object of C file
#
          tmpfile=/usr/tmp/$$edgm
          nm $executable | $MUNCH >$tmpfile.c
          (cd /usr/tmp; cc -c $tmpfile.c)
          status=$?
          if [ $status -ne 0 ] ; then
            echo "Compilation of file generated by munch failed"
            exit $status
          fi
#         Do the link again.
          $link_command $tmpfile.o -lC
          status=$?
          rm -f $tmpfile.c $tmpfile.o
        fi
      fi
      rm -f $rofiles
    fi
  fi
else
  status=$max_status
fi
exit $status
