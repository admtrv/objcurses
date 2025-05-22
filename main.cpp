/*
 * main.cpp
 */

#include <ncurses.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include "entities/geometry/object.h"
#include "entities/rendering/buffer.h"
#include "entities/rendering/renderer.h"
#include "config.h"
#include "version.h"

#ifdef ASAN_OPTIONS
extern "C" const char *__asan_default_options() {
    return ASAN_OPTIONS;
}
#endif


// ncurses

void init_ncurses()
{
    initscr();              // start ncurses mode
    noecho();               // disable echoing of typed characters
    curs_set(0);            // hide the cursor
    keypad(stdscr, true);   // enable special keys (arrows, etc.)
    timeout(1);             // make getch() non-blocking
}

void init_colors(const std::vector<Material> &materials)
{
    if (!has_colors() || !can_change_color())
        return;

    start_color();

    for (size_t i = 0; i < materials.size(); ++i)
    {
        const int pair = static_cast<int>(i) + 1;

        if (pair >= COLORS || pair >= COLOR_PAIRS)
            break;

        const auto &d = materials[i].diffuse; // 0–1
        init_color(pair,
                   static_cast<short>(std::clamp(d.x, 0.0f, 1.0f) * 1000.0f),
                   static_cast<short>(std::clamp(d.y, 0.0f, 1.0f) * 1000.0f),
                   static_cast<short>(std::clamp(d.z, 0.0f, 1.0f) * 1000.0f));
        init_pair(pair, pair, 0);
    }
}

// cli

static void print_help()
{
    std::cout <<
        "Usage: " << APP_NAME << " [OPTIONS] <file.obj>\n"
        "\n"
        "Options:\n"
        "  -c, --color          Enable colors from .mtl file\n"
        "  -l, --light          Disable light rotation\n"
        "  -f, --flip           Flip faces winding order\n"
        "  -x, --invert-x       Flip geometry along X axis\n"
        "  -y, --invert-y       Flip geometry along Y axis\n"
        "  -z, --invert-z       Flip geometry along Z axis\n"
        "  -h, --help           Print help\n"
        "  -v, --version        Print version\n"
        "\n"
        "Controls:\n"
        "  ←, h, a              Rotate left\n"
        "  →, l, d              Rotate right\n"
        "  ↑, k, w              Rotate up\n"
        "  ↓, j, s              Rotate down\n"
        "  +, i                 Zoom in\n"
        "  -, o                 Zoom out\n"
        "  Tab                  Toggle HUD\n"
        "  q                    Quit\n";
}

static void print_version()
{
    std::cout << APP_NAME << " " << APP_VERSION << '\n';
}

struct Args {
    std::filesystem::path input_file;
    bool color_support = false;     // -c / --color
    bool static_light = false;      // -l / --light
    bool flip_faces = false;        // -f / --flip
    bool invert_x = false;          // -x / --invert-x
    bool invert_y = false;          // -y / --invert-y
    bool invert_z = false;          // -z / --invert-z
};

static Args parse_args(int argc, char **argv)
{
    Args a;
    for (int i = 1; i < argc; ++i)
    {
        const std::string_view arg{argv[i]};

        // help
        if (arg == "-h" || arg == "--help")
        {
            print_help();
            std::exit(0);
        }

        // version
        if (arg == "-v" || arg == "--version")
        {
            print_version();
            std::exit(0);
        }

        // flags
        if (arg == "-c" || arg == "--color")
        {
            a.color_support = true;
        }
        else if (arg == "-l" || arg == "--light")
        {
            a.static_light = true;
        }
        else if (arg == "-f" || arg == "--flip")
        {
            a.flip_faces = true;
        }
        else if (arg == "-x" || arg == "--invert-x")
        {
            a.invert_x = true;
        }
        else if (arg == "-y" || arg == "--invert-y")
        {
            a.invert_y = true;
        }
        else if (arg == "-z" || arg == "--invert-z")
        {
            a.invert_z = true;
        }
        else if (arg[0] != '-')
        {
            if (!a.input_file.empty())
            {
                std::cerr << "error: more than one input file\n";
                std::exit(1);
            }
            a.input_file = arg;
        }

        // unknown
        else
        {
            std::cerr << "unknown option: " << arg << '\n';
            std::cerr << "type '--help' for usage\n";
            std::exit(1);
        }
    }

    if (a.input_file.empty())
    {
        std::cerr << "error: no input file\n";
        std::cerr << "type '--help' for usage\n";
        std::exit(1);
    }

    return a;
}

// helpers

void render_hud(const Camera &cam)
{
    mvprintw(0, 0, "zoom     %6.1f x",  cam.zoom);
    mvprintw(1, 0, "azimuth  %6.1f deg", clamp0(rad2deg(cam.azimuth)));
    mvprintw(2, 0, "altitude %6.1f deg", clamp0(rad2deg(cam.altitude)));
}

bool handle_input(int ch, Camera &cam, bool &hud)
{
    switch (ch)
    {
        case 'q': case 'Q':     // exit
            return false;

        case '\t':              // hud
            hud = !hud;
            break;

        // keys / vim / wasd
        case KEY_LEFT: case 'h': case 'H': case 'a' : case 'A':     // left rotation
            cam.rotate_left();
            break;
        case KEY_RIGHT: case 'l': case 'L': case 'd': case 'D':     // right rotation
            cam.rotate_right();
            break;
        case KEY_UP: case 'k': case 'K': case 'w': case 'W':                // up rotation
            cam.rotate_up();
            break;
        case KEY_DOWN: case 'j': case 'J': case 's': case 'S':              // down rotation
            cam.rotate_down();
            break;

        // +- / io
        case '+': case '=': case 'i': case 'I':                 // zoom in
            cam.zoom_in();
            break;
        case '-': case 'o': case 'O':                           // zoom out
            cam.zoom_out();
            break;
    }

    return true;
}

// main
int main(int argc, char **argv)
{
    const Args args = parse_args(argc, argv);

    // load object
    Object obj;
    if (!obj.load(args.input_file.string(), args.color_support))
    {
        return 1;
    }

    // normalize to unit cube
    obj.normalize();

    // resize to make model >= 0.5 screen size
    obj.scale(3.0f);

    // flip faces winding order
    if (args.flip_faces)
        obj.flip_faces();

    // invert along axes
    if (args.invert_x)
        obj.invert_x();

    if (args.invert_y)
        obj.invert_y();

    if (args.invert_z)
        obj.invert_z();

    // init curses
    init_ncurses();

    // init colors
    if (args.color_support)
        init_colors(obj.materials);

    // buffer
    int rows;
    int cols;

    getmaxyx(stdscr, rows, cols);

    const float logical_y = 2.0f;
    const float logical_x = logical_y * static_cast<float>(cols) / (static_cast<float>(rows) * CHAR_ASPECT_RATIO);

    Buffer buf(static_cast<unsigned int>(cols), static_cast<unsigned int>(rows), logical_x, logical_y);

    // view
    Camera cam;         // default
    Light light;        // default
    bool hud = false;

    // main render loop
    while (true)
    {
        // clear buffer
        buf.clear();

        // render model
        Renderer::render(buf, obj, cam, light, args.static_light, args.color_support);

        move(0, 0);
        buf.printw();

        // render hud
        if (hud)
        {
            render_hud(cam);
        }

        // draw buffer
        refresh();

        // handle key
        int ch = getch();

        if (ch == KEY_RESIZE)
        {
            getmaxyx(stdscr, rows, cols);
            const float lx = logical_y * static_cast<float>(cols) / (static_cast<float>(rows) * CHAR_ASPECT_RATIO);
            buf = Buffer(static_cast<unsigned int>(cols), static_cast<unsigned int>(rows), lx, logical_y);
        }

        if (!handle_input(ch, cam, hud))
        {
            break;
        }
    }

    endwin();
    return 0;
}
