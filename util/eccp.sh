#!/bin/sh
# Run the EDG C++ front end into the system cc to compile C++.
# Interface and command-line options are similar to CC.
# CPFE may be set to the executable to use for the C++ front end.
# If CPFE is not set, "cpfe" is used.
#
# Predefined preprocessing variables.
#
defines="-Dsparc -Dunix -Dsun"
EDG_BASE=${EDG_BASE-/edg/cpfe}
EDG_CBASE=${EDG_CBASE-/edg/cfe}
#
# Directory where the C++ include files are to be found.
#
INCLDIR=$EDG_BASE/include
#
# Directory where the C include files are to be found.
#
CINCLDIR=$EDG_CBASE/usr/include
#
# Directory where libC.a is to be found.
#
LIBDIR=${ECCP_LIBDIR-$EDG_BASE/lib}
#
# Compiler executable
#
CPFE=${CPFE-$EDG_BASE/bin/cfe}
#
# "patch" executable
#
PATCH=${EDG_PATCH_PATH-$EDG_BASE/lib/patch}
#
# "munch" executable
#
MUNCH=${EDG_MUNCH_PATH-$EDG_BASE/lib/edg_munch}
#
# "edg_prelink" executable
#
EDG_PRELINK=${EDG_PRELINK_PATH-$EDG_BASE/lib/edg_prelink}
#
# Default options to the prelink command (no default value - use environment
# variable if set)
#
# EDG_PRELINK_DEFAULT_OPTIONS=$EDG_PRELINK_DEFAULT_OPTIONS
#
# Flag indicating whether to use "patch" or "munch" for static initialization.
#
patch_mode=1
#
# Flag indicating whether to do automatic instantiation by default
#
automatic_instantiation=1
#
# Other variables used in automatic instantiation mode
#
if [ $automatic_instantiation -eq 1 ] ; then
  compile_command=$0
  instantiation_libraries="$LIBDIR/libC.a"
fi
#
# Suffix to be applied to the standard C++ library names to select a
# special version.  The names with no suffix are libC.a and libstd.a.
#
EDG_LIB_SUFFIX=${EDG_LIB_SUFFIX-" "}
#
# C compiler to use to compile the output and any options to be used with
# this compiler by default.
#
EDG_C_TO_OBJ_COMPILER=${EDG_C_TO_OBJ_COMPILER-cc}
EDG_C_TO_OBJ_DEFAULT_OPTIONS=${EDG_C_TO_OBJ_DEFAULT_OPTIONS--temp=/usr/tmp}
cc_command="$EDG_C_TO_OBJ_COMPILER $EDG_C_TO_OBJ_DEFAULT_OPTIONS"
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
# When set to 1, the front end is run in preprocessing mode only.
# Among other things, the front end will not create or remove the .ii
# file when only doing preprocessing
#
preprocess_only=0
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
# Were any source files specified on the command line?
#
any_c_files=0
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
# Generate position independent code
#
ccpic=
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
  add_to_instantiation_command=1
  curr_param=$1
  used_two_params=0
  case $1 in
    -O)
#     Allow anachronisms
      feoptions=$feoptions" -O";
      ;;
    -b)
#     cfront compatibility
      feoptions=$feoptions" -b";
      ;;
    -d)
#     Debug information.
      shift;
      feoptions=$feoptions" -d"$1;
      used_two_params=1
      ;;
    -d*)
      feoptions=$feoptions" "$1;
      ;;
    -e)
#     Set error limit.
      shift;
      feoptions=$feoptions" -e"$1;
      used_two_params=1
      ;;
    -e*)
      feoptions=$feoptions" "$1;
      ;;
    -S)
#     Run front end only.
      fe_only=1;
      ;;
    -n)
#     Run front end only, suppress output of .int.c file.
      fe_only=1;
      feoptions=$feoptions" -n";
      ;;
    -N)
#     Suppress IL lowering.  This will ultimately suppress output of a .int.c
#     file.
      fe_only=1;
      feoptions=$feoptions" -N";
      ;;
    -C)
#     Keep comments in preprocessing output.
      feoptions=$feoptions" -C";
      ;;
    -c)
#     Run front end and cc producing a .o file.
      cc_only=1;
      add_to_instantiation_command=0
      ;;
    -command)
#     The command name to be used in the .ii file in place of what is
#     found in argument 0.
      shift
      compile_command=$1
      used_two_params=1
      add_to_instantiation_command=0
      ;;
    -o)
#     Explicitly name the executable.
      shift;
      used_two_params=1
      executable=$1;
      add_to_instantiation_command=0
      ;;
    -w)
#     Suppress warnings.
      feoptions=$feoptions" -w";
      ;;
    -r)
#     Enable remarks.
      feoptions=$feoptions" -r";
      ;;
    -A|-a)
#     Strict ANSI mode.
      feoptions=$feoptions" $1";
      ;;
    -K)
#     cpp compatible mode.
      feoptions=$feoptions" -K";
      cmode=1;
      ;;
    -s)
#     Signed chars.
      feoptions=$feoptions" -s";
      ;;
    -u)
#     Unsigned chars.
      feoptions=$feoptions" -u";
      ;;
    -V)
#     Suppress virtual table definition if no non-inline, pure virtual
#     function exists.
      feoptions=$feoptions" -V";
      ;;
    -x)
#     Disable support for exception handling.
      feoptions=$feoptions" -x";
      ;;
    -v)
#     Verbose mode; display version of front end.
      feoptions=$feoptions" -v";
      ;;
    -E)
#     Preprocessor only.
      fe_only=1;
      preprocessor_only=1
      feoptions=$feoptions" -E";
      ;;
    -P)
#     Preprocessor only.
      fe_only=1;
      preprocessor_only=1
      feoptions=$feoptions" -P";
      ;;
    -M)
#     Generate makefile dependency lines.
      fe_only=1;
      preprocessor_only=1
      feoptions=$feoptions" -M";
      ;;
    -H)
#     Generate names of include files used.
      fe_only=1;
      preprocessor_only=1
      feoptions=$feoptions" -H";
      ;;
    -I)
#     Collect a list of -I options.
      shift;
      feoptions=$feoptions" -I"$1;
      used_two_params=1
      ;;
    -I*)
      feoptions=$feoptions" "$1;
      ;;
    -h)
#     Suppress standard include directory.
      std_incl=0;
      ;;
    -X)
#     Collect a list of -X options.
      shift;
      feoptions=$feoptions" -X"$1;
      used_two_params=1
      ;;
    -X*)
      feoptions=$feoptions" "$1;
      ;;
    -i)
#     Collect a list of -i options.
      shift;
      feoptions=$feoptions" -i"$1;
      used_two_params=1
      ;;
    -i*)
      feoptions=$feoptions" "$1;
      ;;
    -D)
#     Collect a list of -D options.
      shift;
      feoptions=$feoptions" -D"$1;
      used_two_params=1
      ;;
    -D*)
      feoptions=$feoptions" "$1;
      ;;
    -U)
#     Collect a list of -U options.
      shift;
      feoptions=$feoptions" -U"$1;
      used_two_params=1
      ;;
    -U*)
      feoptions=$feoptions" "$1;
      ;;
    -L)
#     Collect a list of -L options to pass to the linker.
      shift;
      Loptions=$Loptions" -L"$1;
      used_two_params=1
      ;;
    -L*)
#     Collect a list of -L options to pass to the linker.
      Loptions=$Loptions" "$1
      ;;
    -m)
#     Process C instead of C++.
      feoptions=$feoptions" "$1;
      cmode=1;
      ;;
    -l*)
#     Collect a list of -l options to pass to the linker.
      loptions=$loptions" "$1
      ;;
    -gn)
#     Enable debugging but don't keep the .int.c file.
      ccsdb=-g;
      ;;
    -g*)
      ccsdb=-g;
      keep_int_file=1;
      ;;
    -munch)
#     Use "munch" for handling static constructors and destructors
      patch_mode=0
      ;;
    -patch)
#     Use "patch" for handling static constructors and destructors
      patch_mode=1
      ;;
    -pic)
#     Generate position independent code
      ccpic=-pic
      ;;
    -target)
#     SunOS 4.n option, as in "-target sun4" -- ignored.
      shift;
      used_two_params=1
      ;;
    -t)
#     Template instantiation mode
      shift;
      feoptions=$feoptions" "$1;
      instantiation_mode_specified=1
      used_two_params=1
      ;;
    -t*)
#     Template instantiation mode
      feoptions=$feoptions" "$1;
      instantiation_mode_specified=1
      ;;
    -T)
#     Supress automatic template instantiation processing
      feoptions=$feoptions" "$1;
      ;;
    -B)
#     Enable or disable implicit inclusion of template instantiation
#     source files (depending on how the front end is configured)
      feoptions=$feoptions" "$1;
      ;;
    -sun*)
#     SunOS 4.n option, as in "-sun4" -- ignored.
      ;;
    -*)
      echo "eccp: unknown option: $1";
      error=1;
      add_to_instantiation_command=0
      ;;
    *\.a)
#     Collect a list of library archive names (.a) files.
      lfiles=$lfiles" "$1
      any_l_or_o_files=1
      add_to_instantiation_command=0
      ;;
    *\.so | *\.so.*)
#     Collect a list of library shared object names (.so) files.
      lfiles=$lfiles" "$1
      any_l_or_o_files=1
      add_to_instantiation_command=0
      ;;
    *\.c | *\.C | *\.cc | *\.cpp | *\.CPP | *\.cxx | *\.CXX)
#     Collect a list of .c files.
      if [ "$cfiles" ]; then more_than_one_c_file=1; fi;
      cfiles=$cfiles" "$1;
      any_c_files=1
      add_to_instantiation_command=0
      ;;
    *\.o)
#     Collect a list of .o files.
      ofiles=$ofiles" "$1;
      any_l_or_o_files=1
      add_to_instantiation_command=0
      ;;
    *)
      echo "eccp: unrecognizable argument";
      error=1;
      ;;
  esac
  if [ $automatic_instantiation -eq 1 -a \
       $add_to_instantiation_command -eq 1 ] ; then
    # In automatic instantiation mode build a version of the command line
    # that can be used to compile one file.  This is mostly like the
    # original command without any file names and without certain linker
    # options.
    instantiation_command_line=$instantiation_command_line" "$curr_param
    if [ $used_two_params -eq 1 ] ; then
      # The option took an argument -- append the argument.
      instantiation_command_line=$instantiation_command_line" "$1
    fi
  fi
  shift;
done

if [ $any_l_or_o_files -eq 0 -a $any_c_files -eq 0 ] ; then
  echo "eccp: no source, object, or library files were specified"
  error=1
fi

# Put the command name on the beginning of the instantiation command line.
# This is done here because a different name can be supplied on the
# command line.  This is useful when the compile command is a script that
# sets some environment variables before invoking this script.
if [ $automatic_instantiation -eq 1 ] ; then
  instantiation_command_line="$compile_command -c"$instantiation_command_line
fi

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
    *\.cc)
    basefile=`basename $cfile .cc`
      ;;
    *\.cpp)
    basefile=`basename $cfile .cpp`
      ;;
    *\.CPP)
    basefile=`basename $cfile .CPP`
      ;;
    *\.cxx)
    basefile=`basename $cfile .cxx`
      ;;
    *\.CXX)
    basefile=`basename $cfile .CXX`
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
  # If we are doing automatic instantiation and if the program involves
  # templates then the a .ii file will exist after the compilation.
  # If a .ii file exists that means that the compilation used templates in
  # some way.  Generate a new .ii file using the current command line.
  #
  if [ $automatic_instantiation -ne 0 ] ; then
    ii_file_name=$basefile.ii
    if [ -f $ii_file_name ] ; then
      # An instantiation file exists which means the compilation involves
      # templates.  Construct the new .ii file.
      ii_tmp_file=/usr/tmp/$$edgII
      sed -e "1,1 d" $ii_file_name >$ii_tmp_file
      echo $instantiation_command_line $cfile >$ii_file_name
      cat $ii_tmp_file >>$ii_file_name
      rm -f $ii_tmp_file
    fi
  fi
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
      $cc_command $ccsdb $ccpic -c $basefile.int.c
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
#
#     If automatic instantiation is enabled, run the prelink phase to
#     determine if any additional instantiations need to be generated
#     or if any existing instantiations are no longer needed.
#     If any instantiation list files are changed edg_prelink will
#     do the necessary recompilations.  When edg_prelink exits
#     the files will have been compiled and the necessary instantiations
#     generated.  Any instantiations that edg_prelink could not find
#     a way to generate will cause linker errors to be issued later.
#
      if [ $automatic_instantiation -ne 0 ] ; then
        $EDG_PRELINK $EDG_PRELINK_DEFAULT_OPTIONS $ofiles $lfiles \
                     $instantiation_libraries
      fi
#     Save the link command in a variable so it can be done again in the
#     "munch" step below.
#     Note:  -lC is missing from this command and is supplied later.
      link_command="$cc_command $ccsdb $Loptions -L$LIBDIR -o $executable \
                       $ofiles $lfiles $loptions -lstd$EDG_LIB_SUFFIX \
		       $EDG_C_TO_OBJ_LIBRARIES"
      $link_command -lC$EDG_LIB_SUFFIX
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
          (cd /usr/tmp; $cc_command -c $tmpfile.c)
          status=$?
          if [ $status -ne 0 ] ; then
            echo "eccp: compilation of file generated by munch failed"
            exit $status
          fi
#         Do the link again.
          $link_command $tmpfile.o -lC$EDG_LIB_SUFFIX
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
