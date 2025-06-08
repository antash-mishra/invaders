#include "glad/glad.h"
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <string>

#include "glm/detail/type_mat.hpp"
#include "glm/detail/type_vec.hpp"
#include "model.h"
#include "shader.h"
#include "camera.h"
#include "stb_image.h"

#include <filesystem>
#include <vector>
namespace fs = std::filesystem;

// GLFW function declarations
void framebuffer_size_callback(GLFWwindow *window, int width, int height);
void processInput(GLFWwindow *window);
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

// ===== GAME STATE =====
int playerScore = 0;
int playerLives = 3;
bool gameOver = false;
bool gameWon = false;

const int MAX_BULLETS = 10;  // Maximum bullets on screen
const float BULLET_SPEED = 6.0f;  // Speed of bullet movement
std::vector<Bullet> bullets(MAX_BULLETS);

const int MAX_EXPLOSIONS = 20;
std::vector<Explosion> explosions(MAX_EXPLOSIONS);  // Explosion pool


// The Width of the screen
const unsigned int SCREEN_WIDTH = 800;
// The height of the screen
const unsigned int SCREEN_HEIGHT = 600;

Camera camera(glm::vec3(0.0f, 0.0f, 5.0f));

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
    // positions (NDC)  // texCoords (for tiling)
    -1.0f,  1.0f,       0.0f, 2.0f,  // top left
    -1.0f, -1.0f,       0.0f, 0.0f,  // bottom left
     1.0f, -1.0f,       2.0f, 0.0f,  // bottom right
    -1.0f,  1.0f,       0.0f, 2.0f,  // top left
     1.0f, -1.0f,       2.0f, 0.0f,  // bottom right
     1.0f,  1.0f,       2.0f, 2.0f   // top right
};


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
        enemies[i].animationTimer += deltaTime;

        // Formation movement (side-to-side like Galaxian)
        float formationSway = sin(currentTime * 0.5f) * 0.3f;
        enemies[i].position.x = enemies[i].formationPosition.x + formationSway;

        // Start dual attack if enough time has passed and no enemies are attacking
        if (!enemies[i].isAttacking && attackingCount == 0 && 
            (currentTime - lastAttackTime) >= ATTACK_INTERVAL) {
            
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
                
                enemies[i].attackSpeed = 0.8f + (rand() % 100) / 300.0f; // Vary speed
                
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
        if (enemies[i].isAlive && !gameOver &&
            checkCollision(enemies[i].position, ENEMY_RADIUS, 
                         glm::vec2(playerPosition.x, playerPosition.y), PLAYER_RADIUS)) {

            createExplosion(enemies[i].position);

            // Player hit by enemy!
            enemies[i].isAlive = false; // Destroy the enemy that hit player
            playerLives--;
            
            std::cout << "Player hit! Lives remaining: " << playerLives << std::endl;
            
            if (playerLives <= 0) {
                gameOver = true;
                std::cout << "Game Over! Final Score: " << playerScore << std::endl;
                std::cout << "Press 'R' to restart the game." << std::endl;
            }
        }

        // Add to alive positions for rendering
        if (enemies[i].isAlive) {
            aliveEnemyPositions.push_back(enemies[i].position);
        }
    }
    
    // Check win condition
    if (!gameWon && !gameOver && aliveEnemyPositions.empty()) {
        gameWon = true;
        std::cout << "Congratulations! You won! Final Score: " << playerScore << std::endl;
        std::cout << "Press 'R' to restart the game." << std::endl;
    }
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

    // Load shaders
    std::string shaderDir = parentDir + "/resources/shaders/";
    Shader playerShader((shaderDir + "playerModel.vs").c_str(), (shaderDir + "playerModel.fs").c_str());
    Shader enemyShader((shaderDir + "enemy.vs").c_str(), (shaderDir + "enemy.fs").c_str());
    Shader backgroundShader((shaderDir + "background.vs").c_str(), (shaderDir + "background.fs").c_str());
    Shader explosionShader((shaderDir + "explosion.vs").c_str(), (shaderDir + "explosion.fs").c_str());

    // load player model
    Model* player = new Model(parentDir + "/resources/Package/MeteorSlicer.obj");

    // Generate enemy formation positions (Galaxian style)
    initializeEnemies();

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
    

    // Load enemy texture
    stbi_set_flip_vertically_on_load(true);
    unsigned int enemyTexture = loadTexture(parentDir + "/resources/spaceship-pack/ship_4.png");
    
    // Load missile texture
    unsigned int missileTexture = loadTexture(parentDir + "/resources/spaceship-pack/missiles.png");

    //Load background texture
    // unsigned int backgroundTexture = loadTexture(parentDir + "/resources/    spaceship-pack/planet_1.png");

    // shader configuration
    // --------------------
    playerShader.use();
    playerShader.setInt("texture_diffuse1", 0);

    enemyShader.use();
    enemyShader.setInt("texture_diffuse0", 0);

    // explosionShader.use();
    // explosionShader.setInt("explosionTexture", 0); // Uncomment if using texture

    // backgroundShader.use();
    // backgroundShader.setInt("backgroundTexture", 0);

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

        // Only update game objects if game is active
        if (!gameOver && !gameWon) {
            updateEnemies(deltaTime);
            updateBullets(deltaTime);
            updateExplosions(deltaTime);
        }

        // render
        // ------
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);


        // For 2D game use orthographic projection
        glm::mat4 view = camera.GetViewMatrix();
        glm::mat4 projection = glm::ortho(-WORLD_HALF_WIDTH, WORLD_HALF_WIDTH, -WORLD_HALF_HEIGHT, WORLD_HALF_HEIGHT, 0.1f, 100.0f);
        


        // Render background
        glDisable(GL_DEPTH_TEST);
        backgroundShader.use();
        backgroundShader.setFloat("time", currentFrame);
        backgroundShader.setFloat("alpha", 0.3f);

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

    glfwTerminate();
    return 0;
}


// process all input: query GLFW whether relevant keys are pressed/released this
// frame and react accordingly
// ---------------------------------------------------------------------------------------------------------
void processInput(GLFWwindow *window) {
    float currentTime = glfwGetTime();
    const float moveSpeed = playerSpeed * deltaTime;

    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    // Handle game restart
    if ((gameOver || gameWon) && glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
        // Reset game state
        gameOver = false;
        gameWon = false;
        playerScore = 0;
        playerLives = 3;
        playerPosition = glm::vec3(0.0f, -2.0f, 0.0f);
        
        // Reset enemies
        initializeEnemies();
        
        // Reset bullets
        for (int i = 0; i < MAX_BULLETS; i++) {
            bullets[i].isActive = false;
        }
        
        // Reset explosions
        for (int i = 0; i < MAX_EXPLOSIONS; i++) {
            explosions[i].isActive = false;
        }
        
        std::cout << "Game restarted!" << std::endl;
        return;
    }

    // Only allow movement and shooting if game is active
    if (!gameOver && !gameWon) {
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            playerPosition.y += moveSpeed;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            playerPosition.y -= moveSpeed;
        if(glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            playerPosition.x -= moveSpeed;
        if(glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
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
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        std::cout << "Texture loaded successfully: " << path << std::endl;
    } else {
        std::cout << "Failed to load texture: " << path << std::endl;
    }

    stbi_image_free(data);
    return textureID;
}
