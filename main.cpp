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
#include <chrono>
#include <thread>

#include "entities/geometry/object.h"
#include "entities/rendering/buffer.h"
#include "entities/rendering/renderer.h"
#include "utils/tools.h"
#include "config.h"
#include "version.h"

#ifdef ASAN_OPTIONS
extern "C" const char *__asan_default_options() {
    return ASAN_OPTIONS;
}
#endif

using SteadyClock = std::chrono::steady_clock;
const auto t0 = SteadyClock::now();

// ncurses

enum class Theme {
    Dark = 1,
    Light = 2,
    Transparent = 3
};

static int g_hud_pair = 0; // hud color pair

void init_ncurses()
{
    initscr();              // start ncurses mode
    noecho();               // disable echoing of typed characters
    curs_set(0);            // hide the cursor
    keypad(stdscr, true);   // enable special keys (arrows, etc.)
    timeout(1);             // make getch() non-blocking
}

void init_colors(const std::vector<Material> &materials, Theme theme)
{
    if (!has_colors() || !can_change_color())
        return;

    start_color();

    const short BG_DEFAULT = -1;

    short bg;
    short hud;

    switch (theme)
    {
        case Theme::Dark:
            bg = COLOR_BLACK;
            hud = COLOR_WHITE;
            break;
        case Theme::Light:
            bg = COLOR_WHITE;
            hud = COLOR_BLACK;
            break;
        case Theme::Transparent:
            bg = BG_DEFAULT;
            hud = COLOR_WHITE;
            break;
    }

    if (bg == BG_DEFAULT)
        use_default_colors();

    size_t limit = std::min(materials.size(), static_cast<size_t>(COLOR_PAIRS - 2));

    for (size_t i = 0; i < limit; i++)
    {
        int pair = static_cast<int>(i) + 1;

        const auto &d = materials[i].diffuse; // 0–1
        if (can_change_color())
            init_color(pair,
                       static_cast<short>(std::clamp(d.x, 0.0f, 1.0f) * 1000.0f),
                       static_cast<short>(std::clamp(d.y, 0.0f, 1.0f) * 1000.0f),
                       static_cast<short>(std::clamp(d.z, 0.0f, 1.0f) * 1000.0f));

        init_pair(pair, pair, bg);
    }

    g_hud_pair = static_cast<int>(limit) + 1;

    if (g_hud_pair < COLOR_PAIRS)
        init_pair(g_hud_pair, hud, bg);

    bkgd(' ' | COLOR_PAIR(g_hud_pair));
}

// cli

static void print_help()
{
    std::cout <<
        "Usage: " << APP_NAME << " [OPTIONS] <file.obj>\n"
        "\n"
        "Options:\n"
        "  -c, --color <theme>  Enable colors support, optional theme {dark|light|transparent}\n"
        "  -l, --light          Disable light rotation\n"
        "  -a, --animate <deg>  Start with animated object, optional speed [default: " << std::fixed << std::setprecision(1) << ANIMATION_STEP << std::defaultfloat << " deg/s]\n"
        "  -z, --zoom <x>       Provide initial zoom [default: " << std::fixed << std::setprecision(1) << ZOOM_START << std::defaultfloat << " x]\n"
        "      --flip           Flip faces winding order\n"
        "      --invert-x       Flip geometry along X axis\n"
        "      --invert-y       Flip geometry along Y axis\n"
        "      --invert-z       Flip geometry along Z axis\n"
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

    bool color_support = false;         // -c / --color
    Theme theme = Theme::Dark;

    bool static_light = false;          // -l / --light
    bool flip_faces = false;            // -f / --flip
    bool invert_x = false;              // -x / --invert-x
    bool invert_y = false;              // -y / --invert-y
    bool invert_z = false;              // -z / --invert-z

    bool animate = false;               // -a / --animate
    float speed = ANIMATION_STEP;   // deg/s

    float zoom = ZOOM_START;            // -z / --zoom
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

            if (i + 1 < argc && argv[i + 1][0] != '-')
            {
                std::string theme = argv[++i];

                if (theme == "dark")
                    a.theme = Theme::Dark;
                else if (theme == "light")
                    a.theme = Theme::Light;
                else if (theme == "transparent")
                    a.theme = Theme::Transparent;
                else
                {
                    std::cerr << "error: invalid theme\n";
                    std::exit(1);
                }
            }
        }
        else if (arg == "-l" || arg == "--light")
        {
            a.static_light = true;
        }
        else if (arg == "-a" || arg == "--animate")
        {
            a.animate = true;

            if (i + 1 < argc && argv[i + 1][0] != '-')
            {
                auto val = safe_stof(argv[++i]);

                if (!val)
                {
                    std::cerr<< "error: invalid speed value\n";
                    std::exit(1);
                }

                a.speed = val.value();
            }
        }
        else if (arg == "-z" || arg == "--zoom")
        {
            if (++i == argc)
            {
                std::cerr << "error: zoom needs value\n";
                std::exit(1);
            }

            auto val = safe_stof(argv[i]);

            if (!val)
            {
                std::cerr << "error: invalid zoom value\n";
                std::exit(1);
            }

            a.zoom = val.value();
        }
        else if (arg == "--flip")
        {
            a.flip_faces = true;
        }
        else if (arg == "--invert-x")
        {
            a.invert_x = true;
        }
        else if (arg == "--invert-y")
        {
            a.invert_y = true;
        }
        else if (arg == "--invert-z")
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

void render_hud(const Camera &cam, const float fps)
{
    if (g_hud_pair)
        attron(COLOR_PAIR(g_hud_pair));

    mvprintw(0, 0, "framerate %6d fps", static_cast<int>(std::round(fps)));
    mvprintw(1, 0, "zoom      %6.1f x", cam.zoom);
    mvprintw(2, 0, "azimuth   %6.1f deg", clamp0(rad2deg(cam.azimuth)));
    mvprintw(3, 0, "altitude  %6.1f deg", clamp0(rad2deg(cam.altitude)));

    if (g_hud_pair)
        attroff(COLOR_PAIR(g_hud_pair));
}

void handle_control(const int ch, Camera &cam)
{
    switch (ch)
    {
        // keys / vim / wasd
        case KEY_LEFT: case 'h': case 'H': case 'a' : case 'A':     // left rotation
            cam.rotate_left();
            break;
        case KEY_RIGHT: case 'l': case 'L': case 'd': case 'D':     // right rotation
            cam.rotate_right();
            break;
        case KEY_UP: case 'k': case 'K': case 'w': case 'W':        // up rotation
            cam.rotate_up();
            break;
        case KEY_DOWN: case 'j': case 'J': case 's': case 'S':      // down rotation
            cam.rotate_down();
            break;

        // +- / io
        case '+': case '=': case 'i': case 'I':     // zoom in
            cam.zoom_in();
            break;
        case '-': case 'o': case 'O':               // zoom out
            cam.zoom_out();
            break;
        default:
            break;
    }
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
        init_colors(obj.materials, args.theme);

    // buffer
    int rows;
    int cols;

    getmaxyx(stdscr, rows, cols);

    const float logical_y = 2.0f;
    const float logical_x = logical_y * static_cast<float>(cols) / (static_cast<float>(rows) * CHAR_ASPECT_RATIO);

    Buffer buf(static_cast<unsigned int>(cols), static_cast<unsigned int>(rows), logical_x, logical_y);

    // view
    Camera cam(args.zoom);  // constructor with zoom
    Light light;            // default
    bool hud = false;

    // animation
    bool rotate = args.animate;
    auto last = SteadyClock::now();

    // optimizing drawing
    bool needs_redraw = true;

    // main render loop
    while (true)
    {
        auto now = SteadyClock::now();
        float dt = std::chrono::duration<float>(now - last).count(); // seconds since previous frame
        last = now;
        float fps = dt > 0.f ? 1.f / dt : 0.f;

        if (rotate) {
            cam.rotate_left(args.speed * dt);
            needs_redraw = true;
        }

        // handle key
        int ch = getch();

        if (ch == KEY_RESIZE)
        {
            getmaxyx(stdscr, rows, cols);
            const float lx = logical_y * static_cast<float>(cols) / (static_cast<float>(rows) * CHAR_ASPECT_RATIO);
            buf = Buffer(static_cast<unsigned int>(cols), static_cast<unsigned int>(rows), lx, logical_y);
            needs_redraw = true;
        }
        else if (ch == 'q' || ch == 'Q')     // exit
        {
            break;
        }
        else if (ch == '\t')                 // toggle hud
        {
            hud = !hud;
            needs_redraw = true;
        }
        else if (ch != ERR)
        {
            rotate = false;                // stop animation on first movement
            handle_control(ch, cam);    // handle camera control
            needs_redraw = true;
        }

        // redrawing
        if (needs_redraw)
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
                render_hud(cam, fps);
            }

            // draw buffer
            refresh();

            needs_redraw = false;
        }
        else if (hud) // update only hud
        {
            render_hud(cam, fps);
            refresh();
        }

        // limiting fps
        auto frame_deadline = now + std::chrono::duration<float>(FRAME_DURATION);
        std::this_thread::sleep_until(frame_deadline);
    }

    endwin();
    return 0;
}
