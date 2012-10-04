/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2001-2012 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
/*

extasm.c -- Scanning and validation of GNU extended asm() statements.

*/

/* Header files common to all files. */
#include "fe_common.h"

#ifdef PCH_PRAGMA_GUARD
/* Mark the end of the sequence of headers subject to precompiled header
   processing. */
#pragma hdrstop
#endif /* ifdef PCH_PRAGMA_GUARD */

#if GNU_EXTENSIONS_ALLOWED

#include "extasm.h"

/* Header files used by files involved in declaration processing. */
#include "decl_hdrs.h"


struct name_to_reg {
  /* Structure to hold a name-to-register mapping entry. */
  char              *name;
  a_named_register  reg;
};

/*
Extra named registers, in addition to the canonical names in il_def.h.
*/
static struct name_to_reg extra_reg_names[] = {
#if GNU_X86_ASM_EXTENSIONS_ALLOWED
  /* x86 integer registers, common set, other possible names... */
  { "al",  (a_named_register)anr_a  },
  { "bl",  (a_named_register)anr_b  },
  { "cl",  (a_named_register)anr_c  },
  { "dl",  (a_named_register)anr_d  },
  { "ah",  (a_named_register)anr_a  },
  { "bh",  (a_named_register)anr_b  },
  { "ch",  (a_named_register)anr_c  },
  { "dh",  (a_named_register)anr_d  },
  { "eax", (a_named_register)anr_a  },
  { "ebx", (a_named_register)anr_b  },
  { "ecx", (a_named_register)anr_c  },
  { "edx", (a_named_register)anr_d  },
  { "esi", (a_named_register)anr_si },
  { "edi", (a_named_register)anr_di },
  { "ebp", (a_named_register)anr_bp },
  { "esp", (a_named_register)anr_sp },
  { "cc",  (a_named_register)anr_flags },

  /* x86-64 additional integer registers, other possible names... */
  { "rax", (a_named_register)anr_a  },
  { "rbx", (a_named_register)anr_b  },
  { "rcx", (a_named_register)anr_c  },
  { "rdx", (a_named_register)anr_d  },
  { "rsi", (a_named_register)anr_si },
  { "rdi", (a_named_register)anr_di },
  { "rbp", (a_named_register)anr_bp },
  { "rsp", (a_named_register)anr_sp },
  { "sil", (a_named_register)anr_si },
  { "dil", (a_named_register)anr_di },
  { "bpl", (a_named_register)anr_bp },
  { "spl", (a_named_register)anr_sp },
  { "r8b",  (a_named_register)anr_r8  },
  { "r8w",  (a_named_register)anr_r8  },
  { "r8d",  (a_named_register)anr_r8  },
  { "r9b",  (a_named_register)anr_r9  },
  { "r9w",  (a_named_register)anr_r9  },
  { "r9d",  (a_named_register)anr_r9  },
  { "r10b", (a_named_register)anr_r10 },
  { "r10w", (a_named_register)anr_r10 },
  { "r10d", (a_named_register)anr_r10 },
  { "r11b", (a_named_register)anr_r11 },
  { "r11w", (a_named_register)anr_r11 },
  { "r11d", (a_named_register)anr_r11 },
  { "r12b", (a_named_register)anr_r12 },
  { "r12w", (a_named_register)anr_r12 },
  { "r12d", (a_named_register)anr_r12 },
  { "r13b", (a_named_register)anr_r13 },
  { "r13w", (a_named_register)anr_r13 },
  { "r13d", (a_named_register)anr_r13 },
  { "r14b", (a_named_register)anr_r14 },
  { "r14w", (a_named_register)anr_r14 },
  { "r14d", (a_named_register)anr_r14 },
  { "r15b", (a_named_register)anr_r15 },
  { "r15w", (a_named_register)anr_r15 },
  { "r15d", (a_named_register)anr_r15 },

  /* 80387 floating point registers, other possible names... */
  { "st1", (a_named_register)anr_st1 },
  { "st2", (a_named_register)anr_st2 },
  { "st3", (a_named_register)anr_st3 },
  { "st4", (a_named_register)anr_st4 },
  { "st5", (a_named_register)anr_st5 },
  { "st6", (a_named_register)anr_st6 },
  { "st7", (a_named_register)anr_st7 },

  /* numeric register names... */
  { "0",   (a_named_register)anr_a  },
  { "1",   (a_named_register)anr_d  },
  { "2",   (a_named_register)anr_c  },
  { "3",   (a_named_register)anr_b  },
  { "4",   (a_named_register)anr_si },
  { "5",   (a_named_register)anr_di },
  { "6",   (a_named_register)anr_bp },
  { "7",   (a_named_register)anr_sp },
  { "8",   (a_named_register)anr_st },
  { "9",   (a_named_register)anr_st1 },
  { "10",  (a_named_register)anr_st2 },
  { "11",  (a_named_register)anr_st3 },
  { "12",  (a_named_register)anr_st4 },
  { "13",  (a_named_register)anr_st5 },
  { "14",  (a_named_register)anr_st6 },
  { "15",  (a_named_register)anr_st7 },
  /* (Registers 16 through 20 do not map on known registers.  Their primary
     names are therefore just "16", "17, "18", "19", and "20".  No secondary
     names are needed here. */
  { "21",  (a_named_register)anr_f0 },
  { "22",  (a_named_register)anr_f1 },
  { "23",  (a_named_register)anr_f2 },
  { "24",  (a_named_register)anr_f3 },
  { "25",  (a_named_register)anr_f4 },
  { "26",  (a_named_register)anr_f5 },
  { "27",  (a_named_register)anr_f6 },
  { "28",  (a_named_register)anr_f7 },
  { "29",  (a_named_register)anr_mm0 },
  { "30",  (a_named_register)anr_mm1 },
  { "31",  (a_named_register)anr_mm2 },
  { "32",  (a_named_register)anr_mm3 },
  { "33",  (a_named_register)anr_mm4 },
  { "34",  (a_named_register)anr_mm5 },
  { "35",  (a_named_register)anr_mm6 },
  { "36",  (a_named_register)anr_mm7 },
  { "37",  (a_named_register)anr_r8 },
  { "38",  (a_named_register)anr_r9 },
  { "39",  (a_named_register)anr_r10 },
  { "40",  (a_named_register)anr_r11 },
  { "41",  (a_named_register)anr_r12 },
  { "42",  (a_named_register)anr_r13 },
  { "43",  (a_named_register)anr_r14 },
  { "44",  (a_named_register)anr_r15 },
  { "45",  (a_named_register)anr_f8 },
  { "46",  (a_named_register)anr_f9 },
  { "47",  (a_named_register)anr_f10 },
  { "48",  (a_named_register)anr_f11 },
  { "49",  (a_named_register)anr_f12 },
  { "50",  (a_named_register)anr_f13 },
  { "51",  (a_named_register)anr_f14 },
  { "52",  (a_named_register)anr_f15 },
#endif /* GNU_X86_ASM_EXTENSIONS_ALLOWED */
#if ACCEPT_UNRECOGNIZED_GNU_ASM_OPERANDS
  { "", (a_named_register)anr_unrecognized },
#endif /* ACCEPT_UNRECOGNIZED_GNU_ASM_OPERANDS */
  { "", (a_named_register)anr_last }
};

/* The complete map between register names and enumerators, and its size. */
static struct name_to_reg *regmap;
static size_t regmap_size;


a_named_register name_to_register(char  *name)
/*
Given the user-specified name of a register as a string, return
its code number, or anr_invalid if there is no such register.
In the latter case, issues an error.
*/
{
  unsigned int      md, mn = 0, mx = regmap_size;
  int               comp;
  a_named_register  result = (a_named_register)anr_invalid;
  char              *name_to_search = name;

#if GNU_X86_ASM_EXTENSIONS_ALLOWED
  if (name[0] == '%') {
    /* Register names are optionally prefixed with the "%" character. */
    ++name_to_search;
  }  /* if */
#endif /* GNU_X86_ASM_EXTENSIONS_ALLOWED */
  /* Binary search regmap array. */
  while (mx > mn) {
    md = (mn + mx) / 2;
    comp = strcmp(name_to_search, regmap[md].name);
    if (comp > 0) {
      mn = md + 1;
    } else if (comp < 0) {
      mx = md;
    } else {
      result = regmap[md].reg;
      break;
    }  /* if */
  }  /* while */
#if ACCEPT_UNRECOGNIZED_GNU_ASM_OPERANDS
  if (result == (a_named_register)anr_invalid) {
    result = (a_named_register)anr_unrecognized;
  }  /* if */
#endif /* ACCEPT_UNRECOGNIZED_GNU_ASM_OPERANDS */
  return result;
}  /* name_to_register */

#if !RECORD_RAW_ASM_OPERAND_DESCRIPTIONS

/*ARGSUSED*/
static a_boolean validate_expr_for_constraints(
                                           an_expr_node_ptr              expr,
                                           an_asm_operand_constraint_ptr cstrt)
/*
Verify that expr can legitimately be used as an asm operand with
constraints given by cstrt.  This code is machine specific and must be
provided by the author of the back end.
*/
{
  return TRUE;
}  /* validate_expr_for_constraints */

#endif /* !RECORD_RAW_ASM_OPERAND_DESCRIPTIONS */

static int find_symbolic_operand(char                **pc,
                                 an_asm_operand_ptr  operands,
                                 a_source_position   *diag_pos)
/*
*pc points to a left bracket ('[') that starts a reference to a symbolic asm
operand.  Advance the *pc pointer to the matching right bracket (or the null
character terminating the string if there is no right bracket) and return the
position of the indicated operand.  If the indicated operand name does not
correspond to a previous operand, issue an error (at the given source
position) and return -1.  operands points to the list of operands created so
far (this routine is sometimes called when that list is still incomplete).
*/
{
  int   result = -1, n = 0;
  char  *start;

  check_assertion(**pc == '[');
  start = ++*pc;
  /* Find the end of the symbolic operand name. */
  while (**pc != ']' && **pc != '\0') {
    ++*pc;
  }  /* while */
  /* Look for an operand with that name in the list of preceding operands. */
  while (operands != NULL) {
    if (operands->name != NULL &&
        strncmp(operands->name, start, *pc-start) == 0 &&
        (sizeof_t)strlen(operands->name) == (sizeof_t)(*pc-start)) {
      result = n;
      break;
    }  /* if */
    operands = operands->next;
    ++n;
  }  /* while */
  if (result == -1) {
    char saved_char = **pc;
    **pc = '\0';
    pos_st_error(ec_invalid_symbolic_asm_operand_name, diag_pos, start);
    **pc = saved_char;
  }  /* if */
  return result;
}  /* find_symbolic_operand */


static void validate_symbolic_operand_references(
                                              a_constant_ptr      asm_string,
                                              an_asm_operand_ptr  operands,
                                              a_source_position   *diag_pos)
/*
Traverse the given asm string and validate any symbolic operand references of
the form "%[<name>]" it contains against the given list of operands.  Invalid
references are reported at the given position.
*/
{
  if (asm_string->kind == (a_constant_repr_kind)ck_string) {
    char  *pc = asm_string->variant.string.value;
    while (*pc != '\0') {
      if (pc[0] == '%' && (pc[1] == '[' || (pc[1] != '\0' && pc[2] == '['))) {
        /* We found a "%[" or "%X[" (where X is an output format modifier)
           construct.  Look up the symbolic operand reference that (normally)
           follows.  The call to find_symbol_operand will trigger any needed
           diagnostics. */
        ++pc;
        if (*pc != '[')  {
          /* An output format modifier between the '%' and '['. */
          ++pc;
        } /* if */
        (void)find_symbolic_operand(&pc, operands, diag_pos);
      } else {
        ++pc;
      }  /* if */
    }  /* while */
  } else {
    check_assertion(is_error_constant(asm_string));
    expect_error();
  }  /* if */
}  /* validate_symbolic_operand_references */

#if !RECORD_RAW_ASM_OPERAND_DESCRIPTIONS

static an_asm_operand_constraint_kind get_symbolic_matching_constraint(
                                                 char                **pc,
                                                 an_asm_operand_ptr  operands,
                                                 a_source_position   *diag_pos)
/*
*pc points to a '[' character starting a symbolic operand name.  Advance this
pointer to the matching ']' character (or to the last character in the string
if none is found) and return a "matching constraint kind" for the operand
name indicated.  operands points to the list of operands already created.
Errors are diagnosed at the given position.
*/
{
  an_asm_operand_constraint_kind  result;
  int                             op_num = find_symbolic_operand(pc, operands,
                                                                 diag_pos);

  if (op_num < 0) {
    /* An error was already issued. */
    result = (an_asm_operand_constraint_kind)aoc_invalid;
  } else if (op_num > 9) {
    pos_error(ec_match_limit_for_symbolic_asm_operand, diag_pos);
    result = (an_asm_operand_constraint_kind)aoc_invalid;
  } else {
    result = (an_asm_operand_constraint_kind)((int)aoc_match_0 + op_num);
  }  /* if */
  return result;
}  /* get_symbolic_matching_constraint */

#endif /* !RECORD_RAW_ASM_OPERAND_DESCRIPTIONS */

#if RECORD_RAW_ASM_OPERAND_DESCRIPTIONS
/* ARGSUSED */  /* operands is not used in some configurations. */
#endif /* RECORD_RAW_ASM_OPERAND_DESCRIPTIONS */
static void process_asm_operand(an_asm_operand_ptr  operand,
                                an_asm_operand_ptr  operands,
                                an_expr_node_ptr    expr,
                                char                *cstring,
                                a_boolean           output)
/*
Fill in *operand (a GNU asm operand description) using the cstring constraints
string and the expr expression.  Output is TRUE if the call is for an output
operand.  If RECORD_RAW_ASM_OPERAND_DESCRIPTIONS is FALSE, validate the
semantic consistency of expr, cstring, and output: If an inconsistency is
detected, an error is issued and *operand is set to "error placemarker" values.
operands points to the operands created so far.
*/
{
#if RECORD_RAW_ASM_OPERAND_DESCRIPTIONS
  /* When the raw form is recorded, consistency checks are not performed by
     the front end.  (A back end may be in a better position to perform those
     checks.) */
  operand->is_output_operand = output;
  operand->constraints_string = cstring;
  operand->expression = expr;
#else /* !RECORD_RAW_ASM_OPERAND_DESCRIPTIONS */
  an_asm_operand_constraint_ptr  *constraint;
  an_asm_operand_constraint_kind ck;
  an_asm_operand_modifier        modifiers;
  a_boolean                      error_occurred = FALSE;
  char                           *p;
  char                           errletter[2];

  operand->constraints = NULL;
  if (cstring == NULL || expr == NULL) {
    /* Syntactically invalid -- an error has already been issued.
       N.B. We do not bail out if expr is an error node, because it's
       still possible and useful to validate the constraint string. */
    goto error_return;
  }  /* if */
  errletter[1] = '\0';
  /* Compute modifiers. */
  modifiers = (an_asm_operand_modifier)aom_invalid;
  for (p = cstring; *p != '\0'; p++) {
    switch (*p) {
      /* Modifiers valid in asm() */
      case '=': modifiers |= (an_asm_operand_modifier)aom_output; break;
      case '+': modifiers |= (an_asm_operand_modifier)aom_modify; break;
      case '&': modifiers |= (an_asm_operand_modifier)aom_earlyclobber; break;
      /* Modifiers ignored in asm(): */
      case '%':  case '*':  case '#':  case '?':  case '!':
        errletter[0] = *p;
#if !ACCEPT_UNRECOGNIZED_GNU_ASM_OPERANDS
        pos_st_warning(ec_asm_modifier_ignored, &operand->position, errletter);
#endif /* !ACCEPT_UNRECOGNIZED_GNU_ASM_OPERANDS */
        break;
      default:
        goto done_with_modifiers;
    }  /* switch */
  }  /* for */
done_with_modifiers:
  if (modifiers == (an_asm_operand_modifier)aom_invalid) {
    /* No modifiers on an input operand. */
    modifiers = (an_asm_operand_modifier)aom_input;
  }  /* if */
  if (*p == '\0') {
    pos_error(ec_missing_constraint_letter, &operand->position);
    goto error_return;
  }  /* if */
  constraint = &operand->constraints;
  /*lint --e{850} p modified in loop */
  for (; *p != '\0'; p++) {
    /* The next thing in the string should be a constraint letter. */
    ck = (an_asm_operand_constraint_kind)aoc_invalid;
    switch (*p) {
      /* Machine independent constraints - miscellaneous. */
      case 'X':
        ck = (an_asm_operand_constraint_kind)aoc_any;     
        break;
      case 'g': 
        ck = (an_asm_operand_constraint_kind)aoc_general; 
        break;
      case '0': 
        ck = (an_asm_operand_constraint_kind)aoc_match_0; 
        break;
      case '1': 
        ck = (an_asm_operand_constraint_kind)aoc_match_1; 
        break;
      case '2': 
        ck = (an_asm_operand_constraint_kind)aoc_match_2; 
        break;
      case '3':
        ck = (an_asm_operand_constraint_kind)aoc_match_3; 
        break;
      case '4': 
        ck = (an_asm_operand_constraint_kind)aoc_match_4; 
        break;
      case '5': 
        ck = (an_asm_operand_constraint_kind)aoc_match_5; 
        break;
      case '6': 
        ck = (an_asm_operand_constraint_kind)aoc_match_6; 
        break;
      case '7': 
        ck = (an_asm_operand_constraint_kind)aoc_match_7; 
        break;
      case '8': 
        ck = (an_asm_operand_constraint_kind)aoc_match_8; 
        break;
      case '9': 
        ck = (an_asm_operand_constraint_kind)aoc_match_9; 
        break;
      case '[':
        ck = get_symbolic_matching_constraint(&p, operands,
                                              &operand->position);
        error_occurred = (ck == (an_asm_operand_constraint_kind)aoc_invalid);
        break;
      /* Registers */
      case 'r': 
        ck = (an_asm_operand_constraint_kind)aoc_reg_integer; 
        break;
      case 'f': 
        ck = (an_asm_operand_constraint_kind)aoc_reg_float;   
        break;
      /* Memory */
      case 'm': 
        ck = (an_asm_operand_constraint_kind)aoc_mem_any;
        break;
      case 'p': 
        ck = (an_asm_operand_constraint_kind)aoc_mem_load;
        break;
      case 'o': 
        ck = (an_asm_operand_constraint_kind)aoc_mem_offset;
        break;
      case 'V': 
        ck = (an_asm_operand_constraint_kind)aoc_mem_nonoffset;
        break;
      case '<': 
        ck = (an_asm_operand_constraint_kind)aoc_mem_autoinc;
        break;
      case '>': 
        ck = (an_asm_operand_constraint_kind)aoc_mem_autodec;
        break;
      /* Immediates */
      case 'i':
        ck = (an_asm_operand_constraint_kind)aoc_imm_int;    
        break;
      case 'n':
        ck = (an_asm_operand_constraint_kind)aoc_imm_number; 
        break;
      case 's':
        ck = (an_asm_operand_constraint_kind)aoc_imm_symbol; 
        break;
      case 'E':
        ck = (an_asm_operand_constraint_kind)aoc_imm_float;  
        break;
      case 'F':
        ck = (an_asm_operand_constraint_kind)aoc_imm_float;  
        break;
#if GNU_X86_ASM_EXTENSIONS_ALLOWED
      /* x86 specific constraints - registers */
      case 'a':
        ck = (an_asm_operand_constraint_kind)aoc_reg_a;         
        break;
      case 'b':
        ck = (an_asm_operand_constraint_kind)aoc_reg_b;         
        break;
      case 'c':
        ck = (an_asm_operand_constraint_kind)aoc_reg_c;         
        break;
      case 'd':
        ck = (an_asm_operand_constraint_kind)aoc_reg_d;         
        break;
      case 'S':
        ck = (an_asm_operand_constraint_kind)aoc_reg_si;        
        break;
      case 'D':
        ck = (an_asm_operand_constraint_kind)aoc_reg_di;        
        break;
      case 'R':
        ck = (an_asm_operand_constraint_kind)aoc_reg_legacy;    
        break;
      case 'q':
        ck = (an_asm_operand_constraint_kind)aoc_reg_q;         
        break;
      case 'Q':
        ck = (an_asm_operand_constraint_kind)aoc_reg_Q;         
        break;
      case 'A':
        ck = (an_asm_operand_constraint_kind)aoc_reg_ad;        
        break;
      case 't':
        ck = (an_asm_operand_constraint_kind)aoc_reg_float_tos; 
        break;
      case 'u':
        ck = (an_asm_operand_constraint_kind)aoc_reg_float_second;
        break;
      case 'x':
        ck = (an_asm_operand_constraint_kind)aoc_reg_sse;       
        break;
      case 'Y':
        ck = (an_asm_operand_constraint_kind)aoc_reg_sse2;      
        break;
      case 'y':
        ck = (an_asm_operand_constraint_kind)aoc_reg_mmx;       
        break;
      /* Immediates */
      case 'I':
        ck = (an_asm_operand_constraint_kind)aoc_imm_short_shift;
        break;
      case 'J':
        ck = (an_asm_operand_constraint_kind)aoc_imm_long_shift;
        break;
      case 'M':
        ck = (an_asm_operand_constraint_kind)aoc_imm_lea_shift; 
        break;
      case 'K':
        ck = (an_asm_operand_constraint_kind)aoc_imm_signed8;   
        break;
      case 'N':
        ck = (an_asm_operand_constraint_kind)aoc_imm_unsigned8; 
        break;
      case 'L':
        ck = (an_asm_operand_constraint_kind)aoc_imm_and_zext;  
        break;
      case 'G':
        ck = (an_asm_operand_constraint_kind)aoc_imm_80387;     
        break;
      case 'H':
        ck = (an_asm_operand_constraint_kind)aoc_imm_sse;       
        break;
      case 'e':
        ck = (an_asm_operand_constraint_kind)aoc_imm_sext32;    
        break;
      case 'Z':
        ck = (an_asm_operand_constraint_kind)aoc_imm_zext32;    
        break;
#endif /* GNU_X86_ASM_EXTENSIONS_ALLOWED */
      default:
        errletter[0] = *p;
#if !ACCEPT_UNRECOGNIZED_GNU_ASM_OPERANDS
        pos_st_error(ispunct((unsigned char)*p) ? 
                     ec_bad_asm_constraint_modifier : 
                     ec_bad_asm_constraint_letter,
                     &operand->position, errletter);
        error_occurred = TRUE;
#endif /* !ACCEPT_UNRECOGNIZED_GNU_ASM_OPERANDS */
        break;
    }  /* switch */
    /* Create a new constraint and add it to the list.  */
    if (ck != (an_asm_operand_constraint_kind)aoc_invalid) {
      *constraint = alloc_asm_operand_constraint(ck);
      constraint = &(*constraint)->next;
    }  /* if */
  }  /* for */
  if (error_occurred) {
    goto error_return;
  }  /* if */
  /* Semantic validation. */
  if (output && !(modifiers & (an_asm_operand_modifier)aom_output)) {
    pos_error(ec_asm_output_must_have_output_mod, &operand->position);
    goto error_return;
  } else if (!output && (modifiers & (an_asm_operand_modifier)aom_output)) {
    pos_error(ec_asm_input_must_not_have_output_mod, &operand->position);
    goto error_return;
  }  /* if */
  if (validate_expr_for_constraints(expr, operand->constraints)) {
    operand->expression = expr;
    operand->modifiers = modifiers;
  } else {
error_return:
    operand->expression = error_node();
    operand->modifiers = (an_asm_operand_modifier)aom_invalid;
  }  /* if */
#endif /* RECORD_RAW_ASM_OPERAND_DESCRIPTIONS */
}  /* process_asm_operand */


/*
Machine-specific tables used by validate_operands_and_clobbers.
*/
#if !RECORD_RAW_ASM_OPERAND_DESCRIPTIONS
typedef struct single_register_constraint {
  /* Structure to hold a constraint-to-register mapping. */
  an_asm_operand_constraint_kind cons;
  a_named_register               reg;
} single_register_constraint;


static single_register_constraint single_register_constraints[] = {
#if GNU_X86_ASM_EXTENSIONS_ALLOWED
  { (an_asm_operand_constraint_kind)aoc_reg_a, (a_named_register)anr_a },
  { (an_asm_operand_constraint_kind)aoc_reg_b, (a_named_register)anr_b },
  { (an_asm_operand_constraint_kind)aoc_reg_c, (a_named_register)anr_c },
  { (an_asm_operand_constraint_kind)aoc_reg_d, (a_named_register)anr_d },
  { (an_asm_operand_constraint_kind)aoc_reg_si, (a_named_register)anr_si },
  { (an_asm_operand_constraint_kind)aoc_reg_di, (a_named_register)anr_di },
  /* 't' and 'u' (x86 reg stack) are not included in this list,
     because the rules are not properly handled by the generic code
     below.  Machine-specific code must be written to handle them. */
#endif /* GNU_X86_ASM_EXTENSIONS_ALLOWED */
  { (an_asm_operand_constraint_kind)aoc_last, (a_named_register)anr_last }
};

#endif /* !RECORD_RAW_ASM_OPERAND_DESCRIPTIONS */

static a_named_register fixed_registers[] = {
#if GNU_X86_ASM_EXTENSIONS_ALLOWED
  (a_named_register)anr_bp, (a_named_register)anr_sp,
#endif /* GNU_X86_ASM_EXTENSIONS_ALLOWED */
  (a_named_register)anr_last
};


void validate_operands_and_clobbers(an_asm_entry_ptr  asm_entry)
/*
Validate the given asm entry.  The machine-independent portion of this code
simply checks that no duplicates appear in the clobbers list, no single-
register constraint is used when the corresponding register is clobbered,
and no un-clobberable registers appear in the clobber list.  It also
verifies that symbolic operand names appearing in the asm string itself do
refer to actual operands.  Back end authors should augment this code to do
complete validation of the lists; it is easy to write an asm statement with
unsatisfiable register allocation requirements that does not trip over any
of the machine-independent checks.

Note that this function never modifies the operands or clobbers lists,
even if they are invalid.
*/
{
#if RECORD_RAW_ASM_OPERAND_DESCRIPTIONS
  /* Only raw operand descriptions are recorded: We do not attempt to
     understand the meaning of those descriptions and therefore we do not
     diagnose semantic errors in them.  We can however diagnose duplicate
     clobbers and attempts to clobber fixed registers. */
  a_byte                     regs_clobbered[(int)anr_last];
  a_named_register_list_ptr  clobber, clobbers = asm_entry->clobbers;
  int                        i;
  a_named_register           r;

  memzero((char*)regs_clobbered, sizeof regs_clobbered);
  for (clobber = clobbers; clobber != NULL; clobber = clobber->next) {
    r = clobber->reg;
#if ACCEPT_UNRECOGNIZED_GNU_ASM_OPERANDS
    /* Ignore entries for unrecognized registers. */
    if (r == (a_named_register)anr_unrecognized) continue;
#endif /* ACCEPT_UNRECOGNIZED_GNU_ASM_OPERANDS */
    if (r != (a_named_register)anr_invalid && regs_clobbered[(int)r] == 1) {
      /* Test clobbered == 1 so the diagnostic is issued at most once per
         register. */
      pos_st_warning(ec_register_clobbered_twice,
                     &asm_entry->source_corresp.decl_position,
                     named_register_names[(int)r]);
    }  /* if */
    ++regs_clobbered[(int)r];
  }  /* for */
  for (i = 0; fixed_registers[i] != (a_named_register)anr_last; i++) {
    r = fixed_registers[i];
#if ACCEPT_UNRECOGNIZED_GNU_ASM_OPERANDS
    /* Ignore entries for unrecognized registers. */
    if (r == (a_named_register)anr_unrecognized) continue;
#endif /* ACCEPT_UNRECOGNIZED_GNU_ASM_OPERANDS */
    if (regs_clobbered[(int)r]) {
      pos_st_error(ec_fixed_register_clobbered,
                   &asm_entry->source_corresp.decl_position,
                   named_register_names[(int)r]);
    }  /* if */
  }  /* for */
#else /* !RECORD_RAW_ASM_OPERAND_DESCRIPTIONS */
  /* Asm operand descriptions are parsed and can therefore be diagnosed for
     consistency. */
  a_byte                        regs_clobbered[(int)anr_last];
  a_byte                        regs_used_in[(int)anr_last];
  a_byte                        regs_used_out[(int)anr_last];
  an_asm_operand_ptr            aop;
  a_named_register_list_ptr     clobber, clobbers = asm_entry->clobbers;
  an_asm_operand_constraint_ptr c;
  int                           i;
  a_named_register              r;

  memzero((char*)regs_clobbered, sizeof regs_clobbered);
  memzero((char*)regs_used_in, sizeof regs_used_in);
  memzero((char*)regs_used_out, sizeof regs_used_out);
  for (aop = asm_entry->operands; aop != NULL; aop = aop->next) {
    for (i = 0;
         single_register_constraints[i].cons !=
                                      (an_asm_operand_constraint_kind)aoc_last;
         i++) {
      a_boolean  input = (aop->modifiers & 
                            ((an_asm_operand_modifier)aom_input |
                             (an_asm_operand_modifier)aom_earlyclobber)) != 0;
      a_boolean  output = (aop->modifiers &
                            ((an_asm_operand_modifier)aom_output |
                             (an_asm_operand_modifier)aom_earlyclobber)) != 0;
      for (c = aop->constraints; c != NULL; c = c->next) {
        if (c->kind == single_register_constraints[i].cons) {
          r = single_register_constraints[i].reg;
#if ACCEPT_UNRECOGNIZED_GNU_ASM_OPERANDS
          /* Ignore entries for unrecognized registers. */
          if (r == (a_named_register)anr_unrecognized) continue;
#endif /* ACCEPT_UNRECOGNIZED_GNU_ASM_OPERANDS */
          /* Test used == 1 so the error is issued once per register. */
          if (r != (a_named_register)anr_invalid &&
              ((input && regs_used_in[(int)r] == 1) ||
               (output && regs_used_out[(int)r] == 1))) {
            pos_st_error(ec_register_used_twice, &aop->position,
                         named_register_names[(int)r]);
          }  /* if */
          if (input) ++regs_used_in[(int)r];
          if (output) ++regs_used_out[(int)r];
        }  /* if */
      }  /* for */
    }  /* for */
  }  /* for */
  for (clobber = clobbers; clobber != NULL; clobber = clobber->next) {
    r = clobber->reg;
#if ACCEPT_UNRECOGNIZED_GNU_ASM_OPERANDS
    /* Ignore entries for unrecognized registers. */
    if (r == (a_named_register)anr_unrecognized) continue;
#endif /* ACCEPT_UNRECOGNIZED_GNU_ASM_OPERANDS */
    if ((regs_used_in[(int)r] || regs_used_out[(int)r]) &&
        !regs_clobbered[(int)r]) {
      /* Test used and not clobbered so the error is issued at most once per
         register. */
      pos_st_error(ec_register_used_and_clobbered,
                   &asm_entry->source_corresp.decl_position,
                   named_register_names[(int)r]);
    } else if (r != (a_named_register)anr_invalid &&
               regs_clobbered[(int)r] == 1) {
      /* Test clobbered == 1 so the diagnostic is issued at most once per
         register. */
      pos_st_warning(ec_register_clobbered_twice,
                     &asm_entry->source_corresp.decl_position,
                     named_register_names[(int)r]);
    }  /* if */
    ++regs_clobbered[(int)r];
  }  /* for */
  for (i = 0; fixed_registers[i] != (a_named_register)anr_last; i++) {
    r = fixed_registers[i];
#if ACCEPT_UNRECOGNIZED_GNU_ASM_OPERANDS
    /* Ignore entries for unrecognized registers. */
    if (r == (a_named_register)anr_unrecognized) continue;
#endif /* ACCEPT_UNRECOGNIZED_GNU_ASM_OPERANDS */
    if (regs_used_in[(int)r] || regs_used_out[(int)r]) {
      pos_st_error(ec_fixed_register_used,
                   &asm_entry->source_corresp.decl_position,
                   named_register_names[(int)r]);
    } else if (regs_clobbered[(int)r]) {
      pos_st_error(ec_fixed_register_clobbered,
                   &asm_entry->source_corresp.decl_position,
                   named_register_names[(int)r]);
    }  /* if */
  }  /* for */
#endif /* RECORD_RAW_ASM_OPERAND_DESCRIPTIONS */
  validate_symbolic_operand_references(
                                    asm_entry->asm_string, asm_entry->operands,
                                    &asm_entry->source_corresp.decl_position);
}  /* validate_operands_and_clobbers */


static void asm_operand(an_asm_operand_ptr operand,
                        an_asm_operand_ptr operands,
                        a_boolean          output)
/*
Scan a single asm-statement operand, writing it into the structure pointed to
by operand.  The syntax is

   string-literal ( expression )

optionally preceded by a symbolic name specifier of the form

   [ identifier ]

operands points to the list of operands created so far and output is TRUE
if we're scanning an output operand.
*/
{
  char              *constraint_string = NULL;
  an_expr_node_ptr  expr = NULL;

  db_enter(4, "asm_operand");
  add_stop_token(tok_comma);
  add_stop_token(tok_colon);
  add_stop_token(tok_colon_colon);
  operand->position = pos_curr_token;
  if (curr_token == tok_lbracket) {
    /* Presumably a named operand.  The next token must be an identifier. */
    (void)get_token();
    add_stop_token(tok_rbracket);
    if (curr_token != tok_identifier) {
      syntax_error(ec_exp_identifier);
    } else {
      /* Record the identifier as the name of the operand. */
      a_symbol_header  *sym_hdr = locator_for_curr_id.symbol_header;
      operand->name = alloc_il(sym_hdr->identifier_length+1);
      (void)strcpy(operand->name, sym_hdr->identifier);
      (void)get_token();
    }  /* if */
    (void)required_token(tok_rbracket, ec_exp_rbracket);
    remove_stop_token(tok_rbracket);
  }  /* if */
  if (curr_token != tok_string_literal) {
    syntax_error(ec_exp_string_literal);
  } else {
    constraint_string = const_for_curr_token.variant.string.value;
    /* Advance past string literal. */
    (void)get_token();
    if (required_token(tok_lparen, ec_exp_lparen)) {
      a_boolean  input = !output;
      if (output && constraint_string != NULL) {
        /* A '+' in the constraint string of an output operand indicates a
           read-modify-write instruction; i.e., the operand is first an input
           operand and then an output operand. */
        input = (strchr(constraint_string, '+') != NULL);
      }  /* if */
      add_stop_token(tok_rparen);
      expr = scan_asm_operand_expression(output, input);
      (void)required_token(tok_rparen, ec_exp_rparen);
      remove_stop_token(tok_rparen);
    }  /* if */
  }  /* if */
  process_asm_operand(operand, operands, expr, constraint_string, output);
  remove_stop_token(tok_comma);
  remove_stop_token(tok_colon);
  remove_stop_token(tok_colon_colon);
  db_exit();
}  /* asm_operand */


an_asm_operand_ptr asm_operands_spec(void)
/*
Parse and validate a list of asm-statement operands.  This handles both input
and output operands.  The list is returned as a sequence of an_asm_operand
entries.
On entry, curr_token is the leading colon of the operands specification; on
exit, it is the leading colon of the clobbers specification, or the close
parenthesis if there are no clobbers.

The syntax is

    : [operand [, operand...]]   // outputs
   [: [operand [, operand...]]]  // inputs

Since both operand lists can be empty, we must cope with two adjacent colons,
which will be tokenized as a single tok_colon_colon (in C++).
*/
{
  int                n = 0;
  a_boolean          output = TRUE;
  an_asm_operand_ptr operands = NULL;
  an_asm_operand_ptr *p_operands = &operands;

  db_enter(3, "asm_operands_spec");
  check_assertion(curr_token == tok_colon || curr_token == tok_colon_colon);
  report_gnu_extension_if_needed(&pos_curr_token,
                                 ec_asm_operand_spec_is_gnu_extension);
  /* :: is interpreted the same as as : :, i.e. an empty output list. */
  if (curr_token == tok_colon_colon) {
    output = FALSE;
  }  /* if */
  /* Skip initial : or ::. */
  (void)get_token();
  /* If the output list is empty, we'll be at another colon. */
  if (output && curr_token == tok_colon) {
    output = FALSE;
    (void)get_token();
  }  /* if */
  while (curr_token == tok_string_literal || curr_token == tok_lbracket) {
    /* There is a hard limit of thirty operands per assembly instruction. */
    if (n == 30) {
      error(ec_too_many_asm_operands);
    }  /* if */
    *p_operands = alloc_asm_operand();
    asm_operand(*p_operands, operands, output);
    p_operands = &(*p_operands)->next;
    ++n;
#if !RECORD_RAW_ASM_OPERAND_DESCRIPTIONS
    if (operands->modifiers == (an_asm_operand_modifier)aom_modify) {
      /* GNU compilers seem to count '+' modifiers as two operands.  Presumably
         because it involves a read and a write operation. */
      ++n;
    }  /* if */
#endif /* !RECORD_RAW_ASM_OPERAND_DESCRIPTIONS */
    /* Next must be a comma, colon, or right paren. */
    if (curr_token == tok_colon) {
      if (output) {
        /* End of output list; consume colon and continue. */
        output = FALSE;
        (void)get_token();
      } /* if */
    } else if (curr_token == tok_colon_colon) {
      if (!output) {
        /* Too many colons.  Flush to right paren and bail out. */
        syntax_error(ec_too_many_asm_colons);
        break;
      }  /* if */
    } else if (curr_token == tok_comma) {
      (void)get_token();
      if (curr_token != tok_string_literal && curr_token != tok_lbracket) {
        syntax_error(ec_exp_asm_operand);
      }  /* if */
    }  /* if */
  }  /* while */
  db_exit();
  return operands;
}  /* asm_operands_spec */


a_named_register_list_ptr asm_clobbers_spec(void)
/*
Parse and validate a list of asm-statement clobbers.  Returns the list of
registers clobbered.

The syntax is

   string-literal [, string-literal ...]

*/
{
  /* There is no hard limit on the number of clobbers. */
  a_named_register           reg;
  int                        nparsed = 0;
  char                       *name;
  a_named_register_list_ptr  first_reg = NULL, last_reg = NULL;

  db_enter(3, "asm_clobbers_spec");
  if (curr_token == tok_colon || curr_token == tok_colon_colon) {
    (void)get_token();
    while (curr_token == tok_string_literal) {
      nparsed++;
      name = const_for_curr_token.variant.string.value;
      if (strcmp(name, "memory") == 0) {
        /* The string "memory" can appear in place of a register name.  */
        reg = (a_named_register)anr_memory;
      } else if (strcmp(name, "cc") == 0) {
        warning(ec_cc_clobber_ignored);
        goto skip_item;
      } else {
        reg = name_to_register(name);
      }  /* if */
      if (reg == (a_named_register)anr_invalid) {
        pos_st_error(ec_bad_reg_name, &pos_curr_token, name);
      } else {
        /* Add this register to our list. */
        if (first_reg == NULL) {
          first_reg = last_reg = alloc_named_register_list();
        } else {
          last_reg->next = alloc_named_register_list();
          last_reg = last_reg->next;
        }  /* if */
        last_reg->reg = reg;
      }  /* if */
skip_item:
      /* Advance past the string literal. */
      (void)get_token();
      /* The next token must be a comma or a right parenthesis. */
      if (curr_token == tok_comma) {
        (void)get_token();
        if (curr_token != tok_string_literal) {
          syntax_error(ec_exp_asm_clobber);
        }  /* if */
      }  /* if */
    }  /* while */
    /* GCC treats an empty clobbers list with a colon as a syntax
       error.  We can parse it correctly, so it's semantic for us.
       Don't issue this error if we saw anything other than a colon
       immediately followed by a right parenthesis. */
    if (curr_token != tok_rparen) {
      syntax_error(ec_exp_rparen);
    } else if (nparsed == 0 && C_mode()) {
      error(ec_empty_clobbers_list);
    }  /* if */
  }  /* if */
  db_exit();
  return first_reg;
}  /* asm_clobbers_spec */


#if BSEARCH_QSORT_FUNCTION_IS_EXTERN_C
BEGIN_EXTERN_C_BLOCK
#endif /* BSEARCH_QSORT_FUNCTION_IS_EXTERN_C */

static int compare_n2r(const void *a,
                       const void *b)
/*
Compare a <=> b, which are really pointers to name_to_reg structures,
by their name strings.
*/
{
  struct name_to_reg *x = (struct name_to_reg *)a;
  struct name_to_reg *y = (struct name_to_reg *)b;

  return strcmp(x->name, y->name);
}  /* compare_n2r */

#if BSEARCH_QSORT_FUNCTION_IS_EXTERN_C
END_EXTERN_C_BLOCK
#endif /* BSEARCH_QSORT_FUNCTION_IS_EXTERN_C */


void extasm_one_time_init(void)
/*
Do one-time initialization of variables related to the processing of
extended asm statements.
*/
{
  int i;
#if CHECKING
  /* Check that the table of mode names is correctly initialized. */
  if (named_register_names[(int)anr_last] == NULL ||
      strcmp(named_register_names[(int)anr_last], "last") != 0) {
    internal_error(
      "extasm_one_time_init: named_register_names: bad init");
  }  /* if */
#if !RECORD_RAW_ASM_OPERAND_DESCRIPTIONS
  /* Ditto the table of constraint letters. */
  if (asm_operand_constraint_letters[(int)aoc_last] != '~') {
    internal_error(
      "extasm_one_time_init: asm_operand_constraint_letters: bad init");
  }  /* if */
#endif /* !RECORD_RAW_ASM_OPERAND_DESCRIPTIONS */
#endif /* CHECKING */
  /* Set up the complete regmap table, including both the official and
     extra register names.  regmap does not include entries for
     anr_invalid or anr_last. */
  regmap_size = (int)anr_last - 1;
  regmap_size += (sizeof(extra_reg_names) /
                  sizeof(struct name_to_reg)) - 1;  /*lint !e845*/
  regmap = (struct name_to_reg *)alloc_general(
                         (sizeof_t)(regmap_size * sizeof(struct name_to_reg)));
  /* Start with i = 1 since anr_invalid is not copied. */
  for (i = 1; i < (int)anr_last; i++) { /*lint !e681*/
    regmap[i-1].name = named_register_names[i];
    regmap[i-1].reg = i;
  }  /* for */
  /* Copy the extra register information. */
  (void)memcpy((char*)&regmap[(int)anr_last-1], (char*)extra_reg_names,
               size_t_arg(sizeof(extra_reg_names) -
                                                  sizeof(struct name_to_reg)));
  /* name_to_register requires that regmap be sorted. */
  qsort((a_void_ptr)regmap, (qsort_nmemb_type)regmap_size,
        (qsort_nmemb_type)sizeof(struct name_to_reg), compare_n2r);
  /* Save variables from extasm.h and extasm.c that are needed for
     precompiled headers */
  if (precompiled_header_processing_required) {
    static a_pch_saved_variable saved_vars[] = {
      pch_saved_var_array_terminating_elem()
    };
    register_pch_saved_variables(saved_vars);
  }  /* if */
}  /* extasm_one_time_init */

#endif /* GNU_EXTENSIONS_ALLOWED */

/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2001-2012 Edison Design Group Inc.                   [_]          *
*                                                                             *
******************************************************************************/
