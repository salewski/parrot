/*

Copyright (C) 2001-2014, Parrot Foundation.

=head1 NAME

parrot_debugger - The Parrot Debugger

=head1 DESCRIPTION

The Parrot debugger

=head1 SYNOPSIS

    parrot_debugger programfile
    parrot_debugger --script scriptfile programfile

=head1 COMMANDS

=over 4

=item C<disassemble>

Disassemble the bytecode.

Use this if you have a PBC file but not the PASM.

=item C<load>

Load a source code file.

=item C<list> or C<l>

List the source code file.

=item C<run> or C<r>

Run the program.

=item C<break> or C<b>

Add a breakpoint at the line number given or the current line if none is given.

    (pdb) b
    Breakpoint 1 at pos 0

    (pdb) b 10
    Breakpoint 1 at pos 10

=item C<watch> or C<w>

Add a watchpoint.

=item C<delete> or C<d>

Given a number n, deletes the n-th breakpoint. To delete the first breakpoint:

    (pdb) d 1
    Breakpoint 1 deleted

=item C<disable>

Disable a breakpoint.

=item C<enable>

Re-enable a disabled breakpoint.

=item C<continue> or C<c>

Continue the program execution.

=item C<next> or C<n>

Run the next instruction

=item C<eval> or C<e>

Run an instruction.

=item C<trace> or C<t>

Trace the next instruction. This is equivalent to printing the source of the
next instruction and then executing it.

=item C<print> or C<p>

Print an interpreter register. If a register I0 has been used, this
would look like:

    (pdb) p I0
    2

If no register number is given then all registers of that type will be printed.
If the two registers I0 and I1 have been used, then this would look like:

    (pdb) p I
    I0 = 2
    I1 = 5

It would be nice if "p" with no arguments printed all registers, but this is
currently not the case.

=item C<stack> or C<s>

Examine the stack.

=item C<info>

Print interpreter information relating to memory allocation and garbage
collection.

=item C<gcdebug>

Toggle garbage collection debugging mode.  In gcdebug mode a garbage collection
cycle is run before each opcode, which is the same as using the gcdebug core.

=item C<quit> or C<q>

Exit the debugger.

=item C<help> or C<h>

Print the help.

=back

=head2 Debug Ops

You can also debug Parrot code by using the C<debug_init>, C<debug_load>
and C<debug_break> ops in F<ops/debug.ops>.

=over 4

=cut

*/

#define PARROT_IN_EXTENSION

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "parrot/parrot.h"
#include "parrot/debugger.h"
#include "parrot/runcore_api.h"

const unsigned char * Parrot_get_config_hash_bytes(void);
int Parrot_get_config_hash_length(void);

/* HEADERIZER HFILE: none */

/* HEADERIZER BEGIN: static */
/* Don't modify between HEADERIZER BEGIN / HEADERIZER END.  Your changes will be lost. */

static void PDB_printwelcome(void);
static void PDB_run_code(PARROT_INTERP, int argc, ARGIN(const char *argv[]))
        __attribute__nonnull__(1)
        __attribute__nonnull__(3);

#define ASSERT_ARGS_PDB_printwelcome __attribute__unused__ int _ASSERT_ARGS_CHECK = (0)
#define ASSERT_ARGS_PDB_run_code __attribute__unused__ int _ASSERT_ARGS_CHECK = (\
       PARROT_ASSERT_ARG(interp) \
    , PARROT_ASSERT_ARG(argv))
/* Don't modify between HEADERIZER BEGIN / HEADERIZER END.  Your changes will be lost. */
/* HEADERIZER END: static */


/*

=item C<int main(int argc, const char *argv[])>

Reads the PIR, PASM, or PBC file from argv[1], loads it, and then calls
Parrot_debug().

=cut

*/

int
main(int argc, const char *argv[])
{
    int nextarg;
    Parrot_Interp     interp;
    PDB_t *pdb;
    const char       *scriptname = NULL;

    interp = Parrot_interp_new(NULL);

    Parrot_debugger_init(interp);
    pdb = interp->pdb;
    pdb->state       = PDB_ENTER;

    Parrot_block_GC_mark(interp);
    Parrot_block_GC_sweep(interp);

    nextarg = 1;
    if (argv[nextarg] && strcmp(argv[nextarg], "--script") == 0) {
        scriptname = argv [++nextarg];
        ++nextarg;
    }

    if (argv[nextarg]) {
        const char * const filename = argv[nextarg];

        if (*filename == '-') {
            fprintf(stderr, "parrot_debugger takes no -x or --xxxx flag arguments\n");
            exit(1);
        }
        else {
            STRING *   const filename_str = Parrot_str_new(interp, filename, 0);
            PackFile * const pfraw        = Parrot_pf_read_pbc_file(interp, filename_str);
            Parrot_PackFile pf;

            if (pfraw == NULL)
                return 1;

            pf = Parrot_pf_get_packfile_pmc(interp, pfraw, filename_str);
            if (pf == NULL)
                return 1;

            Parrot_pf_set_current_packfile(interp, pf);
            Parrot_pf_prepare_packfile_init(interp, pf);
        }
    }
    else {
        /* Generate some code to be able to enter into runloop */
        STRING * const compiler_s = Parrot_str_new_constant(interp, "PIR");
        PMC *    const compiler   = Parrot_interp_get_compiler(interp, compiler_s);
        STRING * const source     = Parrot_str_new_constant(interp,
                                        ".sub aux :main\nexit 0\n.end\n");
        PMC *    const code       = Parrot_interp_compile_string(interp, compiler, source);

        if (PMC_IS_NULL(code))
            Parrot_warn(interp, PARROT_WARNINGS_NONE_FLAG,
                "Unexpected compiler problem at debugger start");
    }

    Parrot_unblock_GC_mark(interp);
    Parrot_unblock_GC_sweep(interp);

    if (scriptname)
        PDB_script_file(interp, scriptname);
    else
        PDB_printwelcome();

    Parrot_runcore_switch(interp, Parrot_str_new_constant(interp, "debugger"));
    PDB_run_code(interp, argc - nextarg, argv + nextarg);

    Parrot_x_exit(interp, 0);
}


/*

=item C<static void PDB_run_code(PARROT_INTERP, int argc, const char *argv[])>

Runs the code, catching exceptions if they are left unhandled.

=cut

*/

static void
PDB_run_code(PARROT_INTERP, int argc, ARGIN(const char *argv[]))
{
    ASSERT_ARGS(PDB_run_code)
    UNUSED(argc)
    UNUSED(argv)

    Parrot_runloop_new_jump_point(interp);
    if (setjmp(interp->current_runloop->resume)) {
        Parrot_runloop_free_jump_point(interp);
        fprintf(stderr, "Caught exception\n");
        return;
    }

    /* Loop to avoid exiting at program end */
    do {
        /* TODO: Replace this with Parrot_pf_execute_bytecode_program */
        /*Parrot_runcode(interp, argc, argv);*/
        interp->pdb->state |= PDB_STOPPED;
    } while (! (interp->pdb->state & PDB_EXIT));
    Parrot_runloop_free_jump_point(interp);
}


/*

=item C<static void PDB_printwelcome(void)>

Prints out the welcome string.

=cut

*/

static void
PDB_printwelcome(void)
{
    ASSERT_ARGS(PDB_printwelcome)
    fprintf(stderr,
        "Parrot " PARROT_VERSION " Debugger\n"
        "(Please note: the debugger is currently under reconstruction)\n");
}

/*

=back

=head1 SEE ALSO

F<src/debug.c>, F<include/parrot/debug.h>.

=head1 HISTORY

=over 4

=item * Initial version by Daniel Grunblatt on 2002.5.19.

=item * Start of rewrite - leo 2005.02.16

The debugger now uses its own interpreter. User code is run in
Interp* debugee. We have:

  debug_interp->pdb->debugee->debugger
    ^                            |
    |                            v
    +------------- := -----------+

Debug commands are mostly run inside the C<debugger>. User code
runs of course in the C<debugee>.

=back

=head1 TODO

=over 4

=item * Check the user input for bad commands, it's quite easy to make
it bang now, try listing the source before loading or disassembling it.

=item * Print the interpreter info.

=item * Make the user interface better (add command history/completion).

=back

=head1 HISTORY

Renamed from F<pdb.c> on 2008.7.15

=cut

*/


/*
 * Local variables:
 *   c-file-style: "parrot"
 * End:
 * vim: expandtab shiftwidth=4 cinoptions='\:2=2' :
 */
