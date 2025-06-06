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

    Enemy() : position(0.0f), velocity(0.0f), isAlive(true), type(GRUNT),
              health(1.0f), scale(1.0f), animationTimer(0.0f),
              isAttacking(false), formationPosition(0.0f) {}
};


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

// Enemy formation constants (Galaxian style)
const int ENEMIES_PER_ROW = 10;
const int ENEMY_ROWS = 3;
const int TOTAL_ENEMIES = ENEMIES_PER_ROW * ENEMY_ROWS;
const float ENEMY_SPACING_X = 1.6f;
const float ENEMY_SPACING_Y = 1.5f;
const float FORMATION_START_X = -7.5f;
const float FORMATION_START_Y = 5.7f;   // Top row Y position

std::vector<Enemy> enemies(TOTAL_ENEMIES);
std::vector<glm::vec2> aliveEnemyPositions;

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

void updateEnemies(float deltaTime) {
    // Update alive enemies list for rendering
    aliveEnemyPositions.clear();

    for (int i = 0; i < TOTAL_ENEMIES; i++) {
        if (!enemies[i].isAlive) continue;

        // Update animation timer
        enemies[i].animationTimer += deltaTime;

        // Formation movement (side-to-side like Galaxian)
        float formationSway = sin(glfwGetTime() * 0.5f) * 0.3f;
        enemies[i].position.x = enemies[i].formationPosition.x + formationSway;

        // Individual enemy attack behavior (random chance)
        if (!enemies[i].isAttacking && rand() % 10000 < 1) { // Very low chance
            enemies[i].isAttacking = true;
            enemies[i].velocity.y = -1.5f; // Move down towards player
            enemies[i].velocity.x = (rand() % 200 - 100) / 100.0f; // Random x movement
        }

        // Update attacking enemies
        if (enemies[i].isAttacking) {
            enemies[i].position += enemies[i].velocity * deltaTime;

            // Return to formation if enemy goes too low
            if (enemies[i].position.y < -8.0f) {
                enemies[i].isAttacking = false;
                enemies[i].position = enemies[i].formationPosition;
                enemies[i].velocity = glm::vec2(0.0f, 0.0f);
            }
        }

        // Add to alive positions for rendering
        aliveEnemyPositions.push_back(enemies[i].position);
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

    // Load enemy texture
    stbi_set_flip_vertically_on_load(true);
    unsigned int enemyTexture = loadTexture(parentDir + "/resources/spaceship-pack/ship_4.png");

    // // Load background texture
    // unsigned int backgroundTexture = loadTexture(parentDir + "/resources/spaceship-pack/planet_1.png");

    // shader configuration
    // --------------------
    playerShader.use();
    playerShader.setInt("texture_diffuse1", 0);

    enemyShader.use();
    enemyShader.setInt("texture_diffuse0", 0);

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

        updateEnemies(deltaTime);

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
            glDrawArraysInstanced(GL_TRIANGLES, 0, 6, TOTAL_ENEMIES);
            glBindVertexArray(0);
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
    glDeleteTextures(1, &enemyTexture);

    glfwTerminate();
    return 0;
}


// process all input: query GLFW whether relevant keys are pressed/released this
// frame and react accordingly
// ---------------------------------------------------------------------------------------------------------
void processInput(GLFWwindow *window) {

    const float moveSpeed = playerSpeed * deltaTime;

    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        playerPosition.y += moveSpeed;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        playerPosition.y -= moveSpeed;
    if(glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        playerPosition.x -= moveSpeed;
    if(glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        playerPosition.x += moveSpeed;

    // Clamp player position to screen bounds
    playerPosition.x = glm::clamp(playerPosition.x, -WORLD_HALF_WIDTH, WORLD_HALF_WIDTH);
    playerPosition.y = glm::clamp(playerPosition.y, -WORLD_HALF_HEIGHT, WORLD_HALF_HEIGHT);
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
