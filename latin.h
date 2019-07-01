#ifndef LATIN_H
#define LATIN_H

#include "puzzles.h"

typedef unsigned char digit;

/* --- Solver structures, definitions --- */

#ifdef STANDALONE_SOLVER
extern int solver_show_working, solver_recurse_depth;
#endif

struct latin_solver {
  int o;                /* order of latin square */
#ifdef SEMI_LATIN
  int depth;			/* depth of latin square */
#endif
  unsigned char *cube;  /* o^3, indexed by x, y, and digit:
                           true in that position indicates a possibility */
  digit *grid;          /* o^2, indexed by x and y: for final deductions */

  unsigned char *row;   /* o^2: row[y*cr+n-1] true if n is in row y */
  unsigned char *col;   /* o^2: col[x*cr+n-1] true if n is in col x */

#ifdef SEMI_LATIN
  bool *force;			/* o^2: force[y*cr+x] true if cell must contain a value */
  bool *forbid;			/* o^2: forbid[y*cr+x] true if cell must be blank */
#endif

#ifdef STANDALONE_SOLVER
  char **names;         /* o: names[n-1] gives name of 'digit' n */
#endif
};
#define cubepos(x,y,n) (((x)*solver->o+(y))*solver->o+(n)-1)
#define cube(x,y,n) (solver->cube[cubepos(x,y,n)])

#define gridpos(x,y) ((y)*solver->o+(x))
#define grid(x,y) (solver->grid[gridpos(x,y)])


/* --- Solver individual strategies --- */

#ifdef SEMI_LATIN
/* Figure out which cells must have or _not_ have a value */
int latin_solver_distribute_force_forbid(struct latin_solver *solver);
#endif

/* Place a value at a specific location. */
void latin_solver_place(struct latin_solver *solver, int x, int y, int n);

/* Positional elimination. */
int latin_solver_elim(struct latin_solver *solver, int start, int step
#ifdef STANDALONE_SOLVER
                      , const char *fmt, ...
#endif
                      );

struct latin_solver_scratch; /* private to latin.c */
/* Set elimination */
int latin_solver_set(struct latin_solver *solver,
                     struct latin_solver_scratch *scratch,
                     int start, int step1, int step2
#ifdef STANDALONE_SOLVER
                     , const char *fmt, ...
#endif
                     );

/* Forcing chains */
int latin_solver_forcing(struct latin_solver *solver,
                         struct latin_solver_scratch *scratch);


/* --- Solver allocation --- */

/* Fills in (and allocates members for) a latin_solver struct.
 * Will allocate members of snew, but not snew itself
 * (allowing 'struct latin_solver' to be the first element in a larger
 * struct, for example). */
void latin_solver_alloc(struct latin_solver *solver, digit *grid, int o
#ifdef SEMI_LATIN
						, int depth, bool *force, bool *forbid
#endif
						);
void latin_solver_free(struct latin_solver *solver);

/* Allocates scratch space (for _set and _forcing) */
struct latin_solver_scratch *
  latin_solver_new_scratch(struct latin_solver *solver);
void latin_solver_free_scratch(struct latin_solver_scratch *scratch);


/* --- Solver guts --- */

/* Looped positional elimination */
int latin_solver_diff_simple(struct latin_solver *solver);

/* Looped set elimination; extreme permits use of the more difficult
 * single-number elimination. */
int latin_solver_diff_set(struct latin_solver *solver,
                          struct latin_solver_scratch *scratch,
                          bool extreme);

typedef int (*usersolver_t)(struct latin_solver *solver, void *ctx);
typedef void *(*ctxnew_t)(void *ctx);
typedef void (*ctxfree_t)(void *ctx);

/* Individual puzzles should use their enumerations for their
 * own difficulty levels, ensuring they don't clash with these. */
enum { diff_impossible = 10, diff_ambiguous, diff_unfinished };

/* Externally callable function that allocates and frees a latin_solver */
int latin_solver(digit *grid, int o
#ifdef SEMI_LATIN
	   , int depth, bool *force, bool *forbid
#endif
	   , int maxdiff, int diff_simple, int diff_set_0, int diff_set_1,
		 int diff_forcing, int diff_recursive,
		 usersolver_t const *usersolvers, void *ctx,
		 ctxnew_t ctxnew, ctxfree_t ctxfree);

/* Version you can call if you want to alloc and free latin_solver yourself */
int latin_solver_main(struct latin_solver *solver, int maxdiff,
		      int diff_simple, int diff_set_0, int diff_set_1,
		      int diff_forcing, int diff_recursive,
		      usersolver_t const *usersolvers, void *ctx,
		      ctxnew_t ctxnew, ctxfree_t ctxfree);

#ifdef SEMI_LATIN
void latin_solver_debug_force_forbid(int o, int depth, bool *force, bool *forbid);
#endif
void latin_solver_debug(unsigned char *cube, int o
#ifdef SEMI_LATIN
						  , int depth
#endif
);

/* --- Generation and checking --- */

digit *latin_generate(int o, random_state *rs);

/* The order of the latin rectangle is max(w,h). */
digit *latin_generate_rect(int w, int h, random_state *rs);

bool latin_check(digit *sq, int order); /* true => not a latin square */

void latin_debug(digit *sq, int order);

#endif
