#include <assert.h>
#include <string.h>
#include <stdarg.h>

#include "puzzles.h"
#include "tree234.h"
#include "matching.h"

#ifdef STANDALONE_LATIN_TEST
#define STANDALONE_SOLVER
#endif

#include "latin.h"

/* --------------------------------------------------------
 * Solver.
 */

static int latin_solver_top(struct latin_solver *solver, int maxdiff,
			    int diff_simple, int diff_set_0, int diff_set_1,
			    int diff_forcing, int diff_recursive,
			    usersolver_t const *usersolvers, void *ctx,
			    ctxnew_t ctxnew, ctxfree_t ctxfree);

#ifdef STANDALONE_SOLVER
int solver_show_working, solver_recurse_depth;
#endif

#ifdef SEMI_LATIN
/* 
 * First it is necessary to deduce in which cells we must or musn't place a value.
 * Otherwise the solver would not be able to make any deductions, except positional elimination.
 * On full latin square these routines are redundant.
 */
 
/* 
 * Figure out which cells must be forbidden from given forced cells.
 */
int latin_solver_assign_forbid(struct latin_solver *solver, int start, int step
#ifdef STANDALONE_SOLVER
		      , const char *fmt, ...
#endif
			  )
{
	int ret = 0, o = solver->o, depth = solver->depth, count, i, n, pos;
	bool *forbid = solver->forbid;
	bool *force = solver->force;
	
	count = 0;
	
	for(i = 0; i < o; i++)
	if(force[start + i*step])
		count++;
	
	ret = 0;
	if(count == depth)
	{
	for(i = 0; i < o; i++)
	{
	pos = start + i*step;
	if(!force[pos] && !forbid[pos])
	{
	#ifdef STANDALONE_SOLVER
		if(solver_show_working)
		{
			if(ret == 0)
			{
				va_list ap;
				printf("%*s", solver_recurse_depth*4, "");
                va_start(ap, fmt);
                vprintf(fmt, ap);
                va_end(ap);
				printf(":\n%*s  forbiding placement at", solver_recurse_depth*4, "");
				printf(" (%d,%d)", pos%o+1, pos/o+1);
			}
			else
				printf(", (%d,%d)", pos%o+1, pos/o+1);
		}
	#endif
	
		assert(!force[pos]);
		
		for(n = 1; n <= depth; n++)
			cube(pos%o, pos/o, n) = false;
	
		
		forbid[pos] = true;
		
		ret = 1;
	}
	}
	#ifdef STANDALONE_SOLVER
		if(solver_show_working)
			if(ret) printf("\n");
	#endif
	}else if (count > depth) {
	#ifdef STANDALONE_SOLVER
		if(solver_show_working)
		{
			va_list ap;
			printf("%*s", solver_recurse_depth*4, "");
            va_start(ap, fmt);
            vprintf(fmt, ap);
            va_end(ap);
			printf(":\n%*s  cannot have more forced cells than depth of the puzzle", solver_recurse_depth*4, "");
		}
	#endif
		return -1;
	}
		
	

	
	return ret;
}

/* 
 * Figure out which cells must be forced given forbidden cells.
 */
int latin_solver_assign_force(struct latin_solver *solver, int start, int step
#ifdef STANDALONE_SOLVER
		      , const char *fmt, ...
#endif
)
{
	int ret = 0, o = solver->o, depth = solver->depth, count, i, n, pos;
	bool *forbid = solver->forbid;
	bool *force = solver->force;
	
	count = 0;
	for(i = 0; i < o; i++) 
	{
	pos = start+i*step;
	if(forbid[pos])
		count++;
	else if(!force[pos])
	{
		for(n = 1; n <= depth; n++) /* might not have labeled the cell as forbidden, so check this here */
		if(cube(pos%o, pos/o, n))
			break;
	
		if(n == solver->depth+1)
		{
			forbid[pos] = true;
			count++;
		}
	}
	}
	
	if(o-count == depth)
	{
	for(i = 0; i < o; i++)
	{
		pos = start+i*step;
		if(!force[pos] && !forbid[pos])
		{
#ifdef STANDALONE_SOLVER
		if(solver_show_working)
		{
			if(ret == 0)
			{
				va_list ap;
				printf("%*s", solver_recurse_depth*4, "");
                va_start(ap, fmt);
                vprintf(fmt, ap);
                va_end(ap);
				printf(":\n%*s  imposing some placement at", solver_recurse_depth*4, "");
				printf(" (%d,%d)", pos%o+1, pos/o+1);
			}
			else
				printf(", (%d,%d)", pos%o+1, pos/o+1);
		}
#endif
			assert(!forbid[pos]);
			
			force[pos] = true;
			ret = 1;
		}
	}
	#ifdef STANDALONE_SOLVER
		if(solver_show_working)
			if(ret) printf("\n");
	#endif
	} else if(o-count<depth) {
	#ifdef STANDALONE_SOLVER
		if(solver_show_working)
		{
				va_list ap;
				printf("%*s", solver_recurse_depth*4, "");
                va_start(ap, fmt);
                vprintf(fmt, ap);
                va_end(ap);
				printf(":\n%*s  cannot have more forbidden cells than o-depth", solver_recurse_depth*4, "");
		}
	#endif
		return -1;
	}
	
	return ret;
}
#endif
	

/*
 * Function called when we are certain that a particular square has
 * a particular number in it. The y-coordinate passed in here is
 * transformed.
 */
void latin_solver_place(struct latin_solver *solver, int x, int y, int n)
{
    int i, o = solver->o;

    assert(n <= o);
    assert(cube(x,y,n));

    /*
     * Rule out all other numbers in this square.
     */
    for (i = 1; i <= o; i++)
	if (i != n)
            cube(x,y,i) = false;

    /*
     * Rule out this number in all other positions in the row.
     */
    for (i = 0; i < o; i++)
	if (i != y)
            cube(x,i,n) = false;

    /*
     * Rule out this number in all other positions in the column.
     */
    for (i = 0; i < o; i++)
	if (i != x)
            cube(i,y,n) = false;

    /*
     * Enter the number in the result grid.
     */
    solver->grid[y*o+x] = n;

    /*
     * Cross out this number from the list of numbers left to place
     * in its row, its column and its block.
     */
    solver->row[y*o+n-1] = solver->col[x*o+n-1] = true;
	
	#ifdef SEMI_LATIN
	/* 
	 * When the value is placed then the cell should be marked as forced.
	 * This is important in some of the other deduction routines.
	 */
	solver->force[y*o+x] = true;
	#endif
}

int latin_solver_elim(struct latin_solver *solver, int start, int step
#ifdef STANDALONE_SOLVER
		      , const char *fmt, ...
#endif
		      )
{
    int o = solver->o;
#ifdef STANDALONE_SOLVER
    char **names = solver->names;
#endif
    int fpos, m, i;

    /*
     * Count the number of set bits within this section of the
     * cube.
     */
    m = 0;
    fpos = -1;
    for (i = 0; i < o; i++)
	if (solver->cube[start+i*step]) {
	    fpos = start+i*step;
	    m++;
	}

    if (m == 1) {
	int x, y, n;
	assert(fpos >= 0);

	n = 1 + fpos % o;
	y = fpos / o;
	x = y / o;
	y %= o;

        if (!solver->grid[y*o+x]) {
#ifdef STANDALONE_SOLVER
            if (solver_show_working) {
                va_list ap;
		printf("%*s", solver_recurse_depth*4, "");
                va_start(ap, fmt);
                vprintf(fmt, ap);
                va_end(ap);
                printf(":\n%*s  placing %s at (%d,%d)\n",
                       solver_recurse_depth*4, "", names[n-1],
		       x+1, y+1);
            }
#endif
            latin_solver_place(solver, x, y, n);
            return +1;
        }
    } else if (m == 0) {
#ifdef STANDALONE_SOLVER
	if (solver_show_working) {
	    va_list ap;
	    printf("%*s", solver_recurse_depth*4, "");
	    va_start(ap, fmt);
	    vprintf(fmt, ap);
	    va_end(ap);
	    printf(":\n%*s  no possibilities available\n",
		   solver_recurse_depth*4, "");
	}
#endif
        return -1;
    }

    return 0;
}

struct latin_solver_scratch {
    unsigned char *grid, *rowidx, *colidx, *set;
#ifdef SEMI_LATIN
	unsigned char *forceidx;
#endif
    int *neighbours, *bfsqueue;
#ifdef STANDALONE_SOLVER
    int *bfsprev;
#endif
};

int latin_solver_set(struct latin_solver *solver,
                     struct latin_solver_scratch *scratch,
                     int start, int step1, int step2
#ifdef STANDALONE_SOLVER
                     , const char *fmt, ...
#endif
                     )
{
    int o = solver->o;
#ifdef STANDALONE_SOLVER
    char **names = solver->names;
#endif
    int i, j, n, count;
    unsigned char *grid = scratch->grid;
    unsigned char *rowidx = scratch->rowidx;
    unsigned char *colidx = scratch->colidx;
    unsigned char *set = scratch->set;
#ifdef SEMI_LATIN
	unsigned char *forceidx = scratch->forceidx;
	bool *impose = solver->force;
#endif
	
#ifdef SEMI_LATIN
	/*
	 * In the deduction routine we only want to count rows 
	 * which we know _must_ have some value.
	 */
	memset(forceidx, false, o);
	for (i = 0; i < o; i++)
	{
		int fpos = start+i*step1;
		int py = fpos / o;
		int px = py / o;
		py %= o;
		if(impose[py*o+px])
			forceidx[i] = true;
	}
#endif

    /*
     * We are passed a o-by-o matrix of booleans. Our first job
     * is to winnow it by finding any definite placements - i.e.
     * any row with a solitary 1 - and discarding that row and the
     * column containing the 1.
     */
    memset(rowidx, true, o);
    memset(colidx, true, o);
    for (i = 0; i < o; i++) {
        int count = 0, first = -1;
        for (j = 0; j < o; j++)
            if (solver->cube[start+i*step1+j*step2])
                first = j, count++;

	if (count == 0) 
#if !defined SEMI_LATIN
		return -1;
#else
		rowidx[i] = false;
#endif
		
    	if (count == 1
#ifdef SEMI_LATIN
			&& forceidx[i]
#endif
		   )
            rowidx[i] = colidx[first] = false;
    }

    /*
     * Convert each of rowidx/colidx from a list of 0s and 1s to a
     * list of the indices of the 1s.
     */
    for (i = j = 0; i < o; i++)
        if (rowidx[i])
            rowidx[j++] = i;
    n = j;
    for (i = j = 0; i < o; i++)
        if (colidx[i])
            colidx[j++] = i;
#if !defined SEMI_LATIN
    assert(n == j);
#else
	int n2 = j;
#endif

    /*
     * And create the smaller matrix.
     */
    for (i = 0; i < n; i++)
#if !defined SEMI_LATIN
        for (j = 0; j < n; j++)
#else
		for (j = 0; j < n2; j++)
#endif
            grid[i*o+j] = solver->cube[start+rowidx[i]*step1+colidx[j]*step2];

    /*
     * Having done that, we now have a matrix in which every row
     * has at least two 1s in. Now we search to see if we can find
     * a rectangle of zeroes (in the set-theoretic sense of
     * `rectangle', i.e. a subset of rows crossed with a subset of
     * columns) whose width and height add up to n.
     */

    memset(set, 0, n);
    count = 0;
    while (1) {
        /*
         * We have a candidate set. If its size is <=1 or >=n-1
         * then we move on immediately.
         */
        if (count > 1 && count < n-1) {
            /*
             * The number of rows we need is n-count. See if we can
             * find that many rows which each have a zero in all
             * the positions listed in `set'.
             */
            int rows = 0;
            for (i = 0; i < n; i++) {
                bool ok = true;
			#ifdef SEMI_LATIN
				if(!forceidx[rowidx[i]]) /* can't make a serious deduction if the cell could be empty */
					continue;
			#endif
			#if !defined SEMI_LATIN
                for (j = 0; j < n; j++)
			#else 
				for (j = 0; j < n2; j++)
			#endif
                    if (set[j] && grid[i*o+j]) {
                        ok = false;
                        break;
                    }
                if (ok)
                    rows++;
            }

            /*
             * We expect never to be able to get _more_ than
             * n-count suitable rows: this would imply that (for
             * example) there are four numbers which between them
             * have at most three possible positions, and hence it
             * indicates a faulty deduction before this point or
             * even a bogus clue.
             */
            if (rows > n - count) {
#ifdef STANDALONE_SOLVER
		if (solver_show_working) {
		    va_list ap;
		    printf("%*s", solver_recurse_depth*4,
			   "");
		    va_start(ap, fmt);
		    vprintf(fmt, ap);
		    va_end(ap);
		    printf(":\n%*s  contradiction reached\n",
			   solver_recurse_depth*4, "");
		}
#endif
		return -1;
	    }

            if (rows >= n - count) {
                bool progress = false;

                /*
                 * We've got one! Now, for each row which _doesn't_
                 * satisfy the criterion, eliminate all its set
                 * bits in the positions _not_ listed in `set'.
                 * Return +1 (meaning progress has been made) if we
                 * successfully eliminated anything at all.
                 *
                 * This involves referring back through
                 * rowidx/colidx in order to work out which actual
                 * positions in the cube to meddle with.
                 */
                for (i = 0; i < n; i++) {
                    bool ok = true;
				#if !defined SEMI_LATIN
                    for (j = 0; j < n; j++)
				#else
					if(!forceidx[rowidx[i]])
						ok = false;
					else 
					for (j = 0; j < n2; j++)
				#endif
                        if (set[j] && grid[i*o+j]) {
                            ok = false;
                            break;
                        }
                    if (!ok) {
					#if !defined SEMI_LATIN
                        for (j = 0; j < n; j++)
					#else 
						for (j = 0; j < n2; j++)
					#endif
                            if (!set[j] && grid[i*o+j]) {
                                int fpos = (start+rowidx[i]*step1+
                                            colidx[j]*step2);
#ifdef STANDALONE_SOLVER
                                if (solver_show_working) {
                                    int px, py, pn;

                                    if (!progress) {
                                        va_list ap;
					printf("%*s", solver_recurse_depth*4,
					       "");
                                        va_start(ap, fmt);
                                        vprintf(fmt, ap);
                                        va_end(ap);
                                        printf(":\n");
                                    }

                                    pn = 1 + fpos % o;
                                    py = fpos / o;
                                    px = py / o;
                                    py %= o;

                                    printf("%*s  ruling out %s at (%d,%d)\n",
					   solver_recurse_depth*4, "",
                                           names[pn-1], px+1, py+1);
                                }
#endif
                                progress = true;
                                solver->cube[fpos] = false;
                            }
                    }
                }

                if (progress) {
                    return +1;
                }
            }
        }

        /*
         * Binary increment: change the rightmost 0 to a 1, and
         * change all 1s to the right of it to 0s.
         */
        i = n;
        while (i > 0 && set[i-1])
            set[--i] = 0, count--;
        if (i > 0)
            set[--i] = 1, count++;
        else
            break;                     /* done */
    }

    return 0;
}

/*
 * Look for forcing chains. A forcing chain is a path of
 * pairwise-exclusive squares (i.e. each pair of adjacent squares
 * in the path are in the same row, column or block) with the
 * following properties:
 *
 *  (a) Each square on the path has precisely two possible numbers.
 *
 *  (b) Each pair of squares which are adjacent on the path share
 *      at least one possible number in common.
 *
 *  (c) Each square in the middle of the path shares _both_ of its
 *      numbers with at least one of its neighbours (not the same
 *      one with both neighbours).
 *
 * These together imply that at least one of the possible number
 * choices at one end of the path forces _all_ the rest of the
 * numbers along the path. In order to make real use of this, we
 * need further properties:
 *
 *  (c) Ruling out some number N from the square at one end
 *      of the path forces the square at the other end to
 *      take number N.
 *
 *  (d) The two end squares are both in line with some third
 *      square.
 *
 *  (e) That third square currently has N as a possibility.
 *
 * If we can find all of that lot, we can deduce that at least one
 * of the two ends of the forcing chain has number N, and that
 * therefore the mutually adjacent third square does not.
 *
 * To find forcing chains, we're going to start a bfs at each
 * suitable square, once for each of its two possible numbers.
 */
int latin_solver_forcing(struct latin_solver *solver,
                         struct latin_solver_scratch *scratch)
{
    int o = solver->o;
#ifdef SEMI_LATIN
	int depth = solver->depth;
	bool *force = solver->force;
#endif
#ifdef STANDALONE_SOLVER
    char **names = solver->names;
#endif
    int *bfsqueue = scratch->bfsqueue;
#ifdef STANDALONE_SOLVER
    int *bfsprev = scratch->bfsprev;
#endif
    unsigned char *number = scratch->grid;
    int *neighbours = scratch->neighbours;
    int x, y;

    for (y = 0; y < o; y++)
        for (x = 0; x < o; x++) {
            int count, t, n;
			
		#ifdef SEMI_LATIN
			/* It would only be sensible to try this technique
			 * if we knew that this cell _must_ have a value */
			if(!force[y*o+x])
				continue;
		#endif

            /*
             * If this square doesn't have exactly two candidate
             * numbers, don't try it.
             *
             * In this loop we also sum the candidate numbers,
             * which is a nasty hack to allow us to quickly find
             * `the other one' (since we will shortly know there
             * are exactly two).
             */
        #ifdef SEMI_LATIN
			for (count = t = 0, n = 1; n <= depth; n++)
		#else
            for (count = t = 0, n = 1; n <= o; n++)
		#endif
                if (cube(x, y, n))
                    count++, t += n;
            if (count != 2)
                continue;

            /*
             * Now attempt a bfs for each candidate.
             */
		#ifdef SEMI_LATIN
			for (n = 1; n <= depth; n++)
		#else
            for (n = 1; n <= o; n++)
		#endif
                if (cube(x, y, n)) {
                    int orign, currn, head, tail;

                    /*
                     * Begin a bfs.
                     */
                    orign = n;

                    memset(number, o+1, o*o);
                    head = tail = 0;
                    bfsqueue[tail++] = y*o+x;
#ifdef STANDALONE_SOLVER
                    bfsprev[y*o+x] = -1;
#endif
                    number[y*o+x] = t - n;

                    while (head < tail) {
                        int xx, yy, nneighbours, xt, yt, i;

                        xx = bfsqueue[head++];
                        yy = xx / o;
                        xx %= o;

                        currn = number[yy*o+xx];

                        /*
                         * Find neighbours of yy,xx.
                         */
                        nneighbours = 0;
                        for (yt = 0; yt < o; yt++)
                            neighbours[nneighbours++] = yt*o+xx;
                        for (xt = 0; xt < o; xt++)
                            neighbours[nneighbours++] = yy*o+xt;

                        /*
                         * Try visiting each of those neighbours.
                         */
                        for (i = 0; i < nneighbours; i++) {
                            int cc, tt, nn;

                            xt = neighbours[i] % o;
                            yt = neighbours[i] / o;

                            /*
                             * We need this square to not be
                             * already visited, and to include
                             * currn as a possible number.
                             */
                            if (number[yt*o+xt] <= o)
                                continue;
                            if (!cube(xt, yt, currn))
                                continue;

                            /*
                             * Don't visit _this_ square a second
                             * time!
                             */
                            if (xt == xx && yt == yy)
                                continue;

                            /*
                             * To continue with the bfs, we need
                             * this square to have exactly two
                             * possible numbers.
                             */
                            for (cc = tt = 0, nn = 1; nn <= o; nn++)
                                if (cube(xt, yt, nn))
                                    cc++, tt += nn;
                            if (cc == 2
#ifdef SEMI_LATIN
								&& force[yt*o+xt]
#endif
                            ) {
                                bfsqueue[tail++] = yt*o+xt;
#ifdef STANDALONE_SOLVER
                                bfsprev[yt*o+xt] = yy*o+xx;
#endif
                                number[yt*o+xt] = tt - currn;
                            }

                            /*
                             * One other possibility is that this
                             * might be the square in which we can
                             * make a real deduction: if it's
                             * adjacent to x,y, and currn is equal
                             * to the original number we ruled out.
                             */
                            if (currn == orign &&
                                (xt == x || yt == y)) {
#ifdef STANDALONE_SOLVER
                                if (solver_show_working) {
                                    const char *sep = "";
                                    int xl, yl;
                                    printf("%*sforcing chain, %s at ends of ",
                                           solver_recurse_depth*4, "",
					   names[orign-1]);
                                    xl = xx;
                                    yl = yy;
                                    while (1) {
                                        printf("%s(%d,%d)", sep, xl+1,
                                               yl+1);
                                        xl = bfsprev[yl*o+xl];
                                        if (xl < 0)
                                            break;
                                        yl = xl / o;
                                        xl %= o;
                                        sep = "-";
                                    }
                                    printf("\n%*s  ruling out %s at (%d,%d)\n",
                                           solver_recurse_depth*4, "",
                                           names[orign-1],
					   xt+1, yt+1);
                                }
#endif
                                cube(xt, yt, orign) = false;
                                return 1;
                            }
                        }
                    }
                }
        }

    return 0;
}

struct latin_solver_scratch *latin_solver_new_scratch(struct latin_solver *solver)
{
    struct latin_solver_scratch *scratch = snew(struct latin_solver_scratch);
    int o = solver->o;
    scratch->grid = snewn(o*o, unsigned char);
    scratch->rowidx = snewn(o, unsigned char);
    scratch->colidx = snewn(o, unsigned char);
    scratch->set = snewn(o, unsigned char);
#ifdef SEMI_LATIN
	scratch->forceidx = snewn(o, unsigned char);
#endif
    scratch->neighbours = snewn(3*o, int);
    scratch->bfsqueue = snewn(o*o, int);
#ifdef STANDALONE_SOLVER
    scratch->bfsprev = snewn(o*o, int);
#endif
    return scratch;
}

void latin_solver_free_scratch(struct latin_solver_scratch *scratch)
{
#ifdef STANDALONE_SOLVER
    sfree(scratch->bfsprev);
#endif
    sfree(scratch->bfsqueue);
    sfree(scratch->neighbours);
    sfree(scratch->set);
    sfree(scratch->colidx);
    sfree(scratch->rowidx);
#ifdef SEMI_LATIN
	sfree(scratch->forceidx);
#endif
    sfree(scratch->grid);
    sfree(scratch);
}

void latin_solver_alloc(struct latin_solver *solver, digit *grid, int o
#ifdef SEMI_LATIN
						, int depth, bool *force, bool *forbid
#endif
)
{
    int x, y;
#ifdef SEMI_LATIN
	int n;
#endif

    solver->o = o;
#ifdef SEMI_LATIN
	solver->depth = depth;
#endif
    solver->cube = snewn(o*o*o, unsigned char);
    solver->grid = grid;		/* write straight back to the input */
    memset(solver->cube, 1, o*o*o);

    solver->row = snewn(o*o, unsigned char);
    solver->col = snewn(o*o, unsigned char);
    memset(solver->row, 0, o*o);
    memset(solver->col, 0, o*o);
	
#ifdef SEMI_LATIN
	solver->force = snewn(o*o, bool);
	solver->forbid = snewn(o*o, bool);
	memset(solver->force, false, o*o);
	memset(solver->forbid, false, o*o);
#endif

    for (x = 0; x < o; x++)
	for (y = 0; y < o; y++)
	    if (grid[y*o+x])
		latin_solver_place(solver, x, y, grid[y*o+x]);
	
#ifdef SEMI_LATIN
	for (x = 0; x < o; x++)
	for (y = 0; y < o; y++)
	for (n = depth+1; n <= o; n++)
		cube(x,y,n) = false;
	
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
#endif

#ifdef STANDALONE_SOLVER
    solver->names = NULL;
#endif
}

void latin_solver_free(struct latin_solver *solver)
{
    sfree(solver->cube);
    sfree(solver->row);
    sfree(solver->col);
	
#ifdef SEMI_LATIN
	sfree(solver->force);
	sfree(solver->forbid);
#endif
	
	
}

int latin_solver_diff_simple(struct latin_solver *solver)
{
    int x, y, n, ret = 0, o = solver->o, depth = solver->depth;
#ifdef STANDALONE_SOLVER
    char **names = solver->names;
#endif

#ifdef SEMI_LATIN
	if(depth < o) {
	/*
	 * Deduce which cells must or musn't contain a value.
	 */
	for(y = 0; y < o; y++)
	{
		ret = latin_solver_assign_forbid(solver, y*o, 1
#ifdef STANDALONE_SOLVER
			, "blank cells deduction, "
			  "row %d", y+1
#endif
		);
		if(ret != 0) return ret;
	}

	for(x = 0; x < o; x++)
	{
		ret = latin_solver_assign_forbid(solver, x, o
#ifdef STANDALONE_SOLVER
			, "blank cells deduction, "
			  "column %d", x+1
#endif
		);
		if(ret != 0) return ret;
	}
	
	for(y = 0; y < o; y++)
	{
		ret = latin_solver_assign_force(solver, y*o, 1
#ifdef STANDALONE_SOLVER
			, "required cells deduction, "
			  "row %d", y+1
#endif
		);
		if(ret != 0) return ret;
	}

	for(x = 0; x < o; x++)
	{
		ret = latin_solver_assign_force(solver, x, o
#ifdef STANDALONE_SOLVER
			, "required cells deduction, "
			  "column %d", x+1
#endif
		);
		if(ret != 0) return ret;
	}
	}
	#endif

    /*
     * Row-wise positional elimination.
     */
    for (y = 0; y < o; y++)
#ifdef SEMI_LATIN
		for (n = 1; n <= depth; n++)
#else
        for (n = 1; n <= o; n++)
#endif
            if (!solver->row[y*o+n-1]) {
                ret = latin_solver_elim(solver, cubepos(0,y,n), o*o
#ifdef STANDALONE_SOLVER
					, "positional elimination,"
					" %s in row %d", names[n-1],
					y+1
#endif
					);
                if (ret != 0) return ret;
            }
    /*
     * Column-wise positional elimination.
     */
    for (x = 0; x < o; x++)
#ifdef SEMI_LATIN
		for (n = 1; n <= depth; n++)
#else
        for (n = 1; n <= o; n++)
#endif
            if (!solver->col[x*o+n-1]) {
                ret = latin_solver_elim(solver, cubepos(x,0,n), o
#ifdef STANDALONE_SOLVER
					, "positional elimination,"
					" %s in column %d", names[n-1], x+1
#endif
					);
                if (ret != 0) return ret;
            }

    /*
     * Numeric elimination.
     */
    for (x = 0; x < o; x++)
        for (y = 0; y < o; y++)
            if (!solver->grid[y*o+x]
#ifdef SEMI_LATIN
				&& solver->force[y*o+x]
#endif
		) {
                ret = latin_solver_elim(solver, cubepos(x,y,1), 1
#ifdef STANDALONE_SOLVER
					, "numeric elimination at (%d,%d)",
					x+1, y+1
#endif
					);
                if (ret != 0) return ret;
            }
			
    return 0;
}

int latin_solver_diff_set(struct latin_solver *solver,
                          struct latin_solver_scratch *scratch,
                          bool extreme)
{
    int x, y, ret, o = solver->o;
#if !defined SEMI_LATIN
	int n;
#endif
	
#if !defined SEMI_LATIN
#ifdef STANDALONE_SOLVER
    char **names = solver->names;
#endif
#endif

    if (!extreme) {
        /*
         * Row-wise set elimination.
         */
        for (y = 0; y < o; y++) {
            ret = latin_solver_set(solver, scratch, cubepos(0,y,1), o*o, 1
#ifdef STANDALONE_SOLVER
                                   , "set elimination, row %d", y+1
#endif
                                  );
            if (ret != 0) return ret;
        }
        /*
         * Column-wise set elimination.
         */
        for (x = 0; x < o; x++) {
            ret = latin_solver_set(solver, scratch, cubepos(x,0,1), o, 1
#ifdef STANDALONE_SOLVER
                                   , "set elimination, column %d", x+1
#endif
                                  );
            if (ret != 0) return ret;
        }
    } else {
	/*
	 * Haven't figured out how this works for partial latin squares.
     * Just going to disable this for now.	 
	 */
	#if !defined SEMI_LATIN
        /*
         * Row-vs-column set elimination on a single number
         * (much tricker for a human to do!)
         */
        for (n = 1; n <= o; n++) {
            ret = latin_solver_set(solver, scratch, cubepos(0,0,n), o*o, o
#ifdef STANDALONE_SOLVER
                                   , "positional set elimination on %s",
				   names[n-1]
#endif
                                  );
            if (ret != 0) return ret;
        }
	#else
		return 0;
	#endif
    }
    return 0;
}

/*
 * Returns:
 * 0 for 'didn't do anything' implying it was already solved.
 * -1 for 'impossible' (no solution)
 * 1 for 'single solution'
 * >1 for 'multiple solutions' (you don't get to know how many, and
 *     the first such solution found will be set.
 *
 * and this function may well assert if given an impossible board.
 */
static int latin_solver_recurse
    (struct latin_solver *solver, int diff_simple, int diff_set_0,
     int diff_set_1, int diff_forcing, int diff_recursive,
     usersolver_t const *usersolvers, void *ctx,
     ctxnew_t ctxnew, ctxfree_t ctxfree)
{
    int best, bestcount;
    int o = solver->o, x, y, n;
#ifdef SEMI_LATIN
	int depth = solver->depth;
#endif
#ifdef STANDALONE_SOLVER
    char **names = solver->names;
#endif

    best = -1;
    bestcount = o+1;

    for (y = 0; y < o; y++)
        for (x = 0; x < o; x++)
            if (!solver->grid[y*o+x]
#ifdef SEMI_LATIN
				&& solver->force[y*o+x]
#endif
				) {
                int count;

                /*
                 * An unfilled square. Count the number of
                 * possible digits in it.
                 */
                count = 0;
                for (n = 1; n <= o; n++)
                    if (cube(x,y,n))
                        count++;

                /*
                 * We should have found any impossibilities
                 * already, so this can safely be an assert.
                 */
                assert(count > 1);

                if (count < bestcount) {
                    bestcount = count;
                    best = y*o+x;
                }
            }

    if (best == -1)
        /* we were complete already. */
		return 0;
			
    else {
        int i, j;
        digit *list, *ingrid, *outgrid;
        int diff = diff_impossible;    /* no solution found yet */

        /*
         * Attempt recursion.
         */
        y = best / o;
        x = best % o;

        list = snewn(o, digit);
        ingrid = snewn(o*o, digit);
        outgrid = snewn(o*o, digit);
        memcpy(ingrid, solver->grid, o*o);

        /* Make a list of the possible digits. */
        for (j = 0, n = 1; n <= o; n++)
            if (cube(x,y,n))
                list[j++] = n;

#ifdef STANDALONE_SOLVER
        if (solver_show_working) {
            const char *sep = "";
            printf("%*srecursing on (%d,%d) [",
                   solver_recurse_depth*4, "", x+1, y+1);
            for (i = 0; i < j; i++) {
                printf("%s%s", sep, names[list[i]-1]);
                sep = " or ";
            }
            printf("]\n");
        }
#endif

        /*
         * And step along the list, recursing back into the
         * main solver at every stage.
         */
        for (i = 0; i < j; i++) {
            int ret;
	    void *newctx;
	    struct latin_solver subsolver;

            memcpy(outgrid, ingrid, o*o);
            outgrid[y*o+x] = list[i];

#ifdef STANDALONE_SOLVER
            if (solver_show_working)
                printf("%*sguessing %s at (%d,%d)\n",
                       solver_recurse_depth*4, "", names[list[i]-1], x+1, y+1);
            solver_recurse_depth++;
#endif

	    if (ctxnew) {
		newctx = ctxnew(ctx);
	    } else {
		newctx = ctx;
	    }
	    latin_solver_alloc(&subsolver, outgrid, o
#ifdef SEMI_LATIN
							, depth, solver->force, solver->forbid
#endif
							);
#ifdef STANDALONE_SOLVER
	    subsolver.names = solver->names;
#endif

#ifdef SEMI_LATIN
		memcpy(subsolver.force, solver->force, o*o);
		memcpy(subsolver.forbid, solver->forbid, o*o);
#endif
            ret = latin_solver_top(&subsolver, diff_recursive,
				   diff_simple, diff_set_0, diff_set_1,
				   diff_forcing, diff_recursive,
				   usersolvers, newctx, ctxnew, ctxfree);
			
	    latin_solver_free(&subsolver);
	    if (ctxnew)
		ctxfree(newctx);

#ifdef STANDALONE_SOLVER
            solver_recurse_depth--;
            if (solver_show_working) {
                printf("%*sretracting %s at (%d,%d)\n",
                       solver_recurse_depth*4, "", names[list[i]-1], x+1, y+1);
            }
#endif
            /* we recurse as deep as we can, so we should never find
             * find ourselves giving up on a puzzle without declaring it
             * impossible.  */
            assert(ret != diff_unfinished);

            /*
             * If we have our first solution, copy it into the
             * grid we will return.
             */
            if (diff == diff_impossible && ret != diff_impossible)
                memcpy(solver->grid, outgrid, o*o);

            if (ret == diff_ambiguous)
                diff = diff_ambiguous;
            else if (ret == diff_impossible)
                /* do not change our return value */;
            else {
                /* the recursion turned up exactly one solution */
                if (diff == diff_impossible)
                    diff = diff_recursive;
                else
                    diff = diff_ambiguous;
            }

            /*
             * As soon as we've found more than one solution,
             * give up immediately.
             */
            if (diff == diff_ambiguous)
                break;
        }

        sfree(outgrid);
        sfree(ingrid);
        sfree(list);

        if (diff == diff_impossible)
            return -1;
        else if (diff == diff_ambiguous)
            return 2;
        else {
            assert(diff == diff_recursive);
            return 1;
        }
    }
}

static int latin_solver_top(struct latin_solver *solver, int maxdiff,
			    int diff_simple, int diff_set_0, int diff_set_1,
			    int diff_forcing, int diff_recursive,
			    usersolver_t const *usersolvers, void *ctx,
			    ctxnew_t ctxnew, ctxfree_t ctxfree)
{
    struct latin_solver_scratch *scratch = latin_solver_new_scratch(solver);
    int ret, diff = diff_simple;

    assert(maxdiff <= diff_recursive);
    /*
     * Now loop over the grid repeatedly trying all permitted modes
     * of reasoning. The loop terminates if we complete an
     * iteration without making any progress; we then return
     * failure or success depending on whether the grid is full or
     * not.
     */
    while (1) {
	int i;

	cont:
#ifdef SEMI_LATIN
		latin_solver_debug_force_forbid(solver->o, solver->depth, solver->force, solver->forbid);
#endif
        latin_solver_debug(solver->cube, solver->o
#ifdef SEMI_LATIN
							  , solver->depth
#endif
		);

	for (i = 0; i <= maxdiff; i++) {
	    if (usersolvers[i])
		ret = usersolvers[i](solver, ctx);
	    else
		ret = 0;
	    if (ret == 0 && i == diff_simple)
		ret = latin_solver_diff_simple(solver);
	    if (ret == 0 && i == diff_set_0)
		ret = latin_solver_diff_set(solver, scratch, false);
	    if (ret == 0 && i == diff_set_1)
		ret = latin_solver_diff_set(solver, scratch, true);
	    if (ret == 0 && i == diff_forcing)
		ret = latin_solver_forcing(solver, scratch);

	    if (ret < 0) {
		diff = diff_impossible;
		goto got_result;
	    } else if (ret > 0) {
		diff = max(diff, i);
		goto cont;
	    }
	}

        /*
         * If we reach here, we have made no deductions in this
         * iteration, so the algorithm terminates.
         */
        break;
    }

    /*
     * Last chance: if we haven't fully solved the puzzle yet, try
     * recursing based on guesses for a particular square. We pick
     * one of the most constrained empty squares we can find, which
     * has the effect of pruning the search tree as much as
     * possible.
     */
    if (maxdiff == diff_recursive) {
        int nsol = latin_solver_recurse(solver,
					diff_simple, diff_set_0, diff_set_1,
					diff_forcing, diff_recursive,
					usersolvers, ctx, ctxnew, ctxfree);
        if (nsol < 0) diff = diff_impossible;
        else if (nsol == 1) diff = diff_recursive;
        else if (nsol > 1) diff = diff_ambiguous;
        /* if nsol == 0 then we were complete anyway
         * (and thus don't need to change diff) */
    } else {
        /*
         * We're forbidden to use recursion, so we just see whether
         * our grid is fully solved, and return diff_unfinished
         * otherwise.
         */
        int x, y, o = solver->o;
#ifdef SEMI_LATIN
		int depth = solver->depth, i, n;
		
		for (i = 0; i < o; i++)
			for (n = 1; n <= depth; n++)
				if(!solver->col[i*o+n-1] || !solver->row[i*o+n-1])
					diff = diff_unfinished;
#endif

        for (y = 0; y < o; y++)
            for (x = 0; x < o; x++)
                if (!solver->grid[y*o+x]
#ifdef SEMI_LATIN
					&& solver->force[y*o+x]
#endif
					)
                    diff = diff_unfinished;
    }

    got_result:

#ifdef STANDALONE_SOLVER
    if (solver_show_working) {
        if (diff != diff_impossible && diff != diff_unfinished &&
            diff != diff_ambiguous) {
            int x, y;

            printf("%*sone solution found:\n", solver_recurse_depth*4, "");

            for (y = 0; y < solver->o; y++) {
                printf("%*s", solver_recurse_depth*4+1, "");
                for (x = 0; x < solver->o; x++) {
                    int val = solver->grid[y*solver->o+x];
#if !defined SEMI_LATIN
                    assert(val);
#endif
#ifdef SEMI_LATIN
					if(val)
#endif
                    printf(" %s", solver->names[val-1]);
#ifdef SEMI_LATIN
					else
					printf(" -");
#endif
                }
                printf("\n");
            }
        } else {
            printf("%*s%s found\n",
                   solver_recurse_depth*4, "",
                   diff == diff_impossible ? "no solution (impossible)" :
                   diff == diff_unfinished ? "no solution (unfinished)" :
                   "multiple solutions");
        }
    }
#endif

    latin_solver_free_scratch(scratch);

    return diff;
}

int latin_solver_main(struct latin_solver *solver, int maxdiff,
		      int diff_simple, int diff_set_0, int diff_set_1,
		      int diff_forcing, int diff_recursive,
		      usersolver_t const *usersolvers, void *ctx,
		      ctxnew_t ctxnew, ctxfree_t ctxfree)
{
    int diff;
#ifdef STANDALONE_SOLVER
    int o = solver->o;
    char *text = NULL, **names = NULL;
#endif

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

    diff = latin_solver_top(solver, maxdiff,
			    diff_simple, diff_set_0, diff_set_1,
			    diff_forcing, diff_recursive,
			    usersolvers, ctx, ctxnew, ctxfree);

#ifdef STANDALONE_SOLVER
    sfree(names);
    sfree(text);
#endif

    return diff;
}

int latin_solver(digit *grid, int o
#ifdef SEMI_LATIN
	   , int depth, bool *force, bool *forbid
#endif
	   , int maxdiff , int diff_simple, int diff_set_0, int diff_set_1,
		 int diff_forcing, int diff_recursive,
		 usersolver_t const *usersolvers, void *ctx,
		 ctxnew_t ctxnew, ctxfree_t ctxfree)
{
    struct latin_solver solver;
    int diff;

    latin_solver_alloc(&solver, grid, o
#ifdef SEMI_LATIN
					   , depth, force, forbid
#endif
						);
    diff = latin_solver_main(&solver, maxdiff,
			     diff_simple, diff_set_0, diff_set_1,
			     diff_forcing, diff_recursive,
			     usersolvers, ctx, ctxnew, ctxfree);
    latin_solver_free(&solver);
    return diff;
}

#ifdef SEMI_LATIN
void latin_solver_debug_force_forbid(int o, int depth, bool *force, bool *forbid)
{
#ifdef STANDALONE_SOLVER
	if (solver_show_working > 1) {
		int x, y;
		for(y = 0; y < o; y++)
		{
		for(x = 0; x < o; x++)
			printf("%*c ", depth, forbid[y*o+x] ? 'X' : force[y*o+x] ? 'O' : '-'); 
		printf("\n");
		}
	}
#endif
}
#endif

void latin_solver_debug(unsigned char *cube, int o
#ifdef SEMI_LATIN
					  , int depth
#endif
)
{
#ifdef STANDALONE_SOLVER
    if (solver_show_working > 1) {
        struct latin_solver ls, *solver = &ls;
        char *dbg;
        int x, y, i, c = 0;

        ls.cube = cube; ls.o = o; /* for cube() to work */

        dbg = snewn(3*o*o*o, char);
        for (y = 0; y < o; y++) {
            for (x = 0; x < o; x++) {
#ifdef SEMI_LATIN
				for(i = 1; i <= depth; i++) {
#else
                for (i = 1; i <= o; i++) {
#endif
                    if (cube(x,y,i))
                        dbg[c++] = i + '0';
                    else
                        dbg[c++] = '.';
                }
                dbg[c++] = ' ';
            }
            dbg[c++] = '\n';
        }
        dbg[c++] = '\n';
        dbg[c++] = '\0';

        printf("%s", dbg);
        sfree(dbg);
    }
#endif
}

void latin_debug(digit *sq, int o)
{
#ifdef STANDALONE_SOLVER
    if (solver_show_working) {
        int x, y;

        for (y = 0; y < o; y++) {
            for (x = 0; x < o; x++) {
                printf("%2d ", sq[y*o+x]);
            }
            printf("\n");
        }
        printf("\n");
    }
#endif
}

/* --------------------------------------------------------
 * Generation.
 */

digit *latin_generate(int o, random_state *rs)
{
    digit *sq;
    int *adjdata, *adjsizes, *matching;
    int **adjlists;
    void *scratch;
    int i, j, k;
    digit *row;

    /*
     * To efficiently generate a latin square in such a way that
     * all possible squares are possible outputs from the function,
     * we make use of a theorem which states that any r x n latin
     * rectangle, with r < n, can be extended into an (r+1) x n
     * latin rectangle. In other words, we can reliably generate a
     * latin square row by row, by at every stage writing down any
     * row at all which doesn't conflict with previous rows, and
     * the theorem guarantees that we will never have to backtrack.
     *
     * To find a viable row at each stage, we can make use of the
     * support functions in matching.c.
     */

    sq = snewn(o*o, digit);

    /*
     * matching.c will take care of randomising the generation of each
     * row of the square, but in case this entire method of generation
     * introduces a really subtle top-to-bottom directional bias,
     * we'll also generate the rows themselves in random order.
     */
    row = snewn(o, digit);
    for (i = 0; i < o; i++)
	row[i] = i;
    shuffle(row, i, sizeof(*row), rs);

    /*
     * Set up the infrastructure for the matching subroutine.
     */
    scratch = smalloc(matching_scratch_size(o, o));
    adjdata = snewn(o*o, int);
    adjlists = snewn(o, int *);
    adjsizes = snewn(o, int);
    matching = snewn(o, int);

    /*
     * Now generate each row of the latin square.
     */
    for (i = 0; i < o; i++) {
        /*
         * Make adjacency lists for a bipartite graph joining each
         * column to all the numbers not yet placed in that column.
         */
        for (j = 0; j < o; j++) {
            int *p, *adj = adjdata + j*o;
            for (k = 0; k < o; k++)
                adj[k] = 1;
            for (k = 0; k < i; k++)
                adj[sq[row[k]*o + j] - 1] = 0;
            adjlists[j] = p = adj;
            for (k = 0; k < o; k++)
                if (adj[k])
                    *p++ = k;
            adjsizes[j] = p - adjlists[j];
        }

	/*
	 * Run the matching algorithm.
	 */
	j = matching_with_scratch(scratch, o, o, adjlists, adjsizes,
                                  rs, matching, NULL);
	assert(j == o);   /* by the above theorem, this must have succeeded */

	/*
	 * And use the output to set up the new row of the latin
	 * square.
	 */
	for (j = 0; j < o; j++)
	    sq[row[i]*o + j] = matching[j] + 1;
    }

    /*
     * Done. Free our internal workspaces...
     */
    sfree(matching);
    sfree(adjlists);
    sfree(adjsizes);
    sfree(adjdata);
    sfree(scratch);
    sfree(row);

    /*
     * ... and return our completed latin square.
     */
    return sq;
}

digit *latin_generate_rect(int w, int h, random_state *rs)
{
    int o = max(w, h), x, y;
    digit *latin, *latin_rect;

    latin = latin_generate(o, rs);
    latin_rect = snewn(w*h, digit);

    for (x = 0; x < w; x++) {
        for (y = 0; y < h; y++) {
            latin_rect[y*w + x] = latin[y*o + x];
        }
    }

    sfree(latin);
    return latin_rect;
}

/* --------------------------------------------------------
 * Checking.
 */

typedef struct lcparams {
    digit elt;
    int count;
} lcparams;

static int latin_check_cmp(void *v1, void *v2)
{
    lcparams *lc1 = (lcparams *)v1;
    lcparams *lc2 = (lcparams *)v2;

    if (lc1->elt < lc2->elt) return -1;
    if (lc1->elt > lc2->elt) return 1;
    return 0;
}

#define ELT(sq,x,y) (sq[((y)*order)+(x)])

/* returns true if sq is not a latin square. */
bool latin_check(digit *sq, int order)
{
    tree234 *dict = newtree234(latin_check_cmp);
    int c, r;
    bool ret = false;
    lcparams *lcp, lc, *aret;

    /* Use a tree234 as a simple hash table, go through the square
     * adding elements as we go or incrementing their counts. */
    for (c = 0; c < order; c++) {
	for (r = 0; r < order; r++) {
	    lc.elt = ELT(sq, c, r); lc.count = 0;
	    lcp = find234(dict, &lc, NULL);
	    if (!lcp) {
		lcp = snew(lcparams);
		lcp->elt = ELT(sq, c, r);
		lcp->count = 1;
                aret = add234(dict, lcp);
		assert(aret == lcp);
	    } else {
		lcp->count++;
	    }
	}
    }

    /* There should be precisely 'order' letters in the alphabet,
     * each occurring 'order' times (making the OxO tree) */
    if (count234(dict) != order) ret = true;
    else {
	for (c = 0; (lcp = index234(dict, c)) != NULL; c++) {
	    if (lcp->count != order) ret = true;
	}
    }
    for (c = 0; (lcp = index234(dict, c)) != NULL; c++)
	sfree(lcp);
    freetree234(dict);

    return ret;
}


/* --------------------------------------------------------
 * Testing (and printing).
 */

#ifdef STANDALONE_LATIN_TEST

#include <stdio.h>
#include <time.h>

const char *quis;

static void latin_print(digit *sq, int order)
{
    int x, y;

    for (y = 0; y < order; y++) {
	for (x = 0; x < order; x++) {
	    printf("%2u ", ELT(sq, x, y));
	}
	printf("\n");
    }
    printf("\n");
}

static void gen(int order, random_state *rs, int debug)
{
    digit *sq;

    solver_show_working = debug;

    sq = latin_generate(order, rs);
    latin_print(sq, order);
    if (latin_check(sq, order)) {
	fprintf(stderr, "Square is not a latin square!");
	exit(1);
    }

    sfree(sq);
}

void test_soak(int order, random_state *rs)
{
    digit *sq;
    int n = 0;
    time_t tt_start, tt_now, tt_last;

    solver_show_working = 0;
    tt_now = tt_start = time(NULL);

    while(1) {
        sq = latin_generate(order, rs);
        sfree(sq);
        n++;

        tt_last = time(NULL);
        if (tt_last > tt_now) {
            tt_now = tt_last;
            printf("%d total, %3.1f/s\n", n,
                   (double)n / (double)(tt_now - tt_start));
        }
    }
}

void usage_exit(const char *msg)
{
    if (msg)
        fprintf(stderr, "%s: %s\n", quis, msg);
    fprintf(stderr, "Usage: %s [--seed SEED] --soak <params> | [game_id [game_id ...]]\n", quis);
    exit(1);
}

int main(int argc, char *argv[])
{
    int i, soak = 0;
    random_state *rs;
    time_t seed = time(NULL);

    quis = argv[0];
    while (--argc > 0) {
	const char *p = *++argv;
	if (!strcmp(p, "--soak"))
	    soak = 1;
	else if (!strcmp(p, "--seed")) {
	    if (argc == 0)
		usage_exit("--seed needs an argument");
	    seed = (time_t)atoi(*++argv);
	    argc--;
	} else if (*p == '-')
		usage_exit("unrecognised option");
	else
	    break; /* finished options */
    }

    rs = random_new((void*)&seed, sizeof(time_t));

    if (soak == 1) {
	if (argc != 1) usage_exit("only one argument for --soak");
	test_soak(atoi(*argv), rs);
    } else {
	if (argc > 0) {
	    for (i = 0; i < argc; i++) {
		gen(atoi(*argv++), rs, 1);
	    }
	} else {
	    while (1) {
		i = random_upto(rs, 20) + 1;
		gen(i, rs, 0);
	    }
	}
    }
    random_free(rs);
    return 0;
}

#endif
/* vim: set shiftwidth=4 tabstop=8: */
