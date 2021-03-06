/*      File code.h: 2.2 (84/11/27,16:26:11) */
/*% cc -O -c %
 *
 */

#ifndef _CODE_H
#define _CODE_H

#include "defs.h"

extern long segment;

void gdata (void);
void gtext (void);
void header (void);
void inc_startup (void);
void asmdefines (void);
void defbyte (void);
void defstorage (void);
void defword (void);
void out_ins (long code, long type, long data);
void out_ins_ex (long code, long type, long data, int imm_type, long imm);
void out_ins_sym (long code, long type, long data, SYMBOL *sym);
void gen_ins (INS *tmp);
void gen_code (INS *tmp);

void dump_ins (INS *tmp);

#endif
