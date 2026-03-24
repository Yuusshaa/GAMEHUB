// ============================================================
//  TRON - GRIDLOCK Synthwave Light Cycles
//  SFML 2.x | C++17
//  P1=WASD, P2=Arrow Keys, ESC=Pause
// ============================================================

#define _CRT_SECURE_NO_WARNINGS
#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <string>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fstream>

// ─────────────────────────────────────────────────────────────
//  SHARED DECLARATIONS  (inlined from games.h)
// ─────────────────────────────────────────────────────────────

static constexpr int WIN_W = 1200;
static constexpr int WIN_H = 730;

struct GameResult { int p1 = 0, p2 = 0; };

// ─────────────────────────────────────────────────────────────
//  CONSTANTS
// ─────────────────────────────────────────────────────────────

static const int   T_CELL = 8;              // Grid cell size (px)
static const int   T_GW = 1280 / T_CELL;  // Grid width in cells (160)
static const int   T_GH = 720 / T_CELL;  // Grid height in cells (90)
static const int   T_MT = T_GW * T_GH;    // Total grid cells (14,400)
static const float T_TICK = 0.055f;         // Movement tick interval (s)
static const int   T_WINS = 5;              // Rounds needed to win
static const float T_CD = 3.f;            // Countdown duration (s)
static const float T_RD = 2.f;            // Result screen duration (s)
static const int   T_MAXP = 200;            // Max explosion particles
static const int   T_MAXS = 60;             // Max spark particles

// ─────────────────────────────────────────────────────────────
//  COLORS
// ─────────────────────────────────────────────────────────────

static const sf::Color TC_BG(12, 4, 22);        // Dark purple bg
static const sf::Color TC_GL(35, 15, 55);        // Grid line color
static const sf::Color TC_P1(255, 50, 130);       // P1 hot pink
static const sf::Color TC_P1G(180, 20, 80, 80);   // P1 trail glow
static const sf::Color TC_P1H(255, 150, 200);       // P1 head highlight
static const sf::Color TC_P2(50, 255, 120);       // P2 neon green
static const sf::Color TC_P2G(20, 150, 60, 80);   // P2 trail glow
static const sf::Color TC_P2H(160, 255, 200);       // P2 head highlight
static const sf::Color TC_WH(255, 255, 255);       // White
static const sf::Color TC_BK(0, 0, 0);         // Black
static const sf::Color TC_RED(220, 50, 40);        // Red (tie/death)

// ─────────────────────────────────────────────────────────────
//  HELPERS
// ─────────────────────────────────────────────────────────────

// Clamp value between lo and hi
static float tc(float v, float lo, float hi)
{
    return v < lo ? lo : v > hi ? hi : v;
}

// Center text origin
static void tCT(sf::Text& t)
{
    auto r = t.getLocalBounds();
    t.setOrigin(r.left + r.width / 2.f, r.top + r.height / 2.f);
}

// Int to string
static std::string tIS(int n)
{
    if (!n) return "0";
    std::string s;
    bool ng = n < 0;
    if (ng) n = -n;
    while (n) { s = char('0' + n % 10) + s; n /= 10; }
    return ng ? "-" + s : s;
}

// ─────────────────────────────────────────────────────────────
//  SOUND SYSTEM
// ─────────────────────────────────────────────────────────────

struct TSFX
{
    sf::SoundBuffer b;
    sf::Sound       p[4];
    int             i = 0;
    bool            ok = false;

    void load(const std::string& pa)
    {
        ok = b.loadFromFile(pa);
        if (ok)
            for (auto& s : p) s.setBuffer(b);
    }

    void play(float pi = 1.f, float vo = 100.f)
    {
        if (!ok) return;
        p[i].setPitch(pi);
        p[i].setVolume(vo);
        p[i].play();
        i = (i + 1) % 4;
    }
};

// ─────────────────────────────────────────────────────────────
//  PARTICLES
// ─────────────────────────────────────────────────────────────

// Explosion particle (on collision)
struct TPart
{
    sf::Vector2f pos, vel;
    float        life, mxL, sz;
    sf::Color    col;
    bool         active;

    void spawn(sf::Vector2f p, sf::Color c)
    {
        pos = p;
        float a = (rand() % 360) * 3.14159f / 180.f;
        float spd = (float)(rand() % 160 + 40);
        vel = { std::cos(a) * spd, std::sin(a) * spd };
        life = mxL = 0.3f + (rand() % 40) * .01f;
        col = c;
        sz = (float)(rand() % 4 + 2);
        active = true;
    }

    void update(float dt)
    {
        if (!active) return;
        pos += vel * dt;
        vel *= .96f;
        life -= dt;
        if (life <= 0) active = false;
    }
};

// Spark particle (head trail)
struct TSpark
{
    sf::Vector2f pos;
    float        life, mxL;
    sf::Color    col;
    bool         active;

    void spawn(sf::Vector2f p, sf::Color c)
    {
        pos = p;
        life = mxL = .15f + (rand() % 10) * .01f;
        col = c;
        active = true;
    }

    void update(float dt)
    {
        if (!active) return;
        life -= dt;
        if (life <= 0) active = false;
    }
};

// ─────────────────────────────────────────────────────────────
//  DIRECTION
// ─────────────────────────────────────────────────────────────

enum TDir { TU = 0, TD, TL, TR };

// Returns true if two directions are directly opposite
static bool topp(TDir a, TDir b)
{
    return (a == TU && b == TD) || (a == TD && b == TU) ||
        (a == TL && b == TR) || (a == TR && b == TL);
}

// ─────────────────────────────────────────────────────────────
//  PLAYER
// ─────────────────────────────────────────────────────────────

struct TPlayer
{
    int  x, y;              // Current grid position
    int  tx[T_MT];          // Trail X positions
    int  ty[T_MT];          // Trail Y positions
    int  tl;                // Trail length
    int  wins;              // Rounds won
    TDir dir, nxt;          // Current and queued direction
    bool alive;

    void reset(int sx, int sy, TDir sd)
    {
        x = sx;
        y = sy;
        dir = nxt = sd;
        tl = 0;
        alive = true;
    }
};

// ─────────────────────────────────────────────────────────────
//  GAME STATE
// ─────────────────────────────────────────────────────────────

enum class TSt { COUNTDOWN, PLAYING, PAUSED, ROUND_END, MATCH_END };

// ─────────────────────────────────────────────────────────────
//  MAIN
// ─────────────────────────────────────────────────────────────

GameResult runTron(sf::RenderWindow& win, const sf::Font& font)
{
    // Audio
    TSFX sfxTurn, sfxCrash, sfxWin;
    sfxTurn.load("turn.wav");
    sfxCrash.load("crash.wav");
    sfxWin.load("win.wav");

    // Background music (looping)
    sf::Music bgMusic;
    bgMusic.openFromFile("tron_bg.ogg");
    bgMusic.setLoop(true);
    bgMusic.play();

    // Background image
    sf::Texture bgTex;
    bgTex.loadFromFile("tron_bg.png");
    sf::Sprite bgSpr(bgTex);
    bgSpr.setScale(
        (float)WIN_W / bgTex.getSize().x,
        (float)WIN_H / bgTex.getSize().y
    );

    // Particle pools
    TPart  parts[T_MAXP];
    TSpark sparks[T_MAXS];
    for (auto& p : parts)  p.active = false;
    for (auto& s : sparks) s.active = false;

    // Occupancy grid: 0=empty, 1=P1 trail, 2=P2 trail
    int grid[T_GW][T_GH];

    auto clrGrid = [&]()
        {
            for (int x = 0; x < T_GW; x++)
                for (int y = 0; y < T_GH; y++)
                    grid[x][y] = 0;
        };

    auto burst = [&](sf::Vector2f p, sf::Color c, int n)
        {
            int sp = 0;
            for (auto& pt : parts)
                if (!pt.active && sp < n) { pt.spawn(p, c); sp++; }
        };

    auto spark = [&](sf::Vector2f p, sf::Color c)
        {
            for (auto& s : sparks)
                if (!s.active) { s.spawn(p, c); return; }
        };

    // Players
    TPlayer p1, p2;
    p1.wins = p2.wins = 0;

    auto resetRound = [&]()
        {
            p1.reset(T_GW / 4, T_GH / 2, TR);
            p2.reset(3 * T_GW / 4, T_GH / 2, TL);
            clrGrid();
            grid[p1.x][p1.y] = 1; p1.tx[0] = p1.x; p1.ty[0] = p1.y; p1.tl = 1;
            grid[p2.x][p2.y] = 2; p2.tx[0] = p2.x; p2.ty[0] = p2.y; p2.tl = 1;
            for (auto& p : parts)  p.active = false;
            for (auto& s : sparks) s.active = false;
        };
    resetRound();

    // ─────────────────────────────────────────────────────────
    //  RENDERING LAMBDAS
    // ─────────────────────────────────────────────────────────

    // Draw background image + dim overlay + pulsing grid lines + border
    auto drawBG = [&](float gp)
        {
            // Background image with dark overlay
            win.draw(bgSpr);
            sf::RectangleShape bgDim({ (float)WIN_W, (float)WIN_H });
            bgDim.setFillColor({ 0, 0, 0, 180 });
            win.draw(bgDim);

            // Pulsing grid lines
            float     pulse = 0.5f + 0.2f * std::sin(gp * 0.8f);
            sf::Uint8 ga = (sf::Uint8)(pulse * 60.f);
            sf::Color lc(TC_GL.r, TC_GL.g, TC_GL.b, ga);

            for (int gx = 0; gx <= T_GW; gx++)
            {
                sf::RectangleShape l({ 1.f, (float)WIN_H });
                l.setPosition((float)(gx * T_CELL), 0);
                l.setFillColor(lc);
                win.draw(l);
            }
            for (int gy = 0; gy <= T_GH; gy++)
            {
                sf::RectangleShape l({ (float)WIN_W, 1.f });
                l.setPosition(0, (float)(gy * T_CELL));
                l.setFillColor(lc);
                win.draw(l);
            }

            // Neon border edges
            sf::Color bc(120, 40, 160, 200);
            for (int e = 0; e < 2; e++)
            {
                sf::RectangleShape r({ (float)WIN_W, 2.f });
                r.setPosition(0, e ? (float)WIN_H - 2 : 0);
                r.setFillColor(bc);
                win.draw(r);
            }
            for (int e = 0; e < 2; e++)
            {
                sf::RectangleShape r({ 2.f, (float)WIN_H });
                r.setPosition(e ? (float)WIN_W - 2 : 0, 0);
                r.setFillColor(bc);
                win.draw(r);
            }
        };

    // Draw a player's trail and glowing head
    auto drawPl = [&](TPlayer& p, sf::Color tc2, sf::Color gc, sf::Color hc, float tt)
        {
            float cf = (float)T_CELL;

            for (int i = 0; i < p.tl; i++)
            {
                float px = (float)p.tx[i] * cf;
                float py = (float)p.ty[i] * cf;

                // Glow halo behind trail cell
                sf::RectangleShape gl({ cf + 4, cf + 4 });
                gl.setPosition(px - 2, py - 2);
                gl.setFillColor(gc);
                win.draw(gl);

                // Trail cell (brighter near head)
                float     age = 1.f - (float)i / (float)(p.tl > 1 ? p.tl : 1);
                sf::Uint8 al = (sf::Uint8)(150 + age * 105);
                sf::RectangleShape cell({ cf - 1, cf - 1 });
                cell.setPosition(px + .5f, py + .5f);
                cell.setFillColor({ tc2.r, tc2.g, tc2.b, al });
                win.draw(cell);
            }

            // Glowing head
            if (p.alive && p.tl > 0)
            {
                float hx = (float)p.x * cf + cf / 2;
                float hy = (float)p.y * cf + cf / 2;
                float pulse = .8f + .2f * std::sin(tt * 12.f);

                // Outer glow
                sf::RectangleShape hgl({ cf + 8, cf + 8 });
                hgl.setOrigin((cf + 8) / 2, (cf + 8) / 2);
                hgl.setPosition(hx, hy);
                sf::Color hgc = gc;
                hgc.a = (sf::Uint8)(pulse * 120);
                hgl.setFillColor(hgc);
                win.draw(hgl);

                // Head body
                sf::RectangleShape head({ cf, cf });
                head.setOrigin(cf / 2, cf / 2);
                head.setPosition(hx, hy);
                head.setFillColor(hc);
                win.draw(head);

                // Bright core
                sf::RectangleShape core({ cf * .5f, cf * .5f });
                core.setOrigin(cf * .25f, cf * .25f);
                core.setPosition(hx, hy);
                core.setFillColor({ 255, 255, 255, (sf::Uint8)(pulse * 200) });
                win.draw(core);
            }
        };

    // Draw explosion particles
    auto drawPts = [&]()
        {
            for (auto& p : parts)
            {
                if (!p.active) continue;
                float     t = tc(p.life / p.mxL, 0, 1);
                sf::Color pc = p.col;
                pc.a = (sf::Uint8)(t * 220);
                sf::RectangleShape s({ p.sz, p.sz });
                s.setOrigin(p.sz / 2, p.sz / 2);
                s.setPosition(p.pos);
                s.setFillColor(pc);
                win.draw(s);
            }
        };

    // Draw head sparks
    auto drawSps = [&]()
        {
            for (auto& s : sparks)
            {
                if (!s.active) continue;
                float     t = tc(s.life / s.mxL, 0, 1);
                sf::Color sc = s.col;
                sc.a = (sf::Uint8)(t * 180);
                float sz = 3.f * t;
                sf::RectangleShape sp({ sz, sz });
                sp.setOrigin(sz / 2, sz / 2);
                sp.setPosition(s.pos);
                sp.setFillColor(sc);
                win.draw(sp);
            }
        };

    // Draw score HUD
    auto drawHUD = [&]()
        {
            sf::Text s1("P1: " + tIS(p1.wins), font, 18);
            s1.setFillColor(TC_P1);
            s1.setOutlineColor(TC_BK);
            s1.setOutlineThickness(2);
            s1.setPosition(20, 10);
            win.draw(s1);

            sf::Text s2("P2: " + tIS(p2.wins), font, 18);
            s2.setFillColor(TC_P2);
            s2.setOutlineColor(TC_BK);
            s2.setOutlineThickness(2);
            auto s2b = s2.getLocalBounds();
            s2.setPosition(WIN_W - s2b.width - 20, 10);
            win.draw(s2);

            sf::Text g("FIRST TO " + tIS(T_WINS), font, 11);
            g.setFillColor({ 160, 100, 200, 180 });
            tCT(g);
            g.setPosition(WIN_W / 2.f, 16);
            win.draw(g);
        };

    // ─────────────────────────────────────────────────────────
    //  GAME LOOP
    // ─────────────────────────────────────────────────────────

    TSt   state = TSt::COUNTDOWN;
    float tickT = 0, cdT = T_CD, resT = 0, totT = 0, gp = 0;
    float duckT = 0;   // bg music duck timer (counts down after crash)
    int   rWin = 0, pSel = 0;

    sf::Clock clk;

    while (win.isOpen())
    {
        float dt = tc(clk.restart().asSeconds(), 0, .05f);
        totT += dt;
        gp += dt;

        // Ramp bg music back up after crash duck
        if (duckT > 0)
        {
            duckT -= dt;
            if (duckT <= 0)
                bgMusic.setVolume(100.f);   // fully restored
            else if (duckT < 1.5f)
                bgMusic.setVolume(20.f + (1.f - duckT / 1.5f) * 100.f);  // fade back in over last 1.5s
        }

        // Events
        sf::Event ev;
        while (win.pollEvent(ev))
        {
            if (ev.type == sf::Event::Closed)
            {
                win.close();
                return { p1.wins, p2.wins };
            }

            if (ev.type == sf::Event::KeyPressed)
            {
                auto k = ev.key.code;

                if (state == TSt::PLAYING)
                {
                    // P1 direction input (WASD)
                    if (k == sf::Keyboard::W && !topp(p1.dir, TU)) { p1.nxt = TU; sfxTurn.play(1.1f, 60); }
                    if (k == sf::Keyboard::S && !topp(p1.dir, TD)) { p1.nxt = TD; sfxTurn.play(1.1f, 60); }
                    if (k == sf::Keyboard::A && !topp(p1.dir, TL)) { p1.nxt = TL; sfxTurn.play(1.1f, 60); }
                    if (k == sf::Keyboard::D && !topp(p1.dir, TR)) { p1.nxt = TR; sfxTurn.play(1.1f, 60); }

                    // P2 direction input (Arrow keys)
                    if (k == sf::Keyboard::Up && !topp(p2.dir, TU)) { p2.nxt = TU; sfxTurn.play(.9f, 60); }
                    if (k == sf::Keyboard::Down && !topp(p2.dir, TD)) { p2.nxt = TD; sfxTurn.play(.9f, 60); }
                    if (k == sf::Keyboard::Left && !topp(p2.dir, TL)) { p2.nxt = TL; sfxTurn.play(.9f, 60); }
                    if (k == sf::Keyboard::Right && !topp(p2.dir, TR)) { p2.nxt = TR; sfxTurn.play(.9f, 60); }

                    if (k == sf::Keyboard::Escape) { pSel = 0; state = TSt::PAUSED; }
                }
                else if (state == TSt::PAUSED)
                {
                    if (k == sf::Keyboard::W || k == sf::Keyboard::Up)   pSel = (pSel + 2) % 3;
                    if (k == sf::Keyboard::S || k == sf::Keyboard::Down)  pSel = (pSel + 1) % 3;
                    if (k == sf::Keyboard::Escape) state = TSt::PLAYING;
                    if (k == sf::Keyboard::Enter || k == sf::Keyboard::Space)
                    {
                        if (pSel == 0) state = TSt::PLAYING;
                        else if (pSel == 1) return { p1.wins, p2.wins };  // Back to hub
                        else
                        {
                            // Restart match
                            p1.wins = p2.wins = 0;
                            resetRound();
                            cdT = T_CD;
                            state = TSt::COUNTDOWN;
                        }
                    }
                }
                else if (state == TSt::ROUND_END && resT > T_RD)
                {
                    if (k == sf::Keyboard::Space || k == sf::Keyboard::Enter)
                    {
                        if (p1.wins >= T_WINS || p2.wins >= T_WINS)
                        {
                            resT = 0;
                            state = TSt::MATCH_END;
                            sfxWin.play();
                        }
                        else
                        {
                            resetRound();
                            cdT = T_CD;
                            state = TSt::COUNTDOWN;
                        }
                    }
                }
                else if (state == TSt::MATCH_END && resT > T_RD)
                {
                    if (k == sf::Keyboard::Space || k == sf::Keyboard::Enter)
                    {
                        // Rematch
                        p1.wins = p2.wins = 0;
                        resetRound();
                        cdT = T_CD;
                        state = TSt::COUNTDOWN;
                    }
                    if (k == sf::Keyboard::Escape)
                        return { p1.wins, p2.wins };
                }
            }
        }

        // Update particles and sparks
        for (auto& p : parts)  p.update(dt);
        for (auto& s : sparks) s.update(dt);

        // Countdown
        if (state == TSt::COUNTDOWN)
        {
            cdT -= dt;
            if (cdT <= 0) state = TSt::PLAYING;
        }

        // Result timers
        if (state == TSt::ROUND_END || state == TSt::MATCH_END)
            resT += dt;

        // Gameplay tick
        if (state == TSt::PLAYING)
        {
            tickT += dt;

            // Spawn head sparks each frame
            if (p1.alive)
                spark({
                    (float)p1.x * T_CELL + T_CELL / 2.f + (float)(rand() % 6 - 3),
                    (float)p1.y * T_CELL + T_CELL / 2.f + (float)(rand() % 6 - 3)
                    }, TC_P1);
            if (p2.alive)
                spark({
                    (float)p2.x * T_CELL + T_CELL / 2.f + (float)(rand() % 6 - 3),
                    (float)p2.y * T_CELL + T_CELL / 2.f + (float)(rand() % 6 - 3)
                    }, TC_P2);

            // Fixed timestep movement ticks
            while (tickT >= T_TICK)
            {
                tickT -= T_TICK;

                // Apply queued direction
                if (p1.alive) p1.dir = p1.nxt;
                if (p2.alive) p2.dir = p2.nxt;

                // Move both players
                auto mv = [&](TPlayer& p)
                    {
                        if (!p.alive) return;
                        if (p.dir == TU) p.y--;
                        else if (p.dir == TD) p.y++;
                        else if (p.dir == TL) p.x--;
                        else                  p.x++;
                    };
                mv(p1);
                mv(p2);

                // Collision detection
                bool d1 = false, d2 = false;

                // Wall collision
                if (p1.alive && (p1.x < 0 || p1.x >= T_GW || p1.y < 0 || p1.y >= T_GH)) d1 = true;
                if (p2.alive && (p2.x < 0 || p2.x >= T_GW || p2.y < 0 || p2.y >= T_GH)) d2 = true;

                // Trail collision
                if (p1.alive && !d1 && grid[p1.x][p1.y]) d1 = true;
                if (p2.alive && !d2 && grid[p2.x][p2.y]) d2 = true;

                // Head-on collision
                if (p1.alive && p2.alive && p1.x == p2.x && p1.y == p2.y)
                    d1 = d2 = true;

                // Handle P1 death
                if (d1 && p1.alive)
                {
                    p1.alive = false;
                    float ex = tc((float)p1.x, 0, (float)(T_GW - 1)) * T_CELL + T_CELL / 2.f;
                    float ey = tc((float)p1.y, 0, (float)(T_GH - 1)) * T_CELL + T_CELL / 2.f;
                    burst({ ex, ey }, TC_P1, 40);
                    burst({ ex, ey }, TC_WH, 15);
                    sfxCrash.play();
                    bgMusic.setVolume(0.f);
                    duckT = 4.6f;   // duck for crash clip duration
                }

                // Handle P2 death
                if (d2 && p2.alive)
                {
                    p2.alive = false;
                    float ex = tc((float)p2.x, 0, (float)(T_GW - 1)) * T_CELL + T_CELL / 2.f;
                    float ey = tc((float)p2.y, 0, (float)(T_GH - 1)) * T_CELL + T_CELL / 2.f;
                    burst({ ex, ey }, TC_P2, 40);
                    burst({ ex, ey }, TC_WH, 15);
                    sfxCrash.play(1.1f);
                    bgMusic.setVolume(20.f);
                    duckT = 4.6f;   // duck for crash clip duration
                }

                // Extend trails for alive players
                if (p1.alive && p1.tl < T_MT)
                {
                    grid[p1.x][p1.y] = 1;
                    p1.tx[p1.tl] = p1.x;
                    p1.ty[p1.tl] = p1.y;
                    p1.tl++;
                }
                if (p2.alive && p2.tl < T_MT)
                {
                    grid[p2.x][p2.y] = 2;
                    p2.tx[p2.tl] = p2.x;
                    p2.ty[p2.tl] = p2.y;
                    p2.tl++;
                }

                // Check round end
                if (!p1.alive || !p2.alive)
                {
                    if (p1.alive) { rWin = 1; p1.wins++; }
                    else if (p2.alive) { rWin = 2; p2.wins++; }
                    else                 rWin = 0;
                    resT = 0;
                    state = TSt::ROUND_END;
                    break;
                }
            }
        }

        // ─────────────────────────────────────────────────────
        //  RENDER
        // ─────────────────────────────────────────────────────

        win.clear(TC_BK);

        if (state == TSt::COUNTDOWN)
        {
            drawBG(gp);
            drawPl(p1, TC_P1, TC_P1G, TC_P1H, totT);
            drawPl(p2, TC_P2, TC_P2G, TC_P2H, totT);

            int   cd = (int)std::ceil(cdT);
            float sc = 1.f + (1.f - std::fmod(cdT, 1.f)) * .4f;

            sf::Text num(cd > 0 ? tIS(cd) : "GO!", font, 80);
            num.setFillColor(cd > 0 ? sf::Color(220, 160, 255) : sf::Color(50, 255, 120));
            num.setOutlineColor(TC_BK);
            num.setOutlineThickness(4);
            tCT(num);
            num.setScale(sc, sc);
            num.setPosition(WIN_W / 2.f, WIN_H * .4f);
            win.draw(num);

            drawHUD();
        }
        else if (state == TSt::PLAYING)
        {
            drawBG(gp);
            drawPl(p1, TC_P1, TC_P1G, TC_P1H, totT);
            drawPl(p2, TC_P2, TC_P2G, TC_P2H, totT);
            drawSps();
            drawHUD();
        }
        else if (state == TSt::PAUSED)
        {
            drawBG(gp);
            drawPl(p1, TC_P1, TC_P1G, TC_P1H, totT);
            drawPl(p2, TC_P2, TC_P2G, TC_P2H, totT);
            drawSps();
            drawHUD();

            sf::RectangleShape dim({ (float)WIN_W, (float)WIN_H });
            dim.setFillColor({ 0, 0, 0, 170 });
            win.draw(dim);

            sf::Text pt("PAUSED", font, 48);
            pt.setFillColor({ 220, 160, 255 });
            pt.setOutlineColor({ 60, 10, 80 });
            pt.setOutlineThickness(4);
            tCT(pt);
            pt.setPosition(WIN_W / 2.f, WIN_H * .22f);
            win.draw(pt);

            const char* items[3] = { "RESUME", "BACK TO HUB", "RESTART MATCH" };
            for (int i = 0; i < 3; i++)
            {
                float iy = WIN_H * .42f + i * 60;
                bool  sel = (i == pSel);

                if (sel)
                {
                    sf::RectangleShape bar({ 320.f, 42.f });
                    bar.setOrigin(160, 21);
                    bar.setPosition(WIN_W / 2.f, iy);
                    bar.setFillColor({ 80, 20, 120, 150 });
                    bar.setOutlineColor(TC_P1);
                    bar.setOutlineThickness(1.5f);
                    win.draw(bar);
                }

                sf::Text it(items[i], font, 20);
                it.setFillColor(sel ? TC_P1 : sf::Color(180, 160, 200));
                it.setOutlineColor(TC_BK);
                it.setOutlineThickness(2);
                tCT(it);
                it.setPosition(WIN_W / 2.f, iy);
                win.draw(it);
            }
        }
        else if (state == TSt::ROUND_END)
        {
            drawBG(gp);
            drawPl(p1, TC_P1, TC_P1G, TC_P1H, totT);
            drawPl(p2, TC_P2, TC_P2G, TC_P2H, totT);
            drawPts();
            drawSps();
            drawHUD();

            float     fi = tc(resT * 2.f, 0, 1);
            sf::Uint8 oa = (sf::Uint8)(fi * 255);

            sf::RectangleShape dim({ (float)WIN_W, (float)WIN_H });
            dim.setFillColor({ 0, 0, 0, (sf::Uint8)(fi * 150) });
            win.draw(dim);

            std::string ws;
            sf::Color   wc;
            if (rWin == 1) { ws = "PLAYER 1 WINS ROUND!"; wc = TC_P1; }
            else if (rWin == 2) { ws = "PLAYER 2 WINS ROUND!"; wc = TC_P2; }
            else { ws = "TIE - NO POINT!";       wc = TC_RED; }

            sf::Text res(ws, font, 28);
            res.setFillColor({ wc.r, wc.g, wc.b, oa });
            res.setOutlineColor({ 0, 0, 0, oa });
            res.setOutlineThickness(3);
            tCT(res);
            res.setPosition(WIN_W / 2.f, WIN_H * .38f);
            win.draw(res);

            sf::Text sc("P1: " + tIS(p1.wins) + "    P2: " + tIS(p2.wins), font, 20);
            sc.setFillColor({ 255, 255, 255, oa });
            tCT(sc);
            sc.setPosition(WIN_W / 2.f, WIN_H * .50f);
            win.draw(sc);

            if (resT > T_RD)
            {
                float    blink = (std::sin(totT * 3.5f) + 1.f) * .5f;
                sf::Text cont("[SPACE] CONTINUE", font, 13);
                cont.setFillColor({ 200, 200, 220, (sf::Uint8)(blink * 180 + 50) });
                tCT(cont);
                cont.setPosition(WIN_W / 2.f, WIN_H * .62f);
                win.draw(cont);
            }
        }
        else if (state == TSt::MATCH_END)
        {
            drawBG(gp);
            drawPts();

            // Confetti burst while results fade in
            if (resT < 3.f)
            {
                sf::Color cc = (p1.wins >= T_WINS) ? TC_P1 : TC_P2;
                float rx = (float)(rand() % WIN_W);
                float ry = (float)(rand() % (WIN_H / 2));
                burst({ rx, ry }, cc, 3);
                burst({ rx, ry }, TC_WH, 1);
            }

            sf::RectangleShape dim({ (float)WIN_W, (float)WIN_H });
            dim.setFillColor({ 0, 0, 0, 180 });
            win.draw(dim);

            float     fi = tc(resT * 1.5f, 0, 1);
            sf::Uint8 oa = (sf::Uint8)(fi * 255);
            bool      p1W = p1.wins >= T_WINS;
            sf::Color mc = p1W ? TC_P1 : TC_P2;
            float     pulse = 1.f + .05f * std::sin(totT * 5.f);

            sf::Text wn(p1W ? "PLAYER 1 WINS THE MATCH!" : "PLAYER 2 WINS THE MATCH!", font, 30);
            wn.setFillColor({ mc.r, mc.g, mc.b, oa });
            wn.setOutlineColor({ 0, 0, 0, oa });
            wn.setOutlineThickness(4);
            tCT(wn);
            wn.setScale(pulse, pulse);
            wn.setPosition(WIN_W / 2.f, WIN_H * .30f);
            win.draw(wn);

            sf::Text p1s("P1: " + tIS(p1.wins), font, 36);
            p1s.setFillColor({ TC_P1.r, TC_P1.g, TC_P1.b, oa });
            tCT(p1s);
            p1s.setPosition(WIN_W / 2.f - 80, WIN_H * .52f);
            win.draw(p1s);

            sf::Text p2s("P2: " + tIS(p2.wins), font, 36);
            p2s.setFillColor({ TC_P2.r, TC_P2.g, TC_P2.b, oa });
            tCT(p2s);
            p2s.setPosition(WIN_W / 2.f + 80, WIN_H * .52f);
            win.draw(p2s);

            if (resT > T_RD)
            {
                float    blink = (std::sin(totT * 3.5f) + 1.f) * .5f;
                sf::Text ag("[SPACE] PLAY AGAIN   |   [ESC] HUB", font, 13);
                ag.setFillColor({ 200, 200, 220, (sf::Uint8)(blink * 180 + 50) });
                tCT(ag);
                ag.setPosition(WIN_W / 2.f, WIN_H * .72f);
                win.draw(ag);
            }
        }

        win.display();
    }

    return { p1.wins, p2.wins };
}