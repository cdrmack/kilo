#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

// DATA
struct termios original_termios;

// TERMINAL
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

int main(void)
{
    enable_raw_mode();

    char c = '\0';
    while (1)
    {
        c = '\0';
        if (read(STDIN_FILENO, &c, 1) == -1)
        {
            die("read");
        }

        if (iscntrl(c))
        {
            printf("%d\r\n", c);
        }
        else
        {
            printf("%d ('%c')\r\n", c, c);
        }

        if (c == 'q')
        {
            break;
        }
    }

    return EXIT_SUCCESS;
}
