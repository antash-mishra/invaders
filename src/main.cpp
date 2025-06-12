#include "glad/glad.h"
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <string>
#include <vector>
#include <float.h>

#include "glm/detail/type_mat.hpp"
#include "glm/detail/type_vec.hpp"
#include "model.h"
#include "shader.h"
#include "camera.h"
#include "stb_image.h"
#include "audio_manager.h"
#include "stb_easy_font.h"

#include <filesystem>
namespace fs = std::filesystem;

// GLFW function declarations
void framebuffer_size_callback(GLFWwindow *window, int width, int height);
void processInput(GLFWwindow *window);
void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
unsigned int loadTexture(const std::string& path);

// ===== ENEMY TRACKING SYSTEM =====
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

    Enemy() : position(0.0f), velocity(0.0f), isAlive(true), type(GRUNT),
              health(1.0f), scale(1.0f), animationTimer(0.0f),
              isAttacking(false), formationPosition(0.0f),
              attackTimer(0.0f), attackStartPos(0.0f), attackTargetPos(0.0f),
              attackPattern(0), attackSpeed(0.7f) {}
};
 
// ===== BULLET SYSTEM =====
struct Bullet {
    glm::vec2 position;
    glm::vec2 velocity;
    bool isActive;
    
    Bullet() : position(0.0f), velocity(0.0f), isActive(false) {}
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

// Level difficulty parameters
struct LevelConfig {
    float enemySpeed;           // Base enemy movement speed
    float formationSwaySpeed;   // How fast formation moves side to side
    float formationSwayAmount; // How far formation moves side to side
    float attackInterval;      // Time between enemy attacks
    float attackSpeed;         // Speed of attacking enemies
    int maxSimultaneousAttacks; // Max enemies attacking at once
    float bulletSpeedMultiplier; // Enemy bullet speed (if you add enemy bullets)
    
    LevelConfig(float speed = 1.0f, float swaySpeed = 0.5f, float swayAmount = 0.3f,
                float interval = 2.0f, float attackSpd = 0.8f, int maxAttacks = 2)
        : enemySpeed(speed), formationSwaySpeed(swaySpeed), formationSwayAmount(swayAmount),
          attackInterval(interval), attackSpeed(attackSpd), maxSimultaneousAttacks(maxAttacks),
          bulletSpeedMultiplier(1.0f) {}
};

// ===== COLLISION DETECTION =====
// Simple circular collision detection
bool checkCollision(glm::vec2 pos1, float radius1, glm::vec2 pos2, float radius2) {
    float distance = glm::length(pos1 - pos2);
    return distance < (radius1 + radius2);
}

// Game object sizes for collision detection
const float PLAYER_RADIUS = 0.15f;      // Player collision radius
const float ENEMY_RADIUS = 0.12f;       // Enemy collision radius  
const float BULLET_RADIUS = 0.05f;      // Bullet collision radius

// ===== GAME STATE SYSTEM =====
enum class GameState { 
    MENU, 
    PLAYING, 
    LEVEL_COMPLETE,
    GAME_OVER, 
    GAME_WON 
};
GameState gameState = GameState::MENU;

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

std::vector<TextButton> menuButtons;

// Text rendering globals
unsigned int textVAO = 0, textVBO = 0;
const int MAX_TEXT_TRIANGLES = 1024;
Shader* textShaderPtr = nullptr;

// ===== GAME STATE =====
int playerScore = 0;
int playerLives = 3;

// ===== LEVEL SYSTEM =====
int currentLevel = 1;
int maxLevel = 10;  // Maximum level (or set to -1 for infinite)
bool levelComplete = false;
float levelTransitionTimer = 0.0f;
const float LEVEL_TRANSITION_DURATION = 3.0f;  // 3 seconds between levels

// Difficulty progression for each level
std::vector<LevelConfig> levelConfigs = {
    LevelConfig(1.0f, 0.5f, 0.3f, 2.0f, 0.8f, 2),   // Level 1
    LevelConfig(1.2f, 0.6f, 0.4f, 1.8f, 0.9f, 2),   // Level 2
    LevelConfig(1.4f, 0.7f, 0.5f, 1.6f, 1.0f, 3),   // Level 3
    LevelConfig(1.6f, 0.8f, 0.6f, 1.4f, 1.1f, 3),   // Level 4
    LevelConfig(1.8f, 0.9f, 0.7f, 1.2f, 1.2f, 4),   // Level 5
    LevelConfig(2.0f, 1.0f, 0.8f, 1.0f, 1.3f, 4),   // Level 6
    LevelConfig(2.2f, 1.1f, 0.9f, 0.9f, 1.4f, 5),   // Level 7
    LevelConfig(2.4f, 1.2f, 1.0f, 0.8f, 1.5f, 5),   // Level 8
    LevelConfig(2.6f, 1.3f, 1.1f, 0.7f, 1.6f, 6),   // Level 9
    LevelConfig(2.8f, 1.4f, 1.2f, 0.6f, 1.7f, 6),   // Level 10
};

// Current level configuration
LevelConfig currentLevelConfig;


const int MAX_BULLETS = 10;  // Maximum bullets on screen
const float BULLET_SPEED = 6.0f;  // Speed of bullet movement
std::vector<Bullet> bullets(MAX_BULLETS);

const int MAX_EXPLOSIONS = 20;
std::vector<Explosion> explosions(MAX_EXPLOSIONS);  // Explosion pool

AudioManager* audioManager = nullptr; // Audio manager for sound effects

// Initial window dimensions
const unsigned int SCREEN_WIDTH = 800;
const unsigned int SCREEN_HEIGHT = 600;

// Current window dimensions (updated on resize)
int currentWindowWidth = SCREEN_WIDTH;
int currentWindowHeight = SCREEN_HEIGHT;

// Time
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// Player position
glm::vec3 playerPosition = glm::vec3(0.0f, -2.0f, 0.0f);
const float playerSpeed = 2.0f;

// Screen bounds in world space (for orthographic projection)
const float WORLD_HALF_WIDTH = 4.0f;    // Half the orthographic width
const float WORLD_HALF_HEIGHT = 3.0f;   // Half the orthographic height

// Enemy formation constants (Galaxian style) - adjusted to fit within orthographic bounds
const int ENEMIES_PER_ROW = 10;
const int ENEMY_ROWS = 3;
const int TOTAL_ENEMIES = ENEMIES_PER_ROW * ENEMY_ROWS;
const float ENEMY_SPACING_X = 0.5f;      // Fits 10 enemies in [-4.0, 4.0] range
const float ENEMY_SPACING_Y = 0.5f;      // Fits 3 rows in upper bounds  
const float FORMATION_START_X = -3.0f;  // Centers formation in X bounds
const float FORMATION_START_Y = 2.0f;    // Positions formation in upper Y area

std::vector<Enemy> enemies(TOTAL_ENEMIES);
std::vector<glm::vec2> aliveEnemyPositions;

// Attack timing control
float lastAttackTime = 0.0f;
const float ATTACK_INTERVAL = 2.0f;  // Time between attacks (2 seconds)

// Bullet timing control
float lastBulletTime = 0.0f;
const float BULLET_COOLDOWN = 0.50f;  // 0.50 seconds between bullets

// Mouse initial position
float lastX = SCREEN_WIDTH/2.0;
float lastY = SCREEN_HEIGHT/2.0;
bool firstMouse = true;
float fov = 45.0f;

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

// Calculate curved attack position using Bezier curves
glm::vec2 calculateCurvedAttackPosition(const Enemy& enemy) {
    float t = enemy.attackTimer * enemy.attackSpeed * 0.3f; // Progress along path
    
    if (t <= 1.0f) {
        glm::vec2 start = enemy.attackStartPos;
        glm::vec2 target = enemy.attackTargetPos;
        
        // Create curved path with control points
        glm::vec2 controlPoint;
        
        if (enemy.attackPattern == 0) { // Left curve
            controlPoint = glm::vec2(start.x - 2.0f, (start.y + target.y) * 0.5f);
        } else if (enemy.attackPattern == 1) { // Right curve  
            controlPoint = glm::vec2(start.x + 2.0f, (start.y + target.y) * 0.5f);
        } else { // Direct path with slight curve
            controlPoint = glm::vec2((start.x + target.x) * 0.5f, start.y - 0.5f);
        }
        
        // Quadratic Bezier curve: P(t) = (1-t)²P₀ + 2(1-t)tP₁ + t²P₂
        float invT = 1.0f - t;
        return invT * invT * start + 
               2.0f * invT * t * controlPoint + 
               t * t * target;
    } else {
        // Continue moving straight down from target position (no jitter)
        float extraTime = (t - 1.0f) / (enemy.attackSpeed * 0.3f);
        return glm::vec2(enemy.attackTargetPos.x, 
                        enemy.attackTargetPos.y - enemy.attackSpeed * extraTime);
    }
}

void createExplosion(glm::vec2 position) {
    for (int i = 0; i < MAX_EXPLOSIONS; i++) {
        if (!explosions[i].isActive) {
            explosions[i].position = position;
            explosions[i].timer = 0.0f;
            explosions[i].duration = 1.2f; // Longer to enjoy the enhanced boom
            explosions[i].isActive = true;
            std::cout << "Explosion created at (" << position.x << ", " << position.y << ")" << std::endl;
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
    for (int row=0; row<ENEMY_ROWS; row++) {
        for (int col=0; col<ENEMIES_PER_ROW; col++) {
            float x = FORMATION_START_X + col * ENEMY_SPACING_X;
            float y = FORMATION_START_Y - row * ENEMY_SPACING_Y;

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
    }
}

// Create a new bullet at player position (from spaceship tip)
void createBullet() {
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!bullets[i].isActive) {
            // PLAY LASER SOUND
            if (audioManager) {
                audioManager->playSound("laser", 0.5f, 1.0f);
            }
            
            // Fire from the tip/front of the spaceship
            // Since spaceship is rotated 90 degrees, the "tip" is in the +Y direction
            bullets[i].position = glm::vec2(playerPosition.x, playerPosition.y + 0.15f); // From spaceship tip
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
                    
                    std::cout << "Enemy destroyed! Score: " << playerScore << std::endl;
                    break; // Bullet can only hit one enemy
                }
            }
            
            // Deactivate bullet if it goes off screen
            if (bullets[i].position.y > WORLD_HALF_HEIGHT + 1.0f) {
                bullets[i].isActive = false;
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
    
    return {ndcX0, ndcY0, ndcX1, ndcY1};  // x0,y0,x1,y1
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
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

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
    const char* titleText = "GALAXIAN";
    float titleScale = 4.0f;
    float titleWidth = getTextWidth(titleText, titleScale);
    float titleX = (currentWindowWidth - titleWidth) / 2.0f;
    TextButton titleButton(titleText, titleX, currentWindowHeight * 0.25f, titleScale, glm::vec3(1.0f, 1.0f, 1.0f));
    titleButton.bounds = calculateTextBounds(titleButton.text.c_str(), titleButton.pixelX, titleButton.pixelY, titleButton.scale);
    menuButtons.push_back(titleButton);
    
    // Add start button (centered)
    const char* startText = "CLICK TO START";
    float startScale = 2.5f;
    float startWidth = getTextWidth(startText, startScale);
    float startX = (currentWindowWidth - startWidth) / 2.0f;
    TextButton startButton(startText, startX, currentWindowHeight/2.0f, startScale, glm::vec3(1.0f, 1.0f, 0.0f));
    startButton.bounds = calculateTextBounds(startButton.text.c_str(), startButton.pixelX, startButton.pixelY, startButton.scale);
    menuButtons.push_back(startButton);
    
    // Add quit button (centered)
    const char* quitText = "PRESS ESC TO QUIT";
    float quitScale = 1.5f;
    float quitWidth = getTextWidth(quitText, quitScale);
    float quitX = (currentWindowWidth - quitWidth) / 2.0f;
    TextButton quitButton(quitText, quitX, currentWindowHeight/2.0f + 60.0f, quitScale, glm::vec3(0.8f, 0.8f, 1.0f));
    quitButton.bounds = calculateTextBounds(quitButton.text.c_str(), quitButton.pixelX, quitButton.pixelY, quitButton.scale);
    menuButtons.push_back(quitButton);
}

void updateEnemies(float deltaTime) {
    // Update alive enemies list for rendering
    aliveEnemyPositions.clear();
    
    float currentTime = glfwGetTime();
    int attackingCount = 0;

    // Count currently attacking enemies
    for (int i = 0; i < TOTAL_ENEMIES; i++) {
        if (enemies[i].isAlive && enemies[i].isAttacking) {
            attackingCount++;
        }
    }

    for (int i = 0; i < TOTAL_ENEMIES; i++) {
        if (!enemies[i].isAlive) continue;

        // Update animation timer
        enemies[i].animationTimer += deltaTime * currentLevelConfig.enemySpeed;

        // Formation movement (side-to-side like Galaxian)
        float formationSway = sin(currentTime * currentLevelConfig.formationSwaySpeed) * currentLevelConfig.formationSwayAmount;
        enemies[i].position.x = enemies[i].formationPosition.x + formationSway;

        // Start dual attack if enough time has passed and no enemies are attacking 
        if (!enemies[i].isAttacking && 
            attackingCount < currentLevelConfig.maxSimultaneousAttacks && 
            (currentTime - lastAttackTime) >= currentLevelConfig.attackInterval) {
            
            // Find leftmost and rightmost alive enemies
            int leftmostIndex = -1, rightmostIndex = -1;
            float leftmostX = 999.0f, rightmostX = -999.0f;
            
            for (int j = 0; j < TOTAL_ENEMIES; j++) {
                if (enemies[j].isAlive && !enemies[j].isAttacking) {
                    if (enemies[j].formationPosition.x < leftmostX) {
                        leftmostX = enemies[j].formationPosition.x;
                        leftmostIndex = j;
                    }
                    if (enemies[j].formationPosition.x > rightmostX) {
                        rightmostX = enemies[j].formationPosition.x;
                        rightmostIndex = j;
                    }
                }
            }
            
            // Start attack for both leftmost and rightmost enemies
            if (i == leftmostIndex || (i == rightmostIndex && leftmostIndex != rightmostIndex)) {
                enemies[i].isAttacking = true;
                enemies[i].attackTimer = 0.0f;
                enemies[i].attackStartPos = enemies[i].position;
                
                // Set target position (toward player with some randomness)
                enemies[i].attackTargetPos = glm::vec2(
                    playerPosition.x + (rand() % 200 - 100) / 300.0f, // Some randomness
                    playerPosition.y - 1.0f // Slightly below player
                );
                
                // Choose attack pattern based on position
                if (i == leftmostIndex) {
                    enemies[i].attackPattern = 1; // Right curve from left side
                } else {
                    enemies[i].attackPattern = 0; // Left curve from right side
                }
                
                enemies[i].attackSpeed = currentLevelConfig.attackSpeed;
                
                if (i == leftmostIndex) {
                    lastAttackTime = currentTime; // Set timer only once
                }
            }
        }

        // Update attacking enemies with curved motion
        if (enemies[i].isAttacking) {
            enemies[i].attackTimer += deltaTime;
            
            // Check bounds before updating position to prevent jitter
            glm::vec2 newPosition = calculateCurvedAttackPosition(enemies[i]);
            
            // Destroy enemy if it would go out of bounds (don't respawn)
            if (newPosition.y < -4.0f || 
                newPosition.x < -5.0f || newPosition.x > 5.0f) {
                enemies[i].isAlive = false; // Destroy permanently
                enemies[i].isAttacking = false;
            } else {
                // Only update position if within bounds
                enemies[i].position = newPosition;
            }
        }

        // Check collision with player
        if (enemies[i].isAlive && gameState == GameState::PLAYING &&
            checkCollision(enemies[i].position, ENEMY_RADIUS, 
                         glm::vec2(playerPosition.x, playerPosition.y), PLAYER_RADIUS)) {

            createExplosion(enemies[i].position);

            // PLAY HIT SOUND
            if (audioManager) {
                audioManager->playSound("hit", 1.0f, 1.0f);
            }

            // Player hit by enemy!
            enemies[i].isAlive = false; // Destroy the enemy that hit player
            playerLives--;
            
            std::cout << "Player hit! Lives remaining: " << playerLives << std::endl;
        }

        // Add to alive positions for rendering
        if (enemies[i].isAlive) {
            aliveEnemyPositions.push_back(enemies[i].position);
        }
    }
    
    // Win condition will be checked in main loop
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
    }

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

void completeLevel() {
    levelComplete = true;
    levelTransitionTimer = 0.0f;
    
    gameState = GameState::LEVEL_COMPLETE;
    int levelBonus = 1000 * currentLevel;
    playerScore += levelBonus;
    std::cout << "Level " << currentLevel << " completed! Bonus: " << levelBonus << std::endl;

}

void advanceToNextLevel() {
    currentLevel++;
    levelComplete = false;

    // Check if this is the last level
    if (maxLevel > 0 && currentLevel > maxLevel) {
        gameState = GameState::GAME_WON;
        std::cout << "You Won! Final Score: " << playerScore << std::endl;
    } else {
        initializeLevel(currentLevel);
        gameState = GameState::PLAYING;
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
}

int main(int argc, char *argv[])
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    glfwWindowHint(GLFW_RESIZABLE, true);

    GLFWwindow *window = glfwCreateWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Space Shooter", NULL, NULL);
    if (window == NULL)
    {
      std::cout << "Failed to create GLFW window" << std::endl;
      glfwTerminate();
      return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);
    // glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

        // glad: load all OpenGL function pointers
    // ---------------------------------------
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    // OpenGL configuration
    // --------------------
    glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);


    std::string parentDir = (fs::current_path().fs::path::parent_path()).string();
    std::cout << "Parent directory: " << parentDir << std::endl;

    // Initialize audio manager
    audioManager = new AudioManager(16);
    if (!audioManager->initialize()) {
        std::cerr << "Failed to initialize audio manager!" << std::endl;
        // Continue without audio (don't exit)
        delete audioManager;
        audioManager = nullptr;
    } else {
        // Load sound effects
        std::string audioDir = parentDir + "/resources/audio/FreeSFX/GameSFX/";
        audioManager->loadSound("hit", audioDir + "Explosion/Retro Explosion Short 01.wav");
        audioManager->loadSound("laser", audioDir + "Weapon/laser/Retro Gun Laser SingleShot 01.wav");
        audioManager->loadSound("explosion", audioDir + "Impact/Retro Impact LoFi 09.wav");
        std::cout << "Audio system loaded successfully" << std::endl;
    }

    // Load shaders
    std::string shaderDir = parentDir + "/resources/shaders/";
    Shader playerShader((shaderDir + "playerModel.vs").c_str(), (shaderDir + "playerModel.fs").c_str());
    Shader enemyShader((shaderDir + "enemy.vs").c_str(), (shaderDir + "enemy.fs").c_str());
    Shader backgroundShader((shaderDir + "background.vs").c_str(), (shaderDir + "background.fs").c_str());
    Shader parallaxShader((shaderDir + "parallax.vs").c_str(), (shaderDir + "parallax.fs").c_str());
    Shader explosionShader((shaderDir + "explosion.vs").c_str(), (shaderDir + "explosion.fs").c_str());
    Shader textShader((shaderDir + "text.vs").c_str(), (shaderDir + "text.fs").c_str());
    textShaderPtr = &textShader;

    // load player model
    Model* player = new Model(parentDir + "/resources/Package/MeteorSlicer.obj");

    // Generate enemy formation positions (Galaxian style)
    initializeEnemies();
    
    // Initialize menu system
    initMenuButtons();

    // Initialize level system
    initializeLevel(currentLevel);
    
    // Setup text rendering VAO
    glGenVertexArrays(1, &textVAO);
    glGenBuffers(1, &textVBO);
    
    glBindVertexArray(textVAO);
    glBindBuffer(GL_ARRAY_BUFFER, textVBO);
    glBufferData(GL_ARRAY_BUFFER, MAX_TEXT_TRIANGLES * 3 * 2 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glBindVertexArray(0);

    // Setup background VAO
    unsigned int backgroundVAO, backgroundVBO;
    glGenVertexArrays(1, &backgroundVAO);
    glGenBuffers(1, &backgroundVBO);

    glBindVertexArray(backgroundVAO);
    glBindBuffer(GL_ARRAY_BUFFER, backgroundVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(backgroundVerticesNDC), backgroundVerticesNDC, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindVertexArray(0);


    // Load enemy with instanced rendering
    unsigned int enemyVAO, enemyVBO, instanceVBO;
    glGenVertexArrays(1, &enemyVAO);
    glGenBuffers(1, &enemyVBO);
    glGenBuffers(1, &instanceVBO);

    glBindVertexArray(enemyVAO);

    // Vertex data
    glBindBuffer(GL_ARRAY_BUFFER, enemyVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    // Instance data
    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec2) * TOTAL_ENEMIES, nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glVertexAttribDivisor(2, 1); // Tell OpenGL this is an instanced vertex attribute

    // unbind the VAO and VBO
    glBindVertexArray(0);

    // Setup bullet VAO (using same quad as enemies)
    unsigned int bulletVAO, bulletVBO;
    glGenVertexArrays(1, &bulletVAO);
    glGenBuffers(1, &bulletVBO);

    glBindVertexArray(bulletVAO);
    glBindBuffer(GL_ARRAY_BUFFER, bulletVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindVertexArray(0);

    // Setup explosion VAO (using same quad as enemies)
    unsigned int explosionVAO, explosionVBO;
    glGenVertexArrays(1, &explosionVAO);
    glGenBuffers(1, &explosionVBO);

    glBindVertexArray(explosionVAO);
    glBindBuffer(GL_ARRAY_BUFFER, explosionVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindVertexArray(0);
    

    // Load parallax background layers (from back to front)
    stbi_set_flip_vertically_on_load(true); // Keep images right-side up for backgrounds
    std::string layerDir = parentDir + "/resources/background/Super Mountain Dusk Files/Assets/version A/Layers/";
    
    parallaxLayers.clear();
    parallaxLayers.push_back(ParallaxLayer(loadTexture(layerDir + "sky.png"), 0.0f, "sky"));              // Static sky
    parallaxLayers.push_back(ParallaxLayer(loadTexture(layerDir + "far-clouds.png"), 0.1f, "far-clouds"));   // Very slow
    parallaxLayers.push_back(ParallaxLayer(loadTexture(layerDir + "far-mountains.png"), 0.2f, "far-mountains")); // Slow
    parallaxLayers.push_back(ParallaxLayer(loadTexture(layerDir + "near-clouds.png"), 0.3f, "near-clouds"));  // Medium slow
    parallaxLayers.push_back(ParallaxLayer(loadTexture(layerDir + "mountains.png"), 0.5f, "mountains"));    // Medium
    parallaxLayers.push_back(ParallaxLayer(loadTexture(layerDir + "trees.png"), 0.8f, "trees"));          // Fast
    
    std::cout << "Loaded " << parallaxLayers.size() << " parallax layers" << std::endl;

    // Load enemy texture
    stbi_set_flip_vertically_on_load(true);
    unsigned int enemyTexture = loadTexture(parentDir + "/resources/spaceship-pack/ship_4.png");
    
    // Load missile texture
    unsigned int missileTexture = loadTexture(parentDir + "/resources/spaceship-pack/missiles.png");

    // shader configuration
    // --------------------
    playerShader.use();
    playerShader.setInt("texture_diffuse1", 0);

    enemyShader.use();
    enemyShader.setInt("texture_diffuse0", 0);

    parallaxShader.use();
    parallaxShader.setInt("backgroundTexture", 0);

    while (!glfwWindowShouldClose(window))
    {

        // calculate delta time
        // --------------------
        float currentFrame = static_cast<float>(glfwGetTime());
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // process input
        // -------------
        processInput(window);

        // Update parallax layers only when they're being rendered (menu and game over states)
        if (gameState == GameState::MENU || gameState == GameState::GAME_OVER || gameState == GameState::GAME_WON) {
            for (auto& layer : parallaxLayers) {
                layer.offsetX += layer.scrollSpeed * deltaTime * 0.1f; // Slow down the effect
                // Wrap around when offset gets too large
                if (layer.offsetX > 1.0f) {
                    layer.offsetX -= 1.0f;
                }
            }
        }

        // Only update game objects if game is active
        if (gameState == GameState::PLAYING) {
            updateEnemies(deltaTime);
            updateBullets(deltaTime);
            updateExplosions(deltaTime);
            
            // Check win/lose conditions
            if (playerLives <= 0) {
                gameState = GameState::GAME_OVER;
                std::cout << "Game Over! Final Score: " << playerScore << std::endl;
            } else if (aliveEnemyPositions.empty() && !levelComplete) {
                completeLevel();
            }
        }

        // Handle level transition state
        if (gameState == GameState::LEVEL_COMPLETE) {
            levelTransitionTimer += deltaTime;
            if (levelTransitionTimer >= LEVEL_TRANSITION_DURATION) { // 2 seconds for transition
                advanceToNextLevel();
            }
        }

        // render
        // ------
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);


        // For 2D game use orthographic projection
        glm::mat4 view = glm::mat4(1.0f);
        view = glm::translate(view, glm::vec3(0.0f, 0.0f, -5.0f));
        glm::mat4 projection = glm::ortho(
            -WORLD_HALF_WIDTH, 
            WORLD_HALF_WIDTH, 
            -WORLD_HALF_HEIGHT, 
            WORLD_HALF_HEIGHT, 
            0.1f, 
            100.0f
        );


        // ===== MENU STATE =====
        if (gameState == GameState::MENU) {
            // Render parallax background layers for menu
            glDisable(GL_DEPTH_TEST);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            
            parallaxShader.use();
            
            for (const auto& layer : parallaxLayers) {
                parallaxShader.setFloat("offsetX", layer.offsetX);
                parallaxShader.setFloat("alpha", 1.0f);
                
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, layer.texture);
                glBindVertexArray(backgroundVAO);
                glDrawArrays(GL_TRIANGLES, 0, 6);
            }
            
            glBindVertexArray(0);
            
            // Render menu buttons with proper centering
            for (const auto& button : menuButtons) {
                renderText(button.text.c_str(), button.pixelX, button.pixelY, button.scale, button.color);
            }
            
            glfwSwapBuffers(window);
            glfwPollEvents();
            continue;
        }

        // ===== GAME OVER / WIN STATES =====
        if (gameState == GameState::GAME_OVER || gameState == GameState::GAME_WON) {
            // Render parallax background layers for game over/win screens
            glDisable(GL_DEPTH_TEST);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            
            parallaxShader.use();
            
            for (const auto& layer : parallaxLayers) {
                parallaxShader.setFloat("offsetX", layer.offsetX);
                parallaxShader.setFloat("alpha", 1.0f);
                
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, layer.texture);
                glBindVertexArray(backgroundVAO);
                glDrawArrays(GL_TRIANGLES, 0, 6);
            }
            
            glBindVertexArray(0);
            
            // Render text on top of background
            std::string message = (gameState == GameState::GAME_OVER) ? "GAME OVER" : "YOU WON!";
            std::string scoreText = "SCORE: " + std::to_string(playerScore);
            std::string restartText = "PRESS R TO RESTART";
            
            renderText(message.c_str(), currentWindowWidth/2.0f - 80.0f, currentWindowHeight/2.0f - 50.0f, 3.0f, 
                      glm::vec3(1.0f, 0.0f, 0.0f));
            renderText(scoreText.c_str(), currentWindowWidth/2.0f - 60.0f, currentWindowHeight/2.0f, 2.0f, 
                      glm::vec3(1.0f, 1.0f, 0.0f));
            renderText(restartText.c_str(), currentWindowWidth/2.0f - 100.0f, currentWindowHeight/2.0f + 50.0f, 1.5f, 
                      glm::vec3(0.8f, 0.8f, 1.0f));
            
            glfwSwapBuffers(window);
            glfwPollEvents();
            continue;
        }

        // ===== LEVEL COMPLETE STATE =====
        if (gameState == GameState::LEVEL_COMPLETE) {
            // Render parallax background layers for level complete
            glDisable(GL_DEPTH_TEST);
            backgroundShader.use();
            backgroundShader.setFloat("time", currentFrame);
            backgroundShader.setFloat("alpha", 1.0f); // Full alpha for starfield visibility
    
            glActiveTexture(GL_TEXTURE0);
            // glBindTexture(GL_TEXTURE_2D, backgroundTexture);
            glBindVertexArray(backgroundVAO);
            glDrawArrays(GL_TRIANGLES, 0, 6);
            glBindVertexArray(0);

            // Render level complete text
            std::string message = "LEVEL " + std::to_string(currentLevel) + " COMPLETE!";
            std::string bonusText = "SCORE: " + std::to_string(playerScore);
            std::string nextLevelText = "ADVANCING TO LEVEL " + std::to_string(currentLevel + 1);
            
            renderText(message.c_str(), currentWindowWidth/2.0f - 150.0f, currentWindowHeight/2.0f - 50.0f, 3.0f, 
                      glm::vec3(1.0f, 1.0f, 1.0f));
            renderText(bonusText.c_str(), currentWindowWidth/2.0f - 100.0f, currentWindowHeight/2.0f, 2.5f, 
                      glm::vec3(1.0f, 1.0f, 0.5f));
            renderText(nextLevelText.c_str(), currentWindowWidth/2.0f - 150.0f, currentWindowHeight/2.0f + 50.0f, 2.5f, 
                      glm::vec3(1.0f, 1.0f, 1.0f));
            
            glfwSwapBuffers(window);
            glfwPollEvents();
            continue;
        }

        // ===== PLAYING STATE - GAME RENDERING =====
        glDisable(GL_DEPTH_TEST);
        backgroundShader.use();
        backgroundShader.setFloat("time", currentFrame);
        backgroundShader.setFloat("alpha", 1.0f); // Full alpha for starfield visibility

        glActiveTexture(GL_TEXTURE0);
        // glBindTexture(GL_TEXTURE_2D, backgroundTexture);
        glBindVertexArray(backgroundVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);
        
        playerShader.use();
        playerShader.setMat4("view", view);
        playerShader.setMat4("projection", projection);

        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, playerPosition);
        model = glm::scale(model, glm::vec3(0.07f, 0.07f, 0.07f));
        model = glm::rotate(model, glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        model = glm::rotate(model, glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        // model = glm::rotate(model, glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        playerShader.setMat4("model", model);
        player->Draw(playerShader);

        // Draw enemy formation (instanced)
        if(aliveEnemyPositions.size() > 0)
        {
            enemyShader.use();
            enemyShader.setMat4("view", view);
            enemyShader.setMat4("projection", projection);

            // Base transformation for all enemies
            glm::mat4 enemyModel = glm::mat4(1.0f);
            enemyModel = glm::scale(enemyModel, glm::vec3(0.25f, 0.25f, 0.25f));
            enemyShader.setMat4("model", enemyModel);

            // update VBO dynamically
            glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0, aliveEnemyPositions.size() * sizeof(glm::vec2), aliveEnemyPositions.data());

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, enemyTexture);
            glBindVertexArray(enemyVAO);
            glDrawArraysInstanced(GL_TRIANGLES, 0, 6, aliveEnemyPositions.size());
            glBindVertexArray(0);
        }

        // Draw bullets
        for (int i = 0; i < MAX_BULLETS; i++) {
            if (bullets[i].isActive) {
                enemyShader.use(); // Reuse enemy shader for bullets
                enemyShader.setMat4("view", view);
                enemyShader.setMat4("projection", projection);

                // Create transformation matrix for this bullet
                glm::mat4 bulletModel = glm::mat4(1.0f);
                bulletModel = glm::translate(bulletModel, glm::vec3(bullets[i].position.x, bullets[i].position.y, 0.0f));
                bulletModel = glm::scale(bulletModel, glm::vec3(0.5f, 0.6f, 1.0f)); // Smaller and taller for bullet shape
                enemyShader.setMat4("model", bulletModel);

                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, missileTexture);
                glBindVertexArray(bulletVAO);
                glDrawArrays(GL_TRIANGLES, 0, 6);
                glBindVertexArray(0);
            }
        }

        // Draw explosions (render on top)
        glDisable(GL_DEPTH_TEST); // Ensure explosions are always visible
        glEnable(GL_BLEND); // Enable transparency for explosions
        glBlendFunc(GL_SRC_ALPHA, GL_ONE); // Additive blending for more boom!
        for (int i = 0; i < MAX_EXPLOSIONS; i++) {
            if (explosions[i].isActive) {
                explosionShader.use();
                explosionShader.setMat4("view", view);
                explosionShader.setMat4("projection", projection);

                // Send correct explosion-specific time and progress
                explosionShader.setFloat("explosionTime", explosions[i].timer);
                explosionShader.setFloat("explosionDuration", explosions[i].duration);
                explosionShader.setVec2("explosionCenter", explosions[i].position);
                
                // Calculate explosion progress (0.0 to 1.0)
                float progress = explosions[i].timer / explosions[i].duration;
                explosionShader.setFloat("explosionProgress", progress);
                explosionShader.setFloat("currentTime", currentFrame); // For additional effects

                glm::mat4 explosionModel = glm::mat4(1.0f);
                explosionModel = glm::translate(explosionModel, glm::vec3(explosions[i].position.x, explosions[i].position.y, 0.0f));
                explosionModel = glm::scale(explosionModel, glm::vec3(0.5f, 0.5f, 1.0f)); // Control explosion size
                explosionShader.setMat4("model", explosionModel);
                
                // Debug output (uncomment to debug)
                // std::cout << "Rendering explosion " << i << " at progress: " << progress << " position: (" << explosions[i].position.x << ", " << explosions[i].position.y << ")" << std::endl;
                
                glBindVertexArray(explosionVAO);
                glDrawArrays(GL_TRIANGLES, 0, 6);
                glBindVertexArray(0);
            }
        }

        // Add HUD display
        std::string levelText = "LEVEL: " + std::to_string(currentLevel);
        std::string scoreText = "SCORE: " + std::to_string(playerScore);
        std::string livesText = "LIVES: " + std::to_string(playerLives);

        renderText(levelText.c_str(), 20.0f, 20.0f, 1.5f, glm::vec3(1.0f, 1.0f, 1.0f));
        renderText(scoreText.c_str(), 20.0f, 50.0f, 1.5f, glm::vec3(1.0f, 1.0f, 0.0f));
        renderText(livesText.c_str(), 20.0f, 80.0f, 1.5f, glm::vec3(1.0f, 0.0f, 0.0f));

        // Update audio listener position to follow player
        if (audioManager) {
            audioManager->setListenerPosition(playerPosition.x, playerPosition.y, 0.0f);
        }

        // glfw: swap buffers and poll IO events (keys pressed/released, mouse moved
        // etc.)
        // -------------------------------------------------------------------------------
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Cleanup resources
    glDeleteVertexArrays(1, &backgroundVAO);
    glDeleteBuffers(1, &backgroundVBO);
    glDeleteVertexArrays(1, &enemyVAO);
    glDeleteBuffers(1, &enemyVBO);
    glDeleteBuffers(1, &instanceVBO);
    glDeleteVertexArrays(1, &bulletVAO);
    glDeleteBuffers(1, &bulletVBO);
    glDeleteVertexArrays(1, &explosionVAO);
    glDeleteBuffers(1, &explosionVBO);
    glDeleteTextures(1, &enemyTexture);
    glDeleteTextures(1, &missileTexture);
    
    // Cleanup parallax textures
    for (const auto& layer : parallaxLayers) {
        glDeleteTextures(1, &layer.texture);
    }
    
    // Cleanup text rendering
    glDeleteVertexArrays(1, &textVAO);
    glDeleteBuffers(1, &textVBO);

    // Cleanup audio manager
    if (audioManager) {
        delete audioManager;
        audioManager = nullptr;
    }

    glfwTerminate();
    return 0;
}


// process all input: query GLFW whether relevant keys are pressed/released this
// frame and react accordingly
// ---------------------------------------------------------------------------------------------------------
void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_PRESS) return;
    
    if (gameState == GameState::MENU) {
        double mouseX, mouseY;
        glfwGetCursorPos(window, &mouseX, &mouseY);
        
        // Convert mouse position to NDC using current window dimensions
        float ndcX =  (float)mouseX / (currentWindowWidth  * 0.5f) - 1.0f;
        float ndcY = -(float)mouseY / (currentWindowHeight * 0.5f) + 1.0f;
        
        // Check if click is on start button
        for (auto& button : menuButtons) {
            if (button.text == "CLICK TO START" &&
                ndcX >= button.bounds.x && ndcX <= button.bounds.z &&
                ndcY >= button.bounds.y && ndcY <= button.bounds.w) {
                
                gameState = GameState::PLAYING;
                
                // Play click sound
                if (audioManager) {
                    audioManager->playSound("laser", 0.3f);
                }
                break;
            }
        }
    }
}

void processInput(GLFWwindow *window) {
    float currentTime = glfwGetTime();
    const float moveSpeed = playerSpeed * deltaTime;

    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    // Handle game restart
    if ((gameState == GameState::GAME_OVER || gameState == GameState::GAME_WON) && 
        glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
        // Reset game state
        resetGame(); // Use new reset function
        return;
    }

    // Allow skipping level transition
    if (gameState == GameState::LEVEL_COMPLETE && 
        glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
        advanceToNextLevel();
        return;
    }

    // Only allow movement and shooting if game is active
    if (gameState == GameState::PLAYING) {
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
            playerPosition.y += moveSpeed;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
            playerPosition.y -= moveSpeed;
        if(glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
            playerPosition.x -= moveSpeed;
        if(glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
            playerPosition.x += moveSpeed;

        // Handle bullet shooting with spacebar
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
            if (currentTime - lastBulletTime >= BULLET_COOLDOWN) {
                createBullet();
                lastBulletTime = currentTime;
            }
        }

        // Clamp player position to screen bounds
        playerPosition.x = glm::clamp(playerPosition.x, -WORLD_HALF_WIDTH, WORLD_HALF_WIDTH);
        playerPosition.y = glm::clamp(playerPosition.y, -WORLD_HALF_HEIGHT, WORLD_HALF_HEIGHT);
    }
}

void framebuffer_size_callback(GLFWwindow *window, int width, int height)
{
    // make sure the viewport matches the new window dimensions; note that width and
    // height will be significantly larger than specified on retina displays.
    glViewport(0, 0, width, height);
    
    // Update current window dimensions
    currentWindowWidth = width;
    currentWindowHeight = height;
    
    // Recalculate text button bounds for the new window size
    if (gameState == GameState::MENU) {
        initMenuButtons();
    }
}

unsigned int loadTexture(const std::string& path)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrChannels;
    unsigned char *data = stbi_load(path.c_str(), &width, &height, &nrChannels, 0);

    if (data) {
        GLenum format;
        if (nrChannels == 1)
            format = GL_RED;
        else if (nrChannels == 3)
            format = GL_RGB;
        else if (nrChannels == 4)
            format = GL_RGBA;

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        // Set texture wrapping to repeat for tiling effect
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); // Prevent vertical tiling
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        std::cout << "Texture loaded successfully: " << path << std::endl;
    } else {
        std::cout << "Failed to load texture: " << path << std::endl;
    }

    stbi_image_free(data);
    return textureID;
}
