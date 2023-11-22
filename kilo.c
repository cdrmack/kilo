#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

struct termios original_termios;

// there are two general kinds of input processing:
// canonical (cooked) and noncanonical (raw)
void disable_raw_mode()
{
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &original_termios);
}

void enable_raw_mode()
{
    tcgetattr(STDIN_FILENO, &original_termios);
    atexit(disable_raw_mode);

    struct termios raw_termios = original_termios;
    raw_termios.c_lflag &= ~(ECHO); // bitwise-NOT and bitwise-AND
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw_termios);
}

int main()
{
    enable_raw_mode();

    char c;
    while (read(STDIN_FILENO, &c, 1) == 1 && c != 'q')
    {
        // TODO
    }

    return 0;
}
