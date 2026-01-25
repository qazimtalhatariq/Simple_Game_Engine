/*
================================================================================
    ENHANCED GAME ENGINE WITH MULTIPLE FEATURES
    ============================================
    
    FEATURES:
    - Player with invincibility timer after collision
    - Multiple walls scattered around the game area
    - Life-boosting power-ups that spawn randomly (GREEN squares)
    - Game over screen with restart/exit options
    - Audio feedback on collisions
    - Text UI for lives and game status
    
    CONTROLS:
    - W/A/S/D: Move player (CYAN square)
    - ENTER: Restart game (when game is over)
    - ESC: Exit game (when game is over)
================================================================================
*/

#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <vector>
#include <memory>
#include <cmath>
#include <iostream>
#include <string>
#include <random>
#include <algorithm>

using namespace std;

// Random number generator for power-up placement
mt19937 rng(static_cast<unsigned>(time(nullptr)));

// ============================================================================
// DAMAGE WALL CLASS - Passthrough walls that reduce player life
// ============================================================================
/**
 * @class DamageWall
 * @brief Passthrough wall that reduces player life on contact
 * Red rectangular obstacles that can be passed through but cost 1 life
 */
class DamageWall {
private:
    sf::RectangleShape m_shape;              // Red rectangle - visual representation
    float m_damage = 1.f;                   // Damage dealt per hit
    bool m_hasHit = false;                  // Flag to prevent multiple hits in same frame

public:
    /**
     * Constructor - Initialize damage wall at random position
     * @param pos Random position where damage wall spawns
     */
    DamageWall(sf::Vector2f pos) {
        // Random size between 40-80 pixels
        uniform_int_distribution<int> sizeDist(40, 80);
        float size = static_cast<float>(sizeDist(rng));
        m_shape.setSize({size, size});
        m_shape.setFillColor(sf::Color::Red);  // Red color indicates damage
        m_shape.setPosition(pos);
    }

    /**
     * Check if player collides with this damage wall
     * Unlike regular walls, player can pass through but takes damage
     * @param playerShape Shape of player to check collision with
     * @return True if collision detected
     */
    bool checkCollision(const sf::RectangleShape& playerShape) {
        auto intersect = m_shape.getGlobalBounds().findIntersection(playerShape.getGlobalBounds());
        if (intersect && !m_hasHit) {
            m_hasHit = true;  // Prevent multiple hits in quick succession
            return true;
        }
        return false;
    }

    /**
     * Reset hit flag (called each frame to allow new hits)
     */
    void resetHitFlag() { m_hasHit = false; }

    /**
     * Get damage amount dealt by this wall
     * @return Damage value (1 life)
     */
    float getDamage() const { return m_damage; }

    /**
     * Get shape for rendering
     * @return Reference to the rectangle shape
     */
    const sf::RectangleShape& getShape() const { return m_shape; }

    /**
     * Draw this damage wall to the game window
     * @param window SFML render window to draw to
     */
    void draw(sf::RenderWindow& window) { window.draw(m_shape); }
};

// ============================================================================
// POWER-UP CLASS - Increases player lives when collected
// ============================================================================
/**
 * @class PowerUp
 * @brief Represents a collectible item that increases lives by 1
 * Spawns randomly on screen and disappears when player collects it
 */
class PowerUp {
private:
    sf::RectangleShape m_shape;      // Green square representing the power-up
    bool m_isCollected = false;      // Flag to mark if player collected this

public:
    /**
     * Constructor - Initialize power-up at given position
     * @param pos Random position where power-up spawns
     */
    PowerUp(sf::Vector2f pos) {
        m_shape.setSize({25.f, 25.f});
        m_shape.setFillColor(sf::Color::Green);
        m_shape.setPosition(pos);
    }

    /**
     * Check if player collides with this power-up
     * @param playerShape Shape of player to check collision with
     * @return True if collision detected, false otherwise
     */
    bool checkCollision(const sf::RectangleShape& playerShape) {
        auto intersect = m_shape.getGlobalBounds().findIntersection(playerShape.getGlobalBounds());
        if (intersect) {
            m_isCollected = true;  // Mark as collected
            return true;
        }
        return false;
    }

    /**
     * Check if this power-up has been collected
     * @return True if collected, false if still on screen
     */
    bool isCollected() const { return m_isCollected; }

    /**
     * Draw this power-up to the window
     * @param window Game window to draw to
     */
    void draw(sf::RenderWindow& window) {
        if (!m_isCollected) {
            window.draw(m_shape);
        }
    }
};

// ============================================================================
// PLAYER CLASS WITH INVINCIBILITY LOGIC
// ============================================================================
/**
 * @class Player
 * @brief Main player character with movement and collision handling
 * Includes invincibility timer after taking damage and collision sounds
 */
class Player {
private:
    sf::RectangleShape m_shape;                      // Cyan square - player visual
    float m_speed = 350.f;                           // Movement speed in pixels/second
    int m_lives = 0;                                 // Current number of lives remaining (starts at 0)
    bool m_isAlive = true;                           // Game active flag

    // Invincibility System - protects player briefly after taking damage
    float m_invincibleTimer = 0.f;                   // Countdown timer for invincibility
    const float INVINCIBLE_DURATION = 1.5f;          // How long invincibility lasts (seconds)

    // Audio System - plays sound when hitting walls
    sf::SoundBuffer m_hitBuffer;                     // Loaded sound data from file
    unique_ptr<sf::Sound> m_hitSound;                // Sound object for playing collision audio

public:
    /**
     * Constructor - Initialize player with size, starting position, and color
     * @param size Width and height of player square
     * @param pos Starting X and Y coordinates on screen
     * @param color RGB color of player (usually cyan)
     */
    Player(sf::Vector2f size, sf::Vector2f pos, sf::Color color) {
        // Set visual properties
        m_shape.setSize(size);
        m_shape.setPosition(pos);
        m_shape.setFillColor(color);

        // Load collision sound effect from file
        if (!m_hitBuffer.loadFromFile("hit.wav")) {
            cout << "Audio Warning: Could not load hit.wav sound!" << endl;
        } else {
            // Create sound object linked to the loaded buffer
            m_hitSound = make_unique<sf::Sound>(m_hitBuffer);
        }
    }

    /**
     * Update player state every frame
     * Handles movement input and invincibility blinking animation
     * @param dt Time since last frame (seconds)
     */
    void update(float dt) {
        // Don't update if game is over
        if (!m_isAlive) return;

        // --- INVINCIBILITY BLINKING EFFECT ---
        // When hit, player blinks for 1.5 seconds to show protection
        if (m_invincibleTimer > 0) {
            m_invincibleTimer -= dt;  // Count down invincibility timer
            
            // Create blinking by toggling alpha transparency
            int blink = static_cast<int>(m_invincibleTimer * 10) % 2;
            sf::Color c = m_shape.getFillColor();
            c.a = (blink == 0) ? 100 : 255;  // Alternate between 40% and 100% opacity
            m_shape.setFillColor(c);
        } else {
            // Not invincible - show fully opaque
            sf::Color c = m_shape.getFillColor();
            c.a = 255;
            m_shape.setFillColor(c);
        }

        // --- MOVEMENT LOGIC ---
        // Read keyboard input for movement direction
        sf::Vector2f dir{0, 0};
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W)) dir.y -= 1;  // Move UP
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S)) dir.y += 1;  // Move DOWN
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A)) dir.x -= 1;  // Move LEFT
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D)) dir.x += 1;  // Move RIGHT

        // Apply smooth movement if any key is pressed
        if (dir.x != 0 || dir.y != 0) {
            // Normalize direction vector to prevent faster diagonal movement
            float len = sqrt(dir.x * dir.x + dir.y * dir.y);
            m_shape.move((dir / len) * m_speed * dt);  // Move at constant speed
        }
    }

    /**
     * Handle collision between player and wall obstacle
     * Reduces lives, triggers sound, applies invincibility, and pushes player out
     * @param wall Wall shape to check collision with
     */
    void handleCollision(const sf::RectangleShape& wall) {
        // Check if player overlaps with wall
        auto intersect = m_shape.getGlobalBounds().findIntersection(wall.getGlobalBounds());
        
        if (intersect) {
            // ONLY take damage if NOT currently invincible
            // This prevents losing multiple lives from same obstacle
            if (m_invincibleTimer <= 0) {
                // Play collision sound effect
                if (m_hitSound) m_hitSound->play();
                
                // Lose one life
                m_lives--;
                
                // Start invincibility protection period
                m_invincibleTimer = INVINCIBLE_DURATION;

                // Check if player is dead
                if (m_lives <= 0) {
                    m_lives = 0;
                    m_isAlive = false;  // End game
                }
            }

            // PUSH PLAYER OUT OF WALL (even if invincible)
            // This prevents player from getting stuck inside obstacles
            if (intersect->size.x < intersect->size.y) {
                // Wall is taller than wide - push player LEFT or RIGHT
                float push = (m_shape.getPosition().x < wall.getPosition().x) ? 
                             -intersect->size.x : intersect->size.x;
                m_shape.move({push, 0});
            } else {
                // Wall is wider than tall - push player UP or DOWN
                float push = (m_shape.getPosition().y < wall.getPosition().y) ? 
                             -intersect->size.y : intersect->size.y;
                m_shape.move({0, push});
            }
        }
    }

    /**
     * Increase player lives by 1 when collecting a power-up
     * Called when player touches a green power-up square
     */
    void addLife() {
        m_lives++;
    }

    /**
     * Draw player to game window
     * @param window SFML render window to draw to
     */
    void draw(sf::RenderWindow& window) { 
        window.draw(m_shape); 
    }

    /**
     * Get current number of lives
     * @return Number of lives remaining (0-6+)
     */
    int getLives() const { return m_lives; }

    /**
     * Check if player is still alive
     * @return True if game is active, false if game over
     */
    bool isAlive() const { return m_isAlive; }

    /**
     * Get player's shape for external collision detection
     * @return Reference to player's rectangular shape
     */
    const sf::RectangleShape& getShape() const { return m_shape; }
};

// ============================================================================
// GAME ENGINE CORE - Main game controller
// ============================================================================
/**
 * @class GameEngine
 * @brief Main game engine managing game logic, rendering, and state
 * Controls player, walls, power-ups, and overall game flow
 */
class GameEngine {
private:
    sf::RenderWindow m_window;                       // Main game window (800x600)
    unique_ptr<Player> m_player;                     // Player character
    vector<sf::RectangleShape> m_walls;              // List of wall obstacles
    vector<PowerUp> m_powerUps;                      // List of active power-ups
    vector<DamageWall> m_damageWalls;                // List of damage walls that reduce life
    sf::Font m_font;                                 // Font for text rendering
    unique_ptr<sf::Text> m_uiText;                   // Lives display (top left)
    unique_ptr<sf::Text> m_gameOverText;             // "GAME OVER!" message
    unique_ptr<sf::Text> m_instructionsText;         // Restart/Exit instructions
    sf::Clock m_clock;                               // Frame timing clock
    float m_powerUpSpawnTimer = 0.f;                 // Counter for power-up spawning
    const float POWER_UP_SPAWN_INTERVAL = 3.0f;      // Spawn a new power-up every 3 seconds
    float m_damageWallSpawnTimer = 0.f;              // Counter for damage wall spawning
    const float DAMAGE_WALL_SPAWN_INTERVAL = 2.5f;   // Spawn a damage wall every 2.5 seconds

public:
    /**
     * Constructor - Initialize game window and all game objects
     * Sets up player, walls, font, and game over screens
     */
    GameEngine() : m_window(sf::VideoMode({800, 600}), "Enhanced Game Engine - Final Project") {
        m_window.setFramerateLimit(60);              // Cap at 60 FPS

        // Initialize player starting at position (50, 50) with size 40x40
        m_player = make_unique<Player>(sf::Vector2f{40, 40}, sf::Vector2f{50, 50}, sf::Color::Cyan);
        
        // Create multiple walls at different positions
        createWalls();

        // Load font for text rendering
        if (!m_font.openFromFile("arial.ttf")) { 
            cout << "Font Warning: Could not load arial.ttf!" << endl;
            m_uiText = make_unique<sf::Text>(m_font, "ERROR");
        } else {
            // Initialize lives display text (shown during gameplay)
            m_uiText = make_unique<sf::Text>(m_font, "Lives Remaining: 0");
            m_uiText->setCharacterSize(25);
            m_uiText->setFillColor(sf::Color::White);
            m_uiText->setPosition({20, 20});

            // Initialize game over message
            m_gameOverText = make_unique<sf::Text>(m_font, "GAME OVER!");
            m_gameOverText->setCharacterSize(60);
            m_gameOverText->setFillColor(sf::Color::Red);
            m_gameOverText->setPosition({180, 150});

            // Initialize restart/exit instructions
            m_instructionsText = make_unique<sf::Text>(m_font, "PRESS ENTER TO RESTART\nPRESS ESC TO EXIT");
            m_instructionsText->setCharacterSize(25);
            m_instructionsText->setFillColor(sf::Color::Yellow);
            m_instructionsText->setPosition({120, 300});
        }
    }

    /**
     * Create multiple wall obstacles at strategic locations
     * Creates 4 walls of varying sizes to form a challenging maze
     */
    void createWalls() {
        // WALL 1 - Large central obstacle (150x150)
        // Positioned in middle-top area
        sf::RectangleShape wall1;
        wall1.setSize({150, 150});
        wall1.setPosition({350, 200});
        wall1.setFillColor(sf::Color(120, 120, 120));  // Gray color
        m_walls.push_back(wall1);

        // WALL 2 - Left side obstacle (100x80)
        // Positioned on left-middle
        sf::RectangleShape wall2;
        wall2.setSize({100, 80});
        wall2.setPosition({150, 350});
        wall2.setFillColor(sf::Color(120, 120, 120));
        m_walls.push_back(wall2);

        // WALL 3 - Right side obstacle (100x80)
        // Positioned on right-middle (mirrors wall2)
        sf::RectangleShape wall3;
        wall3.setSize({100, 80});
        wall3.setPosition({550, 350});
        wall3.setFillColor(sf::Color(120, 120, 120));
        m_walls.push_back(wall3);

        // WALL 4 - Bottom obstacle (120x60)
        // Positioned near bottom-center
        sf::RectangleShape wall4;
        wall4.setSize({120, 60});
        wall4.setPosition({350, 450});
        wall4.setFillColor(sf::Color(120, 120, 120));
        m_walls.push_back(wall4);
    }

    /**
     * Spawn a new power-up at a random location on screen
     * Adds a green square that increases lives by 1 when touched
     */
    void spawnPowerUp() {
        // Generate random coordinates within safe game area
        uniform_int_distribution<int> xDist(50, 750);   // X between 50 and 750
        uniform_int_distribution<int> yDist(100, 550);  // Y between 100 and 550
        
        sf::Vector2f randomPos(xDist(rng), yDist(rng));
        m_powerUps.emplace_back(randomPos);
    }

    /**
     * Spawn a new damage wall at a random location on screen
     * Red squares that can be passed through but reduce life by 1
     */
    void spawnDamageWall() {
        // Generate random coordinates within safe game area
        uniform_int_distribution<int> xDist(50, 700);   // X between 50 and 700
        uniform_int_distribution<int> yDist(100, 500);  // Y between 100 and 500
        
        sf::Vector2f randomPos(xDist(rng), yDist(rng));
        m_damageWalls.emplace_back(randomPos);
    }

    /**
     * Main game loop - runs until window is closed
     * Handles events, updates game logic, and renders frame
     */
    void run() {
        while (m_window.isOpen()) {
            // --- EVENT HANDLING ---
            while (const auto event = m_window.pollEvent()) {
                if (event.value().is<sf::Event::Closed>()) {
                    // User clicked close button
                    m_window.close();
                } else if (event.value().is<sf::Event::KeyPressed>()) {
                    // Handle keyboard input
                    auto keyEvent = event.value().getIf<sf::Event::KeyPressed>();
                    if (!m_player->isAlive()) {
                        // Game over - allow restart or exit
                        if (keyEvent->code == sf::Keyboard::Key::Enter) {
                            restartGame();  // Restart the game
                        } else if (keyEvent->code == sf::Keyboard::Key::Escape) {
                            m_window.close();  // Exit the game
                        }
                    }
                }
            }

            // Calculate time since last frame
            float dt = m_clock.restart().asSeconds();

            // --- UPDATE GAME LOGIC ---
            if (m_player->isAlive()) {
                // Update player position and animation
                m_player->update(dt);

                // Check collisions with all walls
                for (auto& wall : m_walls) {
                    m_player->handleCollision(wall);
                }

                // Check collisions with all power-ups
                for (auto& powerUp : m_powerUps) {
                    if (powerUp.checkCollision(m_player->getShape())) {
                        m_player->addLife();  // Increase lives by 1
                    }
                }

                // Remove collected power-ups from list
                m_powerUps.erase(
                    remove_if(m_powerUps.begin(), m_powerUps.end(),
                    [](const PowerUp& p) { return p.isCollected(); }),
                    m_powerUps.end()
                );

                // Reset hit flags for damage walls each frame
                for (auto& damageWall : m_damageWalls) {
                    damageWall.resetHitFlag();
                }

                // Check collisions with all damage walls (passthrough but damaging)
                for (auto& damageWall : m_damageWalls) {
                    if (damageWall.checkCollision(m_player->getShape())) {
                        m_player->handleCollision(damageWall.getShape());  // Lose 1 life but pass through
                    }
                }

                // Spawn new power-ups periodically
                m_powerUpSpawnTimer += dt;
                if (m_powerUpSpawnTimer >= POWER_UP_SPAWN_INTERVAL) {
                    if (m_powerUps.size() < 3) {  // Keep max 3 power-ups on screen
                        spawnPowerUp();
                    }
                    m_powerUpSpawnTimer = 0.f;
                }

                // Spawn new damage walls periodically
                m_damageWallSpawnTimer += dt;
                if (m_damageWallSpawnTimer >= DAMAGE_WALL_SPAWN_INTERVAL) {
                    if (m_damageWalls.size() < 4) {  // Keep max 4 damage walls on screen
                        spawnDamageWall();
                    }
                    m_damageWallSpawnTimer = 0.f;
                }

                // Update HUD text to show current lives
                m_uiText->setString("Lives Remaining: " + to_string(m_player->getLives()));
                m_uiText->setFillColor(sf::Color::White);
            }

            // --- RENDERING ---
            // Clear screen with dark background
            m_window.clear(sf::Color(15, 15, 18));

            // Draw all walls (gray rectangles)
            for (auto& wall : m_walls) {
                m_window.draw(wall);
            }

            // Draw all damage walls (red squares - passthrough damaging obstacles)
            for (auto& damageWall : m_damageWalls) {
                damageWall.draw(m_window);
            }

            // Draw all power-ups (green squares)
            for (auto& powerUp : m_powerUps) {
                powerUp.draw(m_window);
            }

            // Draw player (cyan square)
            m_player->draw(m_window);

            // Draw HUD text (lives display)
            m_window.draw(*m_uiText);

            // Draw game over screen if player is dead
            if (!m_player->isAlive()) {
                drawGameOverScreen();
            }

            // Display rendered frame
            m_window.display();
        }
    }

    /**
     * Draw game over screen with options
     * Shows "GAME OVER!" message and restart/exit instructions
     */
    void drawGameOverScreen() {
        // Draw semi-transparent dark overlay to dim the game
        sf::RectangleShape overlay(sf::Vector2f(800, 600));
        overlay.setFillColor(sf::Color(0, 0, 0, 150));  // Black with 60% opacity
        m_window.draw(overlay);

        // Draw "GAME OVER!" text
        m_window.draw(*m_gameOverText);

        // Draw restart/exit instructions
        m_window.draw(*m_instructionsText);
    }

    /**
     * Restart the game - create new player and clear all game objects
     * Called when player presses ENTER on game over screen
     */
    void restartGame() {
        // Create new player at starting position
        m_player = make_unique<Player>(sf::Vector2f{40, 40}, sf::Vector2f{50, 50}, sf::Color::Cyan);
        
        // Clear all power-ups from screen
        m_powerUps.clear();
        
        // Clear all damage walls from screen
        m_damageWalls.clear();
        
        // Reset power-up spawn timer
        m_powerUpSpawnTimer = 0.f;
        
        // Reset damage wall spawn timer
        m_damageWallSpawnTimer = 0.f;
    }
};

// ============================================================================
// MAIN FUNCTION - Program Entry Point
// ============================================================================
/**
 * main() - Entry point for the game application
 * Creates game engine and starts the main loop
 * Handles any exceptions that occur during execution
 */
int main() {
    try {
        GameEngine engine;      // Create game engine
        engine.run();           // Start game loop
    } catch (const exception& e) {
        // Display any critical errors
        cerr << "Critical Error: " << e.what() << endl;
        return 1;  // Exit with error code
    }
    return 0;  // Normal exit
}