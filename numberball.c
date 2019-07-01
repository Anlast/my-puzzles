#include <stdio.h>
#include <stdarg.h>

#include "puzzles.h"
#include "latin.h"

/* 
 * MSVC compile:
 * cl numberball.c latin.c malloc.c /nologo /Zi /DSEMI_LATIN /DSTANDALONE_SOLVER
 */

/*
 * Difficulty levels. Same as towers.c.
 */
#define DIFFLIST(A) \
    A(EASY,Easy,solver_easy,e) \
    A(HARD,Hard,solver_hard,h) \
    A(EXTREME,Extreme,NULL,x) \
    A(UNREASONABLE,Unreasonable,NULL,u)
#define ENUM(upper,title,func,lower) DIFF_ ## upper,
#define TITLE(upper,title,func,lower) #title,
#define ENCODE(upper,title,func,lower) #lower
#define CONFIG(upper,title,func,lower) ":" #title
enum { DIFFLIST(ENUM) DIFFCOUNT };
static char const *const numberball_diffnames[] = { DIFFLIST(TITLE) };
static char const numberball_diffchars[] = DIFFLIST(ENCODE);
#define DIFFCONFIG DIFFLIST(CONFIG)

static usersolver_t const numberball_solvers[DIFFCOUNT];

/*
 * Only partial latin square deductions are required for this puzzle.
 */
static int solver(digit *grid, bool *force, bool *forbid, int o, int depth, int maxdiff)
{			   
	int diff = latin_solver(grid, o, depth, force, forbid, maxdiff, 
						DIFF_EASY, DIFF_HARD, DIFF_EXTREME,
						DIFF_EXTREME, DIFF_UNREASONABLE,
						numberball_solvers, NULL, NULL, NULL);
			   
    return diff;
}

		
#ifdef STANDALONE_SOLVER

solver_show_working = 2;

void fatal(const char *fmt, ...)
{
    char buf[2048];
    va_list ap;

    va_start(ap, fmt);
    vsprintf(buf, fmt, ap);
    va_end(ap);

    exit(1);
}

#if 1
	/* 
	 * janko #19: set eliminatuin works
	 * when set count >= 1 (see line 464 of latin.c),
	 * it's a very simple elimination we can get from cells (4,1) and (4,2).
	 */
	#define SIZE 7
	#define DEPTH 4
	char game_desc[] = 
	"3 O - O O - -"
	"O 3 - - O - -"
	"- - X - X - 4"
	"O O - - - - -"
	"- O - - 1 O 2"
	"- - - 2 - - -"
	"- - 1 - - - O";
#endif

#if 0
	/* 
	 * janko #70: set elimination works with count > 1 
	 */
	#define SIZE 7
	#define DEPTH 4
	char game_desc[] =
	"- 1 - 4 - - -"
	"O 4 - 3 - - -"
	"- - - - O O O"
	"O - O - O 3 -"
	"- - 3 - 1 O O"
	"O 3 - - 4 - -"
	"- - X - - O O";
#endif

#define AREA SIZE*SIZE
int main()
{
	char *p;
	int o, depth, i, x, y, diff;

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
	
	diff = solver(grid, forcevals, forbidvals, SIZE, DEPTH, DIFFCOUNT-1);
	printf("difficulty: %s\n", numberball_diffnames[diff]);
	
	return 0;
}
#endif
