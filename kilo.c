#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

// DEFINES
#define CTRL_KEY(k) ((k) & 0x1f) // 0001 1111

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

int main(void)
{
    enable_raw_mode();

    while (1)
    {
        editor_process_keypress();
    }

    return EXIT_SUCCESS;
}
