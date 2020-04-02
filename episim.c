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
#define CMD_BUF_SIZE  50

#define die(...) do {fprintf(stderr, __VA_ARGS__); exit(EXIT_FAILURE);} while (0)

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

struct Cell {
	enum state state;
};

typedef struct World World;
struct World {
	unsigned int id;
	unsigned int gen;
	unsigned int rows;
	unsigned int cols;
	float f;
	float p;
	struct Cell **grid;
	World *next;
	World *prev;
};


unsigned int n_neighbors = sizeof(offsets) / sizeof(struct offset);


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
world_create(unsigned int gen, unsigned int rows, unsigned int cols, float f, float p)
{
	/*
	 * IDs help us spot ancestry bugs, like a missing next/prev pointer.
	 */
	static unsigned int id = 0;

	World *w;
	unsigned int r;
	unsigned int k;

	w = calloc(1, sizeof(World));
	w->id   = id++;
	w->gen  = gen;
	w->rows = rows;
	w->cols = cols;
	w->f = f;
	w->p = p;
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
	unsigned int r;
	unsigned int k;

	for (r = 0; r < w->rows; r++)
		for (k = 0; k < w->cols; k++)
			w->grid[r][k].state = is_probable(w->p) ? TREE : EMPTY;
}


void
world_print(World *w, int playing)
{
	unsigned int r;
	unsigned int k;
	enum state s;
	int ci; /* COLOR_PAIR index */

	for (k = 0; k < w->cols; k++)
		mvprintw(0, k, " ");
	mvprintw(
	    0, 0,
	    "%s | gen: %d | id: %d | p: %.3f | f: %.3f | p/f: %3.f | FPS: %d",
	    (playing ? "|>" : "||"),
	    w->gen, w->id, w->p, w->f, (w->p / w->f), FPS
	);
	for (r = 0; r < w->rows; r++) {
		for (k = 0; k < w->cols; k++) {
			s = w->grid[r][k].state;
			ci = s + 1;
			if (s == BURN)
				attron(A_BOLD);
			attron(COLOR_PAIR(ci));
			mvprintw(r + 1, k, " ");
			attroff(COLOR_PAIR(ci));
			if (s == BURN)
				attroff(A_BOLD);
		}
	}
	refresh();
}

int
world_pos_is_inbounds(World *w, unsigned int row, unsigned int col)
{
	return row < w->rows && col < w->cols;
}

World *
world_next(World *w0)
{
	unsigned int r;
	unsigned int k;
	unsigned int o;   /* offset index */
	unsigned int nr;  /* neighbor's row */
	unsigned int nk;  /* neighbor's col */
	unsigned int n;   /* neighbors burning */
	enum state s0;
	enum state s1;
	struct offset off;
	World *w1 = w0->next;

	if (w1)
		return w1;

	w1       = world_create(w0->gen + 1, w0->rows, w0->cols, w0->f, w0->p);
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

void
world_destroy_future(World *w)
{
	/*
	 * After changing a world parameter, we must remove at least its
	 * consequent, child worlds, since they are no longer valid under the
	 * new parameters and must be re-computed.
	 *
	 * TODO: Should the current world be re-computed as well?
	 * TODO: Perhaps world_next should take these parameters instead of this function?
	 * TODO: Recompute if params are diff from parent?
	 */
	World *cur;
	World *tmp;

	for (cur = w->next; cur != NULL; ) {
		tmp = cur->next;
		free(cur);
		cur = tmp;
	}
	w->next = NULL;
}

int
main()
{
	int control_timeout = -1;
	int playing = 0;
	struct timespec interval = timespec_of_float(1.0 / FPS);
	struct winsize winsize;
	char cmd[CMD_BUF_SIZE];
	World *w;
	float tmp_p;
	float tmp_f;

	srand48(time(NULL));

	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &winsize) < 0)
		die("ioctl: errno = %d, msg = %s\n", errno, strerror(errno));
	w = world_create(
	    0,
	    winsize.ws_row - 1,  /* Leave room for the status line.  */
	    winsize.ws_col,
	    PROB_IGNITION,
	    PROB_GROWTH
	);
	world_init(w);
	memset(cmd, '\0', CMD_BUF_SIZE);

	initscr();
	noecho();
	start_color();
	init_pair(EMPTY + 1, COLOR_BLACK, COLOR_BLACK);
	init_pair(TREE  + 1, COLOR_GREEN, COLOR_GREEN);
	init_pair(BURN  + 1, COLOR_RED  , COLOR_RED);

	control_timeout = -1;
	for (;;) {
		world_print(w, playing);
		control:
			timeout(control_timeout);
			switch (getch()) {
			case 'p':
				if (playing) {
					control_timeout = -1;
					playing = 0;
				} else {
					control_timeout = 0;
					playing = 1;
				}
				goto forward;
			case 'f':
				goto forward;
			case 'b':
				goto back;
			case 's':
				goto stop;
			case ':':
				goto command;
			default:
				if (playing)
					goto forward;
				else
					goto control;
			}
		command:
			timeout(-1);
			mvprintw(winsize.ws_row - 1, 0, ":");
			refresh();
			echo();
			mvgetstr(winsize.ws_row - 1, 1, cmd);
			if (sscanf(cmd, "f %f", &tmp_f) == 1) {
				w->f = tmp_f;
				world_destroy_future(w);
			} else
			if (sscanf(cmd, "p %f", &tmp_p) == 1) {
				w->p = tmp_p;
				world_destroy_future(w);
			}
			memset(cmd, '\0', CMD_BUF_SIZE);
			noecho();
			timeout(control_timeout);

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
