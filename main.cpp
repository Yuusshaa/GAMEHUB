// ============================================================
//  GAME HUB - main.cpp
//  SFML 2.x | C++17
//
//  Central hub that manages all three games:
//    - GRIDLOCK   (Tron light cycles)
//    - FLAPPY BIRD (PvP pipe survival)
//    - NEON PONG  (classic arcade paddle battle)
//
//  Controls:
//    LEFT / RIGHT   Navigate game tiles
//    ENTER          Launch selected game
//    L or X         Open leaderboard
//    ESC            Exit hub
//
//  BACKGROUND SYSTEM:
//    Drop any of these files next to the .exe:
//      hub_bg.png  hub_bg.jpg  background.png  etc.
//    The image is rotated 90 degrees and scaled to fill the window.
//    Without an image, an animated procedural background runs instead.
//    A semi-transparent dark overlay is always applied on top.
//
//  BACKGROUND MUSIC:
//    Place "bgg.mac" next to the .exe.
//    Music loops automatically while the hub is open.
//    Pauses when a game launches, resumes when you return.
//    Press M at any time to mute / unmute.
//    Volume fades in/out smoothly instead of snapping.
// ============================================================

#define _CRT_SECURE_NO_WARNINGS
#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <string>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cstdio>
#include <fstream>

// ─────────────────────────────────────────────────────────────
//  SHARED DECLARATIONS
// ─────────────────────────────────────────────────────────────

static constexpr int WIN_W = 1200;
static constexpr int WIN_H = 730;

struct GameResult { int p1 = 0, p2 = 0; };

GameResult runTron(sf::RenderWindow& window, const sf::Font& font);
GameResult runFlappy(sf::RenderWindow& window, const sf::Font& font);
GameResult runPong(sf::RenderWindow& window, const sf::Font& font,
    const std::string& p1Name, const std::string& p2Name);

// ─────────────────────────────────────────────────────────────
//  SCORE ENTRY  (data record stored by LeaderboardManager)
// ─────────────────────────────────────────────────────────────

struct ScoreEntry
{
    char name[24];   // player name (max 23 chars + null)
    int  score;      // score value
    int  gameId;     // 0=Tron, 1=Flappy, 2=Pong

    // Default constructor — zero everything
    ScoreEntry() : score(0), gameId(0)
    {
        memset(name, 0, 24);
    }

    // Convenience constructor
    ScoreEntry(const std::string& n, int s, int g) : score(s), gameId(g)
    {
        memset(name, 0, 24);
        strncpy(name, n.substr(0, 23).c_str(), 23);
    }
};

// ─────────────────────────────────────────────────────────────
//  LEADERBOARD MANAGER CLASS
//
//  Encapsulates all leaderboard logic in one place:
//    - Stores up to LB_MAX score records in a fixed array
//    - Saves records to a binary file after every change
//    - Loads records from that file on startup
//    - Sorts records (by game first, then score descending)
//    - Provides query helpers used by the hub UI
//
//  Methods match the names required by the assignment spec:
//    AddRecord()          - add a new score entry
//    SaveToFile()         - persist all records to disk
//    LoadFromFile()       - read records from disk on startup
//    SortRecords()        - sort by gameId asc, score desc
//    DisplayLeaderboard() - returns top N entries for a game
// ─────────────────────────────────────────────────────────────

class LeaderboardManager
{
public:

    // Maximum number of records we will ever store
    static const int LB_MAX = 300;

    // File that scores are saved to (sits next to the .exe)
    static const char* SAVE_FILE;

    // Constructor — initialises the array and count to zero
    LeaderboardManager() : count(0)
    {
        // Zero out the array so there is no garbage data
        memset(records, 0, sizeof(records));
    }

    // ── AddRecord ────────────────────────────────────────────
    // Adds a new score entry for a player, then sorts and saves.
    // Ignores scores of 0 or below (not worth recording).
    void AddRecord(const std::string& playerName, int score, int gameId)
    {
        if (score <= 0)       return;   // nothing to record
        if (count >= LB_MAX)  return;   // board is full

        records[count++] = ScoreEntry(playerName, score, gameId);
        SortRecords();
        SaveToFile();
    }

    // ── SaveToFile ───────────────────────────────────────────
    // Writes the entire leaderboard to a binary file.
    // Called automatically by AddRecord so you never lose data.
    void SaveToFile()
    {
        std::ofstream file(SAVE_FILE, std::ios::binary | std::ios::trunc);
        if (!file) return;   // silently fail if file cannot be opened

        // Write the record count first so we know how many to read back
        file.write((char*)&count, sizeof(int));

        // Write each record as a raw binary block
        for (int i = 0; i < count; i++)
            file.write((char*)&records[i], sizeof(ScoreEntry));
    }

    // ── LoadFromFile ─────────────────────────────────────────
    // Reads the leaderboard from disk on startup.
    // If the file does not exist yet, count stays at 0 (safe).
    void LoadFromFile()
    {
        count = 0;   // reset before loading

        std::ifstream file(SAVE_FILE, std::ios::binary);
        if (!file) return;   // no file yet — first run, that is fine

        // Read the count that was saved
        int savedCount = 0;
        file.read((char*)&savedCount, sizeof(int));

        // Clamp to valid range to prevent buffer overflows on corrupt files
        if (savedCount < 0)       savedCount = 0;
        if (savedCount > LB_MAX)  savedCount = LB_MAX;

        // Read each record
        for (int i = 0; i < savedCount; i++)
            file.read((char*)&records[i], sizeof(ScoreEntry));

        count = savedCount;
    }

    // ── SortRecords ──────────────────────────────────────────
    // Insertion sort:  primary key = gameId ascending,
    //                  secondary key = score descending.
    // Insertion sort is stable and efficient for small N.
    void SortRecords()
    {
        for (int i = count - 1; i > 0; i--)
        {
            ScoreEntry& a = records[i - 1];
            ScoreEntry& b = records[i];

            // Decide whether to swap these two neighbours
            bool shouldSwap;
            if (a.gameId != b.gameId)
                shouldSwap = (a.gameId > b.gameId);   // lower gameId first
            else
                shouldSwap = (a.score < b.score);     // higher score first

            if (!shouldSwap) break;   // already in order from here up

            // Swap
            ScoreEntry tmp = a;
            a = b;
            b = tmp;
        }
    }

    // ── DisplayLeaderboard ───────────────────────────────────
    // Fills 'out' with the top N entries for a specific game.
    // Returns the number of entries actually written (0..n).
    // Used by the hub UI to render the leaderboard screen.
    int DisplayLeaderboard(int gameId, ScoreEntry out[], int n = 10) const
    {
        int found = 0;
        for (int i = 0; i < count && found < n; i++)
            if (records[i].gameId == gameId)
                out[found++] = records[i];
        return found;
    }

    // ── GetBestScore ─────────────────────────────────────────
    // Returns the single highest score for a game, or -1 if none.
    // Used to show the "BEST" badge on each game tile.
    int GetBestScore(int gameId) const
    {
        for (int i = 0; i < count; i++)
            if (records[i].gameId == gameId)
                return records[i].score;   // already sorted, first match is best
        return -1;
    }

    // ── GetCount ─────────────────────────────────────────────
    // Returns the total number of stored records.
    int GetCount() const { return count; }

private:

    ScoreEntry records[LB_MAX];   // fixed-size record array
    int        count;             // how many records are currently stored
};

// Static member definition (must live outside the class body)
const char* LeaderboardManager::SAVE_FILE = "hub_scores.dat";

// ─────────────────────────────────────────────────────────────
//  GLOBAL LEADERBOARD INSTANCE
//  One instance shared across the whole hub session.
// ─────────────────────────────────────────────────────────────

static LeaderboardManager g_lb;

// ─────────────────────────────────────────────────────────────
//  HUB MUSIC MANAGER
//
//  Loads "bgg.mac" and plays it on a loop in the background.
//  Smoothly fades volume in/out instead of hard stops.
//  Pauses automatically when a game is launched and resumes
//  when control returns to the hub.
//  Press M to toggle mute at any time.
// ─────────────────────────────────────────────────────────────

class HubMusic
{
public:
    static const int NORMAL_VOLUME = 10;   // 0..100
    static const int FADE_SPEED = 120;  // volume units per second

    HubMusic() : loaded(false), muted(false), currentVol(0.f), targetVol((float)NORMAL_VOLUME) {}

    // Try to open bgg.mac — silent fail if missing
    bool load()
    {
        loaded = stream.openFromFile("bgg.wav");
        if (loaded) { stream.setLoop(true); stream.setVolume(0.f); }
        return loaded;
    }

    // Start playback (call once after load)
    void play()
    {
        if (!loaded) return;
        targetVol = muted ? 0.f : (float)NORMAL_VOLUME;
        stream.play();
    }

    // Pause before launching a game
    void pause() { if (loaded) stream.pause(); }

    // Resume after returning from a game — fades back in
    void resume()
    {
        if (!loaded) return;
        if (!muted) { currentVol = 0.f; targetVol = (float)NORMAL_VOLUME; stream.play(); }
    }

    // M key handler
    void toggleMute()
    {
        muted = !muted;
        targetVol = muted ? 0.f : (float)NORMAL_VOLUME;
    }

    // Call every frame to apply the smooth volume fade
    void update(float dt)
    {
        if (!loaded) return;
        float diff = targetVol - currentVol;
        float step = (float)FADE_SPEED * dt;
        if (diff > step) currentVol += step;
        else if (diff < -step) currentVol -= step;
        else                   currentVol = targetVol;
        stream.setVolume(currentVol);
    }

    // Draw a small pill indicator in the top bar
    void drawIndicator(sf::RenderWindow& w, const sf::Font& f) const
    {
        if (!loaded) return;
        float ix = (float)WIN_W - 160.f, iy = 14.f;

        sf::RectangleShape pill({ 38.f, 24.f });
        pill.setPosition(ix, iy);
        pill.setFillColor(muted ? sf::Color(50, 50, 50, 180) : sf::Color(10, 60, 10, 180));
        pill.setOutlineColor(muted ? sf::Color(80, 80, 80) : sf::Color(16, 124, 16));
        pill.setOutlineThickness(1.f);
        w.draw(pill);

        sf::Text icon(muted ? "M" : "n", f, 13);
        icon.setFillColor(muted ? sf::Color(120, 120, 120) : sf::Color(80, 220, 80));
        sf::FloatRect ib = icon.getLocalBounds();
        icon.setOrigin(ib.left + ib.width / 2.f, ib.top + ib.height / 2.f);
        icon.setPosition(ix + 19.f, iy + 12.f);
        w.draw(icon);
    }

    bool isLoaded() const { return loaded; }
    bool isMuted()  const { return muted; }

private:
    sf::Music stream;
    bool      loaded;
    bool      muted;
    float     currentVol;
    float     targetVol;
};

// Global music instance — pointer shared with drawTopBar
static HubMusic  g_hubMusic;
static HubMusic* g_musicPtr = nullptr;   // set in main(), used by drawTopBar

// ─────────────────────────────────────────────────────────────
//  COLORS
// ─────────────────────────────────────────────────────────────

static const sf::Color H_BG(8, 8, 8);
static const sf::Color H_SURF(22, 22, 22);
static const sf::Color H_SUR2(36, 36, 36);
static const sf::Color H_BORD(55, 55, 55);
static const sf::Color H_GRN(16, 124, 16);
static const sf::Color H_GRNL(82, 176, 67);
static const sf::Color H_GOLD(255, 200, 0);
static const sf::Color H_SILV(200, 200, 200);
static const sf::Color H_BRNZ(205, 127, 50);
static const sf::Color H_WHT(255, 255, 255);
static const sf::Color H_GRY(136, 136, 136);
static const sf::Color H_DGRY(55, 55, 55);

static const sf::Color H_ACC[3] = {
    sf::Color(255, 50,  130),
    sf::Color(255, 215, 0),
    sf::Color(0,   230, 255)
};
static const sf::Color H_ACK[3] = {
    sf::Color(80,  10, 45),
    sf::Color(90,  70, 0),
    sf::Color(0,   60, 95)
};

// ─────────────────────────────────────────────────────────────
//  GAME METADATA
// ─────────────────────────────────────────────────────────────

static const char* G_TITLE[3] = { "GRIDLOCK",        "FLAPPY BIRD",       "NEON PONG" };
static const char* G_SUB[3] = { "Synthwave Racers", "PvP Retro Edition", "Pro Arcade Edition" };
static const char* G_LABA[3] = { "Rounds Won",       "Pipes Passed",      "Points Scored" };
static const char* G_DA[3] = { "Grid-based light cycle battle.",     "Both players, same pipe gauntlet.", "Classic neon paddle battle." };
static const char* G_DB[3] = { "Leave neon trail, force opponent",   "Survive longer, pass more pipes,",  "Custom names, adjustable speed," };
static const char* G_DC[3] = { "into a deadly crash. 5 wins!",       "claim victory in 2 minutes.",       "first to 7 points wins." };

// ─────────────────────────────────────────────────────────────
//  BACKGROUND PARTICLES  (ambient floating dots)
// ─────────────────────────────────────────────────────────────

static const int BG_PARTICLE_COUNT = 80;

struct BgParticle
{
    float     x, y;
    float     vx, vy;
    float     size;
    float     alpha;
    float     alphaDrift;
    float     alphaBase;
    float     phase;
    sf::Color color;
};

static BgParticle g_bgParticles[BG_PARTICLE_COUNT];

static void bgParticleInit(BgParticle& p)
{
    p.x = (float)(rand() % WIN_W);
    p.y = (float)(rand() % WIN_H);
    p.vx = ((float)(rand() % 60) - 30.f) * 0.01f;
    p.vy = ((float)(rand() % 60) - 30.f) * 0.01f;
    p.size = (float)(rand() % 3 + 1);
    p.alphaBase = (float)(rand() % 60 + 20);
    p.alphaDrift = (float)(rand() % 40 + 10) * 0.01f;
    p.phase = (float)(rand() % 628) * 0.01f;
    p.color = H_ACC[rand() % 3];
}

static void bgParticlesInit()
{
    for (int i = 0; i < BG_PARTICLE_COUNT; i++)
        bgParticleInit(g_bgParticles[i]);
}

static void bgParticlesUpdate(float dt, float totalTime)
{
    for (int i = 0; i < BG_PARTICLE_COUNT; i++)
    {
        BgParticle& p = g_bgParticles[i];
        p.x += p.vx * dt * 60.f;
        p.y += p.vy * dt * 60.f;
        if (p.x < -10.f)        p.x = (float)WIN_W + 10.f;
        if (p.x > WIN_W + 10.f) p.x = -10.f;
        if (p.y < -10.f)        p.y = (float)WIN_H + 10.f;
        if (p.y > WIN_H + 10.f) p.y = -10.f;
        p.alpha = p.alphaBase + std::sin(totalTime * p.alphaDrift + p.phase) * 18.f;
        if (p.alpha < 5.f)   p.alpha = 5.f;
        if (p.alpha > 100.f) p.alpha = 100.f;
    }
}

static void bgParticlesDraw(sf::RenderWindow& w)
{
    for (int i = 0; i < BG_PARTICLE_COUNT; i++)
    {
        const BgParticle& p = g_bgParticles[i];
        sf::CircleShape dot(p.size);
        dot.setOrigin(p.size, p.size);
        dot.setPosition(p.x, p.y);
        dot.setFillColor({ p.color.r, p.color.g, p.color.b, (sf::Uint8)p.alpha });
        w.draw(dot);
    }
}

// ─────────────────────────────────────────────────────────────
//  BACKGROUND DRAWING SYSTEM
//
//  Layer order (bottom to top):
//    1. Background image rotated 90 degrees  OR  procedural gradient
//    2. Vignette darkening at screen edges
//    3. Faint pulsing grid lines
//    4. Ambient floating particles
//    5. Semi-transparent dark overlay  (keeps UI readable)
//    6. All UI drawn on top
//
//  OVERLAY_ALPHA controls how "transparent" the background feels.
//  Lower = more background visible.  155 = roughly 60% dark.
// ─────────────────────────────────────────────────────────────

static const sf::Uint8 OVERLAY_ALPHA = 155;

static void drawHubBackground(sf::RenderWindow& w,
    sf::Sprite& bgSprite,
    bool              bgLoaded,
    float             totalTime)
{
    // ── Layer 1: Image (rotated 90°) or procedural blobs ─────

    if (bgLoaded)
    {
        // The sprite is already rotated and positioned in main().
        // Just draw it here.
        w.draw(bgSprite);
    }
    else
    {
        // Procedural fallback: dark base + three slow colour blobs
        sf::RectangleShape base({ (float)WIN_W, (float)WIN_H });
        base.setFillColor({ 10, 8, 16 });
        w.draw(base);

        float blobAlpha = 28.f + 10.f * std::sin(totalTime * 0.4f);

        sf::CircleShape blob1(420.f);
        blob1.setOrigin(420.f, 420.f);
        blob1.setPosition(
            WIN_W * 0.22f + std::sin(totalTime * 0.3f) * 60.f,
            WIN_H * 0.30f + std::cos(totalTime * 0.25f) * 40.f);
        blob1.setFillColor({ 255, 50, 130, (sf::Uint8)blobAlpha });
        w.draw(blob1);

        sf::CircleShape blob2(380.f);
        blob2.setOrigin(380.f, 380.f);
        blob2.setPosition(
            WIN_W * 0.78f + std::cos(totalTime * 0.28f) * 50.f,
            WIN_H * 0.68f + std::sin(totalTime * 0.32f) * 45.f);
        blob2.setFillColor({ 0, 200, 255, (sf::Uint8)(blobAlpha * 0.85f) });
        w.draw(blob2);

        sf::CircleShape blob3(300.f);
        blob3.setOrigin(300.f, 300.f);
        blob3.setPosition(
            WIN_W * 0.50f + std::sin(totalTime * 0.2f) * 80.f,
            WIN_H * 0.50f + std::cos(totalTime * 0.35f) * 60.f);
        blob3.setFillColor({ 16, 124, 16, (sf::Uint8)(blobAlpha * 0.7f) });
        w.draw(blob3);
    }

    // ── Layer 2: Vignette (four gradient edge quads) ─────────
    {
        sf::Uint8 vA = 180;

        sf::VertexArray vt(sf::Quads, 4);
        vt[0] = { {0.f,0.f},{0,0,0,vA} }; vt[1] = { (sf::Vector2f((float)WIN_W,0.f)),{0,0,0,vA} };
        vt[2] = { (sf::Vector2f((float)WIN_W,180.f)),{0,0,0,0} }; vt[3] = { (sf::Vector2f(0.f,180.f)),{0,0,0,0} };
        w.draw(vt);

        sf::VertexArray vb(sf::Quads, 4);
        vb[0] = { (sf::Vector2f(0.f,(float)WIN_H - 180)),{0,0,0,0} }; vb[1] = { (sf::Vector2f((float)WIN_W,(float)WIN_H - 180)),{0,0,0,0} };
        vb[2] = { (sf::Vector2f((float)WIN_W,(float)WIN_H)),{0,0,0,vA} }; vb[3] = { (sf::Vector2f(0.f,(float)WIN_H)),{0,0,0,vA} };
        w.draw(vb);

        sf::VertexArray vl(sf::Quads, 4);
        vl[0] = { (sf::Vector2f(0.f,0.f)),{0,0,0,vA} }; vl[1] = { (sf::Vector2f(160.f,0.f)),{0,0,0,0} };
        vl[2] = { (sf::Vector2f(160.f,(float)WIN_H)),{0,0,0,0} }; vl[3] = { (sf::Vector2f(0.f,(float)WIN_H)),{0,0,0,vA} };
        w.draw(vl);

        sf::VertexArray vr(sf::Quads, 4);
        vr[0] = { (sf::Vector2f((float)WIN_W - 160,0.f)),{0,0,0,0} }; vr[1] = { (sf::Vector2f((float)WIN_W,0.f)),{0,0,0,vA} };
        vr[2] = { (sf::Vector2f((float)WIN_W,(float)WIN_H)),{0,0,0,vA} }; vr[3] = { (sf::Vector2f((float)WIN_W - 160,(float)WIN_H)),{0,0,0,0} };
        w.draw(vr);
    }

    // ── Layer 3: Pulsing faint grid lines ────────────────────
    {
        sf::Uint8 gridAlpha = (sf::Uint8)(10.f + 4.f * std::sin(totalTime * 0.6f));
        sf::Color gc(255, 255, 255, gridAlpha);
        for (int x = 0; x <= WIN_W; x += 80)
        {
            sf::Vertex l[] = { {{(float)x,0.f},gc}, {{(float)x,(float)WIN_H},gc} };
            w.draw(l, 2, sf::Lines);
        }
        for (int y = 0; y <= WIN_H; y += 80)
        {
            sf::Vertex l[] = { {{0.f,(float)y},gc}, {{(float)WIN_W,(float)y},gc} };
            w.draw(l, 2, sf::Lines);
        }
    }

    // ── Layer 4: Floating ambient particles ──────────────────
    bgParticlesDraw(w);

    // ── Layer 5: Dark semi-transparent overlay ────────────────
    sf::RectangleShape overlay({ (float)WIN_W, (float)WIN_H });
    overlay.setFillColor({ 0, 0, 0, OVERLAY_ALPHA });
    w.draw(overlay);
}

// ─────────────────────────────────────────────────────────────
//  HUB STATE
// ─────────────────────────────────────────────────────────────

enum class HubSt { NAME_ENTRY, MAIN, LEADERBOARD };

// ─────────────────────────────────────────────────────────────
//  HELPERS
// ─────────────────────────────────────────────────────────────

static void hCT(sf::Text& t)
{
    auto r = t.getLocalBounds();
    t.setOrigin(r.left + r.width * .5f, r.top + r.height * .5f);
}

static std::string hIS(int n)
{
    if (!n) return "0";
    std::string s; bool ng = n < 0; if (ng) n = -n;
    while (n) { s = char('0' + n % 10) + s; n /= 10; }
    return ng ? "-" + s : s;
}

static void hFR(sf::RenderWindow& w, float x, float y, float rw, float rh,
    sf::Color c, sf::Color oc = sf::Color::Transparent, float th = 0)
{
    sf::RectangleShape r({ rw, rh }); r.setPosition(x, y); r.setFillColor(c);
    if (th > 0) { r.setOutlineColor(oc); r.setOutlineThickness(th); }
    w.draw(r);
}

static void hTX(sf::RenderWindow& w, const sf::Font& f, const std::string& s,
    unsigned sz, float x, float y, sf::Color c, bool cn = false)
{
    if (s.empty()) return;
    sf::Text t(s, f, sz); t.setFillColor(c);
    if (cn) { hCT(t); t.setPosition(x, y); }
    else t.setPosition(x, y);
    w.draw(t);
}

// ─────────────────────────────────────────────────────────────
//  COVER ART  (procedural animated thumbnails)
// ─────────────────────────────────────────────────────────────

static void covTron(sf::RenderWindow& w, float cx, float cy, float cw, float ch, float t)
{
    hFR(w, cx, cy, cw, ch, { 6, 2, 12 });
    for (int i = 0; i * 22 < (int)cw + 22; i++) hFR(w, cx + i * 22.f, cy, 1, ch, { 45, 15, 70, 60 });
    for (int i = 0; i * 22 < (int)ch + 22; i++) hFR(w, cx, cy + i * 22.f, cw, 1, { 45, 15, 70, 60 });
    float t1y = cy + ch * .36f, t1l = cw * .58f;
    hFR(w, cx + cw * .1f, t1y, t1l, 3, { 255, 50, 130, 150 });
    hFR(w, cx + cw * .1f + t1l, t1y - 4, 12, 10, { 255, 160, 210 });
    float t2x = cx + cw * .62f;
    hFR(w, t2x, cy + ch * .28f, 3, ch * .24f, { 50, 255, 120, 150 });
    hFR(w, cx + cw * .28f, cy + ch * .52f, t2x - cx - cw * .28f, 3, { 50, 255, 120, 150 });
    hFR(w, cx + cw * .28f - 12, cy + ch * .52f - 4, 12, 10, { 160, 255, 200 });
    float p = .6f + .4f * std::sin(t * 2.8f);
    hFR(w, cx, cy, cw, 2, { 255, 50, 130, (sf::Uint8)(p * 130) });
    hFR(w, cx, cy + ch - 2, cw, 2, { 50, 255, 120, (sf::Uint8)(p * 100) });
}

static void covFlappy(sf::RenderWindow& w, float cx, float cy, float cw, float ch, float t)
{
    hFR(w, cx, cy, cw, ch * .5f, { 95, 165, 235 });
    hFR(w, cx, cy + ch * .5f, cw, ch * .5f, { 165, 215, 255 });
    hFR(w, cx, cy + ch * .82f, cw, ch * .18f, { 195, 158, 98 });
    hFR(w, cx, cy + ch * .82f, cw, 5, { 150, 110, 55 });
    auto pipe = [&](float px, float gcy) {
        float pw = cw * .13f, cap = 11.f;
        float top = gcy - ch * .12f, bot = gcy + ch * .12f;
        hFR(w, px, cy, pw, top - cy, { 75, 155, 45 });
        hFR(w, px - 3, top - cap, pw + 6, cap, { 75, 155, 45 });
        hFR(w, px, bot, pw, cy + ch * .82f - bot, { 75, 155, 45 });
        hFR(w, px - 3, bot, pw + 6, cap, { 75, 155, 45 });
        };
    pipe(cx + cw * .22f, cy + ch * .46f);
    pipe(cx + cw * .56f, cy + ch * .38f);
    float b1y = cy + ch * .40f + std::sin(t * 2.1f + .3f) * 6;
    sf::CircleShape b1(9); b1.setOrigin(9, 9); b1.setPosition(cx + cw * .41f, b1y); b1.setFillColor({ 255, 215, 0 }); w.draw(b1);
    hFR(w, cx + cw * .41f - 14, b1y + std::sin(t * 8) * 4 - 2, 14, 6, { 220, 140, 0 });
    float b2y = cy + ch * .52f + std::sin(t * 2.3f) * 6;
    sf::CircleShape b2(9); b2.setOrigin(9, 9); b2.setPosition(cx + cw * .44f, b2y); b2.setFillColor({ 80, 200, 255 }); w.draw(b2);
    hFR(w, cx + cw * .44f - 14, b2y + std::sin(t * 8 + 1) * 4 - 2, 14, 6, { 30, 140, 220 });
}

static void covPong(sf::RenderWindow& w, float cx, float cy, float cw, float ch, float t)
{
    hFR(w, cx, cy, cw, ch, { 5, 5, 12 });
    for (int i = 0; i * 14 < (int)ch; i++)
        hFR(w, cx + cw * .5f - 1.5f, cy + i * 14.f, 3, 9, { 255, 255, 255, 25 });
    float lpy = cy + ch * .5f + std::sin(t * 1.7f) * ch * .22f;
    float pw = 8, ph = ch * .26f;
    hFR(w, cx + cw * .07f - 4, lpy - ph * .5f - 4, pw + 8, ph + 8, { 255, 20, 147, 25 });
    hFR(w, cx + cw * .07f, lpy - ph * .5f, pw, ph, { 255, 20, 147, 210 });
    float rpy = cy + ch * .5f + std::sin(t * 2.f + 1.2f) * ch * .2f;
    hFR(w, cx + cw * .88f - 4, rpy - ph * .5f - 4, pw + 8, ph + 8, { 0, 230, 255, 25 });
    hFR(w, cx + cw * .88f, rpy - ph * .5f, pw, ph, { 0, 230, 255, 210 });
    float bx = cx + cw * .28f + std::cos(t * 2.4f) * cw * .26f;
    float by = cy + ch * .5f + std::sin(t * 2.4f * 1.35f) * ch * .27f;
    sf::CircleShape bgc(16); bgc.setOrigin(16, 16); bgc.setPosition(bx, by); bgc.setFillColor({ 0, 230, 255, 28 }); w.draw(bgc);
    sf::CircleShape bl(7);   bl.setOrigin(7, 7);   bl.setPosition(bx, by); bl.setFillColor({ 0, 230, 255, 215 }); w.draw(bl);
}

// ─────────────────────────────────────────────────────────────
//  TOP BAR
// ─────────────────────────────────────────────────────────────

static void drawTopBar(sf::RenderWindow& w, const sf::Font& f)
{
    hFR(w, 0, 0, WIN_W, 52, { 6, 6, 6, 220 });
    hFR(w, 0, 51, WIN_W, 1, H_GRN);

    sf::CircleShape ring(17);
    ring.setOrigin(17, 17); ring.setPosition(36, 26);
    ring.setFillColor(sf::Color::Transparent);
    ring.setOutlineColor(H_GRN); ring.setOutlineThickness(2.5f);
    w.draw(ring);

    sf::Text xl("X", f, 19);
    xl.setFillColor(H_GRN); hCT(xl); xl.setPosition(36, 26); w.draw(xl);
    hFR(w, 61, 12, 1, 28, H_DGRY);

    sf::Text title("GAME  HUB", f, 21);
    title.setFillColor(H_WHT); title.setLetterSpacing(3); title.setPosition(70, 15); w.draw(title);

    // Music mute indicator (drawn before clock so clock sits to the right of it)
    if (g_musicPtr) g_musicPtr->drawIndicator(w, f);

    std::time_t now = std::time(nullptr);
    char buf[12];
    std::strftime(buf, sizeof(buf), "%H:%M", std::localtime(&now));
    sf::Text ti(buf, f, 15); ti.setFillColor(H_GRY);
    auto tb = ti.getLocalBounds();
    ti.setPosition(WIN_W - tb.width - 22, 18); w.draw(ti);

    char db[20];
    std::strftime(db, sizeof(db), "%a %b %d", std::localtime(&now));
    sf::Text dt2(db, f, 10); dt2.setFillColor(H_GRY);
    auto dbb = dt2.getLocalBounds();
    dt2.setPosition(WIN_W - dbb.width - 22, 36); w.draw(dt2);
}

// ─────────────────────────────────────────────────────────────
//  BOTTOM BAR
// ─────────────────────────────────────────────────────────────

static void drawBotBar(sf::RenderWindow& w, const sf::Font& f, HubSt st)
{
    hFR(w, 0, WIN_H - 42, WIN_W, 42, { 6, 6, 6, 220 });
    hFR(w, 0, WIN_H - 42, WIN_W, 1, H_DGRY);

    auto chip = [&](float bx, sf::Color col, const std::string& key, const std::string& lbl)
        {
            float ky = (float)(WIN_H - 21);
            sf::CircleShape c(12); c.setOrigin(12, 12); c.setPosition(bx + 12, ky);
            c.setFillColor(col); w.draw(c);
            sf::Text k(key, f, 13); k.setFillColor(H_WHT); hCT(k); k.setPosition(bx + 12, ky); w.draw(k);
            sf::Text lb(lbl, f, 12); lb.setFillColor(H_GRY); lb.setPosition(bx + 28, WIN_H - 27); w.draw(lb);
        };

    if (st == HubSt::MAIN)
    {
        chip(18, H_GRN, "A", "Launch Game");
        chip(175, { 80, 110, 190 }, "X", "Leaderboard");
        chip(340, { 190, 50, 50 }, "B", "Exit");
        chip(480, { 30, 100, 30 }, "M", "Mute Music");
        sf::Text nav("[< / >] Navigate", f, 11);
        nav.setFillColor(H_DGRY);
        auto nb = nav.getLocalBounds();
        nav.setPosition(WIN_W - nb.width - 20, WIN_H - 27); w.draw(nav);
    }
    else if (st == HubSt::LEADERBOARD)
    {
        chip(18, { 190, 50, 50 }, "B", "Back");
        chip(140, { 55,  55, 55 }, "<>", "Switch");
    }
    else if (st == HubSt::NAME_ENTRY)
    {
        chip(18, H_GRN, "A", "Confirm");
        chip(180, { 55, 55, 55 }, "Tab", "Switch field");
    }
}

// ─────────────────────────────────────────────────────────────
//  GAME TILE
// ─────────────────────────────────────────────────────────────

static void drawTile(sf::RenderWindow& w, const sf::Font& f, int idx, bool sel,
    float tx, float ty, float tw, float th, float totT, int best)
{
    float sc = sel ? 1.f : .94f;
    float dw = tw * sc, dh = th * sc;
    float dx = tx + (tw - dw) * .5f, dy = ty + (th - dh) * .5f;

    if (sel) hFR(w, dx + 6, dy + 6, dw, dh, { 0, 0, 0, 100 });

    sf::Uint8 tileAlpha = sel ? 210 : 185;
    hFR(w, dx, dy, dw, dh,
        sel ? sf::Color(36, 36, 36, tileAlpha) : sf::Color(22, 22, 22, tileAlpha));

    if (sel)
    {
        sf::VertexArray glow(sf::Quads, 4);
        sf::Color gc = H_ACC[idx]; gc.a = 30;
        sf::Color gt = H_ACC[idx]; gt.a = 0;
        glow[0] = { { dx,        dy      }, gc };
        glow[1] = { { dx + 60.f, dy      }, gt };
        glow[2] = { { dx + 60.f, dy + dh }, gt };
        glow[3] = { { dx,        dy + dh }, gc };
        w.draw(glow);
    }

    float ch = dh * .52f;
    switch (idx)
    {
    case 0: covTron(w, dx, dy, dw, ch, totT); break;
    case 1: covFlappy(w, dx, dy, dw, ch, totT); break;
    case 2: covPong(w, dx, dy, dw, ch, totT); break;
    }

    hFR(w, dx, dy + ch, dw, 4, H_ACC[idx]);

    float ix = dx + 14, iw = dw - 28, iy = dy + ch + 11;
    { sf::Text t(G_TITLE[idx], f, sel ? 20 : 17); t.setFillColor(H_WHT); t.setPosition(ix, iy); w.draw(t); iy += 26; }
    { sf::Text t(G_SUB[idx], f, 11); t.setFillColor(sel ? H_GRY : H_DGRY); t.setPosition(ix, iy); w.draw(t); iy += 20; }
    hFR(w, ix, iy, iw, 1, H_DGRY); iy += 9;

    sf::Color dc = sel ? H_GRY : sf::Color(85, 85, 85);
    hTX(w, f, G_DA[idx], 11, ix, iy, dc); iy += 16;
    hTX(w, f, G_DB[idx], 11, ix, iy, dc); iy += 16;
    hTX(w, f, G_DC[idx], 11, ix, iy, dc); iy += 22;

    if (best >= 0)
    {
        hTX(w, f, std::string("BEST ") + G_LABA[idx] + ":", 10, ix, iy, H_DGRY);
        sf::Text vt(hIS(best), f, 14);
        vt.setFillColor(sel ? H_GOLD : sf::Color(150, 120, 0));
        auto vb = vt.getLocalBounds();
        vt.setPosition(dx + dw - vb.width - 14, iy - 1); w.draw(vt);
    }
    else { hTX(w, f, "No scores yet", 10, ix, iy, H_DGRY); }

    float tgW = 88, tgH = 18;
    hFR(w, dx + dw - tgW - 10, dy + dh - tgH - 10, tgW, tgH, H_ACK[idx]);
    hTX(w, f, "2 PLAYERS", 9, dx + dw - tgW * .5f - 10, dy + dh - tgH * .5f - 10, H_ACC[idx], true);

    sf::RectangleShape bdr({ dw, dh });
    bdr.setPosition(dx, dy); bdr.setFillColor(sf::Color::Transparent);
    if (sel)
    {
        float p = .8f + .2f * std::sin(totT * 2.5f);
        bdr.setOutlineColor({ (sf::Uint8)(H_GRN.r * p), (sf::Uint8)(H_GRN.g * p), (sf::Uint8)(H_GRN.b * p) });
        bdr.setOutlineThickness(3);
    }
    else { bdr.setOutlineColor({ 55, 55, 55, 180 }); bdr.setOutlineThickness(1); }
    w.draw(bdr);

    if (sel) hFR(w, dx + dw * .5f - 20, ty - 6, 40, 4, H_GRN);
}

// ─────────────────────────────────────────────────────────────
//  LEADERBOARD SCREEN
// ─────────────────────────────────────────────────────────────

static void drawLeaderboard(sf::RenderWindow& w, const sf::Font& f, int ag, float)
{
    hFR(w, 0, 52, WIN_W, WIN_H - 94, { 10, 10, 10, 210 });

    float tabW = WIN_W / 3.f;
    for (int i = 0; i < 3; i++)
    {
        bool  act = (i == ag); float tx = i * tabW;
        hFR(w, tx, 52, tabW, 42, act ? sf::Color(36, 36, 36, 230) : sf::Color(18, 18, 18, 200));
        if (act)   hFR(w, tx, 90, tabW, 4, H_ACC[i]);
        if (i > 0) hFR(w, tx, 52, 1, 42, H_DGRY);
        hTX(w, f, G_TITLE[i], act ? 15 : 13, tx + tabW * .5f, 72, act ? H_WHT : H_GRY, true);
    }

    float cy = 104;
    { sf::Text hdr(std::string("LEADERBOARD  -  ") + G_TITLE[ag], f, 24); hdr.setFillColor(H_ACC[ag]); hCT(hdr); hdr.setPosition(WIN_W * .5f, cy); w.draw(hdr); cy += 34; }
    hTX(w, f, std::string("Ranked by: ") + G_LABA[ag], 11, WIN_W * .5f, cy, H_GRY, true); cy += 26;

    float lx = WIN_W * .5f - 280, lw = 560;
    hFR(w, lx, cy, lw, 1, H_DGRY); cy += 6;
    hTX(w, f, "RANK", 11, lx + 14, cy, H_GRY);
    hTX(w, f, "PLAYER", 11, lx + 100, cy, H_GRY);
    hTX(w, f, "SCORE", 11, lx + 480, cy, H_GRY);
    cy += 20; hFR(w, lx, cy, lw, 1, H_DGRY); cy += 8;

    // Use DisplayLeaderboard() to fetch the top entries for this game
    ScoreEntry scores[10];
    int count = g_lb.DisplayLeaderboard(ag, scores, 10);

    if (count == 0)
    {
        hTX(w, f, "No scores yet -- play a game to get on the board!", 15, WIN_W * .5f, cy + 80, H_DGRY, true);
    }
    else
    {
        for (int i = 0; i < count; i++)
        {
            float ry = cy + i * 40;
            hFR(w, lx, ry, lw, 38, i % 2 == 0 ? sf::Color(22, 22, 22, 200) : sf::Color(14, 14, 14, 200));
            if (i < 3) { sf::Color mc = (i == 0) ? H_GOLD : (i == 1) ? H_SILV : H_BRNZ; hFR(w, lx, ry, 4, 38, mc); }
            sf::Color rc = (i == 0) ? H_GOLD : (i == 1) ? H_SILV : (i == 2) ? H_BRNZ : H_GRY;
            hTX(w, f, "#" + hIS(i + 1), i < 3 ? 17 : 13, lx + 18, ry + (i < 3 ? 8.f : 11.f), rc);
            hTX(w, f, scores[i].name, 15, lx + 100, ry + 10, i < 3 ? H_WHT : sf::Color(200, 200, 200));
            sf::Text sv(hIS(scores[i].score), f, 19); sv.setFillColor(H_ACC[ag]);
            auto sb = sv.getLocalBounds(); sv.setPosition(lx + lw - sb.width - 18, ry + 7); w.draw(sv);
        }
    }
    hTX(w, f, "[ESC] Back    [< / >] Switch Game", 11, WIN_W * .5f, WIN_H - 65, H_DGRY, true);
}

// ─────────────────────────────────────────────────────────────
//  NAME ENTRY SCREEN
// ─────────────────────────────────────────────────────────────

static bool runNameEntry(sf::RenderWindow& win, const sf::Font& font,
    sf::Sprite& bgSprite, bool bgLoaded,
    std::string& n1, std::string& n2)
{
    n1.clear(); n2.clear();
    int field = 0; float totT = 0;
    sf::Clock clk;

    while (win.isOpen())
    {
        float dt = clk.restart().asSeconds();
        totT += dt;

        sf::Event ev;
        while (win.pollEvent(ev))
        {
            if (ev.type == sf::Event::Closed) { win.close(); return false; }
            if (ev.type == sf::Event::TextEntered)
            {
                sf::Uint32 ch = ev.text.unicode;
                std::string& cur = (field == 0) ? n1 : n2;
                if (ch == 8 && !cur.empty()) cur.pop_back();
                else if (ch == 9) field = 1 - field;
                else if (ch == 13)
                {
                    if (field == 0 && !n1.empty()) field = 1;
                    else if (field == 1 && !n2.empty()) { if (n1.empty()) n1 = "Player 1"; return true; }
                }
                else if (ch >= 32 && ch < 127 && cur.size() < 20) cur += char(ch);
            }
            if (ev.type == sf::Event::KeyPressed)
            {
                if (ev.key.code == sf::Keyboard::Tab)    field = 1 - field;
                if (ev.key.code == sf::Keyboard::Escape) { win.close(); return false; }
                if (ev.key.code == sf::Keyboard::M && g_musicPtr) g_musicPtr->toggleMute();
                if (ev.key.code == sf::Keyboard::Enter)
                {
                    if (field == 0 && !n1.empty()) field = 1;
                    else if (field == 1 && !n2.empty()) { if (n1.empty()) n1 = "Player 1"; return true; }
                }
            }
        }

        bgParticlesUpdate(dt, totT);
        if (g_musicPtr) g_musicPtr->update(dt);   // fade volume each frame
        win.clear({ 8, 8, 8 });
        drawHubBackground(win, bgSprite, bgLoaded, totT);
        drawTopBar(win, font);

        float cx = WIN_W * .5f, midY = WIN_H * .5f;
        hFR(win, cx - 300, midY - 195, 600, 390, { 14,14,14,220 }, H_DGRY, 1.f);
        hFR(win, cx - 300, midY - 195, 600, 4, H_GRN);

        sf::Text titleTxt("ENTER PLAYER NAMES", font, 24);
        titleTxt.setFillColor(H_WHT); titleTxt.setLetterSpacing(3); hCT(titleTxt);
        titleTxt.setPosition(cx, midY - 160); win.draw(titleTxt);

        hTX(win, font, "Both players share this screen for all 3 games.", 11, cx, midY - 130, H_GRY, true);
        hFR(win, cx - 250, midY - 114, 500, 1, H_DGRY);

        auto drawField = [&](const std::string& lbl, const std::string& val, bool act, float fy, sf::Color accent)
            {
                hTX(win, font, lbl, 10, cx - 238, fy, act ? accent : H_GRY);
                hFR(win, cx - 238, fy + 18, 476, 46,
                    { (sf::Uint8)(act ? 28 : 16),(sf::Uint8)(act ? 28 : 16),(sf::Uint8)(act ? 28 : 16),230 },
                    act ? H_GRN : H_DGRY, act ? 2.f : 1.f);
                std::string disp = val;
                if (act) { disp += (std::sin(totT * 5.f) > 0 ? "|" : " "); }
                hTX(win, font, disp, 20, cx - 226, fy + 29, H_WHT);
                sf::CircleShape dot(5.f); dot.setOrigin(5, 5); dot.setPosition(cx - 252, fy + 41);
                dot.setFillColor(act ? accent : H_DGRY); win.draw(dot);
            };

        drawField("PLAYER 1  -  Tron: WASD  |  Flappy: SPACE  |  Pong: W/S", n1, field == 0, midY - 92, H_ACC[0]);
        drawField("PLAYER 2  -  Tron: ARROWS  |  Flappy: ENTER  |  Pong: UP/DOWN", n2, field == 1, midY + 8, H_ACC[2]);

        bool rdy = !n1.empty() && !n2.empty();
        float bw = 220, bh = 46, bx = cx - bw * .5f, by = midY + 86;
        hFR(win, bx, by, bw, bh, rdy ? H_GRN : sf::Color(35, 35, 35, 220), rdy ? H_GRNL : H_DGRY, 1.f);
        hTX(win, font, rdy ? "CONTINUE  [Enter]" : "Fill both names first", 15, cx, by + bh * .5f, rdy ? H_WHT : H_GRY, true);
        hTX(win, font, "Tab = switch   |   Enter = confirm   |   ESC = exit", 10, cx, midY + 150, H_DGRY, true);

        drawBotBar(win, font, HubSt::NAME_ENTRY);
        win.display();
    }
    return false;
}

// ─────────────────────────────────────────────────────────────
//  MAIN
// ─────────────────────────────────────────────────────────────

int main()
{
    std::srand((unsigned)std::time(nullptr));

    // Load leaderboard from disk on startup
    g_lb.LoadFromFile();

    sf::ContextSettings cs; cs.antialiasingLevel = 4;
    sf::RenderWindow window(sf::VideoMode(WIN_W, WIN_H),
        "Game Hub", sf::Style::Close | sf::Style::Titlebar, cs);
    window.setFramerateLimit(60);

    sf::Font font;
    bool fontOK = font.loadFromFile("font.ttf");
    if (!fontOK) fontOK = font.loadFromFile("arial.ttf");

    // ── Load background image ─────────────────────────────────
    // Tries several common filenames. Falls back to procedural if none found.
    sf::Texture bgTexture;
    bool        bgLoaded = false;

    const char* bgPaths[] = {
        "hub_bg.png", "hub_bg.jpg", "hub_bg.jpeg",
        "background.png", "background.jpg", "background.jpeg"
    };
    for (int i = 0; i < 6 && !bgLoaded; i++)
        bgLoaded = bgTexture.loadFromFile(bgPaths[i]);

    sf::Sprite bgSprite;
    if (bgLoaded)
    {
        bgTexture.setSmooth(true);
        bgSprite.setTexture(bgTexture);

        sf::Vector2u ts = bgTexture.getSize();

        // ── Rotate background image 90 degrees ───────────────
        // After rotation the image's original width becomes the
        // new height and vice versa, so we scale and position
        // based on the SWAPPED dimensions to fill the window.
        //
        // setRotation(90) rotates clockwise around the origin.
        // We set the origin to the top-left of the image, rotate,
        // then shift down by the scaled original-width so the
        // rotated image sits flush in the top-left corner.

        // Scale so the rotated image covers the full window.
        // After 90° rotation: image width maps to screen height,
        //                     image height maps to screen width.
        float scaleX = (float)WIN_H / ts.x;   // original width -> screen height
        float scaleY = (float)WIN_W / ts.y;   // original height -> screen width

        // Use the larger scale so the image always fills without gaps
        float sc = scaleX > scaleY ? scaleX : scaleY;

        bgSprite.setScale(sc, sc);
        bgSprite.setOrigin(0.f, 0.f);
        bgSprite.setRotation(90.f);

        // After clockwise 90° rotation the top-left corner moves to
        // the top-right of the bounding box.  Shift it back left by
        // the scaled height of the original image so it starts at (0,0).
        bgSprite.setPosition(ts.y * sc, 0.f);
    }

    bgParticlesInit();

    // ── Background music ──────────────────────────────────────
    // Loads bgg.mac and starts playing. Silent fail if missing.
    g_musicPtr = &g_hubMusic;
    g_hubMusic.load();
    g_hubMusic.play();

    // ── Name entry ────────────────────────────────────────────
    std::string p1Name, p2Name;
    if (!runNameEntry(window, font, bgSprite, bgLoaded, p1Name, p2Name))
        return 0;
    if (p2Name.empty()) p2Name = "Player 2";

    // ── Hub state ─────────────────────────────────────────────
    HubSt state = HubSt::MAIN;
    int   sel = 0;
    int   lbGame = 0;
    float totT = 0;

    static const float TW = 352, TH = 450, GAP = 28;
    float totalW = 3 * TW + 2 * GAP;
    float startX = (WIN_W - totalW) * .5f;
    float startY = 88 + (WIN_H - 42 - 88 - TH) * .5f + 8;

    sf::Clock clk;

    while (window.isOpen())
    {
        float dt = clk.restart().asSeconds();
        if (dt > .05f) dt = .05f;
        totT += dt;

        bgParticlesUpdate(dt, totT);
        g_hubMusic.update(dt);   // smooth volume fade every frame

        sf::Event ev;
        while (window.pollEvent(ev))
        {
            if (ev.type == sf::Event::Closed)
            {
                // Save leaderboard when window is closed
                g_lb.SaveToFile();
                window.close();
            }

            if (ev.type == sf::Event::KeyPressed)
            {
                auto k = ev.key.code;

                if (state == HubSt::MAIN)
                {
                    if (k == sf::Keyboard::Left)  sel = (sel + 2) % 3;
                    if (k == sf::Keyboard::Right)  sel = (sel + 1) % 3;
                    if (k == sf::Keyboard::M)      g_hubMusic.toggleMute();
                    if (k == sf::Keyboard::Escape)
                    {
                        g_lb.SaveToFile();
                        window.close();
                    }

                    if (k == sf::Keyboard::L || k == sf::Keyboard::X)
                    {
                        lbGame = sel; state = HubSt::LEADERBOARD;
                    }

                    if (k == sf::Keyboard::Enter || k == sf::Keyboard::Space)
                    {
                        // Pause hub music before launching the game
                        g_hubMusic.pause();

                        GameResult res;
                        switch (sel)
                        {
                        case 0: res = runTron(window, font);               break;
                        case 1: res = runFlappy(window, font);               break;
                        case 2: res = runPong(window, font, p1Name, p2Name); break;
                        }
                        if (!window.isOpen()) break;

                        // Fade hub music back in after returning from game
                        g_hubMusic.resume();

                        g_lb.AddRecord(p1Name, res.p1, sel);
                        g_lb.AddRecord(p2Name, res.p2, sel);
                        window.setTitle("Game Hub");
                    }
                }
                else if (state == HubSt::LEADERBOARD)
                {
                    if (k == sf::Keyboard::Escape) state = HubSt::MAIN;
                    if (k == sf::Keyboard::Left)   lbGame = (lbGame + 2) % 3;
                    if (k == sf::Keyboard::Right)  lbGame = (lbGame + 1) % 3;
                    if (k == sf::Keyboard::M)      g_hubMusic.toggleMute();
                }
            }
        }

        if (!window.isOpen()) break;

        window.clear({ 8, 8, 8 });
        drawHubBackground(window, bgSprite, bgLoaded, totT);
        drawTopBar(window, font);

        if (state == HubSt::MAIN)
        {
            hFR(window, 0, 52, WIN_W, 32, { 10,10,10,190 });
            hFR(window, 0, 83, WIN_W, 1, { 28,28,28 });

            {
                std::string nm = p1Name + " vs " + p2Name;
                sf::Text nm2(nm, font, 11); nm2.setFillColor(H_DGRY);
                auto nb = nm2.getLocalBounds();
                nm2.setPosition(WIN_W - nb.width - 18, 64); window.draw(nm2);
            }
            {
                sf::Text sub("SELECT  A  GAME", font, 12);
                sub.setFillColor(H_GRY); sub.setLetterSpacing(5); hCT(sub);
                sub.setPosition(WIN_W * .5f, 68); window.draw(sub);
            }

            for (int i = 0; i < 3; i++)
                drawTile(window, font, i, i == sel,
                    startX + i * (TW + GAP), startY, TW, TH, totT,
                    g_lb.GetBestScore(i));   // uses class method

            {
                std::string dots;
                for (int i = 0; i < 3; i++) dots += (i == sel) ? "  *  " : "  .  ";
                hTX(window, font, dots, 13, WIN_W * .5f, startY + TH + 12, H_DGRY, true);
            }
        }
        else if (state == HubSt::LEADERBOARD)
        {
            drawLeaderboard(window, font, lbGame, totT);
        }

        drawBotBar(window, font, state);
        window.display();
    }

    g_lb.SaveToFile();
    return 0;
}