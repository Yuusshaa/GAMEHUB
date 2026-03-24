// ============================================================
//  NEON PONG - Pro Arcade Edition
//  SFML 2.x | C++17
//  Controls: P1=W/S, P2=UP/DOWN, ESC=Pause
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
//  SHARED DECLARATIONS  (inlined - no .h file)
// ─────────────────────────────────────────────────────────────

static constexpr int WIN_W = 1200;
static constexpr int WIN_H = 730;

struct GameResult { int p1 = 0, p2 = 0; };

// ─────────────────────────────────────────────────────────────
//  CONSTANTS
// ─────────────────────────────────────────────────────────────

static const int P_WIN_SC = 7;   // Points needed to win
static const int P_MAXP = 80;  // Max particles on screen

// ─────────────────────────────────────────────────────────────
//  COLORS
// ─────────────────────────────────────────────────────────────

static const sf::Color PC_P1(255, 20, 147);  // P1 hot pink
static const sf::Color PC_P2(0, 230, 255);  // P2 neon cyan
static const sf::Color PC_WH(255, 255, 255);  // White

// ─────────────────────────────────────────────────────────────
//  HELPERS
// ─────────────────────────────────────────────────────────────

// Clamp value between lo and hi
static float pc2(float v, float lo, float hi)
{
    return v < lo ? lo : v > hi ? hi : v;
}

// Center text origin
static void pCT(sf::Text& t)
{
    auto r = t.getLocalBounds();
    t.setOrigin(r.left + r.width / 2.f, r.top + r.height / 2.f);
}

// Draw text with neon glow effect
static void pGlowText(sf::RenderWindow& win, sf::Text txt, sf::Color gc, int passes = 3)
{
    sf::Text glow = txt;
    for (int p = passes; p >= 1; p--)
    {
        sf::Color c = gc;
        c.a = (sf::Uint8)(40 * p / passes);
        glow.setFillColor(c);
        for (int dx = -p * 2; dx <= p * 2; dx += p * 2)
            for (int dy = -p * 2; dy <= p * 2; dy += p * 2)
            {
                glow.setPosition(txt.getPosition() + sf::Vector2f((float)dx, (float)dy));
                win.draw(glow);
            }
    }
    win.draw(txt);
}

// ─────────────────────────────────────────────────────────────
//  PARTICLE
// ─────────────────────────────────────────────────────────────

struct PPart
{
    sf::RectangleShape shape;
    sf::Vector2f       vel;
    float              life, mxL;
    bool               active;

    void init(sf::Vector2f pos, sf::Color color)
    {
        shape.setSize({ 4, 4 });
        shape.setFillColor(color);
        shape.setPosition(pos);

        float a = (float)(rand() % 360) * 3.14159f / 180.f;
        float spd = (float)(rand() % 60 + 20);
        vel = { std::cos(a) * spd, std::sin(a) * spd };
        life = mxL = .45f + (rand() % 20) * .01f;
        active = true;
    }

    void update(float dt)
    {
        if (!active) return;
        shape.move(vel * dt);
        vel *= .97f;
        life -= dt;
        if (life <= 0) { active = false; return; }

        sf::Color c = shape.getFillColor();
        c.a = (sf::Uint8)(life / mxL * 220);
        shape.setFillColor(c);
    }
};

// ─────────────────────────────────────────────────────────────
//  BALL
// ─────────────────────────────────────────────────────────────

struct PBall
{
    sf::CircleShape shape;
    sf::Vector2f    vel;
    PPart           trail[P_MAXP];
    int             ni = 0;
    float           spd = 1.f;

    PBall()
    {
        shape.setRadius(8);
        shape.setFillColor({ 0, 230, 255 });
        shape.setOrigin(8, 8);
        resetC();
    }

    void resetC()
    {
        shape.setPosition(WIN_W / 2.f, WIN_H / 2.f);
        vel = { 0, 0 };
        for (auto& p : trail) p.active = false;
    }

    void launch(int dir)
    {
        float sx = dir * 480.f * spd;
        float sy = (float)(rand() % 140 + 150) * (rand() % 2 == 0 ? 1.f : -1.f) * spd;
        vel = { sx, sy };
    }

    void update(float dt)
    {
        shape.move(vel * dt);
        trail[ni].init(shape.getPosition(), { 0, 200, 255, 100 });
        ni = (ni + 1) % P_MAXP;
        for (auto& p : trail) p.update(dt);
    }

    // Bounce off left paddle
    bool resolveL(float sr)
    {
        if (vel.x >= 0) return false;
        vel.x = std::abs(vel.x) * 1.05f;
        if (vel.x > 1400) vel.x = 1400;
        shape.setPosition(sr + 9, shape.getPosition().y);
        return true;
    }

    // Bounce off right paddle
    bool resolveR(float sl)
    {
        if (vel.x <= 0) return false;
        vel.x = -std::abs(vel.x) * 1.05f;
        if (vel.x < -1400) vel.x = -1400;
        shape.setPosition(sl - 9, shape.getPosition().y);
        return true;
    }

    void bounceY()
    {
        vel.y = -vel.y;
        if (std::abs(vel.y) < 80)
            vel.y = (vel.y > 0 ? 1.f : -1.f) * 80.f;
    }

    sf::Vector2f  pos()    const { return shape.getPosition(); }
    sf::FloatRect bounds() const { return shape.getGlobalBounds(); }

    void draw(sf::RenderWindow& w)
    {
        for (auto& p : trail)
            if (p.active) w.draw(p.shape);
        w.draw(shape);
    }
};

// ─────────────────────────────────────────────────────────────
//  GAME STATE
// ─────────────────────────────────────────────────────────────

enum class PSt2 { SERVING, PLAYING, PAUSED, MATCH_END };

// ─────────────────────────────────────────────────────────────
//  RENDERING HELPERS
// ─────────────────────────────────────────────────────────────

// Draw paddle with neon glow
static void pDrawPaddle(sf::RenderWindow& win, sf::RectangleShape& pad, sf::Color col)
{
    for (int i = 4; i >= 1; i--)
    {
        sf::Vector2f       s = pad.getSize() + sf::Vector2f((float)(i * 4), (float)(i * 4));
        sf::RectangleShape g;
        g.setSize(s);
        g.setOrigin(s.x / 2, s.y / 2);
        g.setPosition(pad.getPosition());
        sf::Color gc = col;
        gc.a = (sf::Uint8)(30 * i);
        g.setFillColor(gc);
        win.draw(g);
    }
    win.draw(pad);
}

// Draw dotted centre divider line
static void pCentreLine(sf::RenderWindow& win)
{
    float              y = 0;
    sf::RectangleShape d({ 3.f, 14.f });
    d.setFillColor({ 255, 255, 255, 35 });
    while (y < WIN_H)
    {
        d.setPosition(WIN_W / 2.f - 1.5f, y);
        win.draw(d);
        y += 24.f;
    }
}

// Draw subtle grid over background
static void pBackground(sf::RenderWindow& win)
{
    sf::Color gc(255, 255, 255, 8);
    for (int x = 0; x <= WIN_W; x += 80)
    {
        sf::Vertex l[] = {
            sf::Vertex({ (float)x, 0 },           gc),
            sf::Vertex({ (float)x, (float)WIN_H }, gc)
        };
        win.draw(l, 2, sf::Lines);
    }
    for (int y = 0; y <= WIN_H; y += 80)
    {
        sf::Vertex l[] = {
            sf::Vertex({ 0,            (float)y }, gc),
            sf::Vertex({ (float)WIN_W, (float)y }, gc)
        };
        win.draw(l, 2, sf::Lines);
    }
}

// Draw CRT scanline overlay
static void pScanlines(sf::RenderWindow& win)
{
    sf::RectangleShape l({ (float)WIN_W, 1.f });
    l.setFillColor({ 0, 0, 0, 18 });
    for (int y = 0; y < WIN_H; y += 3)
    {
        l.setPosition(0, (float)y);
        win.draw(l);
    }
}

// ─────────────────────────────────────────────────────────────
//  MAIN
// ─────────────────────────────────────────────────────────────

GameResult runPong(sf::RenderWindow& win, const sf::Font& font,
    const std::string& p1Name, const std::string& p2Name)
{
    // Audio - SFX
    sf::SoundBuffer boopB, fahhB;
    boopB.loadFromFile("boop.wav");
    fahhB.loadFromFile("fahh.wav");
    sf::Sound boop, fahh;
    boop.setBuffer(boopB);
    fahh.setBuffer(fahhB);

    // Background music (looping)
    sf::Music bgMusic;
    bgMusic.openFromFile("pong_bg.ogg");
    bgMusic.setLoop(true);
    bgMusic.play();

    // Optional desperation mode music
    sf::Music despMusic;
    bool despLoaded = despMusic.openFromFile("desperation.wav");
    despMusic.setLoop(true);
    bool despMode = false;

    // Background image
    sf::Texture bgTex;
    bgTex.loadFromFile("pong_bg.png");
    sf::Sprite bgSpr(bgTex);
    bgSpr.setScale(
        (float)WIN_W / bgTex.getSize().x,
        (float)WIN_H / bgTex.getSize().y
    );

    // Ball
    PBall ball;

    // Paddles
    sf::RectangleShape lPad({ 16.f, 110.f });
    lPad.setOrigin(8, 55);
    lPad.setPosition(60, WIN_H / 2.f);
    lPad.setFillColor(PC_P1);

    sf::RectangleShape rPad({ 16.f, 110.f });
    rPad.setOrigin(8, 55);
    rPad.setPosition(WIN_W - 60, WIN_H / 2.f);
    rPad.setFillColor(PC_P2);

    // Game state
    int   sL = 0, sR = 0, mSel = 0, srv = 0;
    float totT = 0, meT = 0, optT = -1.f;   // optT = "no options" toast timer
    PSt2  state = PSt2::SERVING;
    ball.resetC();

    // Draw score HUD and player names
    auto drawHUD = [&]()
        {
            sf::Text sl(std::to_string(sL), font, 100);
            sl.setFillColor({ PC_P1.r, PC_P1.g, PC_P1.b, 200 });
            sl.setLetterSpacing(1);
            pCT(sl);
            sl.setPosition(WIN_W / 2.f - 160, 60);
            pGlowText(win, sl, PC_P1, 5);

            sf::Text sr(std::to_string(sR), font, 100);
            sr.setFillColor({ PC_P2.r, PC_P2.g, PC_P2.b, 200 });
            sr.setLetterSpacing(1);
            pCT(sr);
            sr.setPosition(WIN_W / 2.f + 160, 60);
            pGlowText(win, sr, PC_P2, 5);

            sf::Text sep(":", font, 80);
            sep.setFillColor({ 255, 255, 255, 30 });
            pCT(sep);
            sep.setPosition(WIN_W / 2.f, 52);
            win.draw(sep);

            sf::Text ftl("FIRST TO " + std::to_string(P_WIN_SC), font, 11);
            ftl.setFillColor({ 255, 255, 255, 60 });
            pCT(ftl);
            ftl.setPosition(WIN_W / 2.f, 162);
            win.draw(ftl);

            sf::Text n1(p1Name, font, 15);
            n1.setFillColor({ PC_P1.r, PC_P1.g, PC_P1.b, 200 });
            n1.setPosition(24, 14);
            win.draw(n1);

            sf::Text n2(p2Name, font, 15);
            n2.setFillColor({ PC_P2.r, PC_P2.g, PC_P2.b, 200 });
            auto n2b = n2.getLocalBounds();
            n2.setPosition(WIN_W - n2b.width - 24, 14);
            win.draw(n2);
        };

    // Draw pause menu overlay
    auto drawPause = [&]()
        {
            sf::RectangleShape dim({ (float)WIN_W, (float)WIN_H });
            dim.setFillColor({ 0, 0, 0, 200 });
            win.draw(dim);

            sf::RectangleShape panel({ 420, 420 });
            panel.setOrigin(210, 210);
            panel.setPosition(WIN_W / 2.f, WIN_H / 2.f);
            panel.setFillColor({ 10, 10, 30, 220 });
            panel.setOutlineColor({ 255, 255, 255, 30 });
            panel.setOutlineThickness(1);
            win.draw(panel);

            sf::Text title("PAUSED", font, 44);
            title.setFillColor({ 255, 255, 255, 230 });
            title.setLetterSpacing(6);
            pCT(title);
            title.setPosition(WIN_W / 2.f, WIN_H / 2.f - 165);
            pGlowText(win, title, { 150, 150, 255 }, 2);

            struct MI { const char* l; sf::Color a; };
            MI items[4] = {
                { "RESUME",              { 0,   230, 255 } },
                { "OPTIONS",             { 255, 190, 0   } },
                { "BACK TO HUB",         { 255, 20,  147 } },
                { "PONG OF DESPERATION", { 180, 0,   255 } }
            };

            for (int i = 0; i < 4; i++)
            {
                float iy = WIN_H / 2.f - 80 + i * 68;
                bool  sel = (mSel == i);

                if (sel)
                {
                    sf::RectangleShape bar({ 320.f, 48.f });
                    bar.setOrigin(160, 24);
                    bar.setPosition(WIN_W / 2.f, iy);
                    sf::Color bc = items[i].a;
                    bc.a = 30;
                    bar.setFillColor(bc);
                    win.draw(bar);

                    float    pulse = std::sin(totT * 8.f) * 4.f;
                    sf::Text arr(">", font, 24);
                    arr.setFillColor(items[i].a);
                    arr.setPosition(WIN_W / 2.f - 175 - pulse, iy - 12);
                    win.draw(arr);
                }

                std::string lbl = items[i].l;
                if (i == 3) lbl += despMode ? "  [ON]" : "  [OFF]";

                sf::Text t(lbl, font, i == 3 ? 18 : 22);
                t.setFillColor(sel ? sf::Color(255, 255, 255, 240) : sf::Color(255, 255, 255, 100));
                t.setLetterSpacing(sel ? 3.f : 1.f);
                pCT(t);
                t.setPosition(WIN_W / 2.f, iy);
                if (sel) pGlowText(win, t, items[i].a, 2);
                else     win.draw(t);
            }

            // "No options" toast
            if (optT > 0)
            {
                float    fade = optT > .4f ? 1.f : optT / .4f;
                sf::Text msg("bro there are no options. just play.", font, 14);
                msg.setFillColor({ 255, 190, 0, (sf::Uint8)(fade * 230) });
                pCT(msg);
                msg.setPosition(WIN_W / 2.f, WIN_H / 2.f + 175);
                pGlowText(win, msg, { 255, 190, 0 }, 1);
            }
        };

    // ─────────────────────────────────────────────────────────
    //  GAME LOOP
    // ─────────────────────────────────────────────────────────

    sf::Clock clk;

    while (win.isOpen())
    {
        float dt = clk.restart().asSeconds();
        if (dt > .05f) dt = .05f;
        totT += dt;
        if (state == PSt2::MATCH_END) meT += dt;
        if (optT > 0) optT -= dt;

        // Events
        sf::Event ev;
        while (win.pollEvent(ev))
        {
            if (ev.type == sf::Event::Closed)
            {
                win.close();
                return { sL, sR };
            }

            if (ev.type == sf::Event::KeyPressed)
            {
                auto k = ev.key.code;

                if (state == PSt2::SERVING)
                {
                    bool lp = (k == sf::Keyboard::W || k == sf::Keyboard::S);
                    bool rp = (k == sf::Keyboard::Up || k == sf::Keyboard::Down);
                    if ((srv == 0 && lp) || (srv == 1 && rp))
                    {
                        ball.launch(srv == 0 ? -1 : 1);
                        state = PSt2::PLAYING;
                    }
                    if (k == sf::Keyboard::Escape) { mSel = 0; state = PSt2::PAUSED; }
                }
                else if (state == PSt2::PLAYING && k == sf::Keyboard::Escape)
                {
                    mSel = 0;
                    state = PSt2::PAUSED;
                }
                else if (state == PSt2::PAUSED)
                {
                    if (k == sf::Keyboard::Up)    mSel = (mSel - 1 + 4) % 4;
                    if (k == sf::Keyboard::Down)   mSel = (mSel + 1) % 4;
                    if (k == sf::Keyboard::Escape) state = PSt2::SERVING;
                    if (k == sf::Keyboard::Enter)
                    {
                        if (mSel == 0)
                        {
                            state = PSt2::SERVING;
                        }
                        else if (mSel == 1)
                        {
                            // "No options" toast
                            optT = 2.5f;
                        }
                        else if (mSel == 2)
                        {
                            // Back to hub
                            if (despMode) { despMusic.stop(); despMode = false; }
                            return { sL, sR };
                        }
                        else if (mSel == 3)
                        {
                            // Toggle desperation mode
                            despMode = !despMode;
                            if (despMode && despLoaded) despMusic.play();
                            else                        despMusic.stop();
                        }
                    }
                }
                else if (state == PSt2::MATCH_END && meT > 1.5f)
                {
                    if (k == sf::Keyboard::Enter || k == sf::Keyboard::Space)
                    {
                        // Rematch
                        sL = sR = 0;
                        ball.resetC();
                        lPad.setPosition(60, WIN_H / 2.f);
                        rPad.setPosition(WIN_W - 60, WIN_H / 2.f);
                        srv = rand() % 2;
                        state = PSt2::SERVING;
                        meT = 0;
                    }
                    if (k == sf::Keyboard::Escape)
                    {
                        if (despMode) { despMusic.stop(); despMode = false; }
                        return { sL, sR };
                    }
                }
            }
        }

        // Paddle movement
        if (state == PSt2::PLAYING || state == PSt2::SERVING)
        {
            float ps = 700;
            float tl = 55;
            float bl = (float)WIN_H - 55;

            if (sf::Keyboard::isKeyPressed(sf::Keyboard::W) && lPad.getPosition().y > tl) lPad.move(0, -ps * dt);
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::S) && lPad.getPosition().y < bl) lPad.move(0, ps * dt);
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Up) && rPad.getPosition().y > tl) rPad.move(0, -ps * dt);
            if (sf::Keyboard::isKeyPressed(sf::Keyboard::Down) && rPad.getPosition().y < bl) rPad.move(0, ps * dt);
        }

        // Ball physics + collision
        if (state == PSt2::PLAYING)
        {
            ball.update(dt);

            // Wall bounce
            if (ball.pos().y < 10 || ball.pos().y > WIN_H - 10)
                ball.bounceY();

            // Paddle collision
            bool hl = ball.bounds().intersects(lPad.getGlobalBounds()) && ball.vel.x < 0;
            bool hr = ball.bounds().intersects(rPad.getGlobalBounds()) && ball.vel.x > 0;
            if (hl) ball.resolveL(lPad.getPosition().x + 8);
            if (hr) ball.resolveR(rPad.getPosition().x - 8);
            if (hl || hr) { boop.stop(); boop.play(); }

            // Scoring
            if (ball.pos().x < -10)
            {
                sR++;
                fahh.play();
                srv = 0;
                ball.resetC();
                if (sR >= P_WIN_SC) { state = PSt2::MATCH_END; meT = 0; }
                else                  state = PSt2::SERVING;
            }
            if (ball.pos().x > WIN_W + 10)
            {
                sL++;
                fahh.play();
                srv = 1;
                ball.resetC();
                if (sL >= P_WIN_SC) { state = PSt2::MATCH_END; meT = 0; }
                else                  state = PSt2::SERVING;
            }
        }

        // ─────────────────────────────────────────────────────
        //  RENDER
        // ─────────────────────────────────────────────────────

        win.clear();
        win.draw(bgSpr);

        // Dim bg so gameplay elements stand out
        sf::RectangleShape bgDim({ (float)WIN_W, (float)WIN_H });
        bgDim.setFillColor({ 0, 0, 0, 170 });
        win.draw(bgDim);

        pBackground(win);   // subtle grid on top
        pCentreLine(win);
        drawHUD();
        pDrawPaddle(win, lPad, PC_P1);
        pDrawPaddle(win, rPad, PC_P2);
        ball.draw(win);

        // Serve prompt
        if (state == PSt2::SERVING)
        {
            bool        iL = (srv == 0);
            sf::Color   ac = iL ? PC_P1 : PC_P2;
            std::string pn = iL ? p1Name : p2Name;
            std::string ky = iL ? "W  or  S" : "UP  or  DOWN";

            sf::RectangleShape panel({ 420.f, 100.f });
            panel.setOrigin(210, 50);
            panel.setPosition(WIN_W / 2.f, WIN_H * .62f);
            panel.setFillColor({ 6, 6, 14, 210 });
            panel.setOutlineColor({ ac.r, ac.g, ac.b, 60 });
            panel.setOutlineThickness(1);
            win.draw(panel);

            sf::Text t(pn + " - press " + ky + " to serve", font, 18);
            t.setFillColor(ac);
            pCT(t);
            t.setPosition(WIN_W / 2.f, WIN_H * .62f);
            pGlowText(win, t, ac, 2);
        }

        if (state == PSt2::PAUSED) drawPause();

        // Match end screen
        if (state == PSt2::MATCH_END)
        {
            sf::RectangleShape dim({ (float)WIN_W, (float)WIN_H });
            dim.setFillColor({ 0, 0, 0, 185 });
            win.draw(dim);

            bool        p1W = sL >= P_WIN_SC;
            sf::Color   wc = p1W ? PC_P1 : PC_P2;
            std::string wn = (p1W ? p1Name : p2Name) + "  WINS!";

            sf::Text wt(wn, font, 48);
            wt.setFillColor(wc);
            pCT(wt);
            wt.setPosition(WIN_W / 2.f, WIN_H * .28f);
            pGlowText(win, wt, wc, 4);

            sf::Text sc(std::to_string(sL) + "  -  " + std::to_string(sR), font, 52);
            sc.setFillColor(PC_WH);
            pCT(sc);
            sc.setPosition(WIN_W / 2.f, WIN_H * .46f);
            win.draw(sc);

            if (meT > 1.5f)
            {
                float    blink = (std::sin(totT * 3.5f) + 1.f) * .5f;
                sf::Text cont("[ENTER] Rematch   |   [ESC] Back to Hub", font, 16);
                cont.setFillColor({ 200, 200, 220, (sf::Uint8)(blink * 180 + 50) });
                pCT(cont);
                cont.setPosition(WIN_W / 2.f, WIN_H * .64f);
                win.draw(cont);
            }
        }

        pScanlines(win);
        win.display();
    }

    return { sL, sR };
}