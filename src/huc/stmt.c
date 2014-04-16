/*	File stmt.c: 2.1 (83/03/20,16:02:17) */
/*% cc -O -c %
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "defs.h"
#include "data.h"
#include "code.h"
#include "error.h"
#include "expr.h"
#include "gen.h"
#include "io.h"
#include "lex.h"
#include "preproc.h"
#include "primary.h"
#include "stmt.h"
#include "sym.h"
#include "while.h"
#include "struct.h"
#include "optimize.h"

/*
 *	statement parser
 *
 *	called whenever syntax requires a statement.  this routine
 *	performs that statement and returns a number telling which one
 *
 *	'func' is true if we require a "function_statement", which
 *	must be compound, and must contain "statement_list" (even if
 *	"declaration_list" is omitted)
 */

long statement (long func)
{
	if ((ch () == 0) & feof (input))
		return (0);
	lastst = 0;
	if (func)
	{
	        top_level_stkp = 1;	/* uninitialized */
		if (match ("{")) {
			compound (YES);
			return (lastst);
		} else
			error ("function requires compound statement");
	}
	if (match ("{"))
		compound (NO);
	else
		stst ();
	return (lastst);
}

/*
 *	declaration
 */
long stdecl (void)
{
	if (amatch("register", 8))
		doldcls(DEFAUTO);
	else if (amatch("auto", 4))
		doldcls(DEFAUTO);
	else if (amatch("static", 6))
		doldcls(LSTATIC);
	else if (doldcls(AUTO)) ;
	else
		return (NO);
	return (YES);
}

long doldcls(long stclass)
{
        struct type t;
	blanks();
	/* we don't do optimizations that would require "volatile" */
	if (match_type(&t, NO, YES)) {
		if (t.type == CSTRUCT && t.otag == -1)
			t.otag = define_struct(t.sname, stclass, !!(t.flags & F_STRUCT));
		if (t.type == CVOID) {
			blanks();
			if (ch() != '*') {
				error("illegal type \"void\"");
				junk();
				return 0;
			}
		}
		declloc(t.type, stclass, t.otag);
	}
	else
		return(0);
	ns();
	return(1);
}


/*
 *	non-declaration statement
 */
void stst (void )
{
	if (amatch ("if", 2)) {
		doif ();
		lastst = STIF;
	} else if (amatch ("while", 5)) {
		dowhile ();
		lastst = STWHILE;
	} else if (amatch ("switch", 6)) {
		doswitch ();
		lastst = STSWITCH;
	} else if (amatch ("do", 2)) {
		dodo ();
		ns ();
		lastst = STDO;
	} else if (amatch ("for", 3)) {
		dofor ();
		lastst = STFOR;
	} else if (amatch ("return", 6)) {
		doreturn ();
		ns ();
		lastst = STRETURN;
	} else if (amatch ("break", 5)) {
		dobreak ();
		ns ();
		lastst = STBREAK;
	} else if (amatch ("continue", 8)) {
		docont ();
		ns ();
		lastst = STCONT;
	} else if (amatch ("goto", 4)) {
		dogoto();
		ns();
		lastst = STGOTO;
	} else if (match (";"))
		;
	else if (amatch ("case", 4)) {
		docase ();
		lastst = statement (NO);
	} else if (amatch ("default", 7)) {
		dodefault ();
		lastst = statement (NO);
	} else if (match ("#asm")) {
		doasm ();
		lastst = STASM;
	} else if (match ("{"))
		compound (NO);
	else {
		int slptr = lptr;
		char lbl[NAMESIZE];
		if (symname(lbl) && ch() == ':') {
			gch();
			dolabel(lbl);
			lastst = statement(NO);
		}
		else {
			lptr = slptr;
			expression (YES);
			ns ();
			lastst = STEXP;
		}
	}
}

/*
 *	compound statement
 *
 *	allow any number of statements to fall between "{" and "}"
 *
 *	'func' is true if we are in a "function_statement", which
 *	must contain "statement_list"
 */
void compound (long func)
{
	long	decls;

	decls = YES;
	ncmp++;
	/* remember stack pointer before entering the first compound
	   statement inside a function */
	if (!func && top_level_stkp == 1 && !norecurse)
	        top_level_stkp = stkp;
	while (!match ("}")) {
		if (feof (input))
			return;
		if (decls) {
			if (!stdecl ()) {
				decls = NO;
				gtext ();
			}
		} else
			stst ();
	}
	ncmp--;
}

/*
 *	"if" statement
 */
void doif (void )
{
	long	fstkp, flab1, flab2;
	SYMBOL	*flev;

	flev = locptr;
	fstkp = stkp;
	flab1 = getlabel ();
	test (flab1, FALSE);
	statement (NO);
	stkp = modstk (fstkp);
	locptr = flev;
	if (!amatch ("else", 4)) {
		gnlabel (flab1);
		return;
	}
	jump (flab2 = getlabel ());
	gnlabel (flab1);
	statement (NO);
	stkp = modstk (fstkp);
	locptr = flev;
	gnlabel (flab2);
}

/*
 *	"while" statement
 */
void dowhile (void )
{
	long	ws[7];

	ws[WSSYM] = (long)locptr;
	ws[WSSP] = stkp;
	ws[WSTYP] = WSWHILE;
	ws[WSTEST] = getlabel ();
	ws[WSEXIT] = getlabel ();
	addwhile (ws);
	gnlabel (ws[WSTEST]);
	test (ws[WSEXIT], FALSE);
	statement (NO);
	jump (ws[WSTEST]);
	gnlabel (ws[WSEXIT]);
	locptr = (SYMBOL*)ws[WSSYM];
	stkp = modstk (ws[WSSP]);
	delwhile ();
}

/*
 *	"do" statement
 */
void dodo (void )
{
	long	ws[7];

	ws[WSSYM] = (long)locptr;
	ws[WSSP] = stkp;
	ws[WSTYP] = WSDO;
	ws[WSBODY] = getlabel ();
	ws[WSTEST] = getlabel ();
	ws[WSEXIT] = getlabel ();
	addwhile (ws);
	gnlabel (ws[WSBODY]);
	statement (NO);
	if (!match ("while")) {
		error ("missing while");
		return;
	}
	gnlabel (ws[WSTEST]);
	test (ws[WSBODY], TRUE);
	gnlabel (ws[WSEXIT]);
	locptr = (SYMBOL*)ws[WSSYM];
	stkp = modstk (ws[WSSP]);
	delwhile ();
}

/*
 *	"for" statement
 */
void dofor (void )
{
	long	ws[7],
		*pws;

	ws[WSSYM] = (long)locptr;
	ws[WSSP] = stkp;
	ws[WSTYP] = WSFOR;
	ws[WSTEST] = getlabel ();
	ws[WSINCR] = getlabel ();
	ws[WSBODY] = getlabel ();
	ws[WSEXIT] = getlabel ();
	addwhile (ws);
	pws = readwhile ();
	needbrack ("(");
	if (!match (";")) {
		expression (YES);
		ns ();
	}
	gnlabel (pws[WSTEST]);
	if (!match (";")) {
		expression (YES);
		testjump (pws[WSBODY], TRUE);
		jump (pws[WSEXIT]);
		ns ();
	} else
		pws[WSTEST] = pws[WSBODY];
	gnlabel (pws[WSINCR]);
	if (!match (")")) {
		expression (YES);
		needbrack (")");
		jump (pws[WSTEST]);
	} else
		pws[WSINCR] = pws[WSTEST];
	gnlabel (pws[WSBODY]);
	statement (NO);
	stkp = modstk (pws[WSSP]);
	jump (pws[WSINCR]);
	gnlabel (pws[WSEXIT]);
	locptr = (SYMBOL*)pws[WSSYM];
	delwhile ();
}

/*
 *	"switch" statement
 */
void doswitch (void )
{
	long	ws[7];
	long	*ptr;

	ws[WSSYM] = (long)locptr;
	ws[WSSP] = stkp;
	ws[WSTYP] = WSSWITCH;
	ws[WSCASEP] = swstp;
	ws[WSTAB] = getlabel ();
	ws[WSDEF] = ws[WSEXIT] = getlabel ();
	addwhile (ws);
	immed (T_LABEL, ws[WSTAB]);
	gpush ();
	needbrack ("(");
	expression (YES);
	needbrack (")");
	stkp = stkp + INTSIZE;  /* '?case' will adjust the stack */
	gjcase ();
	statement (NO);
	ptr = readswitch ();
	jump (ptr[WSEXIT]);
	dumpsw (ptr);
	gnlabel (ptr[WSEXIT]);
	locptr = (SYMBOL*)ptr[WSSYM];
	stkp = modstk (ptr[WSSP]);
	swstp = ptr[WSCASEP];
	delwhile ();
}

/*
 *	"case" label
 */
void docase (void )
{
	long	val;

	val = 0;
	if (readswitch ()) {
		if (!number (&val))
			if (!pstr (&val))
				error ("bad case label");
		addcase (val);
		if (!match (":"))
			error ("missing colon");
	} else
		error ("no active switch");
}

/*
 *	"default" label
 */
void dodefault (void )
{
	long	*ptr,
		lab;

	ptr = readswitch ();
	if (ptr) {
		ptr[WSDEF] = lab = getlabel ();
		gnlabel (lab);
		if (!match (":"))
			error ("missing colon");
	} else
		error ("no active switch");
}

/*
 *	"return" statement
 */
void doreturn (void )
{
        /* Jumping to fexitlab will clean up the top-level stack,
           but we may have added additional stack variables in
           an inner compound statement, and if so we have to
           clean those up as well. */
        if (top_level_stkp != 1 && stkp != top_level_stkp)
                modstk(top_level_stkp);

	if (endst () == 0)
		expression (YES);

	jump(fexitlab);
}

/*
 *	"break" statement
 */
void dobreak (void )
{
	long	*ptr;

	if ((ptr = readwhile ()) == 0)
		return;
	modstk (ptr[WSSP]);
	jump (ptr[WSEXIT]);
}

/*
 *	"continue" statement
 */
void docont (void )
{
	long	*ptr;

	if ((ptr = findwhile ()) == 0)
		return;
	modstk (ptr[WSSP]);
	if (ptr[WSTYP] == WSFOR)
		jump (ptr[WSINCR]);
        	else
		jump (ptr[WSTEST]);
}

void dolabel (char *name)
{
	int i;
	for (i = 0; i < clabel_ptr; i++) {
		if (!strcmp(clabels[i].name, name)) {
			/* This label has been goto'd to before.
			   We have to create a stack pointer offset EQU
			   that describes the stack pointer difference from
			   the goto to here. */
			sprintf(name, "LL%d_stkp", clabels[i].label);
			/* XXX: memleak */
			out_ins_ex(I_DEF, T_LITERAL, (long)strdup(name),
				   T_VALUE, stkp - clabels[i].stkp);
			/* From now on, clabel::stkp contains the relative
			   stack pointer at the location of the label. */
			clabels[i].stkp = stkp;
			gnlabel(clabels[i].label);
			printf("old label %s stkp %ld\n", clabels[i].name, stkp);
			return;
		}
	}
	/* This label has not been referenced before, we need to create a
	   new entry. */
	clabels = realloc(clabels, (clabel_ptr+1) * sizeof(struct clabel));
	strcpy(clabels[clabel_ptr].name, name);
	clabels[clabel_ptr].stkp = stkp;
	clabels[clabel_ptr].label = getlabel();
	printf("new label %s id %d stkp %ld\n", name, clabels[clabel_ptr].label, stkp);
	gnlabel(clabels[clabel_ptr].label);
	clabel_ptr++;
}

void dogoto (void)
{
	int i;
	char sname[NAMESIZE];
	if (!symname(sname)) {
		error("invalid label name");
		return;
	}
	for (i = 0; i < clabel_ptr; i++) {
		if (!strcmp(clabels[i].name, sname)) {
			/* This label has already been defined. All we have
			   to do is to adjust the stack pointer and jump. */
			printf("goto found label %s id %d stkp %d\n", sname, clabels[i].label, clabels[i].stkp);
			modstk(clabels[i].stkp);
			jump(clabels[i].label);
			return;
		}
	}

	/* This label has not been defined yet. We have to create a new
	   entry in the label array. We also don't know yet what the relative
	   SP will be at the label, so we have to emit a stack pointer
	   operation with a symbolic operand that will be defined to the
	   appropriate value at the label definition. */
	clabels = realloc(clabels, (i+1) * sizeof(*clabels));
	strcpy(clabels[i].name, sname);
	/* Save our relative SP in the label entry; this will be corrected
	   to the label's relative SP at the time of definition. */
	clabels[i].stkp = stkp;
	clabels[i].label = getlabel();
	sprintf(sname, "LL%d_stkp", clabels[i].label);
	/* XXX: memleak */
	out_ins(I_ADDMI, T_LITERAL, (long)strdup(sname));
	jump(clabels[i].label);
	clabel_ptr++;
}


/*
 *	dump switch table
 */
void dumpsw (long *ws)
/*long	ws[];*/
{
	long	i,j;

//	gdata ();
	gnlabel (ws[WSTAB]);
	flush_ins();
	if (ws[WSCASEP] != swstp) {
		j = ws[WSCASEP];
		while (j < swstp) {
			defword ();
			i = 4;
			while (i--) {
				outdec (swstcase[j]);
				outbyte (',');
				outlabel (swstlab[j++]);
				if ((i == 0) | (j >= swstp)) {
					nl ();
					break;
				}
				outbyte (',');
			}
		}
	}
	defword ();
	outlabel (ws[WSDEF]);
	outstr (",0");
	nl ();
//	gtext ();
}

void test (long label, long ft)
/* long	label,
	ft; */
{
	needbrack ("(");
	expression (YES);
	needbrack (")");
	testjump (label, ft);
}

