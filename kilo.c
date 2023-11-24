#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/termios.h>
#include <termios.h>
#include <unistd.h>

struct termios original_termios;

// there are two general kinds of input processing:
// canonical (cooked) and noncanonical (raw)
void disable_raw_mode(void)
{
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_termios);
}

// disable echoing and some signals
// canonicalize input lines (input bytes are not assembled into lines)
// termios(4)
void enable_raw_mode(void)
{
    tcgetattr(STDIN_FILENO, &original_termios);
    atexit(disable_raw_mode);

    struct termios raw_termios = original_termios;
    raw_termios.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG); // bitwise-NOT and bitwise-AND
    raw_termios.c_iflag &= ~(ICRNL | IXON);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw_termios);
}

int main(void)
{
    enable_raw_mode();

    char c;
    while (read(STDIN_FILENO, &c, 1) == 1 && c != 'q')
    {
        if (iscntrl(c))
        {
            printf("%d\n", c);
        }
        else
        {
            printf("%d ('%c')\n", c, c);
        }
    }

    return EXIT_SUCCESS;
}
