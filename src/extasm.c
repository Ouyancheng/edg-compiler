/******************************************************************************
*                                                             \  ___  /       *
*                                                               /   \         *
* Edison Design Group C++/C Front End                        - | \^/ | -      *
*                                                               \   /         *
* Proprietary information of Edison Design Group Inc.         /  | |  \       *
* Copyright 2001 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
/*

extasm.c -- Scanning and validation of GNU extended asm() statements.

*/

/* Header files common to all files. */
#include "fe_common.h"

#include "extasm.h"

#ifdef PCH_PRAGMA_GUARD
/* Mark the end of the sequence of headers subject to precompiled header
   processing. */
#pragma hdrstop
#endif /* ifdef PCH_PRAGMA_GUARD */

/* Header files used by files involved in declaration processing. */
#include "decl_hdrs.h"


#if GNU_EXTENSIONS_ALLOWED

struct name_to_reg {
  /* Structure to hold a name-to-register mapping entry. */
  char              *name;
  a_named_register  reg;
};

/*
Extra named registers, in addition to the canonical names in il_def.h.
*/
static struct name_to_reg extra_reg_names[] = {
#if TARG_IS_X86
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

  /* x86-64 additional integer registers, other possible names... */
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
  { "st",  (a_named_register)anr_st0 },
  { "st0", (a_named_register)anr_st0 },
  { "st1", (a_named_register)anr_st1 },
  { "st2", (a_named_register)anr_st2 },
  { "st3", (a_named_register)anr_st3 },
  { "st4", (a_named_register)anr_st4 },
  { "st5", (a_named_register)anr_st5 },
  { "st6", (a_named_register)anr_st6 },
  { "st7", (a_named_register)anr_st7 },
#endif /* TARG_IS_X86 */
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

  /* Binary search regmap array. */
  while (mx > mn) {
    md = (mn + mx) / 2;
    comp = strcmp(name, regmap[md].name);
    if (comp > 0) {
      mn = md + 1;
    } else if (comp < 0) {
      mx = md;
    } else {
      result = regmap[md].reg;
      break;
    }  /* if */
  }  /* while */
  if (result == (a_named_register)anr_invalid) {
    str_error(ec_bad_reg_name, name);
  }  /* if */
  return result;
}  /* name_to_register */


typedef struct a_named_register_list_entry *a_named_register_list_entry_ptr;
typedef struct a_named_register_list_entry {
  /* Helper structure to accumulate a list of registers. */
  a_named_register_list_entry_ptr
		next;
			/* Pointer to the next entry (if any). */
  a_named_register
		reg;
			/* Register description. */
} a_named_register_list_entry;

/* Previously allocated entries available for reuse. */
static a_named_register_list_entry_ptr avail_named_register_list_entries;

static a_named_register_list_entry_ptr alloc_named_register_list_entry(void)
/*
Allocate an entry of a list of named register entries.
*/
{
  a_named_register_list_entry_ptr  result;

  if (avail_named_register_list_entries != NULL) {
    /* Reuse a previously allocated entry. */
    result = avail_named_register_list_entries;
    avail_named_register_list_entries =
                                      avail_named_register_list_entries->next;
  } else {
    /* Allocate memory for a new entry. */
    result = (a_named_register_list_entry_ptr)alloc_fe(
                                         sizeof(a_named_register_list_entry));
  }  /* if */
  return result;
}  /* alloc_named_register_list_entry */


static void free_named_register_list_entry(a_named_register_list_entry_ptr  p)
/*
Return an entry of a list of named register entries to the available pool.
*/
{
  p->next = avail_named_register_list_entries;
  avail_named_register_list_entries = p;
  p->reg = (a_named_register)anr_last;
}  /* free_named_register_list_entry */


/*ARGSUSED*/
static a_boolean validate_expr_for_constraint(an_expr_node_ptr          expr,
                                              an_asm_operand_constraint cstrt)
/*
Verify that expr can legitimately be used as an asm operand with
constraint cstrt.  This code is machine specific and must be provided
by the author of the back end.
*/
{
  return TRUE;
}  /* validate_expr_for_constraint */


static void process_asm_operand(an_asm_operand_ptr  operand,
                                an_expr_node_ptr    expr,
                                char                *cstring,
                                a_boolean           output)
/*
Validate the semantic consistency of expr, constraint, and output.  If
they all match, fill in operand accordingly.  Otherwise, issue an
error, and set operand to "error placemarker" values.
*/
{
  an_asm_operand_constraint  constraint;
  an_asm_operand_modifier    modifiers;
  char                       *p;
  char                       errletter[2];

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
        pos_st_warning(ec_asm_modifier_ignored, &operand->position, errletter);
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
  /* The next thing in the string should be a constraint letter. */
  constraint = (an_asm_operand_constraint)aoc_invalid;
  switch (*p) {
    /* Machine independent constraints - miscellaneous. */
    case 'X': constraint = (an_asm_operand_constraint)aoc_any;     break;
    case 'g': constraint = (an_asm_operand_constraint)aoc_general; break;
    case '0': constraint = (an_asm_operand_constraint)aoc_match_0; break;
    case '1': constraint = (an_asm_operand_constraint)aoc_match_1; break;
    case '2': constraint = (an_asm_operand_constraint)aoc_match_2; break;
    case '3': constraint = (an_asm_operand_constraint)aoc_match_3; break;
    case '4': constraint = (an_asm_operand_constraint)aoc_match_4; break;
    case '5': constraint = (an_asm_operand_constraint)aoc_match_5; break;
    case '6': constraint = (an_asm_operand_constraint)aoc_match_6; break;
    case '7': constraint = (an_asm_operand_constraint)aoc_match_7; break;
    case '8': constraint = (an_asm_operand_constraint)aoc_match_8; break;
    case '9': constraint = (an_asm_operand_constraint)aoc_match_9; break;
    /* Registers */
    case 'r': constraint = (an_asm_operand_constraint)aoc_reg_integer; break;
    case 'f': constraint = (an_asm_operand_constraint)aoc_reg_float;   break;
    /* Memory */
    case 'm': constraint = (an_asm_operand_constraint)aoc_mem_any;       break;
    case 'o': constraint = (an_asm_operand_constraint)aoc_mem_offset;    break;
    case 'V': constraint = (an_asm_operand_constraint)aoc_mem_nonoffset; break;
    case '<': constraint = (an_asm_operand_constraint)aoc_mem_autoinc;   break;
    case '>': constraint = (an_asm_operand_constraint)aoc_mem_autodec;   break;
    /* Immediates */
    case 'i': constraint = (an_asm_operand_constraint)aoc_imm_int;    break;
    case 'n': constraint = (an_asm_operand_constraint)aoc_imm_number; break;
    case 's': constraint = (an_asm_operand_constraint)aoc_imm_symbol; break;
    case 'E': constraint = (an_asm_operand_constraint)aoc_imm_float;  break;
    case 'F': constraint = (an_asm_operand_constraint)aoc_imm_float;  break;
#if TARG_IS_X86
    /* x86 specific constraints - registers */
    case 'a': constraint = (an_asm_operand_constraint)aoc_reg_a;         break;
    case 'b': constraint = (an_asm_operand_constraint)aoc_reg_b;         break;
    case 'c': constraint = (an_asm_operand_constraint)aoc_reg_c;         break;
    case 'd': constraint = (an_asm_operand_constraint)aoc_reg_d;         break;
    case 'S': constraint = (an_asm_operand_constraint)aoc_reg_si;        break;
    case 'D': constraint = (an_asm_operand_constraint)aoc_reg_di;        break;
    case 'R': constraint = (an_asm_operand_constraint)aoc_reg_legacy;    break;
    case 'Q': constraint = (an_asm_operand_constraint)aoc_reg_q;         break;
    case 'A': constraint = (an_asm_operand_constraint)aoc_reg_ad;        break;
    case 't': constraint = (an_asm_operand_constraint)aoc_reg_float_tos; break;
    case 'u': constraint = (an_asm_operand_constraint)aoc_reg_float_second;
                                                                         break;
    case 'x': constraint = (an_asm_operand_constraint)aoc_reg_sse;       break;
    case 'Y': constraint = (an_asm_operand_constraint)aoc_reg_sse2;      break;
    case 'y': constraint = (an_asm_operand_constraint)aoc_reg_mmx;       break;
    /* Immediates */
    case 'I': constraint = (an_asm_operand_constraint)aoc_imm_short_shift;
                                                                         break;
    case 'J': constraint = (an_asm_operand_constraint)aoc_imm_long_shift;
                                                                         break;
    case 'M': constraint = (an_asm_operand_constraint)aoc_imm_lea_shift; break;
    case 'K': constraint = (an_asm_operand_constraint)aoc_imm_signed8;   break;
    case 'N': constraint = (an_asm_operand_constraint)aoc_imm_unsigned8; break;
    case 'L': constraint = (an_asm_operand_constraint)aoc_imm_and_zext;  break;
    case 'G': constraint = (an_asm_operand_constraint)aoc_imm_80387;     break;
    case 'H': constraint = (an_asm_operand_constraint)aoc_imm_sse;       break;
    case 'e': constraint = (an_asm_operand_constraint)aoc_imm_sext32;    break;
    case 'Z': constraint = (an_asm_operand_constraint)aoc_imm_zext32;    break;
#endif /* TARG_IS_X86 */
    case '\0':
      pos_error(ec_missing_constraint_letter, &operand->position);
      goto error_return;
    default:
      errletter[0] = *p;
      pos_st_error(ispunct((unsigned char)*p) ? ec_bad_asm_constraint_modifier
                                              : ec_bad_asm_constraint_letter,
                   &operand->position, errletter);
      goto error_return;
  }  /* switch */
  /* Check that there isn't anything more in the constraint string.
     GCC silently ignores trailing characters, so this is just a warning. */
  if (*++p != '\0') {
    pos_warning(ec_extra_constraint_terms_ignored, &operand->position);
  }  /* if */
  /* Semantic validation. */
  if (output && !(modifiers & (an_asm_operand_modifier)aom_output)) {
    pos_error(ec_asm_output_must_have_output_mod, &operand->position);
    goto error_return;
  } else if (!output && (modifiers & (an_asm_operand_modifier)aom_output)) {
    pos_error(ec_asm_input_must_not_have_output_mod, &operand->position);
    goto error_return;
  }  /* if */
  if (validate_expr_for_constraint(expr, constraint)) {
    operand->expression = expr;
    operand->constraint = constraint;
    operand->modifiers = modifiers;
  } else {
error_return:
    operand->expression = error_node();
    operand->constraint = (an_asm_operand_constraint)aoc_invalid;
    operand->modifiers = (an_asm_operand_modifier)aom_invalid;
  }  /* if */
}  /* process_asm_operand */


/*
Machine-specific tables used by validate_operands_and_clobbers.
*/
typedef struct single_register_constraint {
  /* Structure to hold a constraint-to-register mapping. */
  an_asm_operand_constraint  cons;
  a_named_register           reg;
} single_register_constraint;

static single_register_constraint single_register_constraints[] = {
#if TARG_IS_X86
  { (an_asm_operand_constraint)aoc_reg_a, (a_named_register)anr_a },
  { (an_asm_operand_constraint)aoc_reg_b, (a_named_register)anr_b },
  { (an_asm_operand_constraint)aoc_reg_c, (a_named_register)anr_c },
  { (an_asm_operand_constraint)aoc_reg_d, (a_named_register)anr_d },
  { (an_asm_operand_constraint)aoc_reg_si, (a_named_register)anr_si },
  { (an_asm_operand_constraint)aoc_reg_di, (a_named_register)anr_di },
  /* 't' and 'u' (x86 reg stack) are not included in this list,
     because the rules are not properly handled by the generic code
     below.  Machine-specific code must be written to handle them. */
#endif /* TARG_IS_X86 */
  { (an_asm_operand_constraint)aoc_last, (a_named_register)anr_last }
};

static a_named_register fixed_registers[] = {
#if TARG_IS_X86
  (a_named_register)anr_bp, (a_named_register)anr_sp,
#endif /* TARG_IS_X86 */
  (a_named_register)anr_last
};


void validate_operands_and_clobbers(an_asm_operand_ptr  operands,
                                    int                 num_operands,
                                    a_named_register    *clobbers,
                                    int                 num_clobbers)
/*
Validate the operands and clobbers lists.  The machine-independent
portion of this code simply checks that no duplicates appear in the
clobbers list, no single-register constraint is used when the
corresponding register is clobbered, and no un-clobberable registers
appear in the clobber list.  Back end authors should augment this code
to do complete validation of the lists; it is easy to write an asm
statement with unsatisfiable register allocation requirements that
does not trip over any of the machine-independent checks.

Note that this function never modifies the operands or clobbers lists,
even if they are invalid.
*/
{
  a_byte           regs_clobbered[(int)anr_last];
  a_byte           regs_used[(int)anr_last];
  int              i, j;
  a_named_register r;

  memzero(regs_clobbered, sizeof regs_clobbered);
  memzero(regs_used, sizeof regs_used);
  for (i = 0; i < num_operands; i++) {
    for (j = 0;
         single_register_constraints[j].cons != (a_named_register)anr_last;
         j++) {
      if (operands[i].constraint == single_register_constraints[j].cons) {
        r = single_register_constraints[j].reg;
        /* Test used == 1 so the error is issued once per register. */
        if (regs_used[(int)r] == 1) {
          pos_st_error(ec_register_used_twice, &operands[i].position,
                       named_register_names[(int)r]);
        }  /* if */
        regs_used[(int)r]++;
        break;
      }  /* if */
    }  /* for */
  }  /* for */
  for (i = 0; i < num_clobbers; i++) {
    r = clobbers[i];
    /* Test used and not clobbered so the error is issued once per register. */
    if (regs_used[(int)r] && !regs_clobbered[(int)r]) {
      str_error(ec_register_used_and_clobbered, named_register_names[(int)r]);
    /* Test clobbered == 1 so the error is issued once per register. */
    } else if (regs_clobbered[(int)r] == 1) {
      str_error(ec_register_clobbered_twice, named_register_names[(int)r]);
    }  /* if */
    regs_clobbered[(int)r]++;
  }  /* for */
  for (i = 0; fixed_registers[i] != (a_named_register)anr_last; i++) {
    r = fixed_registers[i];
    if (regs_used[(int)r]) {
      str_error(ec_fixed_register_used, named_register_names[(int)r]);
    } else if (regs_clobbered[(int)r]) {
      str_error(ec_fixed_register_clobbered, named_register_names[(int)r]);
    }  /* if */
  }  /* for */
}  /* validate_operands_and_clobbers */


static void asm_operand (an_asm_operand_ptr operand,
                         a_boolean output)
/*
Scan a single asm-statement operand, writing it into the structure
pointed to by OPERAND.  The syntax is

   string-literal ( expression )
*/
{
  char              *constraint_string = NULL;
  an_expr_node_ptr  expr = NULL;

  db_enter(4, "asm_operand");
  add_stop_token(tok_comma);
  add_stop_token(tok_colon);
  add_stop_token(tok_colon_colon);
  operand->position = pos_curr_token;
  if (curr_token != tok_string_literal) {
    syntax_error(ec_exp_string_literal);
  } else {
    constraint_string = const_for_curr_token.variant.string.value;
    /* Advance past string literal. */
    (void)get_token();
    if (required_token(tok_lparen, ec_exp_lparen)) {
      add_stop_token(tok_rparen);
      expr = scan_asm_operand_expression(output);
      (void)required_token(tok_rparen, ec_exp_rparen);
      remove_stop_token(tok_rparen);
    }  /* if */
  }  /* if */
  process_asm_operand(operand, expr, constraint_string, output);
  remove_stop_token(tok_comma);
  remove_stop_token(tok_colon);
  remove_stop_token(tok_colon_colon);
  db_exit();
}  /* asm_operand */


int asm_operands_spec(an_asm_operand_ptr *p_operands)
/*
Parse and validate a list of asm-statement operands.  This handles
both input and output operands.  The list is written into *p_operands
and the number of operands is returned.  On entry, curr_token is the
leading colon of the operands specification; on exit, it is the leading
colon of the clobbers specification, or the close parenthesis if there
are no clobbers.

The syntax is

    : [operand [, operand...]]   // outputs
   [: [operand [, operand...]]]  // inputs

Since both operand lists can be empty, we must cope with two adjacent
colons, which will be tokenized as a single tok_colon_colon (in C++).
*/
{
  an_asm_operand     operands[10];
  an_asm_operand     overflow;
  an_asm_operand_ptr result = NULL;
  int                n = 0;
  a_boolean          output = TRUE;

  db_enter(3, "asm_operands_spec");
  check_assertion(curr_token == tok_colon || curr_token == tok_colon_colon);
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
  while (curr_token == tok_string_literal) {
    /* There is a hard limit of ten operands per assembly instruction. */
    if (n >= 10) {
      if (n == 10) {
        error(ec_too_many_asm_operands);
      }  /* if */
      asm_operand(&overflow, output);
    } else {
      asm_operand(&operands[n], output);
    }  /* if */
    n++;
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
      if (curr_token != tok_string_literal) {
        syntax_error(ec_exp_asm_operand);
      }  /* if */
    }  /* if */
  }  /* while */
  /* Copy to IL pool and return. */
  if (n > 10) {
    n = 10;
  }  /* if */
  if (n > 0) {
    result = (an_asm_operand_ptr)
      alloc_in_region(curr_il_region_number, n * sizeof(an_asm_operand));
    memcpy(result, operands,
           size_t_arg(n * sizeof(an_asm_operand)));  /*lint !e645*/
  }  /* if */
  *p_operands = result;
  db_exit();
  return n;
}  /* asm_operands_spec */


int asm_clobbers_spec(a_named_register **p_clobbers)
/*
Parse and validate a list of asm-statement clobbers.  This also checks
the clobber list for semantic consistency with the operands list, if
any.  Returns the number of clobbers, places the list of clobbers into
*p_clobbers.

The syntax is

   string-literal [, string-literal ...]

*/
{
  /* There is no hard limit on the number of clobbers. */
  a_named_register                 *clobbers = NULL;
  a_named_register                 reg;
  int                              n = 0, nparsed = 0;
  a_named_register_list_entry_ptr  first_reg = NULL, last_reg = NULL;

  db_enter(3, "asm_clobbers_spec");
  if (curr_token == tok_colon || curr_token == tok_colon_colon) {
    (void)get_token();
    while (curr_token == tok_string_literal) {
      nparsed++;
      reg = name_to_register(const_for_curr_token.variant.string.value);
      if (reg != (a_named_register)anr_invalid) {
        /* Add this register to our list. */
        if (first_reg == NULL) {
          first_reg = last_reg = alloc_named_register_list_entry();
        } else {
          last_reg->next = alloc_named_register_list_entry();
        }  /* if */
        last_reg->reg = reg;
        ++n;
      }  /* if */
      /* advance past string */
      (void)get_token();
      /* next must be comma or right paren */
      if (curr_token == tok_comma) {
        (void) get_token();
        if (curr_token != tok_string_literal) {
          syntax_error(ec_exp_asm_clobber);
        }  /* if */
      }  /* if */
    }  /* while */
    /* GCC treats an empty clobbers list with a colon as a syntax
       error.  We can parse it correctly, so it's semantic for us.
       Don't issue this error if we saw anything other than colon
       immediately followed by right paren. */
    if (curr_token != tok_rparen) {
      syntax_error(ec_exp_rparen);
    } else if (nparsed == 0) {
      error(ec_empty_clobbers_list);
    }  /* if */
  }  /* if */
  /* Copy to IL pool and return. */
  if (n > 0) {
    int i;
    clobbers = (a_named_register *)
         alloc_in_region(curr_il_region_number, n * sizeof(a_named_register));
    for (i = 0; i < n; ++i) {
      a_named_register_list_entry_ptr  to_free = first_reg;
      clobbers[i] = first_reg->reg;
      first_reg = first_reg->next;
      free_named_register_list_entry(to_free);
    }  /* for */
  }  /* if */
  *p_clobbers = clobbers;
  db_exit();
  return n;
}  /* asm_clobbers_spec */


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
  /* Ditto the table of constraint letters. */
  if (asm_operand_constraint_letters[(int)aoc_last] != '~') {
    internal_error(
      "extasm_one_time_init: asm_operand_constraint_letters: bad init");
  }  /* if */
#endif /* CHECKING */
  /* Set up the complete regmap table, including both the official and
     extra register names.  regmap does not include entries for
     anr_invalid or anr_last. */
  regmap_size = (int)anr_last - 1;
  regmap_size += (sizeof(extra_reg_names) / sizeof(struct name_to_reg)) - 1;
  regmap = (struct name_to_reg *)
                      alloc_general(regmap_size * sizeof(struct name_to_reg));
  /* Start with i = 1 since anr_invalid is not copied. */
  for (i = 1; i < (int)anr_last; i++) {
    regmap[i-1].name = named_register_names[i];
    regmap[i-1].reg = i;
  }  /* for */
  memcpy(&regmap[(int)anr_last-1], extra_reg_names,
         regmap_size * sizeof(struct name_to_reg));
  /* name_to_register requires that regmap be sorted. */
  qsort(regmap, (qsort_nmemb_type)regmap_size,
        (qsort_nmemb_type)sizeof(struct name_to_reg), compare_n2r);
  /* Save variables from extasm.h and extasm.c that are needed for
     precompiled headers */
  if (precompiled_header_processing_required) {
    static a_pch_saved_variable saved_vars[] = {
      pch_saved_var_array_elem(avail_named_register_list_entries),
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
* Copyright 2001 Edison Design Group Inc.                        [_]          *
*                                                                             *
******************************************************************************/
