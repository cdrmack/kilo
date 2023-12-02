#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

/*** DEFINES ***/
#define CTRL_KEY(k) ((k) & 0x1f) // 0001 1111

/*** DATA ***/
struct termios original_termios;

/*** TERMINAL ***/
void die(const char* s)
{
    perror(s); // most clib function that fail will set the global errno variable, perror() prints it alongside provided text
    exit(EXIT_FAILURE);
}

// there are two general kinds of input processing:
// canonical (cooked) and noncanonical (raw)
void disable_raw_mode(void)
{
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_termios) == -1)
    {
        die("tcsetattr");
    }
}

// disable echoing and some signals
// canonicalize input lines (input bytes are not assembled into lines)
// termios(4)
void enable_raw_mode(void)
{
    if (tcgetattr(STDIN_FILENO, &original_termios) == -1)
    {
        die("tcgetattr");
    }

    atexit(disable_raw_mode);

    struct termios raw_termios = original_termios;
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

char editor_read_key()
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

    return c;
}

/*** OUTPUT ***/
void editor_refresh_screen()
{
    // first byte - `\x1b` - is an escape character (27)
    // escape sequence starts with an escape character followed by a '[' character
    // --
    // https://vt100.net/docs/vt100-ug/chapter3.html#ED
    // `ESC [ Ps J` - erase some or all of the characters in the display
    // Ps = 0, erase from the active position to the end of the screen (inclusive)
    // Ps = 1, erase from the start of the screen to the active position (inclusive)
    // PS = 2, erase all of the display (all lines are erased, changed to single-width, and the cursor does not move)
    // --
    // we could use ncurses lib, which uses terminfo db to figure out the capabilities of a terminal and what escape sequences to use
    // if we wanted to support more terminals, not only VT100
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3); // move cursor to the top-left corner (home position)
}

/*** INPUT ***/
void editor_process_keypress()
{
    const char key = editor_read_key();

    switch (key)
    {
        case CTRL_KEY('q'):
        {
            exit(EXIT_SUCCESS);
            break;
        }
        default:
            break;
    }
}

/*** INIT ***/
int main(void)
{
    enable_raw_mode();

    while (1)
    {
        editor_refresh_screen();
        editor_process_keypress();
    }

    return EXIT_SUCCESS;
}
