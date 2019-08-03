/*
 * numberball.c - implementation of Nanbaboru from janko.at
 *
 * This is a partial latin square puzzle where you get imposed and forbidden cells as clues.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <math.h>
#include <stdbool.h>

#include "puzzles.h"
#include "latin.h"

/*
 * Difficulty levels (same as towers.c).
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

enum {
    COL_BACKGROUND,
    COL_GRID,
    COL_USER,
    COL_HIGHLIGHT,
    COL_ERROR,
    COL_PENCIL,
    COL_DONE,
    NCOLOURS
};

struct game_params {
    int w, dep, diff;
};

struct clues
{
	int refcount;
	int w;
	int dep;
	/* 
	 * Arrays of w*w digits and bools
	 */
	digit *immutable;
	bool *impose;
	bool *forbid;
};
	
struct game_state 
{
	game_params par;
	struct clues *clues;
    digit *grid;
    int *pencil;		       /* bitmaps using bits 1<<1..1<<n */
    bool *impose, *forbid;	   /* these are special pencil marks */
    bool completed, cheated;
};

static game_params *default_params(void)
{
	game_params *ret = snew(game_params);

    ret->w = 5;
	ret->dep = 3;
    ret->diff = DIFF_EASY;

    return ret;
}

static const struct game_params numberball_presets[] = {
    {  5, 3, DIFF_EASY         },
    {  6, 3, DIFF_EASY         },
    {  6, 4, DIFF_HARD         },
    {  7, 3, DIFF_EASY         },
    {  7, 4, DIFF_HARD         },
    {  8, 4, DIFF_EXTREME      },
    {  8, 5, DIFF_UNREASONABLE },
};

static bool game_fetch_preset(int i, char **name, game_params **params)
{
	game_params *ret;
    char buf[80];

    if (i < 0 || i >= lenof(numberball_presets))
        return false;

    ret = snew(game_params);
    *ret = numberball_presets[i]; /* structure copy */

    sprintf(buf, "%dx%d 1~%d %s", ret->w, ret->w, ret->dep, numberball_diffnames[ret->diff]);

    *name = dupstr(buf);
    *params = ret;
    return true;
}

static void free_params(game_params *params)
{
    sfree(params);
}

static game_params *dup_params(const game_params *params)
{
    game_params *ret = snew(game_params);
    *ret = *params;		       /* structure copy */
    return ret;
}

static void decode_params(game_params *params, char const *string)
{
	char const *p = string;

    params->w = atoi(p);
    while (*p && isdigit((unsigned char)*p)) p++;
	
	p++;
	params->dep = atoi(p);
	while (*p && isdigit((unsigned char)*p)) p++;
	
    if (*p == 'd') {
        int i;
        p++;
        params->diff = DIFFCOUNT+1; /* ...which is invalid */
        if (*p) {
            for (i = 0; i < DIFFCOUNT; i++) {
                if (*p == numberball_diffchars[i])
                    params->diff = i;
            }
            p++;
        }
    }
}

static char *encode_params(const game_params *params, bool full)
{
	char ret[80];

    sprintf(ret, "%dx%d", params->w, params->dep);
    if (full)
        sprintf(ret + strlen(ret), "d%c", numberball_diffchars[params->diff]);

    return dupstr(ret);
}

static config_item *game_configure(const game_params *params)
{
	config_item *ret;
    char buf[80];

    ret = snewn(4, config_item);

    ret[0].name = "Grid size";
    ret[0].type = C_STRING;
    sprintf(buf, "%d", params->w);
    ret[0].u.string.sval = dupstr(buf);
	
	ret[1].name = "Grid depth";
    ret[1].type = C_STRING;
    sprintf(buf, "%d", params->dep);
    ret[1].u.string.sval = dupstr(buf);

    ret[2].name = "Difficulty";
    ret[2].type = C_CHOICES;
    ret[2].u.choices.choicenames = DIFFCONFIG;
    ret[2].u.choices.selected = params->diff;

    ret[3].name = NULL;
    ret[3].type = C_END;

    return ret;
}

static game_params *custom_params(const config_item *cfg)
{
	game_params *ret = snew(game_params);

    ret->w = atoi(cfg[0].u.string.sval);
    ret->dep = atoi(cfg[1].u.string.sval);
    ret->diff = cfg[2].u.choices.selected;

    return ret;
}

static const char *validate_params(const game_params *params, bool full)
{
	if (params->w < 3)
        return "Grid size must be above 3";
	if (params->dep > params->w/2+1)
		return "Grid depth must be below ceiling(1/2 grid size)";
    if (params->diff >= DIFFCOUNT)
        return "Unknown difficulty rating";
    return NULL;
}

static usersolver_t const numberball_solvers[DIFFCOUNT]; /* don't need any */

static int solver(digit *grid, bool *impose, bool *forbid, int o, int depth, int maxdiff)
{	
	int diff = latin_solver(grid, o, depth, impose, forbid, maxdiff, 
						DIFF_EASY, DIFF_HARD, DIFF_EXTREME,
						DIFF_EXTREME, DIFF_UNREASONABLE,
						numberball_solvers, NULL, NULL, NULL);
			   
    return diff;
}

static char *new_game_desc(const game_params *params, random_state *rs,
			   char **aux, bool interactive)
{
	int w = params->w, dep = params->dep, a = w*w;
    digit *grid, *soln, *soln2;
	bool *imp, *imp2, *forb, *forb2;
    int *order;
    int i, ret;
    int diff = params->diff;
    char *desc, *p;

    /*
     * Difficulty exceptions: some combinations of size and
     * difficulty cannot be satisfied, because all puzzles of at
     * most that difficulty are actually even easier.
     *
     * Remember to re-test this whenever a change is made to the
     * solver logic!
     *
     * I tested it using the following shell command:

for d in e h x u; do
  for i in {3..9}; do
    echo -n "./towers --generate 1 ${i}d${d}: "
    perl -e 'alarm 30; exec @ARGV' ./towers --generate 1 ${i}d${d} >/dev/null \
      && echo ok
  done
done

     * Of course, it's better to do that after taking the exceptions
     * _out_, so as to detect exceptions that should be removed as
     * well as those which should be added.
     */
    if (diff > DIFF_HARD && w <= 3)
	diff = DIFF_HARD;

    grid = NULL;
    soln = snewn(a, digit);
    soln2 = snewn(a, digit);
	imp = snewn(a, bool);
	imp2 = snewn(a, bool);
	forb = snewn(a, bool);
	forb2 = snewn(a, bool);
    order = snewn(a, int);

    while (1) {
	/*
	 * Construct a latin square to be the solution.
	 */
	sfree(grid);
	grid = latin_generate(w, rs);
	memset(imp, 0, a);
	memset(imp2, 0, a);
	memset(forb, 0, a);
	memset(forb2, 0, a);
	for(i = 0; i < a; i++)
	if(grid[i] > dep)
	{
		grid[i] = 0;
		forb[i] = true;
	}
		
	/*
	 * Remove the grid numbers or known empty cells, 
	 * and then find cells for which it is enough to know that it must have a value 
	 * without knowing which value exactly, one by one,
	 * for as long as the game remains soluble at the given
	 * difficulty.
	 */
	memcpy(soln, grid, a);

	for (i = 0; i < a; i++)
	    order[i] = i;
	shuffle(order, a, sizeof(*order), rs);
	for (i = 0; i < a; i++) {
	    int j = order[i];

	    memcpy(soln2, grid, a);
		memcpy(forb2, forb, a);
	    if(soln2[j])
			soln2[j] = 0;
		else
			forb2[j] = false;
		
	    ret = solver(soln2, imp2, forb2, w,  dep, diff);
	    if (ret <= diff)
		{
		if(grid[j])
			grid[j] = 0;
		else
			forb[j] = false;
		}
	}
		
	for (i = 0; i < a; i++)
	    order[i] = i;
	shuffle(order, a, sizeof(*order), rs);
	for (i = 0; i < a; i++) {
	    int j = order[i];

	    memcpy(soln2, grid, a);
		memcpy(imp2, imp, a);
	    if(soln2[j])
		{
			soln2[j] = 0;
			imp2[j] = true;
		}
		else
			continue;
		
	    ret = solver(soln2, imp2, forb2, w,  dep, diff);
	    if (ret <= diff)
		{
			grid[j] = 0;
			imp[j] = true;
		}
	}
		
	/*
	 * See if the game can be solved at the specified difficulty
	 * level, but not at the one below.
	 */
	memcpy(soln2, grid, a);
	ret = solver(soln2, imp2, forb2, w,  dep, diff);
	if (ret != diff)
	    continue;		       /* go round again */

	/*
	 * We've got a usable puzzle!
	 */
	break;
    }

    /*
     * Encode the puzzle description.
     */
    desc = snewn(40*a, char);
    p = desc;

	int run = 0;
	for (i = 0; i <= a; i++) {
	    int n = (i < a ? grid[i] : -1);

	    if (!n && !imp[i] && !forb[i])
		run++;
	    else {
		if (run) {
		    while (run > 0) {
			int thisrun = min(run, 26);
			*p++ = thisrun - 1 + 'a';
			run -= thisrun;
		    }
		} else {
		    /*
		     * If there's a number in the very top left or
		     * bottom right, there's no point putting an
		     * unnecessary _ before or after it.
		     */
		    if (i > 0 && (n > 0 || imp[i] || forb[i]))
			*p++ = '_';
		}
		if (n > 0)
		    p += sprintf(p, "%d", n);
		else if(imp[i] && i<a)
			p += sprintf(p, "%c", 'O');
		else if(forb[i] && i<a)
			p += sprintf(p, "%c", 'X');
		
		run = 0;
	    }
	}
    *p++ = '\0';
    desc = sresize(desc, p - desc, char);

    /*
     * Encode the solution.
     */
    *aux = snewn(a+2, char);
    (*aux)[0] = 'S';
    for (i = 0; i < a; i++)
	(*aux)[i+1] = '0' + soln[i];
    (*aux)[a+1] = '\0';

    sfree(grid);
    sfree(soln);
    sfree(soln2);
	sfree(imp);
	sfree(imp2);
	sfree(forb);
	sfree(forb2);
    sfree(order);

    return desc;
}

static const char *validate_desc(const game_params *params, const char *desc)
{
	int w = params->w, a = w*w, dep = params->dep;
    const char *p = desc;
	
	/*
	 * Verify that the right amount of grid data is given, and
	 * that any grid elements provided are in range.
	 */
	int squares = 0;

	while (*p) {
	    int c = *p++;
	    if (c >= 'a' && c <= 'z') {
		squares += c - 'a' + 1;
	    } else if (c == '_') {
		/* do nothing */;
	    } else if (c > '0' && c <= '9') {
		int val = atoi(p-1);
		if (val < 1 || val > dep)
		    return "Out-of-range number in grid description";
		squares++;
		while (*p && isdigit((unsigned char)*p)) p++;
	    } else if (c == 'X' || c == 'O') { 	/* O - not null */
			squares++;
		} else
			return "Invalid character in game description";
	}

	if (squares < a)
	    return "Not enough data to fill grid";

	if (squares > a)
	    return "Too much data to fit in grid";

    return NULL;
}

static key_label *game_request_keys(const game_params *params, int *nkeys)
{
    int i;
    int dep = params->dep;
    key_label *keys = snewn(dep+1, key_label);
    *nkeys = dep + 1;

    for (i = 0; i < dep; i++) {
	if (i<9) keys[i].button = '1' + i;
	else keys[i].button = 'a' + i - 9;

        keys[i].label = NULL;
    }
    keys[dep].button = '\b';
    keys[dep].label = NULL;

    return keys;
}

static game_state *new_game(midend *me, const game_params *params,
                            const char *desc)
{
	int w = params->w, dep = params->dep, a = w*w;
    game_state *state = snew(game_state);
    const char *p = desc;
    int i;

    state->par = *params;	       /* structure copy */
    state->clues = snew(struct clues);
    state->clues->refcount = 1;
    state->clues->w = w;
	state->clues->dep = dep;
    state->clues->immutable = snewn(a, digit);
	state->clues->impose = snewn(a, bool);
	state->clues->forbid = snewn(a, bool);
    state->grid = snewn(a, digit);
	state->impose = snewn(a, bool);
	state->forbid = snewn(a, bool);
    state->pencil = snewn(a, int);

    for (i = 0; i < a; i++) {
	state->grid[i] = state->pencil[i] = 0;
    }

    memset(state->clues->immutable, 0, a);
    memset(state->clues->impose, 0, a);
	memset(state->clues->forbid, 0, a);
	memset(state->impose, 0, a);
	memset(state->forbid, 0, a);
	
	int pos = 0;
	while (*p) {
	    int c = *p++;
	    if (c >= 'a' && c <= 'z') {
			pos += c - 'a' + 1;
	    } else if (c == '_') {
		/* do nothing */;
	    } else if (c > '0' && c <= '9') {
			int val = atoi(p-1);
			assert(val >= 1 && val <= dep);
			assert(pos < a);
			state->grid[pos] = state->clues->immutable[pos] = val;
			pos++;
			while (*p && isdigit((unsigned char)*p)) p++;
	    } else if (c == 'O') 	{ /* O - not null */
			state->clues->impose[pos] = true;
			pos++;
		} else if (c == 'X') { /* capital x */
			state->clues->forbid[pos] = true;
			pos++;
		} else
			assert(!"Corrupt game description");
	}
	assert(pos == a);
    assert(!*p);

    state->completed = false;
    state->cheated = false;

    return state;
}

static game_state *dup_game(const game_state *state)
{
	int w = state->par.w, a = w*w;
    game_state *ret = snew(game_state);

    ret->par = state->par;	       /* structure copy */

    ret->clues = state->clues;
    ret->clues->refcount++;

    ret->grid = snewn(a, digit);
    ret->pencil = snewn(a, int);
	ret->impose = snewn(a, bool);
	ret->forbid = snewn(a, bool);
    memcpy(ret->grid, state->grid, a*sizeof(digit));
    memcpy(ret->pencil, state->pencil, a*sizeof(int));
    memcpy(ret->impose, state->impose, a);
	memcpy(ret->forbid, state->forbid, a);

    ret->completed = state->completed;
    ret->cheated = state->cheated;

    return ret;
}

static void free_game(game_state *state)
{
	sfree(state->grid);
    sfree(state->pencil);
    if (--state->clues->refcount <= 0) {
	sfree(state->clues->immutable);
	sfree(state->clues->impose);
	sfree(state->clues->forbid);
	sfree(state->clues);
    }
    sfree(state);
}

static char *solve_game(const game_state *state, const game_state *currstate,
                        const char *aux, const char **error)
{
	int w = state->par.w, a = w*w, dep = state->par.dep;
    int i, ret;
    digit *soln;
	bool *impose, *forbid;
    char *out;

    if (aux)
	return dupstr(aux);

    soln = snewn(a, digit);
	impose = snewn(a, bool);
	forbid = snewn(a, bool);
    memcpy(soln, state->clues->immutable, a);
    memcpy(impose, state->clues->impose, a);
    memcpy(forbid, state->clues->forbid, a);

    ret = solver(soln, impose, forbid, w, dep, DIFFCOUNT-1);

    if (ret == diff_impossible) {
	*error = "No solution exists for this puzzle";
	out = NULL;
    } else if (ret == diff_ambiguous) {
	*error = "Multiple solutions exist for this puzzle";
	out = NULL;
    } else {
	out = snewn(a+2, char);
	out[0] = 'S';
	for (i = 0; i < a; i++)
	    out[i+1] = '0' + soln[i];
	out[a+1] = '\0';
    }

    sfree(soln);
	sfree(impose);
	sfree(forbid);
    return out;
}

static bool game_can_format_as_text_now(const game_params *params)
{
    return true;
}

static char *game_text_format(const game_state *state)
{
	int x, y, pos, w = state->par.w;
	char *ret, *p;
	
	ret = snewn(w*(w+1)+1, char);

	p = ret;
    for (y = 0; y < w; y++) {
    	for (x = 0; x < w; x++) {
			pos = y*w+x;
			*p++ = ' ';
        	if(state->clues->forbid[pos]) 
				*p++ = 'X';
			else if(state->clues->impose[pos] && state->grid[pos] < 1)
				*p++ = 'O';
			else if(state->grid[pos] > 0)
				*p++ = state->grid[pos] + '0';
			else
				*p++ = '-';
           }
		*p++ = '\n';
    }
   	*p++ = '\0';
    return ret;
}

struct game_ui { /* Same as towers.c */
    int hx, hy;
    bool hpencil, hshow, hcursor;
};

static game_ui *new_ui(const game_state *state)
{
    game_ui *ui = snew(game_ui);

    ui->hx = ui->hy = 0;
    ui->hpencil = false;
    ui->hshow = false;
    ui->hcursor = false;

    return ui;
}

static void free_ui(game_ui *ui)
{
    sfree(ui);
}

static char *encode_ui(const game_ui *ui)
{
    return NULL;
}

static void decode_ui(game_ui *ui, const char *encoding)
{
}

static void game_changed_state(game_ui *ui, const game_state *oldstate,
                               const game_state *newstate)
{
	int w = newstate->par.w;
    if (ui->hshow && ui->hpencil && !ui->hcursor &&
        (newstate->grid[ui->hy * w + ui->hx] != 0 || newstate->clues->forbid[ui->hy * w + ui->hx])) {
        ui->hshow = false;
    }
}

#define PREFERRED_TILESIZE 48
#define TILESIZE (ds->tilesize)
#define BORDER (TILESIZE * 9 / 8)
#define COORD(x) ((x)*TILESIZE + BORDER)
#define FROMCOORD(x) (((x)+(TILESIZE-BORDER)) / TILESIZE - 1)

#define FLASH_TIME 0.4F

#define DF_PENCIL_SHIFT 19
#define DF_IMMUTABLE_CIRCLE 0x40000
#define DF_CROSS 0x20000
#define DF_CIRCLE 0x10000
#define DF_ERROR 0x8000
#define DF_HIGHLIGHT 0x4000
#define DF_HIGHLIGHT_PENCIL 0x2000
#define DF_IMMUTABLE 0x1000
#define DF_PLAYAREA 0x0800
#define DF_DIGIT_MASK 0x00FF

struct game_drawstate {
	int tilesize;
    bool started;
    long *tiles;		       /* w*w temp space */
    long *drawn;		       /* w*w*4: current drawn data */
    bool *errtmp;
};

static bool check_errors(const game_state *state, bool *errors)
{
    int w = state->par.w, a = w*w, dep = state->par.dep;
    digit *grid = state->grid;
    int i, x, y;
    bool errs = false;

    if (errors)
	for (i = 0; i < a; i++)
	    errors[i] = false;

    for (y = 0; y < w; y++) {
	unsigned long mask = 0, errmask = 0;
	for (x = 0; x < w; x++) {
	    unsigned long bit = grid[y*w+x] ? (1UL << grid[y*w+x]) : 0;
	    errmask |= (mask & bit);
	    mask |= bit;
	}

	if (mask != (1L << (dep+1)) - (1L << 1)) {
	    errs = true;
	    errmask &= ~1UL;
	    if (errors) {
		for (x = 0; x < w; x++)
		    if (errmask & (1UL << grid[y*w+x]))
			errors[y*w+x] = true;
	    }
	}
    }

    for (x = 0; x < w; x++) {
	unsigned long mask = 0, errmask = 0;
	for (y = 0; y < w; y++) {
	    unsigned long bit = (grid[y*w+x]) ? (1UL << grid[y*w+x]) : 0;
	    errmask |= (mask & bit);
	    mask |= bit;
	}

	if (mask != (1 << (dep+1)) - (1L << 1)) {
	    errs = true;
	    errmask &= ~1UL;
	    if (errors) {
		for (y = 0; y < w; y++)
		    if (errmask & (1UL << grid[y*w+x]))
			errors[y*w+x] = true;
	    }
	}
    }

    return errs;
}

static char *interpret_move(const game_state *state, game_ui *ui,
                            const game_drawstate *ds,
                            int x, int y, int button)
{
    int w = state->par.w, dep = state->par.dep;
    int tx, ty;
    char buf[80];

    button &= ~MOD_MASK;

    tx = FROMCOORD(x);
    ty = FROMCOORD(y);

    if (tx >= 0 && tx < w && ty >= 0 && ty < w) {
        if (button == LEFT_BUTTON) {
	    if (tx == ui->hx && ty == ui->hy &&
		ui->hshow && !ui->hpencil) {
                ui->hshow = false;
            } else {
                ui->hx = tx;
                ui->hy = ty;
		ui->hshow = !state->clues->immutable[ty*w+tx] && !state->clues->forbid[ty*w+tx];
                ui->hpencil = false;
            }
            ui->hcursor = false;
            return UI_UPDATE;
        }
        if (button == RIGHT_BUTTON) {
            /*
             * Pencil-mode highlighting for non filled squares.
             */
            if (state->grid[ty*w+tx] == 0 && !state->clues->forbid[ty*w+tx]) {
                if (tx == ui->hx && ty == ui->hy &&
                    ui->hshow && ui->hpencil) {
                    ui->hshow = false;
                } else {
                    ui->hpencil = true;
                    ui->hx = tx;
                    ui->hy = ty;
                    ui->hshow = true;
                }
            } else {
                ui->hshow = false;
            }
            ui->hcursor = false;
            return UI_UPDATE;
        }
    } 
    if (IS_CURSOR_MOVE(button)) {
        move_cursor(button, &ui->hx, &ui->hy, w, w, false);
        ui->hshow = true;
        ui->hcursor = true;
        return UI_UPDATE;
    }
    if (ui->hshow &&
        (button == CURSOR_SELECT)) {
        ui->hpencil = !ui->hpencil;
        ui->hcursor = true;
        return UI_UPDATE;
    }

    if (ui->hshow &&
	((button >= '0' && button <= '9' && button - '0' <= dep) ||
	 button == CURSOR_SELECT2 || button == '\b')) {
	int n = button - '0';
	if (button == CURSOR_SELECT2 || button == '\b')
	    n = 0;

        /*
         * Can't make pencil marks in a filled square. This can only
         * become highlighted if we're using cursor keys.
         */
        if (ui->hpencil && state->grid[ui->hy*w+ui->hx])
            return NULL;

	/*
	 * Can't do anything to an immutable square.
	 */
        if (state->clues->immutable[ui->hy*w+ui->hx] || state->clues->forbid[ui->hy*w+ui->hx])
            return NULL;

	sprintf(buf, "%c%d,%d,%d",
		(char)(ui->hpencil && n > 0 ? 'P' : 'R'), ui->hx, ui->hy, n);

        if (!ui->hcursor) ui->hshow = false;

	return dupstr(buf);
    }
	
	if(ui->hshow && (button == 'X' || button == 'x' || button == 'O' || button == 'o')) {
		sprintf(buf, "%c%d,%d", toupper(button), ui->hx, ui->hy);
		return dupstr(buf);
	}

    if (button == 'M' || button == 'm')
        return dupstr("M");

    return NULL;
}

static game_state *execute_move(const game_state *from, const char *move)
{
    int w = from->par.w, a = w*w, dep = from->par.dep;
    game_state *ret = dup_game(from);
    int x, y, i, n;

    if (move[0] == 'S') {
	ret->completed = ret->cheated = true;

	for (i = 0; i < a; i++) {
            if (move[i+1] < '0' || move[i+1] > '0'+dep)
                goto badmove;
	    ret->grid[i] = move[i+1] - '0';
	    ret->pencil[i] = 0;
		if(move[i+1] > '0')
			ret->forbid[i] = false;
	}

        if (move[a+1] != '\0')
            goto badmove;

	return ret;
    } else if ((move[0] == 'P' || move[0] == 'R') &&
	sscanf(move+1, "%d,%d,%d", &x, &y, &n) == 3 &&
	x >= 0 && x < w && y >= 0 && y < w && n >= 0 && n <= w) {
	if (from->clues->immutable[y*w+x])
            goto badmove;

        if (move[0] == 'P' && n > 0) {
            ret->pencil[y*w+x] ^= 1L << n;
			ret->forbid[y*w+x] = false;
        } else {
            ret->grid[y*w+x] = n;
			ret->forbid[y*w+x] = false;
			if(n == 0)
				ret->impose[y*w+x] = false;
            ret->pencil[y*w+x] = 0;

            if (!ret->completed && !check_errors(ret, NULL))
                ret->completed = true;
        }
	return ret;
    } else if (move[0] == 'M') {
	/*
	 * Fill in absolutely all pencil marks everywhere. (I
	 * wouldn't use this for actual play, but it's a handy
	 * starting point when following through a set of
	 * diagnostics output by the standalone solver.)
	 */
	for (i = 0; i < a; i++) {
	    if (!ret->grid[i] && !ret->clues->forbid[i])
		ret->pencil[i] = (1L << (w+1)) - (1L << 1);
	}
	return ret;
    } else if((move[0] == 'X' || move[0] == 'O') && 
			  sscanf(move+1, "%d,%d", &x, &y) == 2 &&
			  x >= 0 && x < w && y >= 0 && y < w) {
		if(move[0] == 'X')
		{
			ret->grid[y*w+x] = 0;
			ret->pencil[y*w+x] = 0;
			ret->impose[y*w+x] = false;
			ret->forbid[y*w+x] = !ret->forbid[y*w+x];
		}
		else if(move[0] == 'O')
		{
			ret->forbid[y*w+x] = false;
			ret->impose[y*w+x] = !ret->impose[y*w+x];
		}
		return ret;
	}
		

  badmove:
    /* couldn't parse move string */
    free_game(ret);
    return NULL;
}

/* ----------------------------------------------------------------------
 * Drawing routines.
 */

#define SIZE(w) ((w) * TILESIZE + 2*BORDER)

static void game_compute_size(const game_params *params, int tilesize,
                              int *x, int *y)
{
    /* Ick: fake up `ds->tilesize' for macro expansion purposes */
    struct { int tilesize; } ads, *ds = &ads;
    ads.tilesize = tilesize;

    *x = *y = SIZE(params->w);
}

static void game_set_size(drawing *dr, game_drawstate *ds,
                          const game_params *params, int tilesize)
{
    ds->tilesize = tilesize;
}

static float *game_colours(frontend *fe, int *ncolours)
{
    float *ret = snewn(3 * NCOLOURS, float);

    frontend_default_colour(fe, &ret[COL_BACKGROUND * 3]);

    ret[COL_GRID * 3 + 0] = 0.0F;
    ret[COL_GRID * 3 + 1] = 0.0F;
    ret[COL_GRID * 3 + 2] = 0.0F;

    ret[COL_USER * 3 + 0] = 0.0F;
    ret[COL_USER * 3 + 1] = 0.6F * ret[COL_BACKGROUND * 3 + 1];
    ret[COL_USER * 3 + 2] = 0.0F;

    ret[COL_HIGHLIGHT * 3 + 0] = 0.78F * ret[COL_BACKGROUND * 3 + 0];
    ret[COL_HIGHLIGHT * 3 + 1] = 0.78F * ret[COL_BACKGROUND * 3 + 1];
    ret[COL_HIGHLIGHT * 3 + 2] = 0.78F * ret[COL_BACKGROUND * 3 + 2];

    ret[COL_ERROR * 3 + 0] = 1.0F;
    ret[COL_ERROR * 3 + 1] = 0.0F;
    ret[COL_ERROR * 3 + 2] = 0.0F;

    ret[COL_PENCIL * 3 + 0] = 0.5F * ret[COL_BACKGROUND * 3 + 0];
    ret[COL_PENCIL * 3 + 1] = 0.5F * ret[COL_BACKGROUND * 3 + 1];
    ret[COL_PENCIL * 3 + 2] = ret[COL_BACKGROUND * 3 + 2];

    ret[COL_DONE * 3 + 0] = ret[COL_BACKGROUND * 3 + 0] / 1.5F;
    ret[COL_DONE * 3 + 1] = ret[COL_BACKGROUND * 3 + 1] / 1.5F;
    ret[COL_DONE * 3 + 2] = ret[COL_BACKGROUND * 3 + 2] / 1.5F;

    *ncolours = NCOLOURS;
    return ret;
}

static game_drawstate *game_new_drawstate(drawing *dr, const game_state *state)
{
    int w = state->par.w /*, a = w*w */;
    struct game_drawstate *ds = snew(struct game_drawstate);
    int i;

    ds->tilesize = 0;
    ds->started = false;
    ds->tiles = snewn(w*w, long);
    ds->drawn = snewn(w*w*4, long);
    for (i = 0; i < w*w*4; i++)
	ds->drawn[i] = -1;
    ds->errtmp = snewn(w*w, bool);

    return ds;
}

static void game_free_drawstate(drawing *dr, game_drawstate *ds)
{
    sfree(ds->errtmp);
    sfree(ds->tiles);
    sfree(ds->drawn);
    sfree(ds);
}

static void draw_tile(drawing *dr, game_drawstate *ds, struct clues *clues,
		      int x, int y, long tile)
{
    int w = clues->w, depth = clues->dep /* , a = w*w */;
    int tx, ty, bg;
    char str[64];

    tx = COORD(x);
    ty = COORD(y);

    bg = (tile & DF_HIGHLIGHT) ? COL_HIGHLIGHT : COL_BACKGROUND;

    /* erase background */
    draw_rect(dr, tx, ty, TILESIZE, TILESIZE, bg);

    /* pencil-mode highlight */
    if (tile & DF_HIGHLIGHT_PENCIL) {
        int coords[6];
        coords[0] = tx;
        coords[1] = ty;
        coords[2] = tx+TILESIZE/2;
        coords[3] = ty;
        coords[4] = tx;
        coords[5] = ty+TILESIZE/2;
        draw_polygon(dr, coords, 3, COL_HIGHLIGHT, COL_HIGHLIGHT);
    }

    /* draw box outline */
    if (tile & DF_PLAYAREA) {
        int coords[8];
        coords[0] = tx;
        coords[1] = ty - 1;
        coords[2] = tx + TILESIZE;
        coords[3] = ty - 1;
        coords[4] = tx + TILESIZE;
        coords[5] = ty + TILESIZE - 1;
        coords[6] = tx;
        coords[7] = ty + TILESIZE - 1;
        draw_polygon(dr, coords, 4, -1, COL_GRID);
    }

    /* new number needs drawing? */
	if (tile & DF_CIRCLE)
	{
		int color;

        if (tile & DF_IMMUTABLE_CIRCLE)
            color = COL_GRID;
        else
            color = COL_PENCIL;
		
		draw_circle(dr, tx + TILESIZE/2, ty + TILESIZE/2, TILESIZE*3/7,
                 -1, color);
	} else if (tile & DF_CROSS)
	{
		int color;

        if (tile & DF_IMMUTABLE)
            color = COL_GRID;
        else
            color = COL_PENCIL;
		
		draw_line(dr, tx+TILESIZE/8, ty+TILESIZE/8, tx+TILESIZE*7/8, ty+TILESIZE*7/8,
                     color);
		
		draw_line(dr, tx+TILESIZE*7/8, ty+TILESIZE/8, tx+TILESIZE/8, ty+TILESIZE*7/8,
                     color);
	}
		
    if (tile & DF_DIGIT_MASK) {
        int color;

	str[1] = '\0';
	str[0] = (tile & DF_DIGIT_MASK) + '0';

        if (tile & DF_ERROR)
            color = COL_ERROR;
		
        else if (x < 0 || y < 0 || x >= w || y >= w)
            color = COL_GRID;
        else if (tile & DF_IMMUTABLE)
            color = COL_GRID;
        else
            color = COL_USER;

	draw_text(dr, tx + TILESIZE/2, ty + TILESIZE/2, FONT_VARIABLE,
		  (tile & DF_PLAYAREA ? TILESIZE/2 : TILESIZE*2/5),
                  ALIGN_VCENTRE | ALIGN_HCENTRE, color, str);
    } else {
        int i, j, npencil;
	int pl, pr, pt, pb;
	float bestsize;
	int pw, ph, minph, pbest, fontsize;

        /* Count the pencil marks required. */
        for (i = 1, npencil = 0; i <= depth; i++)
            if (tile & (1L << (i + DF_PENCIL_SHIFT)))
		npencil++;
	if (npencil) {

	    minph = 2;

	    /*
	     * Determine the bounding rectangle within which we're going
	     * to put the pencil marks.
	     */
	    pl = tx;
	    pr = tx + TILESIZE;
	    pt = ty;
	    pb = ty + TILESIZE;

	    /*
	     * We arrange our pencil marks in a grid layout, with
	     * the number of rows and columns adjusted to allow the
	     * maximum font size.
	     *
	     * So now we work out what the grid size ought to be.
	     */
	    bestsize = 0.0;
	    pbest = 0;
	    /* Minimum */
	    for (pw = 3; pw < max(npencil,4); pw++) {
		float fw, fh, fs;

		ph = (npencil + pw - 1) / pw;
		ph = max(ph, minph);
		fw = (pr - pl) / (float)pw;
		fh = (pb - pt) / (float)ph;
		fs = min(fw, fh);
		if (fs > bestsize) {
		    bestsize = fs;
		    pbest = pw;
		}
	    }
	    assert(pbest > 0);
	    pw = pbest;
	    ph = (npencil + pw - 1) / pw;
	    ph = max(ph, minph);

	    /*
	     * Now we've got our grid dimensions, work out the pixel
	     * size of a grid element, and round it to the nearest
	     * pixel. (We don't want rounding errors to make the
	     * grid look uneven at low pixel sizes.)
	     */
	    fontsize = min((pr - pl) / pw, (pb - pt) / ph);

	    /*
	     * Centre the resulting figure in the square.
	     */
	    pl = pl + (pr - pl - fontsize * pw) / 2;
	    pt = pt + (pb - pt - fontsize * ph) / 2;

	    /*
	     * Now actually draw the pencil marks.
	     */
	    for (i = 1, j = 0; i <= w; i++)
		if (tile & (1L << (i + DF_PENCIL_SHIFT))) {
		    int dx = j % pw, dy = j / pw;

		    str[1] = '\0';
		    str[0] = i + '0';
		    draw_text(dr, pl + fontsize * (2*dx+1) / 2,
			      pt + fontsize * (2*dy+1) / 2,
			      FONT_VARIABLE, fontsize,
			      ALIGN_VCENTRE | ALIGN_HCENTRE, COL_PENCIL, str);
		    j++;
		}
	}
    }
}

static void game_redraw(drawing *dr, game_drawstate *ds,
                        const game_state *oldstate, const game_state *state,
                        int dir, const game_ui *ui,
                        float animtime, float flashtime)
{
    int w = state->par.w /*, a = w*w */;
    int i, x, y;

    if (!ds->started) {
	/*
	 * The initial contents of the window are not guaranteed and
	 * can vary with front ends. To be on the safe side, all
	 * games should start by drawing a big background-colour
	 * rectangle covering the whole window.
	 */
	draw_rect(dr, 0, 0, SIZE(w), SIZE(w), COL_BACKGROUND);

	draw_update(dr, 0, 0, SIZE(w), SIZE(w));

	ds->started = true;
    }

    check_errors(state, ds->errtmp);

    /*
     * Work out what data each tile should contain.
     */
    for (i = 0; i < w*w; i++)
	ds->tiles[i] = 0;	       /* completely blank square */

    for (y = 0; y < w; y++) {
	for (x = 0; x < w; x++) {
	    long tile = DF_PLAYAREA;

	    if (state->grid[y*w+x])
			tile |= state->grid[y*w+x];
	    else
			tile |= (long)state->pencil[y*w+x] << DF_PENCIL_SHIFT;

	    if (ui->hshow && ui->hx == x && ui->hy == y)
			tile |= (ui->hpencil ? DF_HIGHLIGHT_PENCIL : DF_HIGHLIGHT);

	    if (state->clues->immutable[y*w+x])
			tile |= DF_IMMUTABLE;
		
		if (state->clues->impose[y*w+x])
			tile |= DF_CIRCLE | DF_IMMUTABLE_CIRCLE;
		else if (state->clues->forbid[y*w+x])
			tile |= DF_CROSS | DF_IMMUTABLE;
		else if (state->impose[y*w+x])
			tile |= DF_CIRCLE;
		else if (state->forbid[y*w+x])
			tile |= DF_CROSS;

            if (flashtime > 0 &&
                (flashtime <= FLASH_TIME/3 ||
                 flashtime >= FLASH_TIME*2/3))
                tile |= DF_HIGHLIGHT;  /* completion flash */

	    if (ds->errtmp[y*w+x])
			tile |= DF_ERROR;

	    ds->tiles[y*w+x] = tile;
	}
    }

    /*
     * Now actually draw anything that needs to be changed.
     */
    for (y = 0; y < w; y++) {
	for (x = 0; x < w; x++) {
	    long tr;
	    int i = y*w+x;

	    tr = ds->tiles[y*w+x];

		if(ds->drawn[i] != tr)
		{
		clip(dr, COORD(x)-1, COORD(y)-1, TILESIZE+2, TILESIZE+2);

		draw_tile(dr, ds, state->clues, x, y, tr);	

		unclip(dr);
		draw_update(dr, COORD(x), COORD(y), TILESIZE, TILESIZE);

		ds->drawn[i] = tr;
	    }
	}
    }
}

static float game_anim_length(const game_state *oldstate,
                              const game_state *newstate, int dir, game_ui *ui)
{
    return 0.0F;
}

static float game_flash_length(const game_state *oldstate,
                               const game_state *newstate, int dir, game_ui *ui)
{
    if (!oldstate->completed && newstate->completed &&
	!oldstate->cheated && !newstate->cheated)
        return FLASH_TIME;
    return 0.0F;
}

static int game_status(const game_state *state)
{
    return state->completed ? +1 : 0;
}

static bool game_timing_state(const game_state *state, game_ui *ui)
{
    if (state->completed)
	return false;
    return true;
}

static void game_print_size(const game_params *params, float *x, float *y)
{
    int pw, ph;

    /*
     * We use 9mm squares by default, like Solo.
     */
    game_compute_size(params, 900, &pw, &ph);
    *x = pw / 100.0F;
    *y = ph / 100.0F;
}

static void game_print(drawing *dr, const game_state *state, int tilesize)
{
	int w = state->par.w;
    int ink = print_mono_colour(dr, 0);
    int x, y;

    /* Ick: fake up `ds->tilesize' for macro expansion purposes */
    game_drawstate ads, *ds = &ads;
    game_set_size(dr, ds, NULL, tilesize);

    /*
     * Border.
     */
    print_line_width(dr, 3 * TILESIZE / 40);
    draw_rect_outline(dr, BORDER, BORDER, w*TILESIZE, w*TILESIZE, ink);

    /*
     * Main grid.
     */
    for (x = 1; x < w; x++) {
	print_line_width(dr, TILESIZE / 40);
	draw_line(dr, BORDER+x*TILESIZE, BORDER,
		  BORDER+x*TILESIZE, BORDER+w*TILESIZE, ink);
    }
    for (y = 1; y < w; y++) {
	print_line_width(dr, TILESIZE / 40);
	draw_line(dr, BORDER, BORDER+y*TILESIZE,
		  BORDER+w*TILESIZE, BORDER+y*TILESIZE, ink);
    }

    for (y = 0; y < w; y++)
	for (x = 0; x < w; x++)
	{
		if(state->clues->impose[y*w+x] || state->impose[y*w+x])
			draw_circle(dr, x + TILESIZE/2, y + TILESIZE/2, TILESIZE*3/7, -1, ink);
		
		if(state->clues->forbid[y*w+x] || state->forbid[y*w+x])
		{
			draw_line(dr, x*TILESIZE + TILESIZE/8, y*TILESIZE + TILESIZE/8, 
					  	  x*TILESIZE + TILESIZE*7/8, y*TILESIZE + TILESIZE*7/8, ink);
			draw_line(dr, x*TILESIZE + TILESIZE*7/8, y*TILESIZE + TILESIZE/8, 
					  x*TILESIZE + TILESIZE/8, y*TILESIZE + TILESIZE*7/8, ink);
		}
			
	    if (state->grid[y*w+x]) {
		char str[2];
		str[1] = '\0';
		str[0] = state->grid[y*w+x] + '0';
		draw_text(dr, BORDER + x*TILESIZE + TILESIZE/2,
			  BORDER + y*TILESIZE + TILESIZE/2,
			  FONT_VARIABLE, TILESIZE/2,
			  ALIGN_VCENTRE | ALIGN_HCENTRE, ink, str);
	    }
	}
}

#ifdef COMBINED
#define thegame nullgame
#endif

const struct game thegame = {
    "Numberball", "games.numberball", "numberball",
    default_params,
    game_fetch_preset, NULL,
    decode_params,
    encode_params,
    free_params,
    dup_params,
    true, game_configure, custom_params,
    validate_params,
    new_game_desc,
    validate_desc,
    new_game,
    dup_game,
    free_game,
    true, solve_game,
    true, game_can_format_as_text_now, game_text_format,
    new_ui,
    free_ui,
    encode_ui,
    decode_ui,
    game_request_keys,
    game_changed_state,
    interpret_move,
    execute_move,
    PREFERRED_TILESIZE, game_compute_size, game_set_size,
    game_colours,
    game_new_drawstate,
    game_free_drawstate,
    game_redraw,
    game_anim_length,
    game_flash_length,
    game_status,
    false, false, game_print_size, game_print, /* FIX ME add printing function and change first false to true */
    false,			       /* wants_statusbar */
    false, game_timing_state,
    REQUIRE_RBUTTON | REQUIRE_NUMPAD,				       /* flags */
};

#ifdef STANDALONE_SOLVER

#include <stdarg.h>

int main(int argc, char **argv)
{
    game_params *p;
    game_state *s;
    char *id = NULL, *desc;
    const char *err;
    bool grade = false;
    int ret, diff;
    bool really_show_working = false;

    while (--argc > 0) {
        char *p = *++argv;
        if (!strcmp(p, "-v")) {
            really_show_working = true;
        } else if (!strcmp(p, "-g")) {
            grade = true;
        } else if (*p == '-') {
            fprintf(stderr, "%s: unrecognised option `%s'\n", argv[0], p);
            return 1;
        } else {
            id = p;
        }
    }
				   
    if (!id) {
        fprintf(stderr, "usage: %s [-g | -v] <game_id>\n", argv[0]);
        return 1;
    }

    desc = strchr(id, ':');
    if (!desc) {
        fprintf(stderr, "%s: game id expects a colon in it\n", argv[0]);
        return 1;
    }
    *desc++ = '\0';

    p = default_params();
    decode_params(p, id);
    err = validate_desc(p, desc);
    if (err) {
        fprintf(stderr, "%s: %s\n", argv[0], err);
        return 1;
    }
    s = new_game(NULL, p, desc);

    /*
     * When solving an Easy puzzle, we don't want to bother the
     * user with Hard-level deductions. For this reason, we grade
     * the puzzle internally before doing anything else.
     */
    ret = -1;			       /* placate optimiser */
    solver_show_working = 0;
    for (diff = 0; diff < DIFFCOUNT; diff++) {
	memcpy(s->grid, s->clues->immutable, p->w * p->w);
	ret = solver(s->grid, s->clues->impose, s->clues->forbid, p->w, p->dep, diff);
	if (ret <= diff)
	    break;
    }

    if (really_show_working) {
        /*
         * Now run the solver again at the last difficulty level we
         * tried, but this time with diagnostics enabled.
         */
        solver_show_working = really_show_working;
        memcpy(s->grid, s->clues->immutable, p->w * p->w);
        ret = solver(s->grid, s->clues->impose, s->clues->forbid, p->w, p->dep,
                     diff < DIFFCOUNT ? diff : DIFFCOUNT-1);
    }

    if (diff == DIFFCOUNT) {
	if (grade)
	    printf("Difficulty rating: ambiguous\n");
	else
	    printf("Unable to find a unique solution\n");
    } else {
	if (grade) {
	    if (ret == diff_impossible)
		printf("Difficulty rating: impossible (no solution exists)\n");
	    else
		printf("Difficulty rating: %s\n", numberball_diffnames[ret]);
	} else {
	    if (ret != diff)
		printf("Puzzle is inconsistent\n");
	    else
		fputs(game_text_format(s), stdout);
	}
    }

    return 0;
}

#endif
