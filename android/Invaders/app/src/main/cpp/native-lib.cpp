#include <jni.h>
#include <string>
#include <android/log.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <GLES3/gl3.h>
#include <EGL/egl.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cmath>
#include <vector>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <iostream>
#include <chrono>
#include "stb_image.h"
#include "shader.h"
#include "stb_easy_font.h"
#include "include/audio_manager.h"

#define LOG_TAG "InvadersNative"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// ===== Google Play Games JNI globals =====
static JavaVM* g_javaVM = nullptr;
static jobject g_mainActivityObj = nullptr; // Global reference to MainActivity for callbacks

// Using GLM for vector math (same as desktop version)

// ===== ENEMY SYSTEM =====
enum EnemyType {
    GRUNT = 0,
    SERGEANT = 1,
    CAPTAIN = 2
};

struct Enemy {
    glm::vec2 position;
    glm::vec2 velocity;
    bool isAlive;
    EnemyType type;
    float health;
    float scale;
    float animationTimer;
    bool isAttacking;
    glm::vec2 formationPosition; // Original formation position

    // Curved attack variables (Galaxian style)
    float attackTimer;           // Time since attack started
    glm::vec2 attackStartPos;    // Position where attack began
    glm::vec2 attackTargetPos;   // Target position for attack
    int attackPattern;           // 0=left curve, 1=right curve, 2=direct
    float attackSpeed;           // Speed of attack movement
    bool hasFired;
    int bulletsFired;

    Enemy() : position(0.0f), velocity(0.0f), isAlive(true), type(GRUNT),
              health(1.0f), scale(1.0f), animationTimer(0.0f),
              isAttacking(false), formationPosition(0.0f),
              attackTimer(0.0f), attackStartPos(0.0f), attackTargetPos(0.0f),
              attackPattern(0), attackSpeed(0.7f), hasFired(false), bulletsFired(0) {}
};


// ===== BULLET SYSTEM =====
struct Bullet {
    glm::vec2 position;
    glm::vec2 velocity;
    bool isActive;

    Bullet() : position(0.0f), velocity(0.0f), isActive(false) {}
};

struct EnemyBullet {
    glm::vec2 position;
    glm::vec2 velocity;
    bool isActive;

    EnemyBullet() : position(0.0f), velocity(0.0f), isActive(false) {}
};

struct Explosion {
    glm::vec2 position;
    float timer;
    float duration;
    bool isActive;

    Explosion() : position(0.0f), timer(0.0f), duration(1.0f), isActive(false) {}
};

// ===== PARALLAX BACKGROUND SYSTEM =====
struct ParallaxLayer {
    unsigned int texture;
    float scrollSpeed;
    float offsetX;
    std::string name;

    ParallaxLayer() : texture(0), scrollSpeed(0.0f), offsetX(0.0f) {}
    ParallaxLayer(unsigned int tex, float speed, const std::string& layerName)
            : texture(tex), scrollSpeed(speed), offsetX(0.0f), name(layerName) {}
};

// ===== TEXT RENDERING SYSTEM =====
struct TextButton {
    std::string text;
    float pixelX, pixelY, scale;
    glm::vec3 color;
    glm::vec4 bounds;  // NDC bounds for clicking (x0,y0,x1,y1)
    bool isHovered = false;

    TextButton(const std::string& txt, float x, float y, float s, glm::vec3 col)
            : text(txt), pixelX(x), pixelY(y), scale(s), color(col), bounds(0.0f) {}
};

// Level difficulty parameters
struct LevelConfig {
    float enemySpeed;           // Base enemy movement speed
    float formationSwaySpeed;   // How fast formation moves side to side
    float formationSwayAmount; // How far formation moves side to side
    float attackInterval;      // Time between enemy attacks
    float attackSpeed;         // Speed of attacking enemies
    int maxSimultaneousAttacks; // Max enemies attacking at once
    float enemyBulletSpeedBonus; // Additional speed added to enemy bullets
    
    LevelConfig(float speed = 1.0f, float swaySpeed = 0.5f, float swayAmount = 0.3f,
                float interval = 2.0f, float attackSpd = 0.8f, int maxAttacks = 1)
        : enemySpeed(speed), formationSwaySpeed(swaySpeed), formationSwayAmount(swayAmount),
          attackInterval(interval), attackSpeed(attackSpd), maxSimultaneousAttacks(maxAttacks),
          enemyBulletSpeedBonus(0.3f) {}
};


std::vector<TextButton> menuButtons;

// Text rendering globals
unsigned int textVAO = 0, textVBO = 0;
const int MAX_TEXT_TRIANGLES = 1024;
Shader* textShaderPtr = nullptr;

// ===== COLLISION DETECTION =====
// Simple circular collision detection
bool checkCollision(glm::vec2 pos1, float radius1, glm::vec2 pos2, float radius2) {
    float distance = glm::length(pos1 - pos2);
    return distance < (radius1 + radius2);
}

// Game object sizes for collision detection - Updated to match visual size
const float PLAYER_RADIUS = 0.20f;      // Player collision radius (was 0.15f)
const float ENEMY_RADIUS = 0.18f;       // Enemy collision radius (was 0.12f)
const float BULLET_RADIUS = 0.05f;      // Bullet collision radius
const float PLAYER_MOVEMENT_SENSITIVITY = 1.0f; // Adjustable sensitivity for player movement

// ===== GAME STATE SYSTEM =====
enum class GameState { 
    MENU, 
    PLAYING, 
    PAUSED, 
    LEVEL_COMPLETE,
    GAME_OVER, 
    GAME_WON 
};
GameState gameState = GameState::MENU;

// ===== GAME STATE =====
static int playerScore = 0;
static int playerLives = 3;

// ===== LEVEL SYSTEM =====
int currentLevel = 1;
int maxLevel = 10;  // Maximum level (or set to -1 for infinite)
bool levelComplete = false;
float levelTransitionTimer = 0.0f;
const float LEVEL_TRANSITION_DURATION = 3.0f;  // 3 seconds between levels

// Difficulty progression for each level
std::vector<LevelConfig> levelConfigs = {
    LevelConfig(1.0f, 0.3f, 0.3f, 2.0f, 0.8f, 1),   // Level 1
    LevelConfig(1.2f, 0.4f, 0.4f, 1.8f, 0.9f, 2),   // Level 2
    LevelConfig(1.4f, 0.5f, 0.5f, 1.6f, 1.0f, 2),   // Level 3
    LevelConfig(1.6f, 0.6f, 0.6f, 1.4f, 1.1f, 2),   // Level 4
    LevelConfig(1.8f, 0.7f, 0.7f, 1.2f, 1.2f, 3),   // Level 5
    LevelConfig(2.0f, 0.8f, 0.8f, 1.0f, 1.3f, 3),   // Level 6
    LevelConfig(2.2f, 0.9f, 0.9f, 0.9f, 1.4f, 3),   // Level 7
    LevelConfig(2.4f, 1.0f, 1.0f, 0.8f, 1.5f, 4),   // Level 8
    LevelConfig(2.6f, 1.2f, 1.1f, 0.7f, 1.6f, 4),   // Level 9
    LevelConfig(2.8f, 1.4f, 1.2f, 0.6f, 1.7f, 5),   // Level 10
};

// Current level configuration
LevelConfig currentLevelConfig;


const int MAX_BULLETS = 10;  // Maximum bullets on screen
const float BULLET_SPEED = 6.0f;  // Speed of bullet movement
static std::vector<Bullet> bullets(MAX_BULLETS);

const int MAX_EXPLOSIONS = 20;
static std::vector<Explosion> explosions(MAX_EXPLOSIONS);  // Explosion pool

AudioManager* audioManager = nullptr; // Audio manager for sound effects

// Screen dimensions
static int g_screenWidth = 800;
static int g_screenHeight = 600;
static float g_aspectRatio = 1.33f;
static bool g_isInitialized = false;

// Current window dimensions (updated on resize)
int currentWindowWidth = g_screenWidth;
int currentWindowHeight = g_screenHeight;

// Time
static float g_time = 0.0f;
static float g_deltaTime = 0.0f;
static float g_lastTime = 0.0f;

// Player position
glm::vec3 playerPosition = glm::vec3(0.0f, -2.0f, 0.0f);
const float playerSpeed = 2.0f;

// Screen bounds in world space (for orthographic projection)
const float WORLD_HALF_WIDTH = 4.0f;    // Half the orthographic width
const float WORLD_HALF_HEIGHT = 3.0f;   // Half the orthographic height


// Enemy formation constants (Galaxian style) - adjusted dynamically for screen bounds
const int ENEMY_ROWS = 3;
const int ENEMIES_PER_ROW[ENEMY_ROWS] = {6,8,10}; // bottom, middle, top
const int TOTAL_ENEMIES = ENEMIES_PER_ROW[0] + ENEMIES_PER_ROW[1] + ENEMIES_PER_ROW[2];
// Horizontal spacing between enemies (re-computed when the orientation/viewport changes)
static float g_enemySpacingX = 0.35f;        // small width
const float g_enemySpacingY = 0.3f;     // Vertical spacing remains constant
// X-coordinate of left-most enemy in the formation (re-computed together with spacing)
static float g_formationStartX = -3.0f;
 const float g_formationStartY = 2.0f;    // Positions formation in upper Y area

static std::vector<Enemy> enemies(TOTAL_ENEMIES);
static std::vector<glm::vec2> aliveEnemyPositions;

// Attack timing control
static float lastAttackTime = 0.0f;
const float ATTACK_INTERVAL = 2.0f;  // Time between attacks (2 seconds)

// Bullet timing control
static float lastBulletTime = 0.0f;
const float BULLET_COOLDOWN = 0.50f;  // 0.50 seconds between bullets

// Auto-shooting variables
static bool g_autoShootEnabled = true;  // Enable auto-shooting by default
static float g_autoShootInterval = 1.2f;  // Starting interval in seconds
const float AUTO_SHOOT_DECREASE_PER_LEVEL = 0.1f;  // Decrease interval by 0.1s per level
const float AUTO_SHOOT_MIN_INTERVAL = 0.7f;  // Minimum interval threshold

// Enemy bullet constants
const int MAX_ENEMY_BULLETS = 20;  // Maximum enemy bullets on screen
const float ENEMY_BULLET_SPEED = 1.5f;  // Base enemy bullet speed
const float NON_ATTACKING_SHOOT_INTERVAL = 7.0f;  // Random shooting interval for non-attacking enemies
const float NEAREST_SHOOT_INTERVAL = 3.0f;  // More frequent shooting for nearest enemies

// enemy game state variables
std::vector<EnemyBullet> enemyBullets(MAX_ENEMY_BULLETS);
float lastNonAttackingShootTime = 0.0f;

float quadVertices[] = { // vertex attributes for a quad in world space coordinates
        // positions   // texCoords
        -0.5f,  0.5f,  0.0f, 1.0f,  // top left
        -0.5f, -0.5f,  0.0f, 0.0f,  // bottom left
        0.5f, -0.5f,  1.0f, 0.0f,  // bottom right
        -0.5f,  0.5f,  0.0f, 1.0f,  // top left
        0.5f, -0.5f,  1.0f, 0.0f,  // bottom right
        0.5f,  0.5f,  1.0f, 1.0f   // top right
};

// Full-screen background quad in Normalized Device Coordinates (-1 to +1)
float backgroundVerticesNDC[] = {
        // positions (NDC)  // texCoords (for parallax wrapping)
        -1.0f,  1.0f,       0.0f, 1.0f,  // top left
        -1.0f, -1.0f,       0.0f, 0.0f,  // bottom left
        1.0f, -1.0f,       1.0f, 0.0f,  // bottom right
        -1.0f,  1.0f,       0.0f, 1.0f,  // top left
        1.0f, -1.0f,       1.0f, 0.0f,  // bottom right
        1.0f,  1.0f,       1.0f, 1.0f   // top right
};

const int NUM_PARALLAX_LAYERS = 6;
std::vector<ParallaxLayer> parallaxLayers;


// Touch state
static bool g_isTouching = false;
static float g_touchX = 0.0f;
static float g_touchY = 0.0f;
static bool g_shouldShoot = false;

// Relative touch movement variables
static float g_initialTouchX = 0.0f;      // Touch position when first touched
static float g_playerStartX = 0.0f;       // Player position when touch started
static bool g_useRelativeMovement = true; // Enable relative movement

// Asset manager for loading textures
static AAssetManager* g_assetManager = nullptr;

// Texture IDs
static GLuint g_playerTexture = 0;
static GLuint g_enemyTexture = 0;
static GLuint g_bulletTexture = 0;
static GLuint g_enemyMissileTexture = 0;
// Shader objects (same as desktop version)
static Shader* enemyShader = nullptr;
static Shader* explosionShader = nullptr;
static Shader* backgroundShader = nullptr;
static Shader* textShader = nullptr;
static Shader* parallaxShader = nullptr;
static GLuint g_quadVAO = 0;
static GLuint g_quadVBO = 0;
static GLuint g_backgroundVAO = 0;
static GLuint g_backgroundVBO = 0;
static GLuint g_enemyVAO = 0;
static GLuint g_enemyVBO = 0;
static GLuint g_instanceVBO = 0;
static GLuint g_bulletVAO = 0;
static GLuint g_bulletVBO = 0;
static GLuint g_enemyShotVAO =0;
static GLuint g_explosionVAO = 0;
static GLuint g_explosionVBO = 0;

// ... after other static/global variables ...
static bool g_musicStarted = false;
// ... existing code ...

// Forward declarations
static void showLeaderboard();
static void submitScoreToLeaderboard(long score);

// ==== Helper to recompute spacing and start X so formation fits current world width ====
static void recalculateFormationLayout() {
    float margin = 0.01f; // half enemy width padding at each side

    // Compute current horizontal world bounds
    float worldLeft  = -g_aspectRatio * WORLD_HALF_WIDTH + margin;
    float worldRight =  g_aspectRatio * WORLD_HALF_WIDTH  - margin;
    float availableWidth = worldRight - worldLeft;

    // Desired spacing scales with aspect ratio but must leave slack on both sides for movement
    float desiredSpacing = g_aspectRatio * 1.0f; // base tuning constant (tweakable)

    // Allow the grid to occupy at most 80% of the available width so there is ~10% free space per side
    float maxSpacingWithSlack = (availableWidth * 0.6f) / (ENEMIES_PER_ROW[0] - 1);
    g_enemySpacingX = std::min(desiredSpacing, maxSpacingWithSlack);

    // Center formation horizontally
    float formationWidth = (ENEMIES_PER_ROW[0] - 1) * g_enemySpacingX;
    g_formationStartX = worldLeft + (availableWidth - formationWidth) * 0.5f;
}


// Calculate curved attack position with dramatic sweeping curves (enhanced Galaxian style)
glm::vec2 calculateCurvedAttackPosition(const Enemy& enemy) {
    // Use attackSpeed to control the duration of the Bezier dive
    // Higher attackSpeed = shorter duration (faster dive)
    const float baseDuration = 4.0f; // seconds for attackSpeed = 1.0
    float phaseOneDuration = baseDuration / enemy.attackSpeed;
    float t = enemy.attackTimer / phaseOneDuration;

    // Get world bottom bound for off-screen exit
    float worldBottomBound = -g_aspectRatio * WORLD_HALF_HEIGHT;
    float offscreenY = worldBottomBound - 1.5f; // 1.5 units below screen

    // Player position at attack start (use Y from player, X from attackTargetPos for curve variety)
    glm::vec2 playerPosAtAttack = glm::vec2(enemy.attackTargetPos.x, playerPosition.y);

    // Final target is below the player, off-screen
    glm::vec2 target = glm::vec2(enemy.attackTargetPos.x, offscreenY);

    // Control points for dramatic curve
    glm::vec2 start = enemy.attackStartPos;
    glm::vec2 controlPoint1, controlPoint2;
    if (enemy.attackPattern == 0) { // Left curve
        controlPoint1 = glm::vec2(start.x - 3.0f, start.y - 1.0f);
        controlPoint2 = playerPosAtAttack; // Pass through player
    } else if (enemy.attackPattern == 1) { // Right curve
        controlPoint1 = glm::vec2(start.x + 5.0f, start.y - 1.0f);
        controlPoint2 = playerPosAtAttack; // Pass through player
    } else { // Direct
        controlPoint1 = glm::vec2(start.x, start.y - 1.5f);
        controlPoint2 = playerPosAtAttack; // Pass through player
    }

    // Clamp t to 1.0 for the full curve, then continue straight down
    if (t <= 1.0f) {
        // Cubic Bezier: P0=start, P1=control1, P2=player, P3=target (offscreen)
        float invT = 1.0f - t;
        float invT2 = invT * invT;
        float invT3 = invT2 * invT;
        float t2 = t * t;
        float t3 = t2 * t;
        return invT3 * start +
               3.0f * invT2 * t * controlPoint1 +
               3.0f * invT * t2 * controlPoint2 +
               t3 * target;
    } else {
        // After the curve, continue straight down from the last point
        float t1 = 1.0f;
        float invT = 1.0f - t1;
        float invT2 = invT * invT;
        float invT3 = invT2 * invT;
        float t2 = t1 * t1;
        float t3 = t2 * t1;
        glm::vec2 endOfCurve = invT3 * start +
                              3.0f * invT2 * t1 * controlPoint1 +
                              3.0f * invT * t2 * controlPoint2 +
                              t3 * target;
        float extraTime = (enemy.attackTimer - phaseOneDuration);
        return glm::vec2(endOfCurve.x, endOfCurve.y - enemy.attackSpeed * extraTime * 1.2f);
    }
}

void createExplosion(glm::vec2 position) {
    for (int i = 0; i < MAX_EXPLOSIONS; i++) {
        if (!explosions[i].isActive) {
            explosions[i].position = position;
            explosions[i].timer = 0.0f;
            explosions[i].duration = 1.2f;
            explosions[i].isActive = true;
            LOGI("Explosion created at (%.2f, %.2f)", position.x, position.y);
            break;
        }
    }
}

void updateExplosions(float deltaTime) {
    for (int i = 0; i < MAX_EXPLOSIONS; i++) {
        if (explosions[i].isActive) {
            explosions[i].timer += deltaTime;
            if (explosions[i].timer >= explosions[i].duration) {
                explosions[i].isActive = false;
            }
        }
    }
}

void initializeEnemies() {
    int index = 0;
    float y = g_formationStartY;
    for (int row = 0; row < ENEMY_ROWS; row++) {
        int numInRow = ENEMIES_PER_ROW[row];
        // Center this row horizontally
        float rowWidth = (numInRow - 1) * g_enemySpacingX;
        float startX = -rowWidth / 2.0f;
        for (int col = 0; col < numInRow; col++) {
            float x = startX + col * g_enemySpacingX;
            enemies[index].position = glm::vec2(x, y);
            enemies[index].formationPosition = glm::vec2(x, y);
            enemies[index].velocity = glm::vec2(0.0f, 0.0f);
            enemies[index].isAlive = true;
            enemies[index].health = 1.0f;
            enemies[index].scale = 0.25f;
            enemies[index].animationTimer = 0.0f;
            enemies[index].isAttacking = false;
            enemies[index].type = GRUNT;
            index++;
        }
        y -= g_enemySpacingY;
    }
}

// Create a new bullet at player position
void createBullet() {
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!bullets[i].isActive) {
            // PLAY LASER SOUND
            if (audioManager) {
                audioManager->playSound("laser", 0.5f, 1.0f);
            }
            
            LOGI("Firing bullet!");

            // Fire from the tip/front of the spaceship
            bullets[i].position = glm::vec2(playerPosition.x, playerPosition.y + 0.15f);
            bullets[i].velocity = glm::vec2(0.0f, BULLET_SPEED); // Move upward
            bullets[i].isActive = true;
            break; // Only fire one bullet per call
        }
    }
}

// Update all active bullets
void updateBullets(float deltaTime) {
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (bullets[i].isActive) {
            // Move bullet upward
            bullets[i].position += bullets[i].velocity * deltaTime;

            // Check collision with enemies
            for (int j = 0; j < TOTAL_ENEMIES; j++) {
                                if (enemies[j].isAlive &&
                    checkCollision(bullets[i].position, BULLET_RADIUS,
                                 enemies[j].position, ENEMY_RADIUS)) {

                    createExplosion(enemies[j].position);

                    // PLAY EXPLOSION SOUND
                    if (audioManager) {
                        audioManager->play3DSound("explosion", 
                                                enemies[j].position.x, 
                                                enemies[j].position.y, 
                                                0.0f, 
                                                0.5f); // Volume
                    }

                    // Hit detected!
                    enemies[j].isAlive = false;  // Destroy enemy
                    bullets[i].isActive = false; // Destroy bullet

                    // Add score based on enemy type
                    switch(enemies[j].type) {
                        case GRUNT: playerScore += 10; break;
                        case SERGEANT: playerScore += 20; break;
                        case CAPTAIN: playerScore += 50; break;
                    }

                    LOGI("Enemy destroyed! Score: %d", playerScore);
                    break; // Bullet can only hit one enemy
                }
            }

            // Deactivate bullet if it goes off screen
            if (bullets[i].position.y > WORLD_HALF_HEIGHT + 0.5f) {
                bullets[i].isActive = false;
            }
        }
    }
}

// Create enemy bullet at enemy position
void createEnemyBullet(const Enemy& enemy) {
    for (int i = 0; i < MAX_ENEMY_BULLETS; i++) {
        if (!enemyBullets[i].isActive) {
            // Play shoot sound (optional - use a different sound than player)
            if (audioManager) {
                audioManager->play3DSound("laser", enemy.position.x, enemy.position.y, 0.0f, 0.3f);
            }

            enemyBullets[i].position = enemy.position;

            // Calculate direction towards player
            glm::vec2 dirToPlayer = glm::normalize(
                    glm::vec2(playerPosition.x, playerPosition.y) - enemy.position
            );

            // Add slight randomness to shooting direction
            float randomAngle = (rand() % 40 - 20) * 0.01f; // ±20 degrees
            float cs = cos(randomAngle);
            float sn = sin(randomAngle);
            glm::vec2 randomizedDir = glm::vec2(
                    dirToPlayer.x * cs - dirToPlayer.y * sn,
                    dirToPlayer.x * sn + dirToPlayer.y * cs
            );

            // Add the bullet speed bonus to the base speed instead of multiplying
            float totalBulletSpeed = ENEMY_BULLET_SPEED + currentLevelConfig.enemyBulletSpeedBonus;
            enemyBullets[i].velocity = randomizedDir * totalBulletSpeed;
            enemyBullets[i].isActive = true;
            break;
        }
    }
}

// Update enemy bullets
void updateEnemyBullets(float deltaTime) {
    for (int i=0; i<MAX_ENEMY_BULLETS; i++) {
        if (enemyBullets[i].isActive) {
            // Move bullet towards player
            enemyBullets[i].position += enemyBullets[i].velocity * deltaTime;

            // Check collision with player
            if (checkCollision(enemyBullets[i].position, BULLET_RADIUS,
                               glm::vec2(playerPosition.x, playerPosition.y), PLAYER_RADIUS)) {
                // PLAY EXPLOSION SOUND
                if (audioManager) {
                    audioManager->play3DSound("explosion",
                                              playerPosition.x,
                                              playerPosition.y,
                                              0.0f,
                                              0.5f); // Volume
                }

                // Deactivate bullet
                enemyBullets[i].isActive = false;
                // Player hit!
                playerLives--;

                std::cout << "Player hit! Lives remaining: " << playerLives << std::endl;

                // Create explosion at player position
                createExplosion(enemyBullets[i].position);

                // Check game over condition
                if (playerLives <= 0) {
                    gameState = GameState::GAME_OVER;
                    std::cout << "Game Over! Final Score: " << playerScore << std::endl;
                }
                continue; // No need to check further
            }

            // Deactivate bullet if it goes off screen
            if (enemyBullets[i].position.y < -WORLD_HALF_HEIGHT - 1.0f ||
                enemyBullets[i].position.y > WORLD_HALF_HEIGHT + 1.0f ||
                enemyBullets[i].position.x < -WORLD_HALF_WIDTH - 1.0f ||
                enemyBullets[i].position.x > WORLD_HALF_WIDTH + 1.0f) {
                enemyBullets[i].isActive = false;
            }
        }
    }
}


// ===== TEXT RENDERING FUNCTIONS =====
glm::vec4 calculateTextBounds(const char* text, float x, float y, float scale) {
    char buffer[9999];
    int numQuads = stb_easy_font_print(0, 0, (char*)text, nullptr, buffer, sizeof(buffer));

    if (numQuads == 0) return glm::vec4(0.0f);

    float minX = FLT_MAX, minY = FLT_MAX;
    float maxX = -FLT_MAX, maxY = -FLT_MAX;

    // Find bounding box of all character quads
    for (int q = 0; q < numQuads; ++q) {
        // Each quad has 4 vertices, each vertex is 16 bytes
        char* quadData = buffer + q * 4 * 16;

        for (int i = 0; i < 4; ++i) {
            auto* vertex = reinterpret_cast<float*>(quadData + i * 16);
            float px = x + vertex[0] * scale;  // vertex[0] is x
            float py = y + vertex[1] * scale;  // vertex[1] is y

            minX = std::min(minX, px);
            maxX = std::max(maxX, px);
            minY = std::min(minY, py);
            maxY = std::max(maxY, py);
        }
    }

    // Convert to NDC using current window dimensions
    float ndcX0 =  minX / (currentWindowWidth  * 0.5f) - 1.0f;
    float ndcX1 =  maxX / (currentWindowWidth  * 0.5f) - 1.0f;
    float ndcY0 = -maxY / (currentWindowHeight * 0.5f) + 1.0f;  // flip Y
    float ndcY1 = -minY / (currentWindowHeight * 0.5f) + 1.0f;

    // Add some padding to make buttons easier to click
    float paddingX = 0.02f;  // 2% of screen width padding
    float paddingY = 0.02f;  // 2% of screen height padding

    return {ndcX0 - paddingX, ndcY0 - paddingY, ndcX1 + paddingX, ndcY1 + paddingY};  // x0,y0,x1,y1
}

void renderText(const char* txt, float x, float y, float scale, const glm::vec3& rgb) {
    char buffer[9999];
    int numQuads = stb_easy_font_print(0, 0, (char*)txt, nullptr, buffer, sizeof(buffer));

    if (numQuads == 0) return;

    // Convert quads to triangles with proper vertex format parsing
    // stb_easy_font vertex format: x(float), y(float), z(float), color(uint8[4]) = 16 bytes per vertex
    std::vector<float> verts;
    verts.reserve(numQuads * 6 * 2);   // 6 verts per quad, 2 floats each (x,y)

    for (int q = 0; q < numQuads; ++q) {
        // Each quad has 4 vertices, each vertex is 16 bytes
        char* quadData = buffer + q * 4 * 16;
        float vx[4], vy[4];

        // Extract x,y from each vertex (skip z and color)
        for (int i = 0; i < 4; ++i) {
            float* vertex = (float*)(quadData + i * 16);

            // Apply scaling and positioning
            vx[i] = x + vertex[0] * scale;  // vertex[0] is x
            vy[i] = y + vertex[1] * scale;  // vertex[1] is y

            // Convert pixels to NDC using current window dimensions
            vx[i] =  vx[i] / (currentWindowWidth  * 0.5f) - 1.0f;
            vy[i] = -vy[i] / (currentWindowHeight * 0.5f) + 1.0f;  // flip Y
        }

        // Convert quad to two triangles: (0,1,2) and (0,2,3)
        int indices[6] = {0,1,2,0,2,3};
        for (int i = 0; i < 6; ++i) {
            verts.push_back(vx[indices[i]]);
            verts.push_back(vy[indices[i]]);
        }
    }

    if (verts.empty()) return;

    // Check if textShaderPtr is valid
    if (!textShaderPtr) {
        std::cout << "ERROR: textShaderPtr is null!" << std::endl;
        return;
    }

    // Upload vertex data
    glBindBuffer(GL_ARRAY_BUFFER, textVBO);
    if (verts.size() * sizeof(float) <= MAX_TEXT_TRIANGLES * 3 * 2 * sizeof(float)) {
        glBufferSubData(GL_ARRAY_BUFFER, 0, verts.size() * sizeof(float), verts.data());
    } else {
        std::cout << "Text buffer overflow!" << std::endl;
        return;
    }

    // Render
    textShaderPtr->use();
    glm::mat4 I(1.0f);
    textShaderPtr->setMat4("projection", I);      // already in clip space
    textShaderPtr->setVec3("color", rgb);

    // Ensure proper OpenGL state for text rendering
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    // Note: glPolygonMode not available in OpenGL ES - triangles are always filled

    glBindVertexArray(textVAO);
    glDrawArrays(GL_TRIANGLES, 0, verts.size() / 2);
    glBindVertexArray(0);
}

// Helper function to get text width for centering
float getTextWidth(const char* text, float scale) {
    char buffer[9999];
    int numQuads = stb_easy_font_print(0, 0, (char*)text, nullptr, buffer, sizeof(buffer));

    if (numQuads == 0) return 0.0f;

    float minX = FLT_MAX, maxX = -FLT_MAX;
    for (int q = 0; q < numQuads; ++q) {
        // Each quad has 4 vertices, each vertex is 16 bytes
        char* quadData = buffer + q * 4 * 16;
        for (int i = 0; i < 4; ++i) {
            float* vertex = (float*)(quadData + i * 16);
            float px = vertex[0] * scale;  // vertex[0] is x coordinate
            minX = std::min(minX, px);
            maxX = std::max(maxX, px);
        }
    }
    return maxX - minX;
}

void initMenuButtons() {
    menuButtons.clear();

    // Add GALAXIAN title (centered)
    const char* titleText = "INVADERS 1999";
    float titleScale = 10.0f;
    float titleWidth = getTextWidth(titleText, titleScale);
    float titleX = (currentWindowWidth - titleWidth) / 2.0f;
    TextButton titleButton(titleText, titleX, currentWindowHeight * 0.25f, titleScale, glm::vec3(1.0f, 1.0f, 1.0f));
    titleButton.bounds = calculateTextBounds(titleButton.text.c_str(), titleButton.pixelX, titleButton.pixelY, titleButton.scale);
    menuButtons.push_back(titleButton);

    // Add start button (centered)
    const char* startText = "CLICK TO START";
    float startScale = 8.0f;
    float startWidth = getTextWidth(startText, startScale);
    float startX = (currentWindowWidth - startWidth) / 2.0f;
    TextButton startButton(startText, startX, currentWindowHeight/2.0f - 20.0f, startScale, glm::vec3(1.0f, 1.0f, 0.0f));
    startButton.bounds = calculateTextBounds(startButton.text.c_str(), startButton.pixelX, startButton.pixelY, startButton.scale);
    menuButtons.push_back(startButton);

    // Add quit button (centered)
    const char* quitText = "PRESS ESC TO QUIT";
    float quitScale = 7.0f;
    float quitWidth = getTextWidth(quitText, quitScale);
    float quitX = (currentWindowWidth - quitWidth) / 2.0f;
    TextButton quitButton(quitText, quitX, currentWindowHeight/2.0f + 80.0f, quitScale, glm::vec3(0.8f, 0.8f, 1.0f));
    quitButton.bounds = calculateTextBounds(quitButton.text.c_str(), quitButton.pixelX, quitButton.pixelY, quitButton.scale);
    menuButtons.push_back(quitButton);

    // Add leaderboard button (centered below start)
    const char* leaderboardText = "LEADERBOARD";
    float leaderboardScale = 7.5f;
    float leaderboardWidth = getTextWidth(leaderboardText, leaderboardScale);
    float leaderboardX = (currentWindowWidth - leaderboardWidth) / 2.0f;
    TextButton leaderboardButton(leaderboardText, leaderboardX, currentWindowHeight/2.0f + 40.0f, leaderboardScale, glm::vec3(0.0f, 1.0f, 1.0f));
    leaderboardButton.bounds = calculateTextBounds(leaderboardButton.text.c_str(), leaderboardButton.pixelX, leaderboardButton.pixelY, leaderboardButton.scale);
    menuButtons.push_back(leaderboardButton);
}

// Add a global formation phase variable outside the function
static float g_formationPhase = 0.0f;
// Variables to track how far enemies are going offscreen
static float g_maxOffscreenLeft = 0.0f;
static float g_maxOffscreenRight = 0.0f;

void updateEnemies(float deltaTime) {
    // Update the formation phase for horizontal movement
    g_formationPhase += deltaTime * currentLevelConfig.formationSwaySpeed * 1.0f; // Tunable speed multiplier

    // Update alive enemies list for rendering
    aliveEnemyPositions.clear();

    int attackingCount = 0;
    int aliveCount = 0; // Track total alive enemies for adaptive attack logic

    // For enemy shooting
    float nearestDistance = FLT_MAX;
    Enemy* nearestEnemy = nullptr;

    // Count currently attacking enemies
    for (int i = 0; i < TOTAL_ENEMIES; i++) {
        if (enemies[i].isAlive) {
            float dist = glm::length(glm::vec2(playerPosition.x, playerPosition.y) - enemies[i].position);
            // Find nearest enemy
            if (dist < nearestDistance) {
                nearestDistance = dist;
                nearestEnemy = &enemies[i];
            }

            // Count alive enemies
            aliveCount++;
            // Increasing enemy attack count
            if(enemies[i].isAttacking) {
                attackingCount++;
            }
        }
    }

    // -----------------------------------------------
    // Identify edge enemies (left-most & right-most alive per row)
    // -----------------------------------------------
    std::vector<int> edgeEnemies;
    int indexOffset = 0;
    for (int row = 0; row < ENEMY_ROWS; ++row) {
        // Left-most in this row
        for (int col = 0; col < ENEMIES_PER_ROW[row]; ++col) {
            int idx = indexOffset + col;
            if (enemies[idx].isAlive && !enemies[idx].isAttacking) {
                edgeEnemies.push_back(idx);
                break;
            }
        }
        // Right-most in this row
        for (int col = ENEMIES_PER_ROW[row] - 1; col >= 0; --col) {
            int idx = indexOffset + col;
            if (enemies[idx].isAlive && !enemies[idx].isAttacking) {
                if (std::find(edgeEnemies.begin(), edgeEnemies.end(), idx) == edgeEnemies.end()) {
                    edgeEnemies.push_back(idx);
                }
                break;
            }
        }
        indexOffset += ENEMIES_PER_ROW[row];
    }

    // Calculate the formation offset once per frame (outside the enemy loop)
    float worldLeftBound = -g_aspectRatio * WORLD_HALF_WIDTH;
    float worldRightBound = g_aspectRatio * WORLD_HALF_WIDTH;
    
    // Define a fixed amplitude for the sinusoidal movement
    float baseAmplitude = 2.0f; // Larger amplitude to ensure going offscreen
    
    // Create complex sinusoidal movement pattern
    float currentOffset = sin(g_formationPhase) * baseAmplitude;
    currentOffset += sin(g_formationPhase * 0.57f) * baseAmplitude * 0.25f; // subtle drift
    currentOffset += sin(g_formationPhase * 0.23f) * baseAmplitude * 0.40f; // large slow pendulum
    
    // Calculate how far enemies are going offscreen (for debugging)
    float leftmostEnemyX = g_formationStartX + currentOffset;
    float rightmostEnemyX = g_formationStartX + (ENEMIES_PER_ROW[0] - 1) * g_enemySpacingX + currentOffset;
    
    // Track maximum offscreen distance
    if (leftmostEnemyX < worldLeftBound) {
        float offscreenAmount = worldLeftBound - leftmostEnemyX;
        g_maxOffscreenLeft = std::max(g_maxOffscreenLeft, offscreenAmount);
        LOGI("Enemies offscreen left: %.2f units (max: %.2f)", offscreenAmount, g_maxOffscreenLeft);
    }
    
    if (rightmostEnemyX > worldRightBound) {
        float offscreenAmount = rightmostEnemyX - worldRightBound;
        g_maxOffscreenRight = std::max(g_maxOffscreenRight, offscreenAmount);
        LOGI("Enemies offscreen right: %.2f units (max: %.2f)", offscreenAmount, g_maxOffscreenRight);
    }

    // Check if it's time for a new wave of attacks
    bool timeForNewAttacks = (g_time - lastAttackTime) >= currentLevelConfig.attackInterval;
    int attacksLaunchedThisFrame = 0;
    int maxNewAttacksAllowed = currentLevelConfig.maxSimultaneousAttacks - attackingCount;
    
    // If we can launch attacks and it's time for new attacks, attempt a wave.
    // We'll reset the timer only after at least one enemy actually starts attacking.
    if (timeForNewAttacks && maxNewAttacksAllowed > 0) {
        LOGI("Attempting new attack wave. Max allowed: %d", maxNewAttacksAllowed);
    }

    for (int i = 0; i < TOTAL_ENEMIES; i++) {
        if (!enemies[i].isAlive) continue;

        // Update animation timer
        enemies[i].animationTimer += deltaTime * currentLevelConfig.enemySpeed;
        
        // Apply the offset to every enemy so the grid stays intact
        enemies[i].position.x = enemies[i].formationPosition.x + currentOffset;

        // Start enemy attacks if:
        // 1. Enemy is not already attacking
        // 2. We haven't reached the max simultaneous attacks limit
        // 3. It's time for a new wave of attacks
        // 4. We haven't launched too many attacks this frame
        if (!enemies[i].isAttacking && 
            timeForNewAttacks && 
            attacksLaunchedThisFrame < maxNewAttacksAllowed) {

            // Decide attack pattern: 40% edge attacks, 60% random attacks
            bool useEdgeAttack = (rand() % 100) < 40;
            bool shouldAttack = false;
            
            if (useEdgeAttack) {
                // Use the globally computed edgeEnemies list
                if (std::find(edgeEnemies.begin(), edgeEnemies.end(), i) != edgeEnemies.end()) {
                    shouldAttack = true;
                }
            } else {
                // Random attack – raise probability significantly when only a few enemies remain
                float attackChance = 0.05f * maxNewAttacksAllowed;
                attackChance = std::min(0.60f, attackChance);   // cap at 30 %
                shouldAttack = ((rand() % 10000) / 10000.0f) < attackChance;
            }
            
            if (shouldAttack) {
                enemies[i].isAttacking = true;
                enemies[i].attackTimer = 0.0f;
                enemies[i].attackStartPos = enemies[i].position;
                enemies[i].hasFired = false;
                enemies[i].bulletsFired = 0;

                // Set target position (toward player with some randomness)
                enemies[i].attackTargetPos = glm::vec2(
                    playerPosition.x + (rand() % 200 - 100) / 300.0f, // Some randomness
                    playerPosition.y - 1.0f // Slightly below player
                );

                // Choose attack pattern based on position relative to player
                float relativeX = enemies[i].position.x - playerPosition.x;
                if (relativeX > 0.5f) {
                    enemies[i].attackPattern = 0; // Left curve from right side
                } else if (relativeX < -0.5f) {
                    enemies[i].attackPattern = 1; // Right curve from left side
                } else {
                    enemies[i].attackPattern = rand() % 2; // Random left or right curve
                }

                enemies[i].attackSpeed = currentLevelConfig.attackSpeed + (rand() % 100) / 200.0f; // Varied speed
                
                attacksLaunchedThisFrame++;
                LOGI("Enemy %d starting attack! Pattern: %d, AttacksLaunched: %d/%d", 
                     i, enemies[i].attackPattern, attacksLaunchedThisFrame, maxNewAttacksAllowed);
            }
        }

        // Update attacking enemies with curved motion
        if (enemies[i].isAttacking) {
            enemies[i].attackTimer += deltaTime;

            // Calculate new position using dynamic tracking
            glm::vec2 newPosition = calculateCurvedAttackPosition(enemies[i]);

            // Update bounds to match the new world dimensions
            float worldLeftBound = -g_aspectRatio * WORLD_HALF_WIDTH;
            float worldRightBound = g_aspectRatio * WORLD_HALF_WIDTH;
            float worldBottomBound = -WORLD_HALF_HEIGHT;
            
            // Destroy enemy if it goes far enough off-screen (more lenient exit bounds)
            if (newPosition.y < worldBottomBound - 2.0f ||  // Give more room to exit downward
                newPosition.x < worldLeftBound - 2.0f || 
                newPosition.x > worldRightBound + 2.0f) {
                
                enemies[i].isAlive = false; // Destroy permanently
                enemies[i].isAttacking = false;
                LOGI("Enemy %d destroyed - went off screen at (%.2f, %.2f)", i, newPosition.x, newPosition.y);
            } else {
                // Update position
                enemies[i].position = newPosition;
            }
        }

        // Check collision with player
        if (enemies[i].isAlive && gameState == GameState::PLAYING &&
            checkCollision(enemies[i].position, ENEMY_RADIUS,
                           glm::vec2(playerPosition.x, playerPosition.y), PLAYER_RADIUS)) {

            // Calculate collision details for debugging
            float distance = glm::length(enemies[i].position - glm::vec2(playerPosition.x, playerPosition.y));
            glm::vec2 direction = enemies[i].position - glm::vec2(playerPosition.x, playerPosition.y);
            
            LOGI("COLLISION! Enemy at (%.2f, %.2f), Player at (%.2f, %.2f)", 
                 enemies[i].position.x, enemies[i].position.y, playerPosition.x, playerPosition.y);
            LOGI("Distance: %.3f, Combined radius: %.3f, Direction: (%.2f, %.2f)", 
                 distance, ENEMY_RADIUS + PLAYER_RADIUS, direction.x, direction.y);

            createExplosion(enemies[i].position);

            // PLAY HIT SOUND
            if (audioManager) {
                audioManager->playSound("hit", 1.0f, 1.0f);
            }

            // Player hit by enemy!
            enemies[i].isAlive = false; // Destroy the enemy that hit player
            playerLives--;

            LOGI("Player hit! Lives remaining: %d", playerLives);
        }

        // Add to alive positions for rendering
        if (enemies[i].isAlive) {
            // Attacking enemy shooting
            if (enemies[i].isAttacking) {
                if (!enemies[i].hasFired) {
                    // Timed shots during dive
                    const float FIRST_SHOT_TIME  = 0.7f; // seconds since dive start
                    const float SECOND_SHOT_TIME = 1.4f;

                    if (enemies[i].bulletsFired < 1 && enemies[i].attackTimer >= FIRST_SHOT_TIME) {
                        createEnemyBullet(enemies[i]);
                        enemies[i].bulletsFired++;
                        if (enemies[i].bulletsFired >= 2) enemies[i].hasFired = true;
                    }
                    else if (enemies[i].bulletsFired < 2 && enemies[i].attackTimer >= SECOND_SHOT_TIME) {
                        createEnemyBullet(enemies[i]);
                        enemies[i].bulletsFired++;
                        if (enemies[i].bulletsFired >= 2) enemies[i].hasFired = true;
                    }
                }
            }
            // Non-Attacking enemies shooting
            else {
                if (&enemies[i] == nearestEnemy) {
                    if (g_time - lastNonAttackingShootTime > NEAREST_SHOOT_INTERVAL &&
                        (rand() % 100) < 40) {
                        createEnemyBullet(enemies[i]);
                        lastNonAttackingShootTime = g_time;
                    }
                } else {
                    if (g_time - lastNonAttackingShootTime > NON_ATTACKING_SHOOT_INTERVAL &&
                        (rand() % 100) < 10) {
                        createEnemyBullet(enemies[i]);
                        lastNonAttackingShootTime = g_time;
                    }
                }
            }
            aliveEnemyPositions.push_back(enemies[i].position);
        }
    }

    // --------------------------------------------------
    // Reset attack timer only if at least one enemy attacked
    // --------------------------------------------------
    if (attacksLaunchedThisFrame > 0) {
        lastAttackTime = g_time;
    }
}

// Simple PNG header check (very basic)
bool isPNG(const unsigned char* data) {
    return data[0] == 0x89 && data[1] == 0x50 && data[2] == 0x4E && data[3] == 0x47;
}

// Load shader source from Android assets
std::string loadShaderFromAssets(const char* filename) {
    if (!g_assetManager) {
        LOGE("Asset manager not initialized");
        return "";
    }
    
    AAsset* asset = AAssetManager_open(g_assetManager, filename, AASSET_MODE_BUFFER);
    if (!asset) {
        LOGE("Failed to open shader asset: %s", filename);
        return "";
    }
    
    off_t length = AAsset_getLength(asset);
    const char* buffer = (const char*)AAsset_getBuffer(asset);
    
    if (!buffer || length <= 0) {
        LOGE("Failed to read shader asset: %s", filename);
        AAsset_close(asset);
        return "";
    }
    
    std::string shaderSource(buffer, length);
    AAsset_close(asset);
    
    LOGI("Loaded shader: %s (%d bytes)", filename, (int)length);
    return shaderSource;
}

// Load texture from Android assets
GLuint loadTextureFromAssets(const char* filename) {
    if (!g_assetManager) {
        LOGE("Asset manager not initialized");
        return 0;
    }
    
    AAsset* asset = AAssetManager_open(g_assetManager, filename, AASSET_MODE_BUFFER);
    if (!asset) {
        LOGE("Failed to open asset: %s", filename);
        return 0;
    }
    
    off_t length = AAsset_getLength(asset);
    const unsigned char* buffer = (const unsigned char*)AAsset_getBuffer(asset);
    
    if (!buffer || length <= 0) {
        LOGE("Failed to read asset: %s", filename);
        AAsset_close(asset);
        return 0;
    }
    
    // Use stb_image to load the texture (same as desktop version)
    int width, height, nrChannels;
    stbi_set_flip_vertically_on_load(true); // Same as desktop
    
    // stb_image can load from memory buffer
    unsigned char *data = stbi_load_from_memory(buffer, length, &width, &height, &nrChannels, 0);
    
    AAsset_close(asset); // Close asset after reading into memory
    
    if (!data) {
        LOGE("Failed to load texture with stb_image: %s", filename);
        return 0;
    }
    
    // Generate OpenGL texture (same as desktop version)
    GLuint textureID;
    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_2D, textureID);
    
    // Determine format based on channels (same as desktop)
    GLenum format;
    if (nrChannels == 1)
        format = GL_RED;
    else if (nrChannels == 3)
        format = GL_RGB;
    else if (nrChannels == 4)
        format = GL_RGBA;
    else {
        LOGE("Unsupported number of channels: %d", nrChannels);
        stbi_image_free(data);
        glDeleteTextures(1, &textureID);
        return 0;
    }
    
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    
    // Set texture parameters (same as desktop)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    stbi_image_free(data); // Free stb_image data
    
    LOGI("Texture loaded successfully: %s (%dx%d, %d channels, ID: %u)", filename, width, height, nrChannels, textureID);
    return textureID;
}

// Matrix calculations using GLM (same as desktop version)
static glm::mat4 createOrthographicMatrix(float left, float right, float bottom, float top) {
    return glm::ortho(left, right, bottom, top, 0.1f, 100.0f);
}

static glm::mat4 createModelMatrix(float x, float y, float scaleX, float scaleY) {
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(x, y, 0.0f));
    model = glm::scale(model, glm::vec3(scaleX, scaleY, 1.0f));
    return model;
}

// Draw a quad at specified position with color and optional texture (using Shader objects like desktop)
static void drawQuad(float x, float y, float scaleX, float scaleY, float r, float g, float b, float a, Shader* shader = nullptr, GLuint texture = 0) {
    Shader* currentShader = shader ? shader : enemyShader;
    currentShader->use();

    // Create projection matrix using GLM
    glm::mat4 projMatrix = createOrthographicMatrix(-g_aspectRatio * WORLD_HALF_WIDTH, g_aspectRatio * WORLD_HALF_WIDTH, -WORLD_HALF_HEIGHT, WORLD_HALF_HEIGHT);

    // Create model matrix using GLM
    glm::mat4 modelMatrix = createModelMatrix(x, y, scaleX, scaleY);

    // Set uniforms using Shader class methods (like desktop)
    currentShader->setMat4("projection", projMatrix);
    currentShader->setMat4("model", modelMatrix);
    currentShader->setVec3("color", r, g, b);
    currentShader->setFloat("alpha", a);

    // Handle texture
    if (texture) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        currentShader->setInt("useTexture", 1);
        currentShader->setInt("texture0", 0);
    } else {
        currentShader->setInt("useTexture", 0);
    }

    // Draw
    glBindVertexArray(g_quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

// Initialize level
void initializeLevel(int level) {
    std::cout << "Initializing level " << currentLevel << std::endl;

    // Get level config
    if (level <= (int)levelConfigs.size()) {
        currentLevelConfig = levelConfigs[level - 1];
    } else {
        float multiplier = 1.0f + (level - 1) * 0.2f;
        currentLevelConfig = LevelConfig(
            2.8f * multiplier,   // enemySpeed
            1.4f * multiplier,   // formationSwaySpeed
            1.2f * multiplier,   // formationSwayAmount
            std::max(0.3f, 0.6f/multiplier),   // attackInterval
            1.7f * multiplier,   // attackSpeed
            std::min(8, 6+ (level-10))
        );
        // Set enemyBulletSpeedBonus for levels beyond the predefined configs
        currentLevelConfig.enemyBulletSpeedBonus = 0.3f * level;
    }

    // Calculate auto-shoot interval based on level
    g_autoShootInterval = std::max(AUTO_SHOOT_MIN_INTERVAL, 1.2f - (level - 1) * AUTO_SHOOT_DECREASE_PER_LEVEL);
    LOGI("Auto-shoot interval set to %.2f seconds for level %d", g_autoShootInterval, level);
    
    LOGI("Enemy bullet speed bonus set to %.2f for level %d", currentLevelConfig.enemyBulletSpeedBonus, level);

    // Reset enemy formation
    initializeEnemies();

    // Reset game timer
    lastAttackTime = 0.0f;
    lastBulletTime = 0.0f;

    // CLear any bullet
    for (int i = 0; i < MAX_BULLETS; i++) {
        bullets[i].isActive = false;
    }

    for (int i=0; i<MAX_EXPLOSIONS; i++) {
        explosions[i].isActive = false;
    }

    std::cout << "Level " << level << " - Speed: " << currentLevelConfig.enemySpeed 
              << ", Attack Interval: " << currentLevelConfig.attackInterval << std::endl;
    
}

// Helper to update music based on game state
void updateBackgroundMusicForState() {
    if (!audioManager) return;
    switch (gameState) {
        case GameState::MENU:
            audioManager->setMusicVolume(0.6f);
            break;
        case GameState::PLAYING:
            audioManager->setMusicVolume(0.4f);
            break;
        case GameState::LEVEL_COMPLETE:
            audioManager->setMusicVolume(0.2f);
            break;
        case GameState::GAME_OVER:
        case GameState::GAME_WON:
            audioManager->setMusicVolume(0.3f); // Or 0.0f to mute
            break;
        default:
            break;
    }
}

void completeLevel() {
    levelComplete = true;
    levelTransitionTimer = 0.0f;
    
    gameState = GameState::LEVEL_COMPLETE;
    int levelBonus = 1000 * currentLevel;
    playerScore += levelBonus;
    std::cout << "Level " << currentLevel << " completed! Bonus: " << levelBonus << std::endl;
    updateBackgroundMusicForState();
}

void advanceToNextLevel() {
    currentLevel++;
    levelComplete = false;

    // Check if this is the last level
    if (maxLevel > 0 && currentLevel > maxLevel) {
        gameState = GameState::GAME_WON;
        std::cout << "You Won! Final Score: " << playerScore << std::endl;

        // Submit final score upon victory
        submitScoreToLeaderboard(playerScore);
        updateBackgroundMusicForState();
    } else {
        initializeLevel(currentLevel);
        gameState = GameState::PLAYING;
        updateBackgroundMusicForState();
    }
}

void resetGame() {
    currentLevel = 1;
    playerScore = 0;
    playerLives = 3;
    levelComplete = false;
    levelTransitionTimer = 0.0f;
    playerPosition = glm::vec3(0.0f, -2.0f, 0.0f);
    
    initializeLevel(currentLevel);
    gameState = GameState::PLAYING;
    
    std::cout << "Game reset to Level 1" << std::endl;
    updateBackgroundMusicForState();
}

// Initialize OpenGL resources
static bool initializeOpenGL() {
    LOGI("Initializing OpenGL ES");
    
    // Create shaders using Android asset constructor (same API as desktop version)
    enemyShader = new Shader(g_assetManager, "shaders/enemy.vs", "shaders/enemy.fs");
    if (enemyShader->ID == 0) {
        LOGE("Failed to create enemy shader");
        return false;
    }
    
    explosionShader = new Shader(g_assetManager, "shaders/explosion.vs", "shaders/explosion.fs");
    if (explosionShader->ID == 0) {
        LOGE("Failed to create explosion shader");
        return false;
    }
    
    backgroundShader = new Shader(g_assetManager, "shaders/background.vs", "shaders/background.fs");
    if (backgroundShader->ID == 0) {
        LOGE("Failed to create background shader");
        return false;
    }
    
    parallaxShader = new Shader(g_assetManager, "shaders/parallax.vs", "shaders/parallax.fs");
    if (parallaxShader->ID == 0) {
        LOGE("Failed to create parallax shader");
        return false;
    }
    
    textShader = new Shader(g_assetManager, "shaders/text.vs", "shaders/text.fs");
    if (textShader->ID == 0) {
        LOGE("Failed to create text shader");
        return false;
    }
    textShaderPtr = textShader;

    // Setup text rendering VAO
    glGenVertexArrays(1, &textVAO);
    glGenBuffers(1, &textVBO);

    glBindVertexArray(textVAO);
    glBindBuffer(GL_ARRAY_BUFFER, textVBO);
    glBufferData(GL_ARRAY_BUFFER, MAX_TEXT_TRIANGLES * 3 * 2 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glBindVertexArray(0);
    
    // Setup background VAO and VBO
    glGenVertexArrays(1, &g_backgroundVAO);
    glGenBuffers(1, &g_backgroundVBO);
    
    glBindVertexArray(g_backgroundVAO);
    glBindBuffer(GL_ARRAY_BUFFER, g_backgroundVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(backgroundVerticesNDC), backgroundVerticesNDC, GL_STATIC_DRAW);
    // Position attribute
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // Texture coordinate attribute
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    
    // Setup enemy VAO with instanced rendering (like desktop version)
    glGenVertexArrays(1, &g_enemyVAO);
    glGenBuffers(1, &g_enemyVBO);
    glGenBuffers(1, &g_instanceVBO);

    glBindVertexArray(g_enemyVAO);
    // Vertex data for quad
    glBindBuffer(GL_ARRAY_BUFFER, g_enemyVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    // Position attribute
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // Texture coordinate attribute
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    // Instance data for enemy positions
    glBindBuffer(GL_ARRAY_BUFFER, g_instanceVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec2) * TOTAL_ENEMIES, nullptr, GL_DYNAMIC_DRAW);
    // Instance offset attribute (location 2)
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(2);
    glVertexAttribDivisor(2, 1); // Tell OpenGL this is an instanced vertex attribute
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    
    // Setup bullet VAO (using same quad as enemies - like desktop)
    glGenVertexArrays(1, &g_bulletVAO);
    glGenBuffers(1, &g_bulletVBO);
    glBindVertexArray(g_bulletVAO);
    glBindBuffer(GL_ARRAY_BUFFER, g_bulletVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    // Position attribute
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // Texture coordinate attribute
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    // setup enemy shot VAO use same VBO from bullet

    glGenVertexArrays(1, &g_enemyShotVAO);
    glBindVertexArray(g_enemyShotVAO);
    glBindBuffer(GL_ARRAY_BUFFER, g_bulletVBO); // Reuse bullet VBO
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindVertexArray(0);


    // Setup Explosion VAO and VBO
    glGenVertexArrays(1, &g_explosionVAO);
    glGenBuffers(1, &g_explosionVBO);

    glBindVertexArray(g_explosionVAO);
    glBindBuffer(GL_ARRAY_BUFFER, g_explosionVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindVertexArray(0);

    // Generate and bind VAO
    glGenVertexArrays(1, &g_quadVAO);
    glBindVertexArray(g_quadVAO);
    // Generate and bind VBO
    glGenBuffers(1, &g_quadVBO);
    glBindBuffer(GL_ARRAY_BUFFER, g_quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    // Position attribute
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // Texture coordinate attribute
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    // Unbind
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    
    // Enable blending for transparency
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Load textures
    g_playerTexture = loadTextureFromAssets("textures/ship_1.png");
    g_enemyTexture = loadTextureFromAssets("textures/ship_4.png");
    g_bulletTexture = loadTextureFromAssets("textures/missiles.png");
    g_enemyMissileTexture = loadTextureFromAssets("textures/shot-2.png");

    std::string layerDir = "textures/background/Super Mountain Dusk Files/Assets/version A/Layers/";
    parallaxLayers.clear();
    parallaxLayers.push_back(ParallaxLayer(loadTextureFromAssets((layerDir + "sky.png").c_str()), 0.0f, "sky"));              // Static sky
    parallaxLayers.push_back(ParallaxLayer(loadTextureFromAssets((layerDir + "far-clouds.png").c_str()), 0.1f, "far-clouds"));   // Very slow
    parallaxLayers.push_back(ParallaxLayer(loadTextureFromAssets((layerDir + "far-mountains.png").c_str()), 0.2f, "far-mountains")); // Slow
    parallaxLayers.push_back(ParallaxLayer(loadTextureFromAssets((layerDir + "near-clouds.png").c_str()), 0.3f, "near-clouds"));  // Medium slow
    parallaxLayers.push_back(ParallaxLayer(loadTextureFromAssets((layerDir + "mountains.png").c_str()), 0.5f, "mountains"));    // Medium
    parallaxLayers.push_back(ParallaxLayer(loadTextureFromAssets((layerDir + "trees.png").c_str()), 0.8f, "trees"));          // Fast
    
    LOGI("Texture IDs loaded: Player=%u, Enemy=%u, Bullet=%u", g_playerTexture, g_enemyTexture, g_bulletTexture);

    enemyShader->use();
    enemyShader->setInt("texture_diffuse0", 0);

    parallaxShader->use();
    parallaxShader->setInt("backgroundTexture", 0);
    
    // Initialize game
    initializeEnemies();
    
    LOGI("OpenGL ES initialization complete");
    return true;
}

// Game update and render
static void updateGame(float deltaTime) {
    if (gameState == GameState::PAUSED) {
        return; // Skip updates while paused
    }
    // Update player position based on touch with relative movement
    if (g_isTouching) {
        if (g_useRelativeMovement) {
            // Calculate touch delta from initial touch position
            float touchDeltaX = g_touchX - g_initialTouchX;
            
            // Convert touch delta to world space with proper scaling for 1:1 movement feel
            float worldDeltaX = touchDeltaX * PLAYER_MOVEMENT_SENSITIVITY;
            
            // Apply delta to player's starting position without excessive scaling
            playerPosition.x = g_playerStartX + worldDeltaX;
        } else {
            // Old absolute positioning (kept as fallback)
            playerPosition.x = g_touchX * g_aspectRatio * WORLD_HALF_WIDTH;
        }
        
        // Clamp player to actual world bounds
        float worldLeftBound = -g_aspectRatio * WORLD_HALF_WIDTH + 0.1f;  // Small padding
        float worldRightBound = g_aspectRatio * WORLD_HALF_WIDTH - 0.1f;   // Small padding
        
        if (playerPosition.x > worldRightBound) playerPosition.x = worldRightBound;
        if (playerPosition.x < worldLeftBound) playerPosition.x = worldLeftBound;
        
        LOGI("Player position: %.2f (delta: %.2f, start: %.2f)", 
             playerPosition.x, g_touchX - g_initialTouchX, g_playerStartX);
    }
    
    // Handle manual shooting (from touch)
    if (g_shouldShoot && (g_time - lastBulletTime) >= BULLET_COOLDOWN) {
        createBullet();
        lastBulletTime = g_time;
        g_shouldShoot = false;
    }
    
    // Handle auto-shooting if enabled and in playing state
    if (g_autoShootEnabled && gameState == GameState::PLAYING) {
        if (g_time - lastBulletTime >= g_autoShootInterval) {
            createBullet();
            lastBulletTime = g_time;
        }
    }
    
    // Only update game objects if game is active
    if (gameState == GameState::PLAYING) {
        updateEnemies(g_deltaTime);
        updateBullets(g_deltaTime);
        updateEnemyBullets(g_deltaTime);
        updateExplosions(g_deltaTime);
        
        // Update audio listener position to follow player
        if (audioManager) {
            audioManager->setListenerPosition(playerPosition.x, playerPosition.y, 0.0f);
        }
    }
}

static void renderGame() {

    // Update parallax layers only when they're being rendered (menu and game over states)
    if (gameState == GameState::MENU || gameState == GameState::GAME_OVER || gameState == GameState::GAME_WON) {
        for (auto& layer : parallaxLayers) {
            layer.offsetX += layer.scrollSpeed * g_deltaTime * 0.1f; // Slow down the effect
            // Wrap around when offset gets too large
            if (layer.offsetX > 1.0f) {
                layer.offsetX -= 1.0f;
            }
        }
    }

    // Only update game objects if game is active
    if (gameState == GameState::PLAYING) {
        updateEnemies(g_deltaTime);
        updateBullets(g_deltaTime);
        updateEnemyBullets(g_deltaTime);
        updateExplosions(g_deltaTime);

        // Check win/lose conditions
        if (playerLives <= 0) {
            gameState = GameState::GAME_OVER;
            std::cout << "Game Over! Final Score: " << playerScore << std::endl;

            // Submit final score to Google Play Games leaderboard
            submitScoreToLeaderboard(playerScore);
        } else if (aliveEnemyPositions.empty() && !levelComplete) {
            // Level complete condition
            completeLevel();
        }
    }

    // Handle level transition state
    if (gameState == GameState::LEVEL_COMPLETE) {
        levelTransitionTimer += g_deltaTime;
        
        if (levelTransitionTimer >= LEVEL_TRANSITION_DURATION) {
            advanceToNextLevel();
        }
    }

    // Clear screen
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    
    // Create matrices like desktop version
    glm::mat4 projection = createOrthographicMatrix(-g_aspectRatio * WORLD_HALF_WIDTH, g_aspectRatio * WORLD_HALF_WIDTH,
                                                    -WORLD_HALF_HEIGHT, WORLD_HALF_HEIGHT);
    glm::mat4 view = glm::mat4(1.0f);
    view = glm::translate(view, glm::vec3(0.0f, 0.0f, -5.0f)); // Move camera back like desktop

    // ===== MENU STATE =====
    if (gameState == GameState::MENU) {
        // Render parallax background layers for menu
    glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        parallaxShader->use();

        for (const auto& layer : parallaxLayers) {
            parallaxShader->setFloat("offsetX", layer.offsetX);
            parallaxShader->setFloat("alpha", 1.0f);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, layer.texture);
            glBindVertexArray(g_backgroundVAO);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }

        glBindVertexArray(0);

        // Render menu buttons with proper centering
        for (const auto& button : menuButtons) {
            renderText(button.text.c_str(), button.pixelX, button.pixelY, button.scale, button.color);
        }

        return;
    }

    // ===== GAME OVER / WIN STATES =====
    if (gameState == GameState::GAME_OVER || gameState == GameState::GAME_WON) {
        // Render parallax background layers for game over/win screens
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        parallaxShader->use();

        for (const auto& layer : parallaxLayers) {
            parallaxShader->setFloat("offsetX", layer.offsetX);
            parallaxShader->setFloat("alpha", 1.0f);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, layer.texture);
            glBindVertexArray(g_backgroundVAO);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }

        glBindVertexArray(0);

        // Render text on top of background
        std::string message = (gameState == GameState::GAME_OVER) ? "GAME OVER" : "YOU WON!";
        std::string scoreText = "SCORE: " + std::to_string(playerScore);
        std::string restartText = "PRESS R TO RESTART";
        
        renderText(message.c_str(), currentWindowWidth/2.0f - 200.0f, currentWindowHeight/2.0f - 200.0f, 10.0f,
                   glm::vec3(1.0f, 0.0f, 0.0f));
        renderText(scoreText.c_str(), currentWindowWidth/2.0f - 180.0f, currentWindowHeight/2.0f, 8.0f,
                   glm::vec3(1.0f, 1.0f, 0.0f));
        renderText(restartText.c_str(), currentWindowWidth/2.0f - 210.0f, currentWindowHeight/2.0f + 100.0f, 5.0f,
                   glm::vec3(0.8f, 0.8f, 1.0f));
        return;
    }

    // ===== LEVEL COMPLETE STATE =====
    if (gameState == GameState::LEVEL_COMPLETE) {
        // Render animated starfield background (same as playing state)
        glDisable(GL_DEPTH_TEST);
        backgroundShader->use();
        backgroundShader->setFloat("time", g_time);
        backgroundShader->setFloat("alpha", 1.0f);

        glActiveTexture(GL_TEXTURE0);
        glBindVertexArray(g_backgroundVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);

        // Render level complete text
        std::string message = "LEVEL " + std::to_string(currentLevel) + " COMPLETE!";
        std::string bonusText = "SCORE: " + std::to_string(playerScore);
        std::string nextLevelText = "ADVANCING TO LEVEL " + std::to_string(currentLevel + 1);
        std::string tapText = "TAP TO CONTINUE";
        
        // Center the text (adjust positioning as needed for your screen)
        float centerX = currentWindowWidth / 2.0f;
        float centerY = currentWindowHeight / 2.0f;
        
        renderText(message.c_str(), centerX - 200.0f, centerY - 100.0f, 8.0f, 
                glm::vec3(0.0f, 1.0f, 0.0f));
        renderText(bonusText.c_str(), centerX - 100.0f, centerY - 20.0f, 6.0f, 
                glm::vec3(1.0f, 1.0f, 0.0f));
        renderText(nextLevelText.c_str(), centerX - 180.0f, centerY + 60.0f, 6.0f, 
                glm::vec3(1.0f, 1.0f, 1.0f));
        renderText(tapText.c_str(), centerX - 120.0f, centerY + 140.0f, 5.0f, 
                glm::vec3(0.8f, 0.8f, 1.0f));
        
        return;
    }
    

    // ============ PLAYING STATE - GAME RENDERING ==============
    // Render animated starfield background (like desktop version)
    glDisable(GL_DEPTH_TEST);
    backgroundShader->use();
    backgroundShader->setFloat("time", g_time);
    backgroundShader->setFloat("alpha", 1.0f);

    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(g_backgroundVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    
    // Draw player with texture
    drawQuad(playerPosition.x, playerPosition.y, 0.3f, 0.3f, 1.0f, 1.0f, 1.0f, 1.0f, nullptr, g_playerTexture);
    
    // Draw enemies with instanced rendering (like desktop version)
    if(aliveEnemyPositions.size() > 0) {
        enemyShader->use();
        
        // Base transformation for all enemies (scaling only)
        glm::mat4 enemyModel = glm::mat4(1.0f);
        enemyModel = glm::scale(enemyModel, glm::vec3(0.20f, 0.20f, 0.20f));
        
        // Set uniforms using Shader class methods (like desktop)
        enemyShader->setMat4("projection", projection);
        enemyShader->setMat4("view", view);
        enemyShader->setMat4("model", enemyModel);
        
        // Update instance buffer with enemy positions
        glBindBuffer(GL_ARRAY_BUFFER, g_instanceVBO);
        glBufferSubData(GL_ARRAY_BUFFER, 0, aliveEnemyPositions.size() * sizeof(glm::vec2), aliveEnemyPositions.data());
        
        // Bind enemy texture
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, g_enemyTexture);
        // Draw all enemies with instanced rendering
        glBindVertexArray(g_enemyVAO);
        glDrawArraysInstanced(GL_TRIANGLES, 0, 6, aliveEnemyPositions.size());
        glBindVertexArray(0);
    }
    
    // Draw bullets (using enemy shader like desktop)
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (bullets[i].isActive) {
            enemyShader->use();
            // Set matrices (same as desktop)
            
            // Create transformation matrix for this bullet (same as desktop)
            glm::mat4 bulletModel = glm::mat4(1.0f);
            bulletModel = glm::translate(bulletModel, glm::vec3(bullets[i].position.x, bullets[i].position.y, 0.0f));
            bulletModel = glm::scale(bulletModel, glm::vec3(0.5f, 0.5f, 0.5f)); // Smaller and taller for bullet shape (same as desktop)
            
            // Set uniforms
            enemyShader->setMat4("view", view);
            enemyShader->setMat4("projection", projection);
            enemyShader->setMat4("model", bulletModel);
            
            // Bind bullet texture (same as desktop)
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, g_bulletTexture);
            glBindVertexArray(g_bulletVAO);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            glBindVertexArray(0);
        }
    }

    // Draw Enemy Bullets
    enemyShader->use();
    for (int i = 0; i < MAX_ENEMY_BULLETS; i++) {
        if (enemyBullets[i].isActive) {
            glm::mat4 bulletModel = glm::mat4(1.0f);
            bulletModel = glm::translate(bulletModel, glm::vec3(enemyBullets[i].position.x, enemyBullets[i].position.y, 0.0f));
            bulletModel = glm::scale(bulletModel, glm::vec3(0.4f, 0.4f, 1.0f));
            enemyShader->setMat4("model", bulletModel);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, g_enemyMissileTexture);
            glBindVertexArray(g_enemyShotVAO);
            glDrawArrays(GL_TRIANGLES, 0, 6);
        }
    }
    
    // Draw explosions with special shader
    glDisable(GL_DEPTH_TEST); // Ensure explosions are always visible
    glEnable(GL_BLEND); // Enable transparency for explosions
    glBlendFunc(GL_SRC_ALPHA, GL_ONE); // Additive blending for more boom!
    for (int i = 0; i < MAX_EXPLOSIONS; i++) {
        if (explosions[i].isActive) {  
            explosionShader->use();
            explosionShader->setMat4("view", view);
            explosionShader->setMat4("projection", projection);

            // Send correct explosion-specific time and progress
            explosionShader->setFloat("explosionTime", explosions[i].timer);
            explosionShader->setFloat("explosionDuration", explosions[i].duration);
            explosionShader->setVec2("explosionCenter", explosions[i].position);

            // Calculate explosion progress (0.0 to 1.0)
            float progress = explosions[i].timer / explosions[i].duration;
            explosionShader->setFloat("explosionProgress", progress);
            explosionShader->setFloat("currentTime", explosions[i].timer); // For additional effects

            glm::mat4 explosionModel = glm::mat4(1.0f);
            explosionModel = glm::translate(explosionModel, glm::vec3(explosions[i].position.x, explosions[i].position.y, 0.0f));
            explosionModel = glm::scale(explosionModel, glm::vec3(0.5f, 0.5f, 1.0f)); // Control explosion size
            explosionShader->setMat4("model", explosionModel);

            glBindVertexArray(g_explosionVAO);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            glBindVertexArray(0);
        }
    }
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // Reset blending

    // Add HUD display
    std::string levelText = "LEVEL: " + std::to_string(currentLevel);
    std::string scoreText = "SCORE: " + std::to_string(playerScore);
    std::string livesText = "LIVES: " + std::to_string(playerLives);

    renderText(levelText.c_str(), 20.0f, 20.0f, 1.5f, glm::vec3(1.0f, 1.0f, 1.0f));
    renderText(scoreText.c_str(), 20.0f, 50.0f, 1.5f, glm::vec3(1.0f, 1.0f, 0.0f));
    renderText(livesText.c_str(), 20.0f, 80.0f, 1.5f, glm::vec3(1.0f, 0.0f, 0.0f));
    
    // ===== PAUSED OVERLAY =====
    if (gameState == GameState::PAUSED) {
        std::string pausedText = "PAUSED";
        float centerX = currentWindowWidth / 2.0f - 120.0f; // rough centering
        float centerY = currentWindowHeight / 2.0f;
        renderText(pausedText.c_str(), centerX, centerY, 7.0f, glm::vec3(1.0f, 1.0f, 1.0f));
    }
  }
  
  // Get current time in seconds
static float getCurrentTime() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9f;
}

// JNI Functions
extern "C" JNIEXPORT void JNICALL
Java_com_example_invaders_MainActivity_nativeSetAssetManager(JNIEnv *env, jobject thiz, jobject assetManager) {
    g_assetManager = AAssetManager_fromJava(env, assetManager);
    LOGI("Asset manager set");
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_invaders_MainActivity_nativeOnSurfaceCreated(JNIEnv *env, jobject thiz) {
    LOGI("Surface created");
    
    if (!g_isInitialized) {
        srand(time(nullptr)); // Initialize random seed
        
        // Initialize audio manager once
        if (audioManager == nullptr) {
            audioManager = new AudioManager(16);
            if (!audioManager->initialize(env, thiz)) {
                LOGE("Failed to initialize audio manager!");
                delete audioManager;
                audioManager = nullptr;
            } else {
                // Load sound effects from assets
                audioManager->loadSound("hit", "audio/Retro Explosion Short 01.wav");
                audioManager->loadSound("laser", "audio/Retro Gun Laser SingleShot 01.wav");
                audioManager->loadSound("explosion", "audio/Retro Impact LoFi 09.wav");
                // Load background music
                audioManager->loadMusic("bgm", "audio/background1.wav");
                LOGI("Audio system loaded successfully (initial)");
            }
        } else {
            LOGI("Reusing existing AudioManager instance");
        }
        
        g_isInitialized = initializeOpenGL();
        g_lastTime = getCurrentTime();

        // Initialize level system
        initializeLevel(currentLevel);
        LOGI("Level system initialized - Starting at level %d", currentLevel);
        recalculateFormationLayout();

        // Start background music only once
        if (audioManager && !g_musicStarted) {
            audioManager->playMusic("bgm", 0.6f); // Start at menu volume
            g_musicStarted = true;
        }
    }

    // Store JavaVM and activity for callbacks
    if (!g_javaVM) {
        env->GetJavaVM(&g_javaVM);
    }
    if (!g_mainActivityObj) {
        g_mainActivityObj = env->NewGlobalRef(thiz);
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_invaders_MainActivity_nativeOnSurfaceChanged(JNIEnv *env, jobject thiz, 
                                                             jint width, jint height) {
    LOGI("Surface changed: %dx%d", width, height);
    
    g_screenWidth = width;
    g_screenHeight = height;
    g_aspectRatio = (float)width / (float)height;
    
    glViewport(0, 0, width, height);

    // Update current window dimensions for text rendering
    currentWindowWidth = width;
    currentWindowHeight = height;
    
    // Reinitialize menu buttons with correct dimensions
    if (g_isInitialized) {
        initMenuButtons();
    }

    // Update enemy spacing and formation start based on new screen dimensions
    recalculateFormationLayout();
    initializeEnemies();

    glViewport(0, 0, width, height);

}

extern "C" JNIEXPORT void JNICALL
Java_com_example_invaders_MainActivity_nativeOnDrawFrame(JNIEnv *env, jobject thiz) {
    if (!g_isInitialized) return;
    
    // Calculate delta time
    float currentTime = getCurrentTime();
    g_deltaTime = currentTime - g_lastTime;
    g_lastTime = currentTime;
    g_time += g_deltaTime;
    
    // Update and render game
    updateGame(g_deltaTime);
    renderGame();
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_invaders_MainActivity_nativeOnTouchDown(JNIEnv *env, jobject thiz, 
                                                        jfloat x, jfloat y) {
    // Java is already passing normalized coordinates (-1 to 1)
    float ndcX = x;
    float ndcY = y;
    
    LOGI("Touch NDC: (%.3f, %.3f)", ndcX, ndcY);
    
    if (gameState == GameState::MENU) {
        // Check if touch is on any button
        for (auto& button : menuButtons) {
            LOGI("Button '%s' bounds: (%.3f, %.3f, %.3f, %.3f)", 
                button.text.c_str(), button.bounds.x, button.bounds.y, button.bounds.z, button.bounds.w);
            
            if (button.text == "CLICK TO START" &&
                ndcX >= button.bounds.x && ndcX <= button.bounds.z &&
                ndcY >= button.bounds.y && ndcY <= button.bounds.w) {
                
                gameState = GameState::PLAYING;
                LOGI("Starting game!");
                updateBackgroundMusicForState();
                break;
            }

            // Leaderboard tap
            if (button.text == "LEADERBOARD" &&
                ndcX >= button.bounds.x && ndcX <= button.bounds.z &&
                ndcY >= button.bounds.y && ndcY <= button.bounds.w) {
                LOGI("Leaderboard button tapped – opening leaderboard UI");
                showLeaderboard();
                break;
            }
        }
    } 
    // ADD LEVEL TRANSITION HANDLING
    else if (gameState == GameState::LEVEL_COMPLETE) {
        // Allow skipping level transition with any touch (like SPACE on desktop)
        advanceToNextLevel();
        LOGI("Level transition skipped by touch!");
        updateBackgroundMusicForState();
    }
    else if (gameState == GameState::PLAYING) {
        g_isTouching = true;
        
        // Store initial touch position and current player position for relative movement
        g_initialTouchX = ndcX / g_aspectRatio;
        g_playerStartX = playerPosition.x;
        
        g_touchX = ndcX / g_aspectRatio; // Convert to world coordinates
        g_touchY = ndcY;
        g_shouldShoot = true; // Shoot when touching
        
        LOGI("Touch start - Initial: %.2f, Player start: %.2f", g_initialTouchX, g_playerStartX);
    } 
    else if (gameState == GameState::GAME_OVER || gameState == GameState::GAME_WON) {
        // Restart game with level system (like R key on desktop)
        resetGame(); // This will reset to level 1 and call initializeLevel(1)
        LOGI("Game restarted via touch!");
        updateBackgroundMusicForState();
    }
}


extern "C" JNIEXPORT void JNICALL
Java_com_example_invaders_MainActivity_nativeOnTouchMove(JNIEnv *env, jobject thiz, 
                                                        jfloat x, jfloat y) {
    if (gameState == GameState::PLAYING) {
        // Java is already passing normalized coordinates (-1 to 1) - same as TouchDown
        float ndcX = x;
        float ndcY = y;
        
        g_touchX = ndcX / g_aspectRatio; // Convert to world coordinates (same as TouchDown)
        g_touchY = ndcY;
        
        LOGI("Touch move NDC: (%.3f, %.3f) -> World: (%.3f, %.3f)", ndcX, ndcY, g_touchX, g_touchY);
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_invaders_MainActivity_nativeOnTouchUp(JNIEnv *env, jobject thiz, 
                                                      jfloat x, jfloat y) {
    g_isTouching = false;
    LOGI("Touch up: (%.2f, %.2f)", x, y);
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_invaders_MainActivity_nativeOnPause(JNIEnv *env, jobject thiz) {
    LOGI("Game paused");
    if (gameState == GameState::PLAYING) {
        gameState = GameState::PAUSED;
    }
    if (audioManager && g_musicStarted) {
        audioManager->stopMusic();
        g_musicStarted = false;
    }
    // Mark GL context as lost so we rebuild resources on resume
    g_isInitialized = false;
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_invaders_MainActivity_nativeOnResume(JNIEnv *env, jobject thiz) {
    LOGI("Game resumed");
    g_lastTime = getCurrentTime();

    if (gameState == GameState::PAUSED) {
        gameState = GameState::PLAYING;
        // Restart music
        if (audioManager && !g_musicStarted) {
            audioManager->playMusic("bgm", 0.6f);
            g_musicStarted = true;
        }
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_invaders_MainActivity_nativeOnDestroy(JNIEnv *env, jobject thiz) {
    LOGI("Game destroyed");
    if (audioManager && g_musicStarted) {
        audioManager->stopMusic();
        g_musicStarted = false;
    }
    // Clean up OpenGL resources
    if (g_quadVAO) {
        glDeleteVertexArrays(1, &g_quadVAO);
        g_quadVAO = 0;
    }
    if (g_quadVBO) {
        glDeleteBuffers(1, &g_quadVBO);
        g_quadVBO = 0;
    }
    if (g_backgroundVAO) {
        glDeleteVertexArrays(1, &g_backgroundVAO);
        g_backgroundVAO = 0;
    }
    if (g_backgroundVBO) {
        glDeleteBuffers(1, &g_backgroundVBO);
        g_backgroundVBO = 0;
    }
    if (g_enemyVAO) {
        glDeleteVertexArrays(1, &g_enemyVAO);
        g_enemyVAO = 0;
    }
    if (g_enemyVBO) {
        glDeleteBuffers(1, &g_enemyVBO);
        g_enemyVBO = 0;
    }  
    if (g_instanceVBO) {
        glDeleteBuffers(1, &g_instanceVBO);
        g_instanceVBO = 0;
    }
    if (g_bulletVAO) {
        glDeleteVertexArrays(1, &g_bulletVAO);
        g_bulletVAO = 0;
    }
    if (g_bulletVBO) {
        glDeleteBuffers(1, &g_bulletVBO);
        g_bulletVBO = 0;
    }
    if (g_enemyShotVAO) {
        glDeleteVertexArrays(1, &g_enemyShotVAO);
        g_enemyShotVAO = 0;
    }
    if (g_explosionVAO) {
        glDeleteVertexArrays(1, &g_explosionVAO);
        g_explosionVAO = 0;
    }
    if (g_explosionVBO) {
        glDeleteBuffers(1, &g_explosionVBO);
        g_explosionVBO = 0;
    }

    if (g_playerTexture) {
        glDeleteTextures(1, &g_playerTexture);
        g_playerTexture = 0;
    }
    if (g_enemyTexture) {
        glDeleteTextures(1, &g_enemyTexture);
        g_enemyTexture = 0;
    }
    if (g_bulletTexture) {
        glDeleteTextures(1, &g_bulletTexture);
        g_bulletTexture = 0;
    }

    if (g_enemyMissileTexture) {
        glDeleteTextures(1, &g_enemyMissileTexture);
        g_enemyMissileTexture = 0;
    }
    
    // Cleanup parallax textures
    for (const auto& layer : parallaxLayers) {
        glDeleteTextures(1, &layer.texture);
    }
    
    // Cleanup text rendering
    if (textVAO) {
        glDeleteVertexArrays(1, &textVAO);
        textVAO = 0;
    }
    if (textVBO) {
        glDeleteBuffers(1, &textVBO);
        textVBO = 0;
    }
    
    // Cleanup shader objects
    delete enemyShader;
    delete explosionShader;
    delete backgroundShader;
    delete textShader;
    delete parallaxShader;
    enemyShader = nullptr;
    explosionShader = nullptr;
    backgroundShader = nullptr;
    textShader = nullptr;
    parallaxShader = nullptr;
    textShaderPtr = nullptr;
    
    // Cleanup audio manager
    if (audioManager) {
        delete audioManager;
        audioManager = nullptr;
    }
    
    g_isInitialized = false;

    if (g_mainActivityObj) {
        env->DeleteGlobalRef(g_mainActivityObj);
        g_mainActivityObj = nullptr;
    }
}

// ===== Google Play Games bridge =====
// (duplicate JNI globals removed - definitions are at top)
// static JavaVM* g_javaVM = nullptr;
// static jobject g_mainActivityObj = nullptr; // Global ref to be able to call back

static void submitScoreToLeaderboard(long score) {
    if (!g_javaVM || !g_mainActivityObj) return;
    JNIEnv* env = nullptr;
    if (g_javaVM->AttachCurrentThread(&env, nullptr) != JNI_OK) return;
    jclass cls = env->GetObjectClass(g_mainActivityObj);
    if (!cls) return;
    jmethodID method = env->GetMethodID(cls, "submitScoreJNI", "(J)V");
    if (method) {
        env->CallVoidMethod(g_mainActivityObj, method, (jlong)score);
    }
}

static void showLeaderboard() {
    if (!g_javaVM || !g_mainActivityObj) return;
    JNIEnv* env = nullptr;
    if (g_javaVM->AttachCurrentThread(&env, nullptr) != JNI_OK) return;
    jclass cls = env->GetObjectClass(g_mainActivityObj);
    if (!cls) return;
    jmethodID method = env->GetMethodID(cls, "showLeaderboardJNI", "()V");
    if (method) {
        env->CallVoidMethod(g_mainActivityObj, method);
    }
}

// ... existing code ...
// Inside game logic when Game Over or Game Won occurs, submit score
// (add to appropriate sections further below)