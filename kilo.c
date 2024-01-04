#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

/*** DEFINES ***/
#define KILO_VERSION "0.0.1"

#define CTRL_KEY(k) ((k) & 0x1f) // 0001 1111

enum editor_key
{
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    PAGE_UP,
    PAGE_DOWN
};

/*** DATA ***/
struct editor_config
{
    int c_x; // cursor's x position
    int c_y; // cursor's y position
    int screenrows;
    int screencols;
    struct termios original_termios;
};

struct editor_config EDITOR_CONF;

/*** TERMINAL ***/
void
die(const char *s)
{
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);

    perror(s); // most clib function that fail will set the global errno variable, perror() prints it alongside provided text
    exit(EXIT_FAILURE);
}

// there are two general kinds of input processing:
// canonical (cooked) and noncanonical (raw)
void
disable_raw_mode(void)
{
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &EDITOR_CONF.original_termios) == -1)
    {
        die("tcsetattr");
    }
}

// disable echoing and some other signals
// canonicalize input lines (input bytes are not assembled into lines)
// termios(4)
void
enable_raw_mode(void)
{
    if (tcgetattr(STDIN_FILENO, &EDITOR_CONF.original_termios) == -1)
    {
        die("tcgetattr");
    }

    atexit(disable_raw_mode);

    struct termios raw_termios = EDITOR_CONF.original_termios;
    raw_termios.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw_termios.c_oflag &= ~(OPOST);
    raw_termios.c_cflag |= ~(CS8);
    raw_termios.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG); // bitwise-NOT and bitwise-AND
    raw_termios.c_cc[VMIN] = 0; // minimum number of bytes of input needed before `read()` can return
    raw_termios.c_cc[VTIME] = 1; // maximum amount of time (in 1/10 of second) to wait before `read()` returns, 100ms

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw_termios) == -1)
    {
        die("tcsetattr");
    }
}

int
editor_read_key(void)
{
    ssize_t nread = 0;
    char c = '\0';

    while((nread = read(STDIN_FILENO, &c, 1)) != 1)
    {
        if (nread == -1 && errno != EAGAIN)
        {
            die("read");
        }
    }

    if (c != '\x1b')
    {
        return c;
    }

    char seq[3];

    if (read(STDIN_FILENO, &seq[0], 1) != 1)
    {
        return '\x1b';
    }

    if (read(STDIN_FILENO, &seq[1], 1) != 1)
    {
        return '\x1b';
    }

    if (seq[0] == '[')
    {
        if (seq[1] >= '0' && seq[1] <= '9')
        {
            if (read(STDIN_FILENO, &seq[2], 1) != 1)
            {
                return '\x1b';
            }

            if (seq[2] == '~')
            {
                switch (seq[1])
                {
                    case '5':
                        return PAGE_UP;
                    case '6':
                        return PAGE_DOWN;
                }
            }
        }
        else
        {
            switch (seq[1])
            {
                case 'A':
                    return ARROW_UP;
                case 'B':
                    return ARROW_DOWN;
                case 'C':
                    return ARROW_RIGHT;
                case 'D':
                    return ARROW_LEFT;
                default:
                    return '\x1b';
            }
        }
    }
    return '\x1b';
}

int
get_window_size(int *rows, int *cols)
{
    struct winsize ws = { 0 };

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0)
    {
        return -1;
    }
    else
    {
        *rows = ws.ws_row;
        *cols = ws.ws_col;
        return 0;
    }
}

/*** APPEND BUFFER ***/
struct append_buf
{
    char *b;
    int len;
};

void
ab_append(struct append_buf *ab, const char *s, int len)
{
    char *new_buf = realloc(ab->b, ab->len + len);

    if (new_buf == NULL)
    {
        return;
    }

    memcpy(&new_buf[ab->len], s, len);
    ab->b = new_buf;
    ab->len += len;
}

void
ab_free(struct append_buf *ab)
{
    free(ab->b);
}

/*** OUTPUT ***/
void
editor_draw_welcome_message(struct append_buf *ab)
{
    char welcome[80];
    int welcome_len = snprintf(welcome, sizeof(welcome), "Kilo editor -- version %s", KILO_VERSION);
    if (welcome_len > EDITOR_CONF.screencols)
    {
        welcome_len = EDITOR_CONF.screencols;
    }

    int padding = (EDITOR_CONF.screencols - welcome_len) / 2;
    if (padding)
    {
        ab_append(ab, "~", 1);
        padding--;
    }

    while (padding--)
    {
        ab_append(ab, " ", 1);
    }

    ab_append(ab, welcome, welcome_len);
}

void
editor_draw_rows(struct append_buf *ab)
{
    for (int y = 0; y < EDITOR_CONF.screenrows; ++y)
    {
        if (y == EDITOR_CONF.screenrows / 3)
        {
            editor_draw_welcome_message(ab);
        }
        else
        {
            ab_append(ab, "~", 1);
        }

        // erase from the active position to the end of the line
        ab_append(ab, "\x1b[K", 3);

        if (y < EDITOR_CONF.screenrows - 1)
        {
            ab_append(ab, "\r\n", 2);
        }
    }
}

// first byte - `\x1b` - is an escape character (27)
// escape sequence starts with an escape character followed by a '[' character
// https://vt100.net/docs/vt100-ug/chapter3.html#ED
// ---
// we could use ncurses lib, which uses terminfo db to figure out the capabilities of a terminal and what escape sequences to use
// if we wanted to support more terminals, not only VT100
void
editor_refresh_screen(void)
{
    struct append_buf ab = { nullptr, 0 };

    // hide cursor while refreshing
    ab_append(&ab, "\x1b[?25l", 6);

    // move cursor to the top-left corner (home position)
    ab_append(&ab, "\x1b[H", 3);

    editor_draw_rows(&ab);

    // Cursor Position
    // ESC [ Pn ; Pn H
    char buf[32];
    snprintf(buf, sizeof(buf), "\x1b[%d;%dH", EDITOR_CONF.c_y + 1, EDITOR_CONF.c_x + 1);
    ab_append(&ab, buf, strlen(buf));

    // show cursor
    ab_append(&ab, "\x1b[?25h", 6);

    write(STDOUT_FILENO, ab.b, ab.len);
    ab_free(&ab);
}

/*** INPUT ***/
void
editor_move_cursor(int key)
{
    switch (key)
    {
        case ARROW_LEFT:
            if (EDITOR_CONF.c_x != 0)
            {
                EDITOR_CONF.c_x--;
            }
            break;
        case ARROW_RIGHT:
            if (EDITOR_CONF.c_x != EDITOR_CONF.screencols - 1)
            {
                EDITOR_CONF.c_x++;
            }
            break;
        case ARROW_UP:
            if (EDITOR_CONF.c_y != 0)
            {
                EDITOR_CONF.c_y--;
            }
            break;
        case ARROW_DOWN:
            if (EDITOR_CONF.c_y != EDITOR_CONF.screenrows - 1)
            {
                EDITOR_CONF.c_y++;
            }
            break;
        default:
            break;
    }
}

void
editor_process_keypress(void)
{
    const int key = editor_read_key();

    switch (key)
    {
        case CTRL_KEY('q'):
        {
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(EXIT_SUCCESS);
            break;
        }
        case ARROW_UP:
        case ARROW_DOWN:
        case ARROW_LEFT:
        case ARROW_RIGHT:
            editor_move_cursor(key);
            break;
        case PAGE_UP:
        case PAGE_DOWN:
        {
            int times = EDITOR_CONF.screenrows;
            while (times--)
            {
                editor_move_cursor(key == PAGE_UP ? ARROW_UP : ARROW_DOWN);
            }
            break;
        }
        default:
            break;
    }
}

/*** INIT ***/
void
init_editor(void)
{
    EDITOR_CONF.c_x = 0;
    EDITOR_CONF.c_y = 0;

    if (get_window_size(&EDITOR_CONF.screenrows, &EDITOR_CONF.screencols) == -1)
    {
        die("get_window_size");
    }
}

int
main()
{
    enable_raw_mode();
    init_editor();

    while (1)
    {
        editor_refresh_screen();
        editor_process_keypress();
    }

    return EXIT_SUCCESS;
}
