// shooter_stages_colors_fatter_fighters_less_flat_day_night_toggle.cpp
// Same as your original file but with a simple day/night sky toggle using N key.

#include <windows.h>
#include <GL/glut.h>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <iostream>
#include <string>
#include <cstring>
#include <algorithm>

using namespace std;

const int WIDTH = 1000, HEIGHT = 700;

// Shared drawing scale controls
const float ENEMY_SCALE = 4.0f;
const float DIST_SCALE_NEAR = 1.0f;
const float DIST_SCALE_FAR = 1.6f;
const float DIST_FOR_MAX = 260.0f;

// Lock missile
const float LOCK_MISSILE_SPEED = 11.0f;
const float LOCK_MISSILE_RADIUSW = 1.0f;
const float LOCK_MISSILE_RADIUSH = 1.0f;
const float LOCK_MISSILE_RADIUSD = 2.2f;
const float LOCK_MISSILE_DRAW_W = 1.2f;
const float LOCK_MISSILE_DRAW_H = 1.2f;
const float LOCK_MISSILE_DRAW_D = 2.6f;

// Crash effect
const int   CRASH_MAX_FRAMES = 20;
const float CRASH_EXPAND_RATE = 0.18f;

// Stages
int   stageIndex = 1; // 1=fighters, 2=helicopters
float stageTimer = 0.0f;
const float STAGE_TIMEOUT = 90.0f;

// Game state
bool gameOver = false;
int  score = 0;

// --------- DAY / NIGHT MODE ----------
bool isNightMode = true;   // start with your current night sky

struct Player3 { float x, y, z; float yaw; float w, h, d; } player;

float playerSpeed = 0.6f;
float verticalSpeed = 0.6f;

struct Bullet3 { float x, y, z; float vx, vy, vz; bool active; };
vector<Bullet3> bullets;

// Enemy type: 0=fighter, 1=helicopter
struct Enemy3 {
    float x, y, z, w, h, d; // AABB
    bool alive;
    float speed;
    bool aggressive;
    int   type;
    float r, g, b;
};
vector<Enemy3> enemies;

struct Star3 { float x, y, z; };
vector<Star3> stars;

struct LockMissile { float x, y, z; float vx, vy, vz; int target; bool active; };
vector<LockMissile> lockMissiles;

struct CrashFx { float x, y, z; int frames; float size; bool active; };
vector<CrashFx> crashes;

// Environment colliders
struct AABB { float x, y, z, w, h, d; };
vector<AABB> buildingColliders;
vector<AABB> treeColliders;

float camDist = 12.0f, camHeight = 6.0f, camLookAhead = 6.0f;

bool keyState[256] = { 0 };
bool specialKeyState[256] = { 0 };

int  lockedIndex = -1;
bool keyToggleT = false;
const float LOCK_RANGE = 220.0f;
const float LOCK_COS_FOV = 0.85f;

// Rotor for helicopters
float rotorAngle = 0.0f;

// Intro overlay
bool showIntro = true;

// Forward decls
void drawCube(float x, float y, float z, float w, float h, float d, float r, float g, float b);
void drawFighterFlatColored(float x, float y, float z, float scale, float r, float g, float b);
void drawHelicopterColored(float x, float y, float z, float scale, float rotorDeg, float r, float g, float b);
void drawPlayerJet(float x, float y, float z, float yaw);
void drawStars3D();
void drawText2D(float x, float y, const string& text, float r = 1, float g = 1, float b = 1);
void drawLockReticle3D(float x, float y, float z);
void drawCrashFx(const CrashFx& fx);

// Environment forward decls
void drawGroundEnvironment();
void drawSkyText();
void drawIntroOverlayStroke();

// Utilities
float frand(float a, float b) { return a + float(rand()) / RAND_MAX * (b - a); }

bool aabbCollision(float ax, float ay, float az, float aw, float ah, float ad,
    float bx, float by, float bz, float bw, float bh, float bd) {
    return fabs(ax - bx) * 2.0f < (aw + bw) &&
        fabs(ay - by) * 2.0f < (ah + bh) &&
        fabs(az - bz) * 2.0f < (ad + bd);
}

static inline void forwardFromYaw(float yaw, float& fx, float& fz) { fx = sinf(yaw); fz = cosf(yaw); }

static inline void setVelocityToward(float sx, float sy, float sz,
    float tx, float ty, float tz,
    float speed, float& vx, float& vy, float& vz) {
    float dx = tx - sx, dy = ty - sy, dz = tz - sz; float L = sqrtf(dx * dx + dy * dy + dz * dz);
    if (L < 1e-4f) { vx = vy = vz = 0; return; }
    vx = dx / L * speed; vy = dy / L * speed; vz = dz / L * speed;
}

// Color palettes
struct RGB { float r, g, b; };
static const RGB LIGHTS[] = {
    {0.85f,0.95f,1.0f}, {1.0f,0.92f,0.8f}, {0.9f,0.9f,0.95f}, {0.85f,1.0f,0.85f}, {1.0f,0.85f,0.95f}
};
static const RGB BRIGHTS[] = {
    {1.0f,0.2f,0.2f}, {1.0f,0.8f,0.1f}, {0.2f,0.9f,1.0f}, {1.0f,0.4f,0.1f}, {0.6f,0.2f,1.0f}
};

void drawStars3D() {
    glDisable(GL_LIGHTING);
    glPointSize(2.0f);
    glBegin(GL_POINTS);
    glColor3f(1, 1, 1);
    for (const auto& s : stars) glVertex3f(s.x, s.y, s.z);
    glEnd();
    glEnable(GL_LIGHTING);
}

// Fighter model (fatter, deeper)
void drawFighterFlatColored(float x, float y, float z, float scale, float r, float g, float b) {
    glPushMatrix();
    glTranslatef(x, y, z);
    glScalef(scale, scale, scale * 1.50f);

    float r2 = min(1.0f, r * 0.8f), g2 = min(1.0f, g * 0.8f), b2 = min(1.0f, b * 0.8f);
    float r3 = min(1.0f, r * 1.1f), g3 = min(1.0f, g * 1.1f), b3 = min(1.0f, b * 1.1f);

    glColor3f(r3, g3, b3);
    glBegin(GL_TRIANGLES);
    glVertex3f(0.0f, 0.06f, -2.5f);
    glVertex3f(-0.32f, 0.06f, -1.45f);
    glVertex3f(0.32f, 0.06f, -1.45f);
    glEnd();

    glColor3f(r, g, b);
    glBegin(GL_TRIANGLES);
    glVertex3f(-0.74f, 0.02f, -0.92f); glVertex3f(-0.19f, 0.02f, -1.02f); glVertex3f(-0.19f, 0.02f, -0.48f);
    glVertex3f(0.74f, 0.02f, -0.92f); glVertex3f(0.19f, 0.02f, -1.02f); glVertex3f(0.19f, 0.02f, -0.48f);
    glEnd();

    glColor3f(r2, g2, b2);
    glBegin(GL_TRIANGLES);
    glVertex3f(-2.2f, 0.0f, 0.25f); glVertex3f(-0.24f, 0.0f, -0.1f); glVertex3f(-0.24f, 0.0f, 1.25f);
    glVertex3f(2.2f, 0.0f, 0.25f); glVertex3f(0.24f, 0.0f, -0.1f); glVertex3f(0.24f, 0.0f, 1.25f);
    glEnd();

    glColor3f(r * 0.6f, g * 0.6f, b * 0.6f);
    glBegin(GL_QUADS);
    glVertex3f(-0.30f, 0.12f, -1.35f);
    glVertex3f(0.30f, 0.12f, -1.35f);
    glVertex3f(0.40f, 0.06f, 1.55f);
    glVertex3f(-0.40f, 0.06f, 1.55f);
    glEnd();

    glColor3f(r3, g3, b3);
    glBegin(GL_TRIANGLES);
    glVertex3f(-0.32f, 0.82f, 1.10f); glVertex3f(-0.54f, 0.02f, 0.60f); glVertex3f(-0.10f, 0.02f, 0.74f);
    glVertex3f(0.32f, 0.82f, 1.10f); glVertex3f(0.54f, 0.02f, 0.60f); glVertex3f(0.10f, 0.02f, 0.74f);
    glEnd();

    glPopMatrix();
}

void drawHelicopterColored(float x, float y, float z, float scale, float rotorDeg, float r, float g, float b) {
    glPushMatrix(); glTranslatef(x, y, z); glScalef(scale, scale, scale);

    glColor3f(r * 0.5f, g * 0.5f, b * 0.5f);
    glBegin(GL_QUADS);
    glVertex3f(-0.6f, -0.25f, 0.9f); glVertex3f(-0.6f, 0.25f, 0.5f); glVertex3f(-0.6f, 0.25f, -0.9f); glVertex3f(-0.6f, -0.25f, -0.5f);
    glVertex3f(0.6f, -0.25f, 0.9f); glVertex3f(0.6f, 0.25f, 0.5f); glVertex3f(0.6f, 0.25f, -0.9f); glVertex3f(0.6f, -0.25f, -0.5f);
    glEnd();

    glColor3f(min(1.0f, r * 0.8f), min(1.0f, g * 0.8f), min(1.0f, b * 0.8f));
    glBegin(GL_TRIANGLES);
    glVertex3f(0.0f, 0.10f, -1.5f); glVertex3f(-0.35f, 0.10f, -0.9f); glVertex3f(0.35f, 0.10f, -0.9f);
    glEnd();

    glColor3f(0.35f, 0.35f, 0.35f);
    glBegin(GL_QUADS);
    glVertex3f(-0.7f, -0.35f, 0.6f); glVertex3f(0.7f, -0.35f, 0.6f); glVertex3f(0.7f, -0.35f, 0.5f); glVertex3f(-0.7f, -0.35f, 0.5f);
    glVertex3f(-0.7f, -0.35f, -0.2f); glVertex3f(0.7f, -0.35f, -0.2f); glVertex3f(0.7f, -0.35f, -0.3f); glVertex3f(-0.7f, -0.35f, -0.3f);
    glEnd();

    glPushMatrix(); glTranslatef(0.0f, 0.35f, 0.0f); glRotatef(rotorDeg, 0, 1, 0);
    glColor3f(0.95f, 0.95f, 0.95f);
    glBegin(GL_QUADS);
    glVertex3f(-1.6f, 0.0f, -0.05f); glVertex3f(1.6f, 0.0f, -0.05f); glVertex3f(1.6f, 0.0f, 0.05f); glVertex3f(-1.6f, 0.0f, 0.05f);
    glEnd(); glPopMatrix();

    glPushMatrix(); glTranslatef(0.0f, 0.05f, 1.7f); glRotatef(rotorDeg * 2.0f, 0, 0, 1);
    glColor3f(0.95f, 0.95f, 0.95f);
    glBegin(GL_QUADS);
    glVertex3f(-0.35f, 0.0f, -0.02f); glVertex3f(0.35f, 0.0f, -0.02f); glVertex3f(0.35f, 0.0f, 0.02f); glVertex3f(-0.35f, 0.0f, 0.02f);
    glEnd(); glPopMatrix();

    glPopMatrix();
}

// Player jet
void drawPlayerJet(float x, float y, float z, float yaw) {
    glPushMatrix(); glTranslatef(x, y, z); glRotatef(yaw * 180.0f / 3.1415926f, 0, 1, 0);
    glColor3f(0.22f, 0.42f, 0.95f);
    glBegin(GL_QUADS);
    glVertex3f(-0.45f, -0.20f, 1.4f); glVertex3f(-0.45f, 0.10f, 1.0f); glVertex3f(-0.45f, 0.10f, -1.6f); glVertex3f(-0.45f, -0.20f, -1.2f);
    glVertex3f(0.45f, -0.20f, 1.4f); glVertex3f(0.45f, 0.10f, 1.0f); glVertex3f(0.45f, 0.10f, -1.6f); glVertex3f(0.45f, -0.20f, -1.2f);
    glEnd();
    glColor3f(0.25f, 0.5f, 1.0f);
    glBegin(GL_TRIANGLES);
    glVertex3f(0.0f, 0.05f, -2.3f); glVertex3f(-0.30f, 0.05f, -1.6f); glVertex3f(0.30f, 0.05f, -1.6f);
    glEnd();
    glColor3f(0.85f, 0.90f, 1.0f);
    glBegin(GL_TRIANGLES);
    glVertex3f(0.0f, 0.32f, -0.9f); glVertex3f(-0.22f, 0.10f, -1.1f); glVertex3f(0.22f, 0.10f, -1.1f);
    glEnd();
    glColor3f(0.18f, 0.36f, 0.82f);
    glBegin(GL_TRIANGLES);
    glVertex3f(-1.6f, 0.0f, -0.2f); glVertex3f(-0.15f, 0.0f, -0.4f); glVertex3f(-0.15f, 0.0f, 0.8f);
    glVertex3f(1.6f, 0.0f, -0.2f); glVertex3f(0.15f, 0.0f, -0.4f); glVertex3f(0.15f, 0.0f, 0.8f);
    glEnd();
    glColor3f(0.16f, 0.33f, 0.75f);
    glBegin(GL_TRIANGLES);
    glVertex3f(0.0f, 0.8f, 1.0f); glVertex3f(-0.16f, 0.02f, 0.6f); glVertex3f(0.16f, 0.02f, 0.6f);
    glEnd(); glPopMatrix();
}

void drawCube(float x, float y, float z, float w, float h, float d, float r, float g, float b) {
    glPushMatrix(); glTranslatef(x, y, z); glScalef(w, h, d); glColor3f(r, g, b); glutSolidCube(1.0f); glPopMatrix();
}

void drawText2D(float x, float y, const string& text, float r, float g, float b) {
    glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity(); gluOrtho2D(0, WIDTH, 0, HEIGHT);
    glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();
    glDisable(GL_LIGHTING); glColor3f(r, g, b); glRasterPos2f(x, y);
    for (char c : text) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, c);
    glEnable(GL_LIGHTING); glPopMatrix(); glMatrixMode(GL_PROJECTION); glPopMatrix(); glMatrixMode(GL_MODELVIEW);
}

void drawLockReticle3D(float x, float y, float z) {
    glDisable(GL_LIGHTING); glLineWidth(2.0f); glColor3f(1.0f, 1.0f, 0.2f);
    glBegin(GL_LINE_LOOP); float s = 1.6f;
    glVertex3f(x - s, y + s, z); glVertex3f(x + s, y + s, z);
    glVertex3f(x + s, y - s, z); glVertex3f(x - s, y - s, z);
    glEnd(); glEnable(GL_LIGHTING);
}

void drawCrashFx(const CrashFx& fx) {
    if (!fx.active) return;
    glDisable(GL_LIGHTING); glLineWidth(3.0f); glColor3f(1.0f, 0.8f, 0.2f);
    float s = fx.size; glBegin(GL_LINES);
    glVertex3f(fx.x - s, fx.y, fx.z); glVertex3f(fx.x + s, fx.y, fx.z);
    glVertex3f(fx.x, fx.y - s, fx.z); glVertex3f(fx.x, fx.y + s, fx.z);
    glEnd(); glEnable(GL_LIGHTING);
}

// Lighting
void setupLights() {
    glEnable(GL_LIGHTING); glEnable(GL_LIGHT0);
    float ambient[] = { 0.2f,0.2f,0.25f,1 };
    float diffuse[] = { 0.95f,0.95f,0.95f,1 };
    float specular[] = { 0.6f,0.6f,0.6f,1 };
    float pos[] = { 50,120,50,1 };
    glLightfv(GL_LIGHT0, GL_AMBIENT, ambient);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuse);
    glLightfv(GL_LIGHT0, GL_SPECULAR, specular);
    glLightfv(GL_LIGHT0, GL_POSITION, pos);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT, GL_AMBIENT_AND_DIFFUSE);
    float spec[] = { 0.2f,0.2f,0.2f,1 };
    glMaterialfv(GL_FRONT, GL_SPECULAR, spec);
    glMateriali(GL_FRONT, GL_SHININESS, 16);
}

// Aggressors
void updateAggressors() {
    const int MAX_AGGRESSORS = 4;
    for (auto& e : enemies) e.aggressive = false;
    vector<pair<float, int>> order; order.reserve(enemies.size());
    for (size_t i = 0; i < enemies.size(); ++i) {
        if (!enemies[i].alive) continue;
        float dz = enemies[i].z - player.z; if (dz < -20.f) continue;
        float dx = enemies[i].x - player.x, dy = enemies[i].y - player.y, d = sqrtf(dx * dx + dy * dy + dz * dz);
        order.push_back({ d,(int)i });
    }
    sort(order.begin(), order.end(), [](const pair<float, int>& a, const pair<float, int>& b) { return a.first < b.first; });
    for (int i = 0; i < (int)order.size() && i < MAX_AGGRESSORS; ++i) enemies[order[i].second].aggressive = true;
}

// ---------- Environment helpers ----------
static void drawQuadTop(float x, float y, float z, float w, float d, float r, float g, float b) {
    glDisable(GL_LIGHTING);
    glColor3f(r, g, b);
    glBegin(GL_QUADS);
    glVertex3f(x - w * 0.5f, y, z - d * 0.5f);
    glVertex3f(x + w * 0.5f, y, z - d * 0.5f);
    glVertex3f(x + w * 0.5f, y, z + d * 0.5f);
    glVertex3f(x - w * 0.5f, y, z + d * 0.5f);
    glEnd();
    glEnable(GL_LIGHTING);
}

static void drawPrismBox(float x, float y, float z, float w, float h, float d, float r, float g, float b) {
    glPushMatrix(); glTranslatef(x, y + h * 0.5f, z); glScalef(w, h, d); glColor3f(r, g, b);
    glBegin(GL_QUADS);
    // +X
    glVertex3f(0.5f, -0.5f, -0.5f); glVertex3f(0.5f, 0.5f, -0.5f); glVertex3f(0.5f, 0.5f, 0.5f); glVertex3f(0.5f, -0.5f, 0.5f);
    // -X
    glVertex3f(-0.5f, -0.5f, -0.5f); glVertex3f(-0.5f, -0.5f, 0.5f); glVertex3f(-0.5f, 0.5f, 0.5f); glVertex3f(-0.5f, 0.5f, -0.5f);
    // +Z
    glVertex3f(-0.5f, -0.5f, 0.5f); glVertex3f(0.5f, -0.5f, 0.5f); glVertex3f(0.5f, 0.5f, 0.5f); glVertex3f(-0.5f, 0.5f, 0.5f);
    // -Z
    glVertex3f(-0.5f, -0.5f, -0.5f); glVertex3f(-0.5f, 0.5f, -0.5f); glVertex3f(0.5f, 0.5f, -0.5f); glVertex3f(0.5f, -0.5f, -0.5f);
    // +Y
    glVertex3f(-0.5f, 0.5f, -0.5f); glVertex3f(-0.5f, 0.5f, 0.5f); glVertex3f(0.5f, 0.5f, 0.5f); glVertex3f(0.5f, 0.5f, -0.5f);
    // -Y
    glVertex3f(-0.5f, -0.5f, -0.5f); glVertex3f(0.5f, -0.5f, -0.5f); glVertex3f(0.5f, -0.5f, 0.5f); glVertex3f(-0.5f, -0.5f, 0.5f);
    glEnd(); glPopMatrix();
}

static void drawTree(float x, float z, float h = 10.0f) {
    GLUquadric* q = gluNewQuadric();
    float trunkH = h * 0.4f;
    float crownH = h * 0.7f;
    glPushMatrix();
    glTranslatef(x, 0.0f, z);
    // Trunk
    glColor3f(0.35f, 0.2f, 0.05f);
    glPushMatrix();
    glTranslatef(0.0f, trunkH * 0.5f, 0.0f);
    glRotatef(-90.0f, 1, 0, 0);
    gluCylinder(q, 0.6f, 0.5f, trunkH, 10, 1);
    glPopMatrix();
    // Crown cone
    glColor3f(0.12f, 0.45f, 0.12f);
    glPushMatrix();
    glTranslatef(0.0f, trunkH + crownH * 0.5f, 0.0f);
    glRotatef(-90.0f, 1, 0, 0);
    glutSolidCone(3.2f, crownH, 10, 2);
    glPopMatrix();
    glPopMatrix();
    gluDeleteQuadric(q);
    // Add collider
    treeColliders.push_back(AABB{ x, (trunkH + crownH) * 0.5f, z, 3.6f, trunkH + crownH, 3.6f });
}

// Billboard stroke text facing camera (no mirror)
static void drawStrokeTextFacingCamera(const char* txt, float wx, float wy, float wz, float scale, const float camX, const float camZ) {
    float dx = camX - wx;
    float dz = camZ - wz;
    float yawDeg = atan2f(dx, dz) * 180.0f / 3.1415926f;
    glPushMatrix();
    glDisable(GL_LIGHTING);
    glTranslatef(wx, wy, wz);
    glRotatef(yawDeg, 0, 1, 0);
    glScalef(scale, scale, scale);
    glColor3f(0.98f, 0.95f, 0.2f);
    float width = 0.0f; for (const char* p = txt; *p; ++p) width += glutStrokeWidth(GLUT_STROKE_ROMAN, *p);
    glTranslatef(-width * 0.5f, 0.0f, 0.0f);
    glLineWidth(2.5f);
    for (const char* p = txt; *p; ++p) glutStrokeCharacter(GLUT_STROKE_ROMAN, *p);
    glEnable(GL_LIGHTING);
    glPopMatrix();
}

// Building with varied size/color and collider
struct BuildSpec { float x, z, w, h, d; float r, g, b; };
static void addBuildingSpec(const BuildSpec& bs) {
    drawPrismBox(bs.x, 0.0f, bs.z, bs.w, bs.h, bs.d, bs.r, bs.g, bs.b);
    buildingColliders.push_back(AABB{ bs.x, bs.h * 0.5f, bs.z, bs.w, bs.h, bs.d });
}

// Generate varied building color (cool and warm tints)
static RGB varyColor(const RGB& base, float t) {
    RGB c;
    c.r = min(1.0f, max(0.0f, base.r * (0.70f + 0.6f * t)));
    c.g = min(1.0f, max(0.0f, base.g * (0.70f + 0.6f * (1.0f - t))));
    c.b = min(1.0f, max(0.0f, base.b * (0.70f + 0.6f * (0.3f + 0.7f * t))));
    return c;
}

// Draw one repeated environment segment centered around z0 (no road, just grass)
static void drawEnvironmentSegment(float z0) {
    // Ground
    drawQuadTop(0.0f, 0.0f, z0, 360.0f, 360.0f, 0.12f, 0.30f, 0.12f);

    // Subtle grid
    glDisable(GL_LIGHTING);
    glLineWidth(1.0f); glColor3f(0.16f, 0.40f, 0.16f);
    glBegin(GL_LINES);
    for (int i = -7; i <= 7; ++i) {
        float x = i * 26.0f;
        glVertex3f(x, 0.01f, z0 - 180.0f); glVertex3f(x, 0.01f, z0 + 180.0f);
    }
    for (int j = -7; j <= 7; ++j) {
        float z = z0 + j * 26.0f;
        glVertex3f(-140.0f, 0.01f, z); glVertex3f(140.0f, 0.01f, z);
    }
    glEnd();
    glEnable(GL_LIGHTING);

    // Trees
    for (int i = -2; i <= 2; ++i) {
        float baseZ = z0 + i * 60.0f;
        drawTree(50.0f, baseZ - 8.0f, 11.0f);
        drawTree(-55.0f, baseZ + 8.0f, 13.0f);
        drawTree(80.0f, baseZ + 22.0f, 14.0f);
        drawTree(-85.0f, baseZ - 20.0f, 12.0f);
    }

    // Buildings: varied sizes and different colors, |x| >= 35
    for (int k = 0; k < 6; ++k) {
        float side = (k % 2 == 0 ? -1.0f : 1.0f);
        float bx = side * (60.0f + (k * 10.0f));
        float bz = z0 + (-70.0f + k * 28.0f);
        float w = 14.0f + (k % 3) * 6.0f;
        float h = 24.0f + (k % 4) * 8.0f;
        float d = 14.0f + ((k + 1) % 3) * 6.0f;
        RGB base = (k % 3 == 0 ? RGB{ 0.28f,0.32f,0.70f }
            : (k % 3 == 1 ? RGB{ 0.70f,0.30f,0.28f }
        : RGB{ 0.30f,0.65f,0.35f }));
        float t = 0.15f * (k + 1);
        RGB vc = varyColor(base, t);
        addBuildingSpec(BuildSpec{ bx, bz, w, h, d, vc.r, vc.g, vc.b });
    }
}

// Repeat segments around the player to create infinite feel and rebuild colliders each frame
void drawGroundEnvironment() {
    buildingColliders.clear();
    treeColliders.clear();
    float seg = 180.0f;
    int center = int(floor(player.z / seg));
    for (int i = -2; i <= 3; ++i) {
        float z0 = (center + i) * seg;
        drawEnvironmentSegment(z0);
    }
}

// Draw sky text ahead of player, facing the camera, not mirrored
void drawSkyText() {
    const char* phrase = "Bangladesh university of Business and technology";
    float fx, fz; forwardFromYaw(player.yaw, fx, fz);
    float ahead = 200.0f;
    float sx = player.x + fx * ahead;
    float sz = player.z + fz * ahead;
    float sy = max(40.0f, player.y + 22.0f);
    float camX = player.x - fx * camDist;
    float camZ = player.z - fz * camDist;
    drawStrokeTextFacingCamera(phrase, sx, sy, sz, 0.055f, camX, camZ);
}

// ---------- Spawns ----------
RGB pickLight() { int i = rand() % (int)(sizeof(LIGHTS) / sizeof(LIGHTS[0]));  return LIGHTS[i]; }
RGB pickBright() { int i = rand() % (int)(sizeof(BRIGHTS) / sizeof(BRIGHTS[0])); return BRIGHTS[i]; }

// Higher enemy altitudes
void spawnFighterWave(int count) {
    enemies.clear();
    for (int i = 0; i < count; ++i) {
        Enemy3 e;
        e.w = 10.2f; e.h = 7.8f; e.d = 10.6f;
        e.x = frand(-70.f, 70.f);
        e.y = frand(16.0f, 36.0f);
        e.z = frand(100.f + i * 28.f, 180.f + i * 28.f);
        e.alive = true;
        e.speed = frand(0.45f, 1.10f);
        e.aggressive = false;
        e.type = 0;
        RGB c = (i % 2 == 0 ? pickLight() : pickBright());
        e.r = c.r; e.g = c.g; e.b = c.b;
        enemies.push_back(e);
    }
}

void spawnHeliWave(int count) {
    enemies.clear();
    for (int i = 0; i < count; ++i) {
        Enemy3 e;
        e.w = 3.8f; e.h = 2.6f; e.d = 3.8f;
        e.x = frand(-60.f, 60.f);
        e.y = frand(18.0f, 38.0f);
        e.z = frand(130.f + i * 32.f, 220.f + i * 32.f);
        e.alive = true;
        e.speed = frand(0.25f, 0.6f);
        e.aggressive = false;
        e.type = 1;
        RGB c = pickBright();
        e.r = c.r; e.g = c.g; e.b = c.b;
        enemies.push_back(e);
    }
}

bool allEnemiesDown() { for (auto& e : enemies) if (e.alive) return false; return true; }

// ---------- Intro overlay ----------
static void drawCenteredStrokeLine(const char* txt, float cx, float cy, float scale, float r, float g, float b) {
    glPushMatrix();
    glDisable(GL_LIGHTING);
    glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity(); gluOrtho2D(0, WIDTH, 0, HEIGHT);
    glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();
    glTranslatef(cx, cy, 0.0f);
    glScalef(scale, scale, 1.0f);
    glColor3f(r, g, b);
    float width = 0.0f; for (const char* p = txt; *p; ++p) width += glutStrokeWidth(GLUT_STROKE_ROMAN, *p);
    glTranslatef(-width * 0.5f, 0.0f, 0.0f);
    glLineWidth(3.2f);
    for (const char* p = txt; *p; ++p) glutStrokeCharacter(GLUT_STROKE_ROMAN, *p);
    glPopMatrix(); glMatrixMode(GL_PROJECTION); glPopMatrix(); glMatrixMode(GL_MODELVIEW);
    glEnable(GL_LIGHTING);
    glPopMatrix();
}

void drawIntroOverlayStroke() {
    if (!showIntro) return;
    float cx = WIDTH * 0.5f;
    float cy = HEIGHT * 0.65f;
    float s1 = 0.38f; // title
    float s2 = 0.30f; // names
    float s3 = 0.32f; // dept

    drawCenteredStrokeLine("Authors:", cx, cy + 120, s1, 1.0f, 1.0f, 1.0f);

    drawCenteredStrokeLine("Al-Amin", cx, cy + 80, s2, 1.00f, 0.95f, 0.20f);
    drawCenteredStrokeLine("Nowrin", cx, cy + 50, s2, 1.00f, 0.85f, 0.25f);
    drawCenteredStrokeLine("Atik", cx, cy + 20, s2, 1.00f, 0.70f, 0.30f);
    drawCenteredStrokeLine("Nasim", cx, cy - 10, s2, 1.00f, 0.55f, 0.30f);
    drawCenteredStrokeLine("Mahadi", cx, cy - 40, s2, 0.98f, 0.40f, 0.30f);

    drawCenteredStrokeLine("Dept of CSE,BUBT", cx, cy - 90, s3, 0.95f, 0.90f, 0.20f);

    drawCenteredStrokeLine("Press any control (W/S/Arrows/Space/T/N) to start", cx, cy - 140, 0.24f, 0.9f, 0.95f, 1.0f);
}

// ---------- Init and game loop ----------
void initGame() {
    srand((unsigned)time(nullptr));
    score = 0; gameOver = false; lockedIndex = -1; showIntro = true;
    bullets.clear(); enemies.clear(); stars.clear(); lockMissiles.clear(); crashes.clear();
    buildingColliders.clear(); treeColliders.clear();
    stageIndex = 1; stageTimer = 0.0f; rotorAngle = 0.0f;

    player.x = 0; player.y = 6.0f; player.z = 0;
    player.w = 1.8f; player.h = 2.0f; player.d = 2.8f; player.yaw = 0.0f;

    for (int i = 0; i < 600; ++i)
        stars.push_back(Star3{ frand(-300.f,300.f), frand(10.f,160.f), frand(-220.f,620.f) });

    spawnFighterWave(12);

    memset(keyState, 0, sizeof(keyState));
    memset(specialKeyState, 0, sizeof(specialKeyState));
}

// ---------- Display ----------
void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    float fx, fz; forwardFromYaw(player.yaw, fx, fz);
    float camX = player.x - fx * camDist, camY = player.y + camHeight, camZ = player.z - fz * camDist;
    float lookX = player.x + fx * camLookAhead, lookY = player.y, lookZ = player.z + fz * camLookAhead;

    glMatrixMode(GL_MODELVIEW); glLoadIdentity(); gluLookAt(camX, camY, camZ, lookX, lookY, lookZ, 0, 1, 0);

    drawGroundEnvironment();
    drawSkyText();
    drawStars3D();

    // Enemies
    for (size_t i = 0; i < enemies.size(); ++i) {
        const auto& e = enemies[i]; if (!e.alive) continue;
        float dx = e.x - player.x, dy = e.y - player.y, dz = e.z - player.z;
        float dist = sqrtf(dx * dx + dy * dy + dz * dz);
        float t = fminf(fmaxf(dist / DIST_FOR_MAX, 0.0f), 1.0f);
        float addScale = DIST_SCALE_NEAR + (DIST_SCALE_FAR - DIST_SCALE_NEAR) * t;

        if (e.type == 0) {
            drawFighterFlatColored(e.x, e.y, e.z, ENEMY_SCALE * addScale, e.r, e.g, e.b);
        }
        else {
            drawHelicopterColored(e.x, e.y, e.z, ENEMY_SCALE * 1.3f * addScale, rotorAngle, e.r, e.g, e.b);
        }
        if ((int)i == lockedIndex) drawLockReticle3D(e.x, e.y, e.z);
    }

    // Projectiles and FX
    for (const auto& b : bullets) { if (!b.active) continue; drawCube(b.x, b.y, b.z, 0.6f, 0.6f, 1.6f, 1.0f, 0.95f, 0.4f); }
    for (const auto& m : lockMissiles) { if (!m.active) continue; drawCube(m.x, m.y, m.z, LOCK_MISSILE_DRAW_W, LOCK_MISSILE_DRAW_H, LOCK_MISSILE_DRAW_D, 1.0f, 0.4f, 0.1f); }
    for (const auto& fx : crashes) { if (fx.active) drawCrashFx(fx); }

    // Player
    drawPlayerJet(player.x, player.y, player.z, player.yaw);

    // HUD
    string stageText = (stageIndex == 1 ? "Stage 1: Less-Flat Fighters" : "Stage 2: Attack Helicopters");
    drawText2D(10, HEIGHT - 30, "Score: " + to_string(score));
    drawText2D(10, HEIGHT - 55, "Arrows Turn/Ascend/Descend  W/S Forward/Back  Space Shoot  T Lock/Unlock  N Day/Night  R Restart");
    drawText2D(WIDTH - 320, HEIGHT - 30, stageText);

    if (gameOver) {
        drawText2D(WIDTH / 2 - 80, HEIGHT / 2 + 20, "GAME OVER!", 1, 0.2f, 0.2f);
        drawText2D(WIDTH / 2 - 120, HEIGHT / 2 - 10, "Press R to Restart", 1, 1, 1);
    }

    drawIntroOverlayStroke();

    glutSwapBuffers();
}

// ---------- Update ----------
void update(int) {
    if (!gameOver) {
        for (auto& s : stars) {
            s.z -= 0.6f;
            if (s.z < -300.0f) { s.x = frand(-300.f, 300.f); s.y = frand(5.f, 160.f); s.z = frand(200.f, 700.f); }
        }

        const float yawSpeed = 0.045f;
        if (specialKeyState[GLUT_KEY_LEFT]) { player.yaw -= yawSpeed; showIntro = false; }
        if (specialKeyState[GLUT_KEY_RIGHT]) { player.yaw += yawSpeed; showIntro = false; }
        if (specialKeyState[GLUT_KEY_UP]) { player.y += verticalSpeed; showIntro = false; }
        if (specialKeyState[GLUT_KEY_DOWN]) { player.y -= verticalSpeed; showIntro = false; }
        if (player.y < 0.5f) player.y = 0.5f; if (player.y > 80.0f) player.y = 80.0f;

        float fx, fz; forwardFromYaw(player.yaw, fx, fz);
        float dx = 0, dz = 0;
        if (keyState['w'] || keyState['W']) { dx += fx * playerSpeed; dz += fz * playerSpeed; showIntro = false; }
        if (keyState['s'] || keyState['S']) { dx -= fx * playerSpeed * 0.8f; dz -= fz * playerSpeed * 0.8f; showIntro = false; }
        player.x += dx; player.z += dz;

        rotorAngle += 12.0f; if (rotorAngle > 360.0f) rotorAngle -= 360.0f;

        updateAggressors();

        // Bullets vs enemies
        for (auto& b : bullets) {
            if (!b.active) continue;
            b.x += b.vx; b.y += b.vy; b.z += b.vz;
            if (fabs(b.x) > 1200 || fabs(b.y) > 500 || fabs(b.z) > 1200) b.active = false;
            for (size_t i = 0; i < enemies.size(); ++i) {
                auto& e = enemies[i]; if (!e.alive) continue;
                if (aabbCollision(b.x, b.y, b.z, 0.6f, 0.6f, 1.2f, e.x, e.y, e.z, e.w, e.h, e.d)) {
                    e.alive = false; b.active = false; score += (e.type == 0 ? 12 : 15);
                    crashes.push_back(CrashFx{ e.x,e.y,e.z, CRASH_MAX_FRAMES, (e.type == 0 ? 0.5f : 0.8f), true });
                    if ((int)i == lockedIndex) lockedIndex = -1;
                }
            }
        }

        // Lock missiles home
        for (auto& m : lockMissiles) {
            if (!m.active) continue; int ti = m.target;
            if (ti < 0 || ti >= (int)enemies.size() || !enemies[ti].alive) {
                m.x += m.vx; m.y += m.vy; m.z += m.vz;
            }
            else {
                float nvx, nvy, nvz; setVelocityToward(m.x, m.y, m.z, enemies[ti].x, enemies[ti].y, enemies[ti].z, LOCK_MISSILE_SPEED, nvx, nvy, nvz);
                m.vx = nvx; m.vy = nvy; m.vz = nvz; m.x += m.vx; m.y += m.vy; m.z += m.vz;
                auto& e = enemies[ti];
                if (aabbCollision(m.x, m.y, m.z, LOCK_MISSILE_RADIUSW, LOCK_MISSILE_RADIUSH, LOCK_MISSILE_RADIUSD, e.x, e.y, e.z, e.w, e.h, e.d)) {
                    e.alive = false; m.active = false; score += (e.type == 0 ? 22 : 25);
                    crashes.push_back(CrashFx{ e.x,e.y,e.z, CRASH_MAX_FRAMES, (e.type == 0 ? 0.8f : 1.0f), true });
                    if (lockedIndex == ti) lockedIndex = -1;
                }
            }
            if (fabs(m.x) > 1400 || fabs(m.y) > 700 || fabs(m.z) > 1400) m.active = false;
        }

        // Crash FX
        for (auto& fx : crashes) { if (!fx.active) continue; fx.frames -= 1; fx.size += CRASH_EXPAND_RATE; if (fx.frames <= 0) fx.active = false; }

        // Enemy movement and constraints vs buildings
        for (auto& e : enemies) {
            if (!e.alive) continue;
            e.z -= e.speed;

            for (const auto& bcol : buildingColliders) {
                bool overlapXZ = (fabs(e.x - bcol.x) * 2.0f < (e.w + bcol.w)) &&
                    (fabs(e.z - bcol.z) * 2.0f < (e.d + bcol.d));
                if (overlapXZ) {
                    float minY = bcol.y + bcol.h * 0.5f + e.h * 0.5f + 0.5f;
                    if (e.y < minY) e.y = minY;
                }
            }

            if (e.y < 14.0f) e.y = 14.0f;

            if (e.z < player.z - 60.0f) {
                e.x = frand(-70.f, 70.f);
                e.z = frand(player.z + 120.f, player.z + 420.f);
                e.y = (e.type == 0 ? frand(16.0f, 36.0f) : frand(18.0f, 38.0f));
                e.speed = (e.type == 0 ? frand(0.45f, 1.10f) : frand(0.25f, 0.6f));
                e.alive = true;
            }

            if (aabbCollision(player.x, player.y, player.z, player.w, player.h, player.d, e.x, e.y, e.z, e.w, e.h, e.d)) {
                gameOver = true; std::cout << "GAME OVER!\n";
            }
        }

        // Player collision with buildings/trees: destroy on touch
        for (const auto& bc : buildingColliders) {
            if (aabbCollision(player.x, player.y, player.z, player.w, player.h, player.d, bc.x, bc.y, bc.z, bc.w, bc.h, bc.d)) {
                crashes.push_back(CrashFx{ player.x, player.y, player.z, CRASH_MAX_FRAMES, 1.0f, true });
                gameOver = true;
            }
        }
        for (const auto& tc : treeColliders) {
            if (aabbCollision(player.x, player.y, player.z, player.w, player.h, player.d, tc.x, tc.y, tc.z, tc.w, tc.h, tc.d)) {
                crashes.push_back(CrashFx{ player.x, player.y, player.z, CRASH_MAX_FRAMES, 0.8f, true });
                gameOver = true;
            }
        }

        // Stage progression
        stageTimer += 0.016f;
        if (allEnemiesDown() || stageTimer > STAGE_TIMEOUT) {
            stageTimer = 0.0f;
            if (stageIndex == 1) { stageIndex = 2; spawnHeliWave(8); lockedIndex = -1; }
            else { stageIndex = 1; int count = 14 + (score / 100); if (count > 24) count = 24; spawnFighterWave(count); lockedIndex = -1; }
        }

        // Cull
        if (bullets.size() > 400) { vector<Bullet3> tmp; for (auto& b : bullets) if (b.active) tmp.push_back(b); bullets.swap(tmp); }
        if (lockMissiles.size() > 400) { vector<LockMissile> tmp; for (auto& m : lockMissiles) if (m.active) tmp.push_back(m); lockMissiles.swap(tmp); }
        if (crashes.size() > 200) { vector<CrashFx> tmp; for (auto& c : crashes) if (c.active) tmp.push_back(c); crashes.swap(tmp); }
    }
    glutPostRedisplay(); glutTimerFunc(16, update, 0);
}

// ---------- Input ----------
int findLockCandidate();
void keyboardDown(unsigned char key, int, int) {
    keyState[key] = true; if (key == 27) exit(0);
    if ((key == 't' || key == 'T') && !keyToggleT) { keyToggleT = true; if (lockedIndex >= 0) lockedIndex = -1; else lockedIndex = findLockCandidate(); showIntro = false; }

    // DAY / NIGHT TOGGLE
    if (key == 'n' || key == 'N') {
        isNightMode = !isNightMode;
        if (isNightMode) {
            // your original dark / night sky
            glClearColor(0.02f, 0.03f, 0.07f, 1.0f);
        }
        else {
            // brighter daytime sky (light blue)
            glClearColor(0.52f, 0.78f, 0.95f, 1.0f);
        }
        showIntro = false;
    }

    if (key == ' ' && !gameOver) {
        showIntro = false;
        if (lockedIndex >= 0 && lockedIndex < (int)enemies.size() && enemies[lockedIndex].alive) {
            LockMissile m; m.x = player.x; m.y = player.y; m.z = player.z; m.target = lockedIndex; m.active = true;
            setVelocityToward(m.x, m.y, m.z, enemies[m.target].x, enemies[m.target].y, enemies[m.target].z, LOCK_MISSILE_SPEED, m.vx, m.vy, m.vz);
            lockMissiles.push_back(m);
        }
        else {
            float fx, fz; forwardFromYaw(player.yaw, fx, fz);
            Bullet3 b; b.x = player.x + fx * (player.d / 2.0f + 1.2f); b.y = player.y; b.z = player.z + fz * (player.d / 2.0f + 1.2f);
            float speed = 10.0f; b.vx = fx * speed; b.vy = 0.0f; b.vz = fz * speed; b.active = true; bullets.push_back(b);
        }
    }
    if (key == 'r' || key == 'R') { initGame(); gameOver = false; }
}
void keyboardUp(unsigned char key, int, int) { keyState[key] = false; if (key == 't' || key == 'T') keyToggleT = false; }
void specialDown(int key, int, int) { if (key >= 0 && key < 256) { specialKeyState[key] = true; showIntro = false; } }
void specialUp(int key, int, int) { if (key >= 0 && key < 256) specialKeyState[key] = false; }

// ---------- Lock target ----------
int findLockCandidate() {
    float fx, fz; forwardFromYaw(player.yaw, fx, fz);
    float bestD = 1e9f; int best = -1;
    for (size_t i = 0; i < enemies.size(); ++i) {
        if (!enemies[i].alive) continue;
        float dx = enemies[i].x - player.x, dy = enemies[i].y - player.y, dz = enemies[i].z - player.z;
        float d = sqrtf(dx * dx + dy * dy + dz * dz); if (d > LOCK_RANGE) continue;
        float planar = sqrtf(dx * dx + dz * dz) + 1e-6f; float proj = (dx * fx + dz * fz) / planar;
        if (proj < LOCK_COS_FOV) continue;
        if (d < bestD) { bestD = d; best = (int)i; }
    }
    return best;
}

// ---------- GL init / reshape / main ----------
void setupLights();
void initGL() {
    glEnable(GL_DEPTH_TEST);
    glShadeModel(GL_SMOOTH);
    setupLights();
    // start in night mode (your original)
    glClearColor(0.02f, 0.03f, 0.07f, 1.0f);
}
void reshape(int w, int h) {
    float aspect = float(w) / float(h ? h : 1);
    glViewport(0, 0, w, h);
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    gluPerspective(60.0f, aspect, 0.1f, 3000.0f);
    glMatrixMode(GL_MODELVIEW);
}

int main(int argc, char** argv) {
    srand((unsigned)time(nullptr));
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(WIDTH, HEIGHT);
    glutCreateWindow("3D Shooter (Infinite Ground, Sky Text, Varied Buildings, Trees, Day/Night)");
    initGL(); initGame();
    glutDisplayFunc(display); glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboardDown); glutKeyboardUpFunc(keyboardUp);
    glutSpecialFunc(specialDown); glutSpecialUpFunc(specialUp);
    glutTimerFunc(16, update, 0);
    cout << "Controls:\n  Arrows: Left/Right turn, Up/Down ascend/descend\n"
        << "  W/S: Forward/Back\n"
        << "  Space: Shoot (Lock -> big homing missile)\n"
        << "  T: Toggle lock-on\n"
        << "  N: Toggle Day/Night sky\n"
        << "  R: Restart\n"
        << "  Esc: Quit\n";
    glutMainLoop(); return 0;
}
