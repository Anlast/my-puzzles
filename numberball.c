#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <math.h>

#include <stdarg.h>

#include "puzzles.h"
#include "latin.h"

/* 
 * MSVC compile:
 * cl nanbaboru.c latin.c malloc.c /nologo /Zi /DSEMI_LATIN /DSTANDALONE_SOLVER /DNANBABORU_TEST /DSOLVER_SET /DSOLVER_FORCING
 */
 
void debug_force_forbid(struct latin_solver *solver)
{
	int x, y, o = solver->o;
	for(y = 0; y < o; y++)
	{
	for(x = 0; x < o; x++)
		printf("%c ", solver->forbid[y*o+x] ? 'X' : (solver->force[y*o+x]) ? 'O' : '-'); 
	printf("\n");
	}
}

/*
 * Test solver
 */
/* 
 * TODO: add a very simple recursion which tries a value in a forced cell 
 * and which would make all other cells blank
 */
void nanbaboru_solver(digit *grid, bool *force, bool *forbid, int o, int depth)
{
    struct latin_solver *solver;
	int ret;
	int i, x, y, n;
	char *text;
	
	solver = malloc(sizeof(struct latin_solver));
	latin_solver_alloc(solver, grid, o, depth);
	
	#ifdef STANDALONE_SOLVER
	if (!solver->names) {
	char *p;
	int i;

	text = snewn(40 * o, char);
	p = text;

	solver->names = snewn(o, char *);

	for (i = 0; i < o; i++) {
	    solver->names[i] = p;
	    p += 1 + sprintf(p, "%d", i+1);
	}
    }
	#endif
	
	/* Allocating initial game data */
	for(y = 0; y < o; y++)
	for(x = 0; x < o; x++)
	if(forbid[y*o+x]) {
		solver->forbid[y*o+x] = true;
		
		for (n = 1; n <= depth; n++)
            cube(x,y,n) = false;
		}
		
	for(y = 0; y < o; y++)
	for(x = 0; x < o; x++)
	if(force[y*o+x])
		solver->force[y*o+x] = true;
	
	/* Solving */
	struct latin_solver_scratch *scratch = latin_solver_new_scratch(solver);
	
	do{
	ret = 0;
	while(latin_solver_diff_simple(solver) == 1)
		ret |= 1;
	
	#ifdef SOLVER_SET
	debug_force_forbid(solver);
	latin_solver_debug(solver->cube, o);
	while(latin_solver_diff_set(solver, scratch, false) == 1)
		ret |= 1;
	#endif
	
	#ifdef SOLVER_FORCING
	debug_force_forbid(solver);
	latin_solver_debug(solver->cube, o);
	while(latin_solver_forcing(solver, scratch) == 1)
		ret |= 1;
	#endif
	}while(ret);
	latin_solver_free_scratch(scratch);
	
	debug_force_forbid(solver);
	latin_solver_debug(solver->cube, o);
	latin_debug(grid, o);
	
	latin_solver_free(solver);
	sfree(solver);
	sfree(text);
}
		
#ifdef NANBABORU_TEST

solver_show_working = 2;
solver_recurse_depth = 1;

void fatal(const char *fmt, ...)
{
    char buf[2048];
    va_list ap;

    va_start(ap, fmt);
    vsprintf(buf, fmt, ap);
    va_end(ap);

    exit(1);
}

#define AREA SIZE*SIZE
int main()
{
#if 1
	#define SIZE 7
	#define DEPTH 4
	char game_desc[] = /* 19: set eliminatuin works with count == 1 but not if count > 1,
	                    * see line 432 of latin.c */
	"3 O - O O - -"
	"O 3 - - O - -"
	"- - X - X - 4"
	"O O - - - - -"
	"- O - - 1 O 2"
	"- - - 2 - - -"
	"- - 1 - - - O";
#endif
#if 0
	#define SIZE 9
	#define DEPTH 5
	char game_desc[] = /* 30: set elimination works too, haven't checked count here */
	"2 - X O - - X 5 -"
	"- - X - - - X - -"
	"- O - O - 4 - - O"
	"- 4 X - - - - - -"
	"3 X - - - X 4 O -"
	"O - - O O - 3 - -"
	"O - - 5 - X - - -"
	"- 3 - O - O - O 5"
	"- X 3 - - X 2 - X";
#endif
#if 0
	#define SIZE 9
	#define DEPTH 5
	char game_desc[] = /* 129: solved with forcing chain */
	"O 5 - O 3 4 - - -"
	"- - - - X - - X X"
	"- - - 3 - O - O X"
	"4 - - - - O - - 5"
	"- 2 - - O - O O -"
	"X X X - - - 2 - 1"
	"O - 3 - - 5 4 O -"
	"- - - - 4 - 5 O -"
	"X - - X - - O - -";
#endif
	char *p;
	int o, depth, i, x, y;
	struct latin_solver solver;
	digit grid[AREA];
	bool forcevals[AREA];
	bool forbidvals[AREA];
	memset(grid, 0, AREA);
	memset(forcevals, 0, AREA);
	memset(forbidvals, 0, AREA);
	o = SIZE;
	depth = DEPTH;
	
	i = 0;
	p = game_desc;
	for( ; *p != '\0'; p++)
	{
		if(isdigit(*p))
			grid[i++] = *p - '0';
		else if(*p == 'X')
			forbidvals[i++] = true;
		else if(*p == 'O')
			forcevals[i++] = true;
		else if(*p == '-')
			i++;
	}
	
	nanbaboru_solver(grid, forcevals, forbidvals, o, DEPTH);
	
	return 0;
}
#endif
