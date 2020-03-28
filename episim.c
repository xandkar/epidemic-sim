#include <time.h>

#include <stdlib.h>
#include <unistd.h>

#include <ncurses.h>

#define ROWS 40
#define COLS 80

#define STATE_LO 0
#define STATE_HI 1

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
	DEAD  = STATE_LO,
	ALIVE = STATE_HI,
};

struct Agent {
	enum state state;
};

typedef struct World {
	int gen;
	int rows;
	int cols;
	struct Agent **grid;
} World;


int n_neighbors = sizeof(offsets) / sizeof(struct offset);


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

	w->grid = calloc(rows, sizeof(struct Agent*));
	for (r = 0; r < w->rows; r++)
		w->grid[r] = calloc(cols, sizeof(struct Agent));

	for (r = 0; r < w->rows; r++)
		for (k = 0; k < w->cols; k++)
			w->grid[r][k].state = rand() % (STATE_HI + 1);

	return w;
}


void
world_print(World *w)
{
	int r;
	int k;
	char c;

	mvprintw(0, 0, "gen: %d", w->gen);
	for (r = 0; r < w->rows; r++) {
		for (k = 0; k < w->cols; k++) {
			c = w->grid[r][k].state == ALIVE ? '+' : ' ';
			mvprintw(r + 1, k, "%c", c);
		}
	}
}

int
world_is_inbounds(World *w, int row, int col)
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
	int n;   /* neighbors state sum */
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
				if (world_is_inbounds(w0, nr, nk))
					n += w0->grid[nr][nk].state;
			}
			if (
			    (s0 == ALIVE && (n == 2 || n == 3))
			    ||
			    (s0 == DEAD && (n == 3))
			)
				s1 = ALIVE;
			else
				s1 = DEAD;
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

	srand(time(NULL));

	temp = world_create(rows, cols);
	curr = world_create(rows, cols);
	next = world_create(rows, cols);

	initscr();
	noecho();

	for (;;) {
		world_print(curr);
		for (;;) {
			switch (getch()) {
			case 'f': goto forward;
			case 'b': goto backward;
			case 'q': goto quit;
			default : continue;
			}
		}
		forward: {
			world_next(curr, next);
			temp = curr;
			curr = next;
			next = temp;
			continue;
		}
		backward: {
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
