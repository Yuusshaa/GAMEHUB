// ============================================================
//  FLAPPY BIRD - PvP Edition
//  SFML 2.x | C++17
//  Controls: P1=SPACE, P2=ENTER, ESC=Pause
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

static const float F_GRAV = 1600.f;  // Gravity (px/s²)
static const float F_FLAP = -520.f;  // Flap velocity (px/s)
static const float F_PSPD = 220.f;   // Base pipe speed (px/s)
static const float F_PGAP = 170.f;   // Gap between pipes (px)
static const float F_PW = 60.f;    // Pipe width (px)
static const float F_PSPC = 280.f;   // Distance between pipes (px)
static const float F_GH = 76.f;    // Ground height (px)
static const float F_BR = 16.f;    // Bird radius (px)
static const int   F_MAXPP = 16;      // Max pipes on screen
static const int   F_MAXFP = 120;     // Max particles on screen

// Cap dimensions (the wide rim at the open end of each pipe)
static const float F_CAP_H = 20.f;
static const float F_CAP_W = F_PW + 10.f;  // slightly wider than body

// ─────────────────────────────────────────────────────────────
//  COLORS
// ─────────────────────────────────────────────────────────────

static const sf::Color FC_PIPE(88, 172, 60);
static const sf::Color FC_PIDK(58, 130, 35);
static const sf::Color FC_PILT(130, 210, 90);
static const sf::Color FC_GOLD(255, 200, 0);
static const sf::Color FC_WH(255, 255, 255);
static const sf::Color FC_BK(0, 0, 0);
static const sf::Color FC_P1(255, 215, 0);
static const sf::Color FC_P2(80, 200, 255);

// ─────────────────────────────────────────────────────────────
//  HELPERS
// ─────────────────────────────────────────────────────────────

// Linear interpolation
static float fl(float a, float b, float t)
{
    return a + (b - a) * t;
}

// Clamp value between lo and hi
static float fc(float v, float lo, float hi)
{
    return v < lo ? lo : v > hi ? hi : v;
}

// Center text origin
static void fCT(sf::Text& t)
{
    auto r = t.getLocalBounds();
    t.setOrigin(r.left + r.width / 2.f, r.top + r.height / 2.f);
}

// Format seconds as M:SS
static std::string fmtT(float s)
{
    char b[16];
    snprintf(b, 16, "%d:%02d", (int)s / 60, (int)s % 60);
    return b;
}

// ─────────────────────────────────────────────────────────────
//  STRUCTS
// ─────────────────────────────────────────────────────────────

struct FPart
{
    sf::Vector2f pos, vel;
    float        life, mxL, sz;
    sf::Color    col;
    bool         active = false;

    void spawn(sf::Vector2f p, sf::Color c)
    {
        pos = p;
        float a = (rand() % 360) * 3.14159f / 180.f;
        float spd = (float)(rand() % 120 + 40);
        vel = { std::cos(a) * spd, std::sin(a) * spd };
        life = mxL = .45f + (rand() % 30) * .01f;
        col = c;
        sz = (float)(rand() % 5 + 3);
        active = true;
    }

    void update(float dt)
    {
        if (!active) return;
        pos += vel * dt;
        vel.y += 280.f * dt;
        life -= dt;
        if (life <= 0) active = false;
    }
};

struct FPipe
{
    float x, gy;
    bool  s1, s2, active;

    // Collision rect for the TOP pipe (ceiling down to gap top)
    sf::FloatRect top() const
    {
        return { x, 0.f, F_PW, gy - F_PGAP / 2.f };
    }

    // Collision rect for the BOTTOM pipe (gap bottom down to ground)
    sf::FloatRect bot() const
    {
        float botY = gy + F_PGAP / 2.f;
        return { x, botY, F_PW, (float)WIN_H - F_GH - botY };
    }
};

struct FBird
{
    float x, y, vy, angle, fp, dT, pop;
    bool  alive;
    int   score;

    void reset(float sx)
    {
        x = sx;
        y = WIN_H * .45f;
        vy = 0;
        angle = 0;
        fp = 0;
        alive = true;
        score = 0;
        dT = 0;
        pop = 1;
    }
};

enum class FSt { COUNTDOWN, PLAYING, PAUSED, RESULTS };

// ─────────────────────────────────────────────────────────────
//  MAIN
// ─────────────────────────────────────────────────────────────

GameResult runFlappy(sf::RenderWindow& win, const sf::Font& font)
{
    // ── Textures ─────────────────────────────────────────────
    sf::Texture bgTex, b1Tex, b2Tex;
    bgTex.loadFromFile("flappy_bg.png");
    b1Tex.loadFromFile("bird1.png");
    b2Tex.loadFromFile("bird2.png");

    sf::Sprite bgSpr(bgTex);
    bgSpr.setScale(
        (float)WIN_W / bgTex.getSize().x,
        (float)WIN_H / bgTex.getSize().y);

    sf::Sprite b1Spr(b1Tex), b2Spr(b2Tex);
    b1Spr.setOrigin(b1Tex.getSize().x / 2.f, b1Tex.getSize().y / 2.f);
    b2Spr.setOrigin(b2Tex.getSize().x / 2.f, b2Tex.getSize().y / 2.f);
    b1Spr.setScale((F_BR * 2.f) / b1Tex.getSize().x, (F_BR * 2.f) / b1Tex.getSize().x);
    b2Spr.setScale((F_BR * 2.f) / b2Tex.getSize().x, (F_BR * 2.f) / b2Tex.getSize().x);

    // ── Audio ─────────────────────────────────────────────────
    sf::Music bgMusic;
    bgMusic.openFromFile("flappy_bg.ogg");
    bgMusic.setLoop(true);
    bgMusic.setVolume(18.f);   // kept low so it doesn't overpower gameplay
    bgMusic.play();

    sf::SoundBuffer deathBuf, scoreBuf;
    deathBuf.loadFromFile("flappy_death.wav");
    scoreBuf.loadFromFile("flappy_score.wav");
    sf::Sound deathSnd(deathBuf), scoreSnd(scoreBuf);
    deathSnd.setVolume(55.f);
    scoreSnd.setVolume(45.f);

    // ── Particle pool ─────────────────────────────────────────
    FPart parts[F_MAXFP];

    auto burst = [&](sf::Vector2f p, sf::Color c, int n)
        {
            int sp = 0;
            for (auto& pt : parts)
                if (!pt.active && sp < n) { pt.spawn(p, c); sp++; }
        };

    // ── Pipe pool ─────────────────────────────────────────────
    FPipe pipes[F_MAXPP];

    auto pInit = [&]()
        {
            for (auto& p : pipes) p.active = false;
        };

    auto pPush = [&](float px, float pgy)
        {
            for (auto& p : pipes)
                if (!p.active) { p = { px, pgy, false, false, true }; return; }
        };

    auto pClean = [&]()
        {
            for (auto& p : pipes)
                if (p.active && p.x + F_PW < -10) p.active = false;
        };

    // ── drawPipe ──────────────────────────────────────────────
    //
    //  Draws one pipe half (top or bottom) correctly.
    //
    //  TOP pipe  (flipped = false):
    //    Body runs from y=0 down to just above the gap.
    //    Cap sits at the BOTTOM of the body, open end faces the gap.
    //
    //  BOTTOM pipe  (flipped = true):
    //    Cap sits at the TOP of the body, open end faces the gap.
    //    Body runs below the cap down to the ground.
    //
    //  px, py = top-left corner of the pipe area
    //  ph     = total pixel height of the pipe area
    //
    auto drawPipe = [&](float px, float py, float ph, bool flipped)
        {
            if (ph <= 0.f) return;

            // Cap is slightly wider than body — centre it horizontally
            float capX = px - (F_CAP_W - F_PW) / 2.f;

            if (!flipped)
            {
                // ── TOP pipe ─────────────────────────────────────
                // Draw body from ceiling down
                sf::RectangleShape body({ F_PW, ph });
                body.setPosition(px, py);
                body.setFillColor(FC_PIPE);
                win.draw(body);

                // Highlight strip on left side of body
                sf::RectangleShape hi({ 8.f, ph });
                hi.setPosition(px + 6.f, py);
                hi.setFillColor(FC_PILT);
                win.draw(hi);

                // Dark strip on right side of body
                sf::RectangleShape dk({ 6.f, ph });
                dk.setPosition(px + F_PW - 6.f, py);
                dk.setFillColor(FC_PIDK);
                win.draw(dk);

                // Cap at the bottom of the top pipe (open end faces gap)
                sf::RectangleShape cap({ F_CAP_W, F_CAP_H });
                cap.setPosition(capX, py + ph - F_CAP_H);
                cap.setFillColor(FC_PIPE);
                win.draw(cap);

                // Highlight on top edge of cap
                sf::RectangleShape capHi({ F_CAP_W, 5.f });
                capHi.setPosition(capX, py + ph - F_CAP_H);
                capHi.setFillColor(FC_PILT);
                win.draw(capHi);
            }
            else
            {
                // ── BOTTOM pipe ───────────────────────────────────
                // Draw cap first at the top (open end faces gap)
                sf::RectangleShape cap({ F_CAP_W, F_CAP_H });
                cap.setPosition(capX, py);
                cap.setFillColor(FC_PIPE);
                win.draw(cap);

                // Dark strip on the bottom edge of the cap
                sf::RectangleShape capDk({ F_CAP_W, 4.f });
                capDk.setPosition(capX, py + F_CAP_H - 4.f);
                capDk.setFillColor(FC_PIDK);
                win.draw(capDk);

                // Body below the cap, extending down to the ground
                float bodyY = py + F_CAP_H;
                float bodyH = ph - F_CAP_H;

                if (bodyH > 0.f)
                {
                    sf::RectangleShape body({ F_PW, bodyH });
                    body.setPosition(px, bodyY);
                    body.setFillColor(FC_PIPE);
                    win.draw(body);

                    sf::RectangleShape hi({ 8.f, bodyH });
                    hi.setPosition(px + 6.f, bodyY);
                    hi.setFillColor(FC_PILT);
                    win.draw(hi);

                    sf::RectangleShape dk({ 6.f, bodyH });
                    dk.setPosition(px + F_PW - 6.f, bodyY);
                    dk.setFillColor(FC_PIDK);
                    win.draw(dk);
                }
            }
        };

    // Draw all active pipe pairs
    auto drawAllPipes = [&]()
        {
            for (auto& p : pipes)
            {
                if (!p.active) continue;

                // Top pipe: ceiling down to gap top
                float topH = p.gy - F_PGAP / 2.f;
                drawPipe(p.x, 0.f, topH, false);

                // Bottom pipe: gap bottom down to ground
                float botY = p.gy + F_PGAP / 2.f;
                float botH = (float)WIN_H - F_GH - botY;
                drawPipe(p.x, botY, botH, true);
            }
        };

    // Draw all active particles
    auto drawParts = [&]()
        {
            for (auto& p : parts)
            {
                if (!p.active) continue;

                float     t2 = fc(p.life / p.mxL, 0.f, 1.f);
                sf::Color pc = p.col;
                pc.a = (sf::Uint8)(t2 * 200);

                sf::RectangleShape ps({ p.sz, p.sz });
                ps.setOrigin(p.sz / 2.f, p.sz / 2.f);
                ps.setPosition(p.pos);
                ps.setFillColor(pc);
                win.draw(ps);
            }
        };

    // ── Game state ────────────────────────────────────────────
    FBird p1, p2;
    p1.reset(WIN_W * .28f);
    p2.reset(WIN_W * .32f);
    pInit();

    float pTimer = 0, spd = 1, cdT = 3;
    float rTimer = 0, totT = 0, gScr = 0;
    float shk1 = 0, shk2 = 0;
    int   pSel = 0;
    FSt   state = FSt::COUNTDOWN;
    auto resetMatch = [&]()
        {
            p1.reset(WIN_W * .28f);
            p2.reset(WIN_W * .32f);
            pInit();
            pTimer = 0;
            spd = 1;
            shk1 = shk2 = rTimer = 0;
            for (auto& p : parts) p.active = false;
        };

    // ─────────────────────────────────────────────────────────
    //  GAME LOOP
    // ─────────────────────────────────────────────────────────

    sf::Clock clk;

    while (win.isOpen())
    {
        float dt = fc(clk.restart().asSeconds(), 0.f, .05f);
        totT += dt;

        // ── Events ───────────────────────────────────────────
        sf::Event ev;
        while (win.pollEvent(ev))
        {
            if (ev.type == sf::Event::Closed)
            {
                win.close();
                return { p1.score, p2.score };
            }

            if (ev.type == sf::Event::KeyPressed)
            {
                auto k = ev.key.code;

                if (state == FSt::PLAYING)
                {
                    if (k == sf::Keyboard::Space && p1.alive)
                    {
                        p1.vy = F_FLAP;
                        p1.fp = 0;
                        burst({ p1.x, p1.y }, FC_P1, 5);
                    }
                    if (k == sf::Keyboard::Enter && p2.alive)
                    {
                        p2.vy = F_FLAP;
                        p2.fp = 0;
                        burst({ p2.x, p2.y }, FC_P2, 5);
                    }
                    if (k == sf::Keyboard::Escape)
                    {
                        state = FSt::PAUSED;
                        pSel = 0;
                    }
                }
                else if (state == FSt::PAUSED)
                {
                    if (k == sf::Keyboard::Up)    pSel = (pSel - 1 + 3) % 3;
                    if (k == sf::Keyboard::Down)   pSel = (pSel + 1) % 3;
                    if (k == sf::Keyboard::Escape) state = FSt::PLAYING;

                    if (k == sf::Keyboard::Enter || k == sf::Keyboard::Space)
                    {
                        if (pSel == 0)
                        {
                            state = FSt::PLAYING;
                        }
                        else if (pSel == 1)
                        {
                            resetMatch();
                            cdT = 3;
                            state = FSt::COUNTDOWN;
                        }
                        else
                        {
                            return { p1.score, p2.score };
                        }
                    }
                }
                else if (state == FSt::RESULTS && rTimer > 1.8f)
                {
                    if (k == sf::Keyboard::Space || k == sf::Keyboard::Enter)
                    {
                        resetMatch();
                        cdT = 3;
                        state = FSt::COUNTDOWN;
                    }
                    if (k == sf::Keyboard::Escape)
                        return { p1.score, p2.score };
                }
            }
        }

        // ── Update particles ──────────────────────────────────
        for (auto& p : parts) p.update(dt);

        // ── Countdown ─────────────────────────────────────────
        if (state == FSt::COUNTDOWN)
        {
            gScr += 80 * dt;
            cdT -= dt;
            if (cdT <= 0) state = FSt::PLAYING;
        }

        // ── Playing ───────────────────────────────────────────
        if (state == FSt::PLAYING)
        {
            float curSpd = F_PSPD * spd;
            gScr += curSpd * dt;

            shk1 = shk1 - dt * 7 > 0 ? shk1 - dt * 7 : 0.f;
            shk2 = shk2 - dt * 7 > 0 ? shk2 - dt * 7 : 0.f;

            float avg = ((float)p1.score + (float)p2.score) / 2.f;
            spd = fc(1.f + (avg / 10.f) * .05f, 1.f, 2.f);



            // Bird physics
            auto upBird = [&](FBird& b)
                {
                    if (!b.alive)
                    {
                        b.dT += dt;
                        b.vy += F_GRAV * dt * .5f;
                        b.y += b.vy * dt;
                        b.angle = fl(b.angle, 90.f, 8.f * dt);
                        return;
                    }
                    b.vy += F_GRAV * dt;
                    b.vy = fc(b.vy, -800.f, 900.f);
                    b.y += b.vy * dt;
                    b.fp += dt * 10.f;
                    b.angle = fl(b.angle, fc(b.vy * .06f, -30.f, 80.f), 12.f * dt);
                    b.pop = fl(b.pop, 1.f, 10.f * dt);
                    if (b.y - F_BR < 0.f) { b.y = F_BR; b.vy = std::abs(b.vy) * .3f; }
                };
            upBird(p1);
            upBird(p2);

            // Spawn new pipe pair periodically
            pTimer += dt;
            if (pTimer >= F_PSPC / curSpd)
            {
                pTimer = 0;
                // Keep the gap well away from both ceiling and ground
                // so neither pipe half ever appears tiny or floating.
                float mn = WIN_H * 0.30f;   // gap centre no higher than 30% down
                float mx = WIN_H * 0.65f;   // gap centre no lower than 65% down
                float gy = (float)(rand() % (int)(mx - mn)) + mn;
                pPush((float)WIN_W + F_PW, gy);
            }

            // Hitboxes (slightly smaller than visual for fairness)
            sf::FloatRect h1(p1.x - F_BR + 4, p1.y - F_BR + 4, (F_BR - 4) * 2, (F_BR - 4) * 2);
            sf::FloatRect h2(p2.x - F_BR + 4, p2.y - F_BR + 4, (F_BR - 4) * 2, (F_BR - 4) * 2);

            for (auto& pipe : pipes)
            {
                if (!pipe.active) continue;

                pipe.x -= curSpd * dt;

                // Score when pipe clears the bird
                if (!pipe.s1 && pipe.x + F_PW < p1.x && p1.alive)
                {
                    pipe.s1 = true;
                    p1.score++;
                    p1.pop = 1.45f;
                    scoreSnd.play();
                    burst({ p1.x, p1.y - 28 }, FC_GOLD, 10);
                }
                if (!pipe.s2 && pipe.x + F_PW < p2.x && p2.alive)
                {
                    pipe.s2 = true;
                    p2.score++;
                    p2.pop = 1.45f;
                    scoreSnd.play();
                    burst({ p2.x, p2.y - 28 }, FC_GOLD, 10);
                }

                // Pipe collision
                if (p1.alive && (h1.intersects(pipe.top()) || h1.intersects(pipe.bot())))
                {
                    p1.alive = false;
                    shk1 = 1;
                    deathSnd.play();
                    burst({ p1.x, p1.y }, FC_P1, 28);
                }
                if (p2.alive && (h2.intersects(pipe.top()) || h2.intersects(pipe.bot())))
                {
                    p2.alive = false;
                    shk2 = 1;
                    deathSnd.play();
                    burst({ p2.x, p2.y }, FC_P2, 28);
                }
            }
            pClean();

            // Ground collision
            if (p1.alive && p1.y + F_BR > WIN_H - F_GH)
            {
                p1.alive = false;
                shk1 = 1;
                deathSnd.play();
                burst({ p1.x, p1.y }, FC_P1, 28);
            }
            if (p2.alive && p2.y + F_BR > WIN_H - F_GH)
            {
                p2.alive = false;
                shk2 = 1;
                deathSnd.play();
                burst({ p2.x, p2.y }, FC_P2, 28);
            }

            if (!p1.alive && !p2.alive)
            {
                state = FSt::RESULTS;
                rTimer = 0;
            }
        }

        if (state == FSt::RESULTS) rTimer += dt;

        // ─────────────────────────────────────────────────────
        //  RENDER
        // ─────────────────────────────────────────────────────

        win.clear();

        // ── Countdown screen ──────────────────────────────────
        if (state == FSt::COUNTDOWN)
        {
            win.draw(bgSpr);

            int   cd = (int)std::ceil(cdT);
            float sc = 1.f + (1.f - std::fmod(cdT, 1.f)) * .5f;

            sf::Text num(cd > 0 ? std::to_string(cd) : "GO!", font, 90);
            num.setFillColor(cd > 0 ? sf::Color(255, 200, 60) : sf::Color(100, 240, 100));
            num.setOutlineColor(FC_BK);
            num.setOutlineThickness(5);
            fCT(num);
            num.setScale(sc, sc);
            num.setPosition(WIN_W / 2.f, WIN_H * .38f);
            win.draw(num);
        }

        // ── Playing / Results screen ──────────────────────────
        else if (state == FSt::PLAYING || state == FSt::RESULTS)
        {
            // Camera shake on death
            float    shk = std::max(shk1, shk2);
            sf::View v = win.getDefaultView();
            if (state == FSt::PLAYING && shk > 0)
                v.move(
                    (rand() % 2 == 0 ? 1.f : -1.f) * shk * 3,
                    (rand() % 2 == 0 ? 1.f : -1.f) * shk * 3);
            win.setView(v);

            win.draw(bgSpr);
            drawAllPipes();
            drawParts();

            // P2 bird (drawn behind P1)
            float a2 = p2.alive ? 255.f : fc(255.f - (p2.dT - .6f) * 400.f, 0.f, 255.f);
            b2Spr.setPosition(p2.x, p2.y);
            b2Spr.setRotation(p2.angle);
            b2Spr.setColor({ 255, 255, 255, (sf::Uint8)a2 });
            win.draw(b2Spr);

            // P1 bird
            float a1 = p1.alive ? 255.f : fc(255.f - (p1.dT - .6f) * 400.f, 0.f, 255.f);
            b1Spr.setPosition(p1.x, p1.y);
            b1Spr.setRotation(p1.angle);
            b1Spr.setColor({ 255, 255, 255, (sf::Uint8)a1 });
            win.draw(b1Spr);

            win.setView(win.getDefaultView());

            // HUD
            if (state == FSt::PLAYING)
            {
                sf::Text s1(std::to_string(p1.score), font, 40);
                s1.setFillColor({ FC_P1.r, FC_P1.g, FC_P1.b, 220 });
                s1.setOutlineColor(FC_BK);
                s1.setOutlineThickness(3);
                s1.setScale(p1.pop, p1.pop);
                s1.setPosition(20, 14);
                win.draw(s1);

                sf::Text s2(std::to_string(p2.score), font, 40);
                s2.setFillColor({ FC_P2.r, FC_P2.g, FC_P2.b, 220 });
                s2.setOutlineColor(FC_BK);
                s2.setOutlineThickness(3);
                s2.setScale(p2.pop, p2.pop);
                auto s2b = s2.getLocalBounds();
                s2.setPosition(WIN_W - s2b.width * p2.pop - 20, 14);
                win.draw(s2);


                if (!p1.alive)
                {
                    sf::Text d("P1 DEAD", font, 12);
                    d.setFillColor({ 220, 60, 40, 200 });
                    d.setPosition(14, 70);
                    win.draw(d);
                }
                if (!p2.alive)
                {
                    sf::Text d("P2 DEAD", font, 12);
                    d.setFillColor({ 220, 60, 40, 200 });
                    auto db = d.getLocalBounds();
                    d.setPosition(WIN_W - db.width - 14, 70);
                    win.draw(d);
                }
            }

            // Results overlay
            if (state == FSt::RESULTS && rTimer > .5f)
            {
                float     fi = fc((rTimer - .5f) * 1.8f, 0.f, 1.f);
                sf::Uint8 oa = (sf::Uint8)(fi * 220);

                sf::RectangleShape dim({ (float)WIN_W, (float)WIN_H });
                dim.setFillColor({ 0, 0, 0, (sf::Uint8)(fi * 170) });
                win.draw(dim);

                sf::Text ro("ROUND OVER", font, 28);
                ro.setFillColor({ 220, 60, 40, oa });
                ro.setOutlineColor({ 0, 0, 0, oa });
                ro.setOutlineThickness(3);
                fCT(ro);
                ro.setPosition(WIN_W / 2.f, WIN_H / 2.f - 152);
                win.draw(ro);

                sf::Text p1s(std::to_string(p1.score), font, 58);
                p1s.setFillColor({ 255, 255, 255, oa });
                p1s.setOutlineColor({ 0, 0, 0, oa });
                p1s.setOutlineThickness(4);
                fCT(p1s);
                p1s.setPosition(WIN_W / 2.f - 90, WIN_H / 2.f - 18);
                win.draw(p1s);

                sf::Text p2s(std::to_string(p2.score), font, 58);
                p2s.setFillColor({ 255, 255, 255, oa });
                p2s.setOutlineColor({ 0, 0, 0, oa });
                p2s.setOutlineThickness(4);
                fCT(p2s);
                p2s.setPosition(WIN_W / 2.f + 90, WIN_H / 2.f - 18);
                win.draw(p2s);

                if (rTimer > 1.f)
                {
                    sf::Uint8   wa = (sf::Uint8)(fc((rTimer - 1.f) * 2, 0, 1) * 255);
                    std::string wt;
                    sf::Color   wc;

                    if (p1.score > p2.score)
                    {
                        wt = "PLAYER 1 WINS!";
                        wc = { FC_P1.r, FC_P1.g, FC_P1.b, wa };
                    }
                    else if (p2.score > p1.score)
                    {
                        wt = "PLAYER 2 WINS!";
                        wc = { FC_P2.r, FC_P2.g, FC_P2.b, wa };
                    }
                    else
                    {
                        wt = "IT'S A TIE!";
                        wc = { 255, 200, 80, wa };
                    }

                    sf::Text wr(wt, font, 20);
                    wr.setFillColor(wc);
                    wr.setOutlineColor({ 0, 0, 0, wa });
                    wr.setOutlineThickness(3);
                    fCT(wr);
                    wr.setPosition(WIN_W / 2.f, WIN_H / 2.f + 72);
                    win.draw(wr);
                }

                if (rTimer > 1.8f)
                {
                    float    blink = (std::sin(totT * 3.5f) + 1.f) * .5f;
                    sf::Text pr("[SPACE] AGAIN   |   [ESC] HUB", font, 12);
                    pr.setFillColor({ 200, 200, 220, (sf::Uint8)(blink * 160 + 50) });
                    fCT(pr);
                    pr.setPosition(WIN_W / 2.f, WIN_H / 2.f + 140);
                    win.draw(pr);
                }
            }
        }

        // ── Pause screen ──────────────────────────────────────
        else if (state == FSt::PAUSED)
        {
            win.draw(bgSpr);
            drawAllPipes();

            b2Spr.setPosition(p2.x, p2.y);
            b2Spr.setRotation(p2.angle);
            win.draw(b2Spr);

            b1Spr.setPosition(p1.x, p1.y);
            b1Spr.setRotation(p1.angle);
            win.draw(b1Spr);

            sf::RectangleShape dim({ (float)WIN_W, (float)WIN_H });
            dim.setFillColor({ 0, 0, 0, 175 });
            win.draw(dim);

            sf::Text pt("PAUSED", font, 30);
            pt.setFillColor(FC_WH);
            fCT(pt);
            pt.setPosition(WIN_W / 2.f, WIN_H / 2.f - 118);
            win.draw(pt);

            const char* items[3] = { "RESUME", "RESTART", "BACK TO HUB" };

            for (int i = 0; i < 3; i++)
            {
                float iy = WIN_H / 2.f - 36 + i * 62;
                bool  sel = (pSel == i);

                if (sel)
                {
                    sf::RectangleShape bar({ 260.f, 46.f });
                    bar.setOrigin(130, 23);
                    bar.setPosition(WIN_W / 2.f, iy);
                    bar.setFillColor({ 40, 40, 40, 80 });
                    win.draw(bar);
                }

                sf::Text it(items[i], font, sel ? 22 : 18);
                it.setFillColor(sel ? FC_WH : sf::Color(200, 185, 145, 160));
                fCT(it);
                it.setPosition(WIN_W / 2.f, iy);
                win.draw(it);
            }
        }

        win.display();
    }

    return { p1.score, p2.score };
}