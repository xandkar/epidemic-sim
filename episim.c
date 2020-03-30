#include <sys/ioctl.h>
#include <time.h>

#include <assert.h>
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <ncurses.h>

#define FPS 20
#define PROB_IGNITION 0.001  /* f */
#define PROB_GROWTH   0.025  /* p */

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
world_print(World *w, int playing)
{
	int r;
	int k;
	enum state s;
	int ci; /* COLOR_PAIR index */

	for (k = 0; k < w->cols; k++)
		mvprintw(0, k, " ");
	mvprintw(
	    0, 0,
	    "%s | gen: %4d | p: %.3f | f: %.3f | p/f: %3.f | FPS: %d",
	    (playing ? "|>" : "||"),
	    w->gen, w->p, w->f, (w->p / w->f), FPS
	);
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
	int playing = 0;
	struct timespec interval = timespec_of_float(1.0 / FPS);
	struct winsize winsize;
	World *w;

	srand48(time(NULL));

	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &winsize) < 0)
		die("ioctl: errno = %d, msg = %s\n", errno, strerror(errno));
	w = world_create(
	    winsize.ws_row - 1,  /* Leave room for the status line.  */
	    winsize.ws_col
	);
	world_init(w);

	initscr();
	noecho();
	start_color();
	init_pair(EMPTY + 1, COLOR_BLACK, COLOR_BLACK);
	init_pair(TREE  + 1, COLOR_GREEN, COLOR_BLACK);
	init_pair(BURN  + 1, COLOR_RED  , COLOR_BLACK);

	timeout(-1);
	for (;;) {
		world_print(w, playing);
		control:
			switch (getch()) {
			case 'p':
				if (playing) {
					timeout(-1);
					playing = 0;
				} else {
					timeout(0);
					playing = 1;
				}
				goto forward;
			case 'f':
				goto forward;
			case 'b':
				goto back;
			case 's':
				goto stop;
			default:
				if (playing)
					goto forward;
				else
					goto control;
			}
		forward:
			w = world_next(w);
			goto delay;
		back:
			if(w->prev)
				w = w->prev;
			goto delay;
		delay:
			if (nanosleep(&interval, NULL) < 0 && errno != EINTR)
				die("nanosleep: %s", strerror(errno));
	}

	stop:
	endwin();
	return 0;
}
