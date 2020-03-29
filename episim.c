#include <time.h>

#include <assert.h>
#include <stdlib.h>
#include <unistd.h>

#include <ncurses.h>

#define ROWS 50
#define COLS 100

#define STATE_LO 1
#define STATE_HI 3

#define PROB_IGNITION 0.00001
#define PROB_GROWTH   0.75

struct offset {
	int row;
	int col;
};

struct offset offsets[] = {
	{.row = -1, .col = -1}, {.row = -1, .col =  0}, {.row = -1, .col =  1},
	{.row =  0, .col = -1},                         {.row =  0, .col =  1},
	{.row =  1, .col = -1}, {.row =  1, .col =  0}, {.row =  1, .col =  1},
};

enum state {
	/*
	 * Do not want to start from 0 because I want to use these labels as
	 * ncurses color_pair index, which cannot be 0.
	 */
	EMPTY = STATE_LO,
	TREE,
	BURN = STATE_HI,
};

struct Cell {
	enum state state;
};

typedef struct World {
	int gen;
	int rows;
	int cols;
	float f;
	float p;
	struct Cell **grid;
} World;


int n_neighbors = sizeof(offsets) / sizeof(struct offset);


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
	w->gen  = 0;
	w->rows = rows;
	w->cols = cols;
	w->f = PROB_IGNITION;
	w->p = PROB_GROWTH;

	w->grid = calloc(rows, sizeof(struct Cell*));
	for (r = 0; r < w->rows; r++)
		w->grid[r] = calloc(cols, sizeof(struct Cell));

	for (r = 0; r < w->rows; r++)
		for (k = 0; k < w->cols; k++)
			w->grid[r][k].state = is_probable(w->p) ? TREE : EMPTY;

	return w;
}


void
world_print(World *w)
{
	int r;
	int k;
	enum state s;
	char c;

	mvprintw(0, 0, "gen: %d", w->gen);
	for (r = 0; r < w->rows; r++) {
		for (k = 0; k < w->cols; k++) {
			s = w->grid[r][k].state;
			switch (s) {
			case EMPTY:
				c = ' ';
				break;
			case TREE:
				c = 'T';
				break;
			case BURN:
				attron(A_BOLD);
				c = '#';
				break;
			default:
				assert(0);
			}
			attron(COLOR_PAIR(s));
			mvprintw(r + 1, k, "%c", c);
			attroff(COLOR_PAIR(s));
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

void
world_next(World *w0, World *w1)
{
	int r;
	int k;
	int o;   /* offset index */
	int nr;  /* neighbor's row */
	int nk;  /* neighbor's col */
	int n;   /* neighbors burning */
	enum state ns;  /* neighbor's state */
	enum state s0;
	enum state s1;
	struct offset off;

	w1->gen = w0->gen + 1;
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
}

int
main()
{
	int rows = ROWS;
	int cols = COLS;
	int r;
	int k;
	World *temp;
	World *curr;
	World *next;

	srand48(time(NULL));

	temp = world_create(rows, cols);
	curr = world_create(rows, cols);
	next = world_create(rows, cols);

	initscr();
	noecho();
	start_color();
	init_pair(EMPTY, COLOR_BLACK, COLOR_BLACK);
	init_pair(TREE , COLOR_GREEN, COLOR_BLACK);
	init_pair(BURN , COLOR_RED  , COLOR_BLACK);

	for (;;) {
		world_print(curr);
		for (;;) {
			switch (getch()) {
			case 'n': goto next_gen;
			case 'p': goto prev_gen;
			case 'q': goto quit;
			default : continue;
			}
		}
		next_gen: {
			world_next(curr, next);
			temp = curr;
			curr = next;
			next = temp;
			continue;
		}
		prev_gen: {
			temp = next;
			next = curr;
			curr = temp;
			continue;
		}
	}

	quit:
	endwin();
	return 0;
}
