#!/bin/sh
# Run the EDG C++ front end into the system cc to compile C++.
# Interface and command-line options are similar to CC.
# CPFE may be set to the executable to use for the C++ front end.
#
# Predefined preprocessing variables.
#
defines=${EDG_DEFAULT_DEFINES-"-Dsparc -Dunix -Dsun"}
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
# Flag indicating whether to use "patch" or "munch" for static initialization.
#
patch_mode=${EDG_PATCH_MODE-1}
#
# Options to be passed to the nm command when using munch
#
EDG_MUNCH_NM_OPTIONS=${EDG_MUNCH_NM_OPTIONS-""}
#
# "edg_prelink" executable
#
EDG_PRELINK=${EDG_PRELINK_PATH-$EDG_BASE/lib/edg_prelink}
#
# Default library paths of C to object compiler.  Used by the prelinker
# to find libraries specified with the -l option.
#
EDG_DEFAULT_LIB_PATHS=${EDG_DEFAULT_LIB_PATHS-"-L/lib -L/usr/lib"}
#
# Default options to the prelink command (no default value - use environment
# variable if set)
#
# EDG_PRELINK_DEFAULT_OPTIONS=$EDG_PRELINK_DEFAULT_OPTIONS
#
# edg_decode (demangler) executable.  If no edg_decode is available,
# use /bin/cat.  The script will work properly, you just won't get
# demangled names in linker output messages.
#
EDG_DECODE=${EDG_DECODE_PATH-$EDG_BASE/lib/edg_decode}
#
# Flag indicating whether to do automatic instantiation by default
#
automatic_instantiation=1
#
# Directory to be used for temporary files.
#
export TMPDIR
TMPDIR=${TMPDIR-/tmp}
#
# Suffix to be applied to the standard C++ library (libC.a) to select a
# special version.
#
EDG_LIB_SUFFIX=${EDG_LIB_SUFFIX-""}
#
# Library names to be used on the link command
#
EDG_STD_LIBS=${EDG_STD_LIBS-"-lstd"}
#
# C compiler to use to compile the output and any options to be used with
# this compiler by default.
#
EDG_C_TO_OBJ_COMPILER=${EDG_C_TO_OBJ_COMPILER-cc}
EDG_C_TO_OBJ_DEFAULT_OPTIONS=${EDG_C_TO_OBJ_DEFAULT_OPTIONS--temp=$TMPDIR}
cc_command="$EDG_C_TO_OBJ_COMPILER $EDG_C_TO_OBJ_DEFAULT_OPTIONS"
#
# Flag that indicates that the generated C file should always be created
# in the current directory.  This provides compatibility with earlier
# versions of the front and that don't support then --gen_c_file_name
# option.
#
gen_c_in_curr_dir=${EDG_GEN_C_IN_CURR_DIR-0}
#
# Other variables used in automatic instantiation mode
#
if [ $automatic_instantiation -eq 1 ] ; then
  compile_command=$0
  instantiation_libraries="$LIBDIR/libC$EDG_LIB_SUFFIX.a"
fi
#
# Flag that indicates that the old instantiation information file
# format (without the current directory) should be used.
#
old_ii_format=${EDG_OLD_II_FORMAT-0}
#
# The suffix to be used on the generated C file and generate .o files.
#
gen_c_suffix=${EDG_GEN_C_SUFFIX-".int.c"}
gen_o_suffix=`expr $gen_c_suffix : '\(.*\)\.'`.o
#
# Default options to be passed to edgcpfe
#
EDG_CPFE_DEFAULT_OPTIONS=${EDG_CPFE_DEFAULT_OPTIONS-""}
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
preprocessor_only=0
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
# Other options to be passed to the linker
#
ldoptions=
#
# Options to be passed to the underlying C compiler
#
c_to_obj_options=
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
# If the generated C file suffix does not end with .c, then tell the front end
# to generate old style line commands.
#
if [ `basename $gen_c_suffix .c` = $gen_c_suffix ] ; then
        # This is true when the suffix does not end in .c
	feoptions=$feoptions" --old_line_commands"
fi
#
# Keep the generated C file
#
keep_int_file=0
#
# Should we link using the purify command?
#
link_using_purify=0
#
# Should we link using the quantify command?
#
link_using_quantify=0
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
# Remove #line directives from the generated C
#
strip_line_dirs=0
#
# Suppress diagnostics from the underlying C compiler
#
suppress_c_to_object_diagnostics=0
#
# Options to be passed to the prelinker
#
prelink_options=
#
# Prelinker instantiation options.  prelink_local_only indicates
# that only local files (i.e., those in the current directory) are
# candidates for assignment of instantiations.  prelink_copy_if_nonlocal
# is TRUE if the assignment of an instantiation to a nonlocal object file
# should result in the object file being recompiled in the current
# directory.
#
prelink_local_only=0
prelink_copy_if_nonlocal=0
#
# Show commands as they are executed
#
driver_debug=0
#
# Special option for testing precompiled headers
#
pch_test_mode=0
#
# Go through every argument, identify it, and add it a list if appropriate.
#
while [ -n "$1" ]
do
  add_to_instantiation_command=1
  curr_param=$1
  used_two_params=0
  case $1 in
###############################################################################
# Options used by the driver
###############################################################################
    --driver_debug)
#     Show commands as they are executed.
      driver_debug=1
      ;;
    -O | --optimize)
#     Generate optimized code
      c_to_obj_options=$c_to_obj_options" -O";
      ;;
    -O*)
#     Generate optimized code (e.g., -O2)
      c_to_obj_options=$c_to_obj_options" $1";
      ;;
    --optimize=*)
#     Generate optimized code (e.g., --optimize=2)
      arg=`expr $1 : '.*=\(.*\)'`    # Get the string after the =
      c_to_obj_options=$c_to_obj_options" -O$arg";
      ;;
    -S | --cpfe_only)
#     Run front end only.
      fe_only=1;
      keep_int_file=1;
      ;;
    -c | --compile)
#     Run front end and cc producing a .o file.
      cc_only=1;
      add_to_instantiation_command=0
      ;;
    -command | --command)
#     The command name to be used in the .ii file in place of what is
#     found in argument 0.
      shift
      compile_command=$1
      used_two_params=1
      add_to_instantiation_command=0
      ;;
    -o | --output)
#     Explicitly name the executable.
      shift;
      used_two_params=1
      executable=$1;
      add_to_instantiation_command=0
      ;;
    --output=*)
#     Explicitly name the executable.
      executable=`expr $1 : '.*=\(.*\)'`    # Get the string after the =
      add_to_instantiation_command=0
      ;;
    -L | --library_directory)
#     Collect a list of -L options to pass to the linker.
      shift;
      Loptions=$Loptions" -L"$1;
      used_two_params=1
      ;;
    -L*)
#     Collect a list of -L options to pass to the linker.
      Loptions=$Loptions" "$1
      ;;
    --library_directory=*)
#     Collect a list of -L options to pass to the linker.
      arg=`expr $1 : '.*=\(.*\)'`    # Get the string after the =
      Loptions=$Loptions" "-L$arg
      ;;
    -l*)
#     Collect a list of -l options to pass to the linker.
      loptions=$loptions" "$1
      any_l_or_o_files=1
      ;;
    -g*)
#     Generate debugging information
      c_to_obj_options="$c_to_obj_options $1"
      ;;
    --debug)
#     Generate debugging information
      c_to_obj_options="$c_to_obj_options -g"
      ;;
    --debug=*)
#     Generate debugging information
      arg=`expr $1 : '.*=\(.*\)'`    # Get the string after the =
      c_to_obj_options="$c_to_obj_options -g$arg"
      ;;
    -h | --no_standard_includes)
#     Suppress standard include directory.
      std_incl=0
      ;;
    -k | --keep_gen_c)
#     Keep generated C file
      keep_int_file=1;
      ;;
    -munch | --munch)
#     Use "munch" for handling static constructors and destructors
      patch_mode=0
      ;;
    -patch | --patch)
#     Use "patch" for handling static constructors and destructors
      patch_mode=1
      ;;
    -pic | --pic)
#     Generate position independent code
      c_to_obj_options="$c_to_obj_options -pic"
      ;;
    -target)
#     SunOS 4.n option, as in "-target sun4" -- ignored.
      shift;
      used_two_params=1
      ;;
    -Bstatic)
#     Pass through to linker.
      ldoptions=$ldoptions" "$1
      ;;
    -purify | --purify)
#     Link using the purify command
      link_using_purify=1
      ;;
    --quantify)
#     Link using the quantify command
      link_using_quantify=1
      ;;
    -strip_line_dirs | --strip_line_dirs)
#     Remove #line directives from generated C
      strip_line_dirs=1
      ;;
    -suppress_c_to_obj_diagnostics | --suppress_c_to_obj_diagnostics)
#     Suppress diagnostics from the underlying C compiler
      suppress_c_to_object_diagnostics=1
      ;;
    --prelink_local_only)
#     Only files compiled in the current directory may have instantiations
#     assigned to them
      prelink_local_only=1
      ;;
    --prelink_copy_if_nonlocal)
#     If an assignment is done to a nonlocal object file, create a local
#     copy.
      prelink_copy_if_nonlocal=1
      ;;
    -sun*)
#     SunOS 4.n option, as in "-sun4" -- ignored.
      ;;
    --pch_test_mode)
#     Special option for testing precompiled headers
      pch_test_mode=1
      ;;
    --old_ii_format)
#     Use the old .ii file format that does not include the current directory
      old_ii_format=1
      ;;
    *\.a)
#     Collect a list of library archive names (.a) files.
      lfiles=$lfiles" "$1
      any_l_or_o_files=1
      add_to_instantiation_command=0
      ;;
    *\.so | *\.so\.*)
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
###############################################################################
# Options passed to the front end that take no arguments
###############################################################################
    -a |--strict_warnings | \
    -b | --cfront_2.1 | \
         --cfront_3.0 | \
    -j | --no_use_before_set_warnings | \
    -m | --c | \
    -p | --c++ | \
    -r | --remarks | \
    -s | --signed_chars | \
    -u | --unsigned_chars | \
    -v | --version | \
    -w | --no_warnings | \
    -x | --exceptions | \
         --no_exceptions | \
    -A | --strict | \
    -B | --implicit_include | \
         --no_implicit_include | \
    -C | --comments | \
    -K | --old_c | \
    -T | --auto_instantiation | \
         --no_auto_instantiation | \
    -V | --suppress_vtbl | \
         --anachronisms | \
         --no_anachronisms | \
    -# | --timing | \
         --display_error_number | \
	 --old_line_commands | \
	 --microsoft | \
	 --no_microsoft | \
	 --long_lifetime_temps | \
	 --short_lifetime_temps | \
         --force_vtbl)
      feoptions=$feoptions" $1"
#     Check for C or C++ mode
      case $curr_param in
        -m | --c | -K | --old_c)
         cmode=1
         ;;
        -p | --c++)
         cmode=0
         ;;
      esac
      ;;
###############################################################################
# Options passed to the front end that take no arguments and must appear at
# the beginning of the option list passed to the front end.
###############################################################################
         --create_pch | \
         --pch | \
         --pch_messages | \
         --no_pch_messages)
      feoptions=$1" $feoptions"
      ;;
###############################################################################
# Front end options with no arguments that suppress code generation.
###############################################################################
    -n | --no_code_gen | \
    -N | --no_il_lowering)
      feoptions=$feoptions" $1";
      fe_only=1
      ;;
###############################################################################
# Front end options with the argument specified in the following argument.
# For example, -d 5 or --db 5 look like two arguments to the shell.
###############################################################################
    -d | --db | \
    -e | --error_limit | \
    -i | --module_init | \
    -t | --instantiate | \
    -D | --define_macro | \
    -I | --include_directory | \
    -U | --undefine_macro | \
    -X | --xref | \
         --list | \
         --error_output | \
         --diag_suppress | \
         --diag_remark | \
         --diag_warning | \
         --diag_error)
      feoptions=$feoptions" $1 $2"
      shift
      used_two_params=1
#     See if an instantiation mode was specified
      case $curr_param in
        -t | --instantiate)
          instantiation_mode_specified=1
      esac
      ;;
###############################################################################
# Same as above, except these options must appear at the beginning of the
# command line passed to the front end.
###############################################################################
         --pch_mem | \
         --pch_dir | \
         --use_pch)
      feoptions=$1" $2 $feoptions"
      shift
      used_two_params=1
      ;;
###############################################################################
# Front end options with the argument included as part of the option argument.
# For example, -d5 or --db=5 look like a single argument to the shell.
###############################################################################
    -d* | --db=* | \
    -e* | --error_limit=* | \
    -i* | --module_init=* | \
    -t* | --instantiate=* | \
    -D* | --define_macro=* | \
    -I* | --include_directory=* | \
    -U* | --undefine_macro=* | \
    -X* | --xref=* | \
          --list=* | \
          --error_output=* | \
          --diag_suppress=* | \
          --diag_remark=* | \
          --diag_warning=* | \
          --diag_error=*)
      feoptions=$feoptions" $1"
#     See if an instantiation mode was specified
      case $curr_param in
        -t* | --instantiate=*)
          instantiation_mode_specified=1
      esac
      ;;
###############################################################################
# Same as above, except these options must appear at the beginning of the
# command line passed to the front end.
###############################################################################
         --pch_mem=* | \
         --pch_dir=* | \
         --use_pch=*)
      feoptions=$1" $feoptions"
      ;;
###############################################################################
# Preprocessing options
###############################################################################
    -E | --preprocess | \
    -H | --trace_includes | \
    -M | --dependencies | \
    -P | --no_line_commands)
      fe_only=1;
      preprocessor_only=1
      feoptions=$feoptions" $1";
      ;;
###############################################################################
    -*)
      echo "eccp: unknown option: $1";
      error=1;
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

# If we are in C mode then disable automatic instantiation just for
# efficiency.
if [ $cmode -eq 1 ] ; then
  automatic_instantiation=0
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
# The old .ii format cannot be used with the new prelinker nonlocal file
# options.
#
if [ $old_ii_format -ne 0 ] ; then
  if [ $prelink_local_only -ne 0 -o $prelink_copy_if_nonlocal -ne 0 ] ; then
    echo eccp: old .ii format may not be used with nonlocal prelinker options
    exit 1
  fi
fi
#
# Convert --prelink_local_only to the appropriate prelinker option.
#
if [ $prelink_local_only -ne 0 ] ; then
  prelink_options=$prelink_options" -D"
fi
#
# Convert --prelink_copy_if_nonlocal to the appropriate prelinker option.
#
if [ $prelink_copy_if_nonlocal -ne 0 ] ; then
  new_obj_list_file=$TMPDIR/nolf$$
  prelink_options=$prelink_options" -N $new_obj_list_file"
fi
#
# If we should use the old .ii file format, update the prelinker default
# options.
#
if [ $old_ii_format -ne 0 ] ; then
  prelink_options=$prelink_options" -R1"  
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
    echo "$cfile:" 1>&2
  fi
  if [ $keep_int_file -eq 1 -o $gen_c_in_curr_dir -eq 1 ] ; then
    gen_c_file_name=$basefile$gen_c_suffix
    gen_c_obj_name=$basefile$gen_o_suffix
    if [ $gen_c_suffix != ".int.c" ] ; then
      feoptions=$feoptions" "--gen_c_file_name=$gen_c_file_name
    fi
  else
    gen_c_file_name=$TMPDIR/$basefile.$$""$gen_c_suffix
    gen_c_obj_name=$basefile.$$""$gen_o_suffix
    feoptions=$feoptions" "--gen_c_file_name=$gen_c_file_name
  fi
  command=${CPFE}" "$feoptions" "$EDG_CPFE_DEFAULT_OPTIONS" "$cfile
  if [ $driver_debug -ne 0 ] ; then
    echo $command
  fi
  if [ $pch_test_mode -eq 1 ] ; then
    # In PCH test mode, we immediately repeat the same compilation.
    # The first compilation should generate a PCH file, the second should
    # use the generated file.  The output of the first compilation is
    # discarded.
    $command >/dev/null 2>&1
    $command
    status=$?
    rm -f *.pch
  else
    # Normal mode, just run the front end.
    $command
    status=$?
  fi
  #
  # If we are doing automatic instantiation and if the program involves
  # templates then the a .ii file will exist after the compilation.
  # If a .ii file exists that means that the compilation used templates in
  # some way.  Generate a new .ii file using the current command line.
  # The front end only generates the .ii file when the back end is run
  # (i.e., no "fe-only" options were specified and no errors occurred.
  #
  if [ $automatic_instantiation -ne 0 -a $preprocessor_only -eq 0 \
       -a $fe_only -eq 0 -a $status -eq 0 ] ; then
    ii_file_name=$basefile.ii
    if [ -f $ii_file_name ] ; then
      # An instantiation file exists which means the compilation involves
      # templates.  Construct the new .ii file.
      ii_tmp_file=$TMPDIR/$$edgII
      if [ $old_ii_format -ne 1 ] ; then
#       New format
        sed -e "1,3 d" $ii_file_name >$ii_tmp_file
        echo $instantiation_command_line >$ii_file_name
        pwd >>$ii_file_name
	echo $cfile >>$ii_file_name
      else
#       Old format
        sed -e "1,1 d" $ii_file_name >$ii_tmp_file
        echo $instantiation_command_line $cfile >$ii_file_name
      fi
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
    cc_tmp_file=$TMPDIR/$$cc
    if [ $fe_only -ne 1 ]
    then
#
# Remove #line directives if requested to do so.
#
      if [ $strip_line_dirs -eq 1 ] ; then
        # Replace the #line directives with blank lines.
        sed -e "s/#line.*//" $gen_c_file_name >/tmp/$$sld
        mv -f /tmp/$$sld $gen_c_file_name
      fi
      command="$cc_command $c_to_obj_options -c $gen_c_file_name"
      if [ $driver_debug -ne 0 ] ; then
        echo $command
      fi
      $command >$cc_tmp_file 2>&1
      status=$?
#
#     Display any diagnostics generated by the C compiler
#
      if [ -s $cc_tmp_file -a $suppress_c_to_object_diagnostics -eq 0 ] ; then
        echo eccp: diagnostics generated from compilation of $basefile$gen_c_suffix: >&2
        cat $cc_tmp_file >&2
        echo eccp: end of diagnostics from compilation of $basefile$gen_c_suffix >&2
      fi
      rm -f $cc_tmp_file
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
	command="mv -f $gen_c_obj_name $basefile.o"
        if [ $driver_debug -ne 0 ] ; then
          echo $command
        fi
        $command
#
#       Add the file to the list of .o files to be removed later.
#
	rofiles=$rofiles" "$basefile.o
      fi
    fi
  fi
#
#     Remove the .int.c file.
#
  if [ $keep_int_file -eq 0 ]
  then
    rm -f $gen_c_file_name
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
        command="$EDG_PRELINK $EDG_PRELINK_DEFAULT_OPTIONS \
                     $prelink_options \
		     $Loptions -L$LIBDIR \
                     $EDG_DEFAULT_LIB_PATHS \
		     $loptions $ofiles $lfiles \
                     $instantiation_libraries"
        if [ $driver_debug -ne 0 ] ; then
          echo $command
        fi
        eval $command
      fi
#
#     When the --prelink_copy_if_nonlocal option is used, the prelinker outputs
#     an updated list of object files.  Replace the original ofiles list with
#     the updated one.
#
      if [ $prelink_copy_if_nonlocal -ne 0 ] ; then
        new_list=`cat $new_obj_list_file`
        rm -f $new_obj_list_file
        if [ $driver_debug -ne 0 ] ; then
          if [ "$ofiles" != "$new_obj_list_file" ] ; then
            echo Updating object file list
            echo "  old list: $ofiles"
            echo "  new list: $new_list"
          fi
        fi
        ofiles="$new_list"
      fi
#     Save the link command in a variable so it can be done again in the
#     "munch" step below.
#     Note:  -lC is missing from this command and is supplied later using
#     the variable link_command_suffix.
      link_command="$cc_command $c_to_obj_options $Loptions -L$LIBDIR \
                       $ldoptions -o $executable \
                       $ofiles $lfiles $loptions $EDG_STD_LIBS \
		       $EDG_C_TO_OBJ_LIBRARIES"
      link_command_suffix=" -lC$EDG_LIB_SUFFIX"
      if [ $link_using_purify -eq 1 ] ; then
        link_command="purify $link_command"
      fi
      if [ $link_using_quantify -eq 1 ] ; then
        link_command="quantify $link_command"
      fi
#
#     Link the executable.  The linker output is saved to a file and then
#     fed through edg_decode to demangle the names.
#
      if [ $driver_debug -ne 0 ] ; then
        echo $link_command $link_command_suffix
      fi
      link_error_file=$TMPDIR/eccperr$$
      $link_command $link_command_suffix >$link_error_file 2>&1
      status=$?
      $EDG_DECODE <$link_error_file 1>&2
      if [ $status = 0 -a $cmode -eq 0 ]
      then
#       Do processing to handle calling static constructors and destructors.
        if [ $patch_mode = 1 ] ; then
#         Do "patch" processing.
          chmod 664 $executable
          command="$PATCH $executable"
          if [ $driver_debug -ne 0 ] ; then
            echo $command
          fi
          $command
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
          tmpfile=$TMPDIR/$$edgm
          command="nm $EDG_MUNCH_NM_OPTIONS $executable | $MUNCH"
          if [ $driver_debug -ne 0 ] ; then
            echo $command
          fi
          eval $command >$tmpfile.c
          command="$cc_command $c_to_obj_options -c $tmpfile.c"
          if [ $driver_debug -ne 0 ] ; then
            echo $command
          fi
          (cd $TMPDIR; $command)
          status=$?
          if [ $status -ne 0 ] ; then
            echo "eccp: compilation of file generated by munch failed"
            exit $status
          fi
#         Do the link again.
          command="$link_command $tmpfile.o $link_command_suffix"
          if [ $driver_debug -ne 0 ] ; then
            echo $command
          fi
          $command >$link_error_file 2>&1
          status=$?
          $EDG_DECODE <$link_error_file 1>&2
          rm -f $tmpfile.c $tmpfile.o
        fi
      fi
      if [ "$rofiles" != "" ] ; then
        rm -f $rofiles
      fi
      rm -f $link_error_file
    fi
  fi
else
  status=$max_status
fi
exit $status
