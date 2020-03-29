#include <time.h>

#include <assert.h>
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <ncurses.h>

#define ROWS 50
#define COLS 100

#define PROB_IGNITION 0.00001
#define PROB_GROWTH   0.75

#define die(...) {fprintf(stderr, __VA_ARGS__); exit(EXIT_FAILURE);}

struct offset {
	int row;
	int col;
};

struct offset offsets[] = {
	{-1, -1}, {-1, 0}, {-1, 1},
	{ 0, -1},          { 0, 1},
	{ 1, -1}, { 1, 0}, { 1, 1},
};

enum state {
	EMPTY,
	TREE,
	BURN,
};

static const char state2char[] = {
	' ',
	'T',
	'#',
};

struct Cell {
	enum state state;
};

typedef struct World World;
struct World {
	int gen;
	int rows;
	int cols;
	float f;
	float p;
	struct Cell **grid;
	World *next;
	World *prev;
};


/*
 * THE authoritative source of IDs in this program. To help spot ancestry bugs,
 * like a missing next/prev pointer.
 */
int id = 0;

int n_neighbors = sizeof(offsets) / sizeof(struct offset);


struct timespec
timespec_of_float(double n)
{
	double integral;
	double fractional;
	struct timespec t;

	fractional = modf(n, &integral);
	t.tv_sec = (int) integral;
	t.tv_nsec = (int) (1E9 * fractional);

	return t;
}

int
is_probable(float probability)
{
	return probability >= drand48();
}

World *
world_create(int rows, int cols)
{
	World *w;
	int r;
	int k;

	w = calloc(1, sizeof(World));
	w->gen  = id++;
	w->rows = rows;
	w->cols = cols;
	w->f = PROB_IGNITION;
	w->p = PROB_GROWTH;
	w->grid = calloc(rows, sizeof(struct Cell*));
	w->next = NULL;
	w->prev = NULL;

	for (r = 0; r < w->rows; r++)
		w->grid[r] = calloc(cols, sizeof(struct Cell));

	for (r = 0; r < w->rows; r++)
		for (k = 0; k < w->cols; k++)
			w->grid[r][k].state = EMPTY;

	return w;
}

void
world_init(World *w)
{
	int r;
	int k;

	for (r = 0; r < w->rows; r++)
		for (k = 0; k < w->cols; k++)
			w->grid[r][k].state = is_probable(w->p) ? TREE : EMPTY;
}


void
world_print(World *w)
{
	int r;
	int k;
	enum state s;
	int ci; /* COLOR_PAIR index */

	for (k = 0; k < w->cols; k++)
		mvprintw(0, k, " ");
	mvprintw(0, 0, "gen: %d", w->gen);
	for (r = 0; r < w->rows; r++) {
		for (k = 0; k < w->cols; k++) {
			s = w->grid[r][k].state;
			ci = s + 1;
			if (s == BURN)
				attron(A_BOLD);
			attron(COLOR_PAIR(ci));
			mvprintw(r + 1, k, "%c", state2char[s]);
			attroff(COLOR_PAIR(ci));
			if (s == BURN)
				attroff(A_BOLD);
		}
	}
	refresh();
}

int
world_pos_is_inbounds(World *w, int row, int col)
{
	return row >= 0 && row < w->rows && col >= 0 && col < w->cols;
}

World *
world_next(World *w0)
{
	int r;
	int k;
	int o;   /* offset index */
	int nr;  /* neighbor's row */
	int nk;  /* neighbor's col */
	int n;   /* neighbors burning */
	enum state s0;
	enum state s1;
	struct offset off;
	World *w1 = w0->next;

	if (w1)
		return w1;

	w1       = world_create(w0->rows, w0->cols);
	w1->prev = w0;
	w0->next = w1;

	for (r = 0; r < w0->rows; r++)
		for (k = 0; k < w0->cols; k++) {
			s0 = w0->grid[r][k].state;
			n = 0;
			for (o = 0; o < n_neighbors; o++) {
				off = offsets[o];
				nr = r + off.row;
				nk = k + off.col;
				if (world_pos_is_inbounds(w0, nr, nk))
					if (w0->grid[nr][nk].state == BURN)
						n++;
			}
			switch (s0) {
			case BURN:
				s1 = EMPTY;
				break;
			case TREE:
				if (n > 0) {
					s1 = BURN;
					break;
				}
				if (is_probable(w0->f)) {
					s1 = BURN;
					break;
				}
				s1 = TREE;
				break;
			case EMPTY:
				if (is_probable(w0->p)) {
					s1 = TREE;
					break;
				}
				s1 = EMPTY;
				break;
			default:
				assert(0);
			}
			w1->grid[r][k].state = s1;
		}
	return w1;
}

int
main()
{
	int rows = ROWS;
	int cols = COLS;
	int go = 0;
	struct timespec interval = timespec_of_float(0.1);
	World *w;

	srand48(time(NULL));

	w = world_create(rows, cols);
	world_init(w);

	initscr();
	noecho();
	start_color();
	init_pair(EMPTY + 1, COLOR_BLACK, COLOR_BLACK);
	init_pair(TREE  + 1, COLOR_GREEN, COLOR_BLACK);
	init_pair(BURN  + 1, COLOR_RED  , COLOR_BLACK);

	for (;;) {
		world_print(w);
		if (!go)
			for (;;) {
				switch (getch()) {
				case 'n': goto next_gen;
				case 'p': goto prev_gen;
				case 'g': go = 1; goto next_gen;
				case 'q': goto quit;
				default : continue;
				}
			}
		else
			if (nanosleep(&interval, NULL) < 0)
				die("nanosleep: %s", strerror(errno));
		next_gen:
			w = world_next(w);
			continue;
		prev_gen:
			if(w->prev)
				w = w->prev;
			continue;
	}

	quit:
	endwin();
	return 0;
}
