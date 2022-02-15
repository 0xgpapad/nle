#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <termios.h>
#include <unistd.h>

#include "nleinstance.h"

extern "C" {
#include "hack.h"
}

class ScopedTC
{
  public:
    ScopedTC() : old_{}
    {
        tcgetattr(STDIN_FILENO, &old_);
        struct termios tty = old_;
        tty.c_lflag &= ~ICANON;
        tty.c_lflag &= ~ECHO;
        tcsetattr(STDIN_FILENO, TCSANOW, &tty);
    }

    ~ScopedTC()
    {
        tcsetattr(STDIN_FILENO, TCSANOW, &old_);
    }

  private:
    struct termios old_;
};

void
play(Instance &nle, nle_obs *obs, nle_settings *settings)
{
    while (!obs->done) {
        for (int r = 0; r < ROWNO; ++r) {
            for (int c = 0; c < COLNO - 1; ++c)
                std::cout << obs->chars[r * (COLNO - 1) + c];
            std::cout << std::endl;
        }
        for (int i = 0; i < 23; ++i) {
            std::cout << obs->blstats[i] << " ";
        }
        std::cout << std::endl;
        read(STDIN_FILENO, &obs->action, 1);
        if (obs->action == 'r')
            nle.reset(obs, nullptr, nullptr, settings);
        nle.step(obs);
    }
}

void
randplay(Instance &nle, nle_obs *obs)
{
    int actions[] = {
        13, 107, 108, 106, 104, 117, 110, 98, 121,
        75, 76,  74,  72,  85,  78,  66,  89,
    };
    size_t n = sizeof(actions) / sizeof(actions[0]);

    for (int i = 0; !obs->done && i < 10000; ++i) {
        obs->action = actions[rand() % n];
        nle.step(obs);
    }
    if (!obs->done) {
        std::cerr << "Episode didn't end after 10000 steps, aborting."
                  << std::endl;
    }
}

void
randgame(Instance &nle, nle_obs *obs, const int no_episodes,
         nle_settings *settings)
{
    for (int i = 0; i < no_episodes; ++i) {
        randplay(nle, obs);
        if (i < no_episodes - 1)
            nle.reset(obs, nullptr, nullptr, settings);
    }
}

int
main(int argc, char **argv)
{
    nle_obs obs{};
    constexpr int dungeon_size = ROWNO * (COLNO - 1);
    short glyphs[dungeon_size];
    obs.glyphs = &glyphs[0];

    unsigned char chars[dungeon_size];
    obs.chars = &chars[0];

    unsigned char colors[dungeon_size];
    obs.colors = &colors[0];

    unsigned char specials[dungeon_size];
    obs.specials = &specials[0];

    unsigned char message[256];
    obs.message = &message[0];

    long blstats[NLE_BLSTATS_SIZE];
    obs.blstats = &blstats[0];

    int program_state[NLE_PROGRAM_STATE_SIZE];
    obs.program_state = &program_state[0];

    int internal[NLE_INTERNAL_SIZE];
    obs.internal = &internal[0];

    std::unique_ptr<FILE, int (*)(FILE *)> ttyrec(
        fopen("nle.ttyrec.bz2", "a"), fclose);

    nle_settings settings{};
    char *hackdir = getenv("HACKDIR");

    if (hackdir)
        strncpy(settings.hackdir, hackdir, sizeof(settings.hackdir));
    else
        throw std::runtime_error(
            "Set HACKDIR env variable to NetHack installation");

    ScopedTC tc;

    char *cwd = getcwd(nullptr, 0); /* glibc's allocating version. */
    std::string path(cwd);
    free(cwd);

    if (path.back() != '/')
        path.append("/");

    path.append("libnethack.so");

    Instance nle(path, &obs, ttyrec.get(), nullptr, &settings);
    if (argc > 1 && argv[1][0] == 'r') {
        randgame(nle, &obs, 3, &settings);
    } else {
        for (int i = 0; i < 10; ++i) {
            play(nle, &obs, &settings);
            nle.reset(&obs, nullptr, nullptr, &settings);
        }
    }
    nle.close();
}
