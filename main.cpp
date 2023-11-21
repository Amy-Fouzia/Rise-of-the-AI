/**
* Author: [Your name here]
* Assignment: Rise of the AI
* Date due: 2023-11-18, 11:59pm
* I pledge that I have completed this assignment without
* collaborating with anyone else, in conformance with the
* NYU School of Engineering Policies and Procedures on
* Academic Misconduct.
**/

#define GL_SILENCE_DEPRECATION
#define STB_IMAGE_IMPLEMENTATION
#define LOG(argument) std::cout << argument << '\n'
#define GL_GLEXT_PROTOTYPES 1
#define FIXED_TIMESTEP 0.0166666f
#define ENEMY_COUNT 3
#define LEVEL1_WIDTH 14
#define LEVEL1_HEIGHT 5

#ifdef _WINDOWS
#include <GL/glew.h>
#endif

#include <SDL_mixer.h>
#include <SDL.h>
#include <SDL_opengl.h>
#include "glm/mat4x4.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "ShaderProgram.h"
#include "stb_image.h"
#include "cmath"
#include <ctime>
#include <vector>
#include "Entity.h"
#include "Map.h"

// ————— GAME STATE ————— //
struct GameState
{
    Entity* player;
    Entity* enemies;
    Entity* background;
    Entity* spit;
    Entity* tear;

    Map* map;

    Mix_Music* bgm;
    Mix_Chunk* jump_sfx;
};

// ————— CONSTANTS ————— //
const int   WINDOW_WIDTH = 640,
            WINDOW_HEIGHT = 480;

const float BG_RED = 0.5f;      // Adjust the red component
const float BG_BLUE = 0.0f;     // You may want to reduce the blue component
const float BG_GREEN = 0.2f;    // Adjust the green component
const float BG_OPACITY = 1.0f;  // Opacity remains unchanged

const int   VIEWPORT_X = 0,
            VIEWPORT_Y = 0,
            VIEWPORT_WIDTH = WINDOW_WIDTH,
            VIEWPORT_HEIGHT = WINDOW_HEIGHT;

const char GAME_WINDOW_NAME[] = "Rise of the AI";

const char  V_SHADER_PATH[] = "shaders/vertex_textured.glsl",
            F_SHADER_PATH[] = "shaders/fragment_textured.glsl";

const float MILLISECONDS_IN_SECOND = 1000.0;

const char  SPRITESHEET_FILEPATH[]  = "assets/images/stoopid.png",
            ENEMY_FILEPATH[]        = "assets/images/uglee.png",
            SPIT_FILEPATH[]         = "assets/images/spit.png",
            TEAR_FILEPATH[]         = "assets/images/tear.png",
            MAP_TILESET_FILEPATH[]  = "assets/images/tiles.png",
            WIN_FILEPATH[]          = "assets/images/win.jpg",
            LOSS_FILEPATH[]         = "assets/images/lose.jpg",
            BGM_FILEPATH[]          = "assets/audio/dooblydoo.mp3",
            JUMP_SFX_FILEPATH[]     = "assets/audio/bounce.wav",
            BACKGROUND_FILEPATH[]   = "assets/images/background.png",
            FONT_FILEPATH[]         = "assets/images/font1.png";

const int NUMBER_OF_TEXTURES = 1;
const GLint LEVEL_OF_DETAIL = 0;
const GLint TEXTURE_BORDER = 0;

unsigned int LEVEL_1_DATA[] =
{
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0,
    1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0,
    2, 2, 0, 1, 1, 0, 0, 0, 0, 1, 2, 2, 1, 1,
    2, 2, 1, 2, 2, 1, 0, 1, 1, 2, 2, 2, 2, 2
};

// ————— VARIABLES ————— //
GameState g_game_state;

SDL_Window* g_display_window;
bool g_game_is_running  = true;

ShaderProgram g_shader_program;
glm::mat4 g_view_matrix, g_projection_matrix;

float   g_previous_ticks = 0.0f,
        g_accumulator = 0.0f;

int game_win = 0; //-1: lose, 1: win

bool ammo = true;


// ————— GENERAL FUNCTIONS ————— //
GLuint load_texture(const char* filepath)
{
    int width, height, number_of_components;
    unsigned char* image = stbi_load(filepath, &width, &height, &number_of_components, STBI_rgb_alpha);

    if (image == NULL)
    {
        LOG("Unable to load image. Make sure the path is correct.");
        assert(false);
    }

    GLuint texture_id;
    glGenTextures(NUMBER_OF_TEXTURES, &texture_id);
    glBindTexture(GL_TEXTURE_2D, texture_id);
    glTexImage2D(GL_TEXTURE_2D, LEVEL_OF_DETAIL, GL_RGBA, width, height, TEXTURE_BORDER, GL_RGBA, GL_UNSIGNED_BYTE, image);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    stbi_image_free(image);

    return texture_id;
}

const int FONTBANK_SIZE = 16;

void draw_text(ShaderProgram* program, GLuint font_texture_id, std::string text, float screen_size, float spacing, glm::vec3 position)
{
    // Scale the size of the fontbank in the UV-plane
    // We will use this for spacing and positioning
    float width = 1.0f / FONTBANK_SIZE;
    float height = 1.0f / FONTBANK_SIZE;

    // Instead of having a single pair of arrays, we'll have a series of pairs—one for each character
    // Don't forget to include <vector>!
    std::vector<float> vertices;
    std::vector<float> texture_coordinates;

    // For every character...
    for (int i = 0; i < text.size(); i++) {
        // 1. Get their index in the spritesheet, as well as their offset (i.e. their position
        //    relative to the whole sentence)
        int spritesheet_index = (int)text[i];  // ascii value of character
        float offset = (screen_size + spacing) * i;

        // 2. Using the spritesheet index, we can calculate our U- and V-coordinates
        float u_coordinate = (float)(spritesheet_index % FONTBANK_SIZE) / FONTBANK_SIZE;
        float v_coordinate = (float)(spritesheet_index / FONTBANK_SIZE) / FONTBANK_SIZE;

        // 3. Inset the current pair in both vectors
        vertices.insert(vertices.end(), {
            offset + (-0.5f * screen_size), 0.5f * screen_size,
            offset + (-0.5f * screen_size), -0.5f * screen_size,
            offset + (0.5f * screen_size), 0.5f * screen_size,
            offset + (0.5f * screen_size), -0.5f * screen_size,
            offset + (0.5f * screen_size), 0.5f * screen_size,
            offset + (-0.5f * screen_size), -0.5f * screen_size,
            });

        texture_coordinates.insert(texture_coordinates.end(), {
            u_coordinate, v_coordinate,
            u_coordinate, v_coordinate + height,
            u_coordinate + width, v_coordinate,
            u_coordinate + width, v_coordinate + height,
            u_coordinate + width, v_coordinate,
            u_coordinate, v_coordinate + height,
            });
    }

    // 4. And render all of them using the pairs
    glm::mat4 model_matrix = glm::mat4(1.0f);
    model_matrix = glm::translate(model_matrix, position);

    program->set_model_matrix(model_matrix);
    glUseProgram(g_shader_program.get_program_id());

    glVertexAttribPointer(g_shader_program.get_position_attribute(), 2, GL_FLOAT, false, 0, vertices.data());
    glEnableVertexAttribArray(g_shader_program.get_position_attribute());
    glVertexAttribPointer(g_shader_program.get_tex_coordinate_attribute(), 2, GL_FLOAT, false, 0, texture_coordinates.data());
    glEnableVertexAttribArray(g_shader_program.get_tex_coordinate_attribute());

    glBindTexture(GL_TEXTURE_2D, font_texture_id);
    glDrawArrays(GL_TRIANGLES, 0, (int)(text.size() * 6));

    glDisableVertexAttribArray(g_shader_program.get_position_attribute());
    glDisableVertexAttribArray(g_shader_program.get_tex_coordinate_attribute());
}

void initialise()
{
    // ————— GENERAL ————— //
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    g_display_window = SDL_CreateWindow(GAME_WINDOW_NAME,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT,
        SDL_WINDOW_OPENGL);

    SDL_GLContext context = SDL_GL_CreateContext(g_display_window);
    SDL_GL_MakeCurrent(g_display_window, context);

#ifdef _WINDOWS
    glewInit();
#endif

    // ————— VIDEO SETUP ————— //
    glViewport(VIEWPORT_X, VIEWPORT_Y, VIEWPORT_WIDTH, VIEWPORT_HEIGHT);

    g_shader_program.load(V_SHADER_PATH, F_SHADER_PATH);

    g_view_matrix = glm::mat4(1.0f);
    g_projection_matrix = glm::ortho(-5.0f, 5.0f, -3.75f, 3.75f, -1.0f, 1.0f);

    g_shader_program.set_projection_matrix(g_projection_matrix);
    g_shader_program.set_view_matrix(g_view_matrix);

    glUseProgram(g_shader_program.get_program_id());

    glClearColor(BG_RED, BG_BLUE, BG_GREEN, BG_OPACITY);

    // ————— MAP SET-UP ————— //
    GLuint map_texture_id = load_texture(MAP_TILESET_FILEPATH);
    g_game_state.map = new Map(LEVEL1_WIDTH, LEVEL1_HEIGHT, LEVEL_1_DATA, map_texture_id, 1.0f, 4, 1);

    // ————— BACKGROUND SET-UP ————— //
    g_game_state.background = new Entity();
    g_game_state.background->set_entity_type(BACKGROUND);
    g_game_state.background->set_position(glm::vec3(0.0f, 0.0f, 0.0f));
    g_game_state.background->m_texture_id = load_texture(BACKGROUND_FILEPATH);

    g_game_state.background->m_texture_id = load_texture(BACKGROUND_FILEPATH);

    // ————— GEORGE SET-UP ————— //
    // Existing
    g_game_state.player = new Entity();
    g_game_state.player->set_entity_type(PLAYER);
    g_game_state.player->set_position(glm::vec3(0.0f, 0.0f, 0.0f));
    g_game_state.player->set_movement(glm::vec3(0.0f));
    g_game_state.player->set_speed(2.5f);
    g_game_state.player->set_acceleration(glm::vec3(0.0f, -9.81f, 0.0f));
    g_game_state.player->m_texture_id = load_texture(SPRITESHEET_FILEPATH);

    //Tears
    g_game_state.tear = new Entity();
    g_game_state.tear->set_entity_type(PLAYER);
    g_game_state.tear->set_position(glm::vec3(0.0f, -1.1f, 0.0f));
    g_game_state.tear->set_movement(glm::vec3(0.0f));
    g_game_state.tear->set_speed(2.5f);
    g_game_state.tear->m_texture_id = load_texture(TEAR_FILEPATH);

    // Walking
    g_game_state.player->m_walking[g_game_state.player->LEFT] = new int[4] { 1, 5, 9, 13 };
    g_game_state.player->m_walking[g_game_state.player->RIGHT] = new int[4] { 3, 7, 11, 15 };
    g_game_state.player->m_walking[g_game_state.player->UP] = new int[4] { 2, 6, 10, 14 };
    g_game_state.player->m_walking[g_game_state.player->DOWN] = new int[4] { 0, 4, 8, 12 };

    g_game_state.player->m_animation_indices = g_game_state.player->m_walking[g_game_state.player->RIGHT];  // start George looking left
    g_game_state.player->m_animation_frames = 4;
    g_game_state.player->m_animation_index = 0;
    g_game_state.player->m_animation_time = 0.0f;
    g_game_state.player->m_animation_cols = 4;
    g_game_state.player->m_animation_rows = 4;
    g_game_state.player->set_height(0.8f);
    g_game_state.player->set_width(0.8f);

    // Jumping
    g_game_state.player->m_jumping_power = 5.0f;

    Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 4096);

    g_game_state.bgm = Mix_LoadMUS(BGM_FILEPATH);
    Mix_PlayMusic(g_game_state.bgm, -1);
    Mix_VolumeMusic(MIX_MAX_VOLUME / 16.0f);

    g_game_state.jump_sfx = Mix_LoadWAV(JUMP_SFX_FILEPATH);

    //Enemies
    GLuint enemy_texture_id = load_texture(ENEMY_FILEPATH);

    g_game_state.enemies = new Entity[ENEMY_COUNT];
    for (int i = 0; i < ENEMY_COUNT; i++) {
        g_game_state.enemies[i].set_entity_type(ENEMY);
        g_game_state.enemies[i].m_texture_id = enemy_texture_id;
        g_game_state.enemies[i].set_movement(glm::vec3(0.0f));
        g_game_state.enemies[i].set_speed(1.0f);
        g_game_state.enemies[i].set_acceleration(glm::vec3(0.0f, -9.81f, 0.0f));
    }

    //jumping enemy
    g_game_state.enemies[0].set_position(glm::vec3(3.0f, 0.0f, 0.0f));
    g_game_state.enemies[0].m_jumping_power = 3.0f;
    g_game_state.enemies[0].set_ai_type(JUMPER);
    g_game_state.enemies[0].set_ai_state(JUMPING);

    
    //patrolling enemy
    g_game_state.enemies[1].set_position(glm::vec3(6.0f, 2.0f, 0.0f));
    g_game_state.enemies[0].set_ai_type(WALKER);
    g_game_state.enemies[0].set_ai_state(WALKING);

    //spitting enemy
    g_game_state.enemies[2].set_position(glm::vec3(10.0f, 1.0f, 0.0f));
    g_game_state.enemies[0].set_ai_type(SHOOTER);
    g_game_state.enemies[0].set_ai_state(SHOOTING);
    
    GLuint spit_texture_id = load_texture(SPIT_FILEPATH);
    g_game_state.spit = new Entity();
    g_game_state.spit->m_texture_id = spit_texture_id;
    g_game_state.spit->set_movement(glm::vec3(0.0f));
    g_game_state.spit->set_speed(3.0f);
    g_game_state.spit->set_position(glm::vec3(10.0f, -1.0f, 0.0f));

    // ————— BLENDING ————— //
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void process_input()
{
    g_game_state.player->set_movement(glm::vec3(0.0f));

    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        switch (event.type) {
        case SDL_QUIT:
        case SDL_WINDOWEVENT_CLOSE:
            g_game_is_running  = false;
            break;

        case SDL_KEYDOWN:
            switch (event.key.keysym.sym) {
            case SDLK_q:
                // Quit the game with a keystroke
                g_game_is_running  = false;
                break;

            case SDLK_SPACE:
                // Jump
                if (g_game_state.player->m_collided_bottom_map && game_win == 0)
                {
                    g_game_state.player->m_is_jumping = true;
                    Mix_PlayChannel(-1, g_game_state.jump_sfx, 0);
                }
                break;

            case SDLK_a:
                //shoot left
                if (ammo) { 
                    g_game_state.tear->move_left(); 
                    ammo = false;
                }
                break;

            case SDLK_d:
                //shoot right
                if (ammo) {
                    g_game_state.tear->move_right();
                    ammo = false;
                }
                break;

            default:
                break;
            }

        default:
            break;
        }
    }

    const Uint8* key_state = SDL_GetKeyboardState(NULL);

    if (key_state[SDL_SCANCODE_LEFT] && game_win == 0)
    {
        g_game_state.player->move_left();
        g_game_state.player->m_animation_indices = g_game_state.player->m_walking[g_game_state.player->LEFT];
    }
    else if (key_state[SDL_SCANCODE_RIGHT] && game_win == 0)
    {
        g_game_state.player->move_right();
        g_game_state.player->m_animation_indices = g_game_state.player->m_walking[g_game_state.player->RIGHT];
    }

    // This makes sure that the player can't move faster diagonally
    if (glm::length(g_game_state.player->get_movement()) > 1.0f)
    {
        g_game_state.player->set_movement(glm::normalize(g_game_state.player->get_movement()));
    }
}

void update()
{
    float ticks = (float)SDL_GetTicks() / MILLISECONDS_IN_SECOND;
    float delta_time = ticks - g_previous_ticks;
    g_previous_ticks = ticks;

    delta_time += g_accumulator;

    if (delta_time < FIXED_TIMESTEP)
    {
        g_accumulator = delta_time;
        return;
    }

    while (delta_time >= FIXED_TIMESTEP)
    {
        g_game_state.tear->update(FIXED_TIMESTEP, g_game_state.player, NULL, 0, g_game_state.map);
        for (int i = 0; i < ENEMY_COUNT; i++) g_game_state.enemies[i].update(FIXED_TIMESTEP, g_game_state.player, NULL, 0, g_game_state.map);
        g_game_state.spit->update(FIXED_TIMESTEP, g_game_state.player, g_game_state.player, 1, g_game_state.map);
        g_game_state.player->update(FIXED_TIMESTEP, g_game_state.player, g_game_state.enemies, ENEMY_COUNT, g_game_state.map);
        delta_time -= FIXED_TIMESTEP;
    }

    g_accumulator = delta_time;

    g_view_matrix = glm::mat4(1.0f);
    g_view_matrix = glm::translate(g_view_matrix, glm::vec3(-g_game_state.player->get_position().x, 0.0f, 0.0f));

    //move jumping enemy
    if (g_game_state.enemies[0].m_collided_bottom_map) // && game_win == 0
    {
        g_game_state.enemies[0].m_is_jumping = true;
    }

    //move patrolling enemy
    if (g_game_state.enemies[1].get_position().x <= 6.0) {
        g_game_state.enemies[1].move_right();
    }
    if (g_game_state.enemies[1].get_position().x >= 7.0) {
        g_game_state.enemies[1].move_left();
    }

    //move projectile for spitting enemy
    if (!(g_game_state.spit->m_collided_left_map)) {
        g_game_state.spit->move_left();
    }

    //reset projectile position after hitting platform
    else{ g_game_state.spit->set_position(glm::vec3(10.0f, -1.0f, 0.0f)); }

    if (ammo) { g_game_state.tear->set_position(g_game_state.player->get_position()); }

    //reset player projectile after collision
    if (g_game_state.tear->m_collided_left_map || g_game_state.tear->m_collided_right_map || g_game_state.tear->get_position().x > 15 || g_game_state.tear->get_position().x < -5) {
        ammo = true;
    }

    //if player falls
    if (g_game_state.player->get_position().y < -3.75) {
        game_win = -1;
    }

    //if player collides with enemy or spit
    if (g_game_state.player->m_collided_left_entity || g_game_state.player->m_collided_right_entity || g_game_state.player->m_collided_top_entity) {
        game_win = -1;
    }

    if (g_game_state.spit->m_collided_bottom_entity || g_game_state.spit->m_collided_top_entity || g_game_state.spit->m_collided_right_entity || g_game_state.spit->m_collided_left_entity) {
        game_win = -1;
    }

    //if player jumps on enemy
    for (int i = 0; i < ENEMY_COUNT; i++) {
        if (g_game_state.player->m_collided_bottom_entity) {
            g_game_state.enemies[i].health = 0;
            g_game_state.enemies[i].set_position(glm::vec3(-15.0, -5.0, 0.0));
        }
    }

    //if player shoots enemy
    for (int i = 0; i < ENEMY_COUNT; i++)
    {
        float x_distance = fabs(g_game_state.tear->get_position().x - g_game_state.enemies[i].get_position().x) - ((g_game_state.tear->get_width() + g_game_state.enemies[i].get_width()) / 2.0f);
        float y_distance = fabs(g_game_state.tear->get_position().y - g_game_state.enemies[i].get_position().y) - ((g_game_state.tear->get_height() + g_game_state.enemies[i].get_height()) / 2.0f);

        if (x_distance < 0.0f && y_distance < 0.0f)
        {
            float x_distance = fabs(g_game_state.tear->get_position().x - g_game_state.enemies[i].get_position().x);
            float x_overlap = fabs(x_distance - (g_game_state.tear->get_width() / 2.0f) - (g_game_state.enemies[i].get_width() / 2.0f));
            if (g_game_state.tear->get_velocity().x > 0 && ammo == 0) {
                g_game_state.enemies[i].health = 0;
                g_game_state.enemies[i].set_position(glm::vec3(-15.0, -5.0, 0.0));
            }
            else if (g_game_state.tear->get_velocity().x < 0 && ammo == 0) {
                g_game_state.enemies[i].health = 0;
                g_game_state.enemies[i].set_position(glm::vec3(-15.0, -5.0, 0.0));
            }
        }
    }

    //if player kills all enemies
    if (g_game_state.enemies[0].health == 0 && g_game_state.enemies[1].health == 0 && g_game_state.enemies[2].health == 0) {
        game_win = 1;
    }

}

void render()
{
    g_shader_program.set_view_matrix(g_view_matrix);

    glClear(GL_COLOR_BUFFER_BIT);

    g_game_state.background->render(&g_shader_program, g_view_matrix);
    g_game_state.map->render(&g_shader_program);
    if (g_game_state.enemies[2].health == 1) {
        g_game_state.spit->render(&g_shader_program, g_view_matrix);
    }
    for (int i = 0; i < ENEMY_COUNT; i++) {
        if (g_game_state.enemies[i].health == 1){
            g_game_state.enemies[i].render(&g_shader_program, g_view_matrix);
        }   
    }
    g_game_state.tear->render(&g_shader_program, g_view_matrix);
    g_game_state.player->render(&g_shader_program, g_view_matrix);
    //game win
    if (game_win == 1) {
        draw_text(&g_shader_program, load_texture(FONT_FILEPATH), "you win", .5, .05, glm::vec3(g_game_state.player->get_position().x - 2, 2.0, 0.0));
    }

    //game loss
    if (game_win == -1) {
        draw_text(&g_shader_program, load_texture(FONT_FILEPATH), "you lose", .5, .05, glm::vec3(g_game_state.player->get_position().x - 2, 2.0, 0.0));
    }
    SDL_GL_SwapWindow(g_display_window);
}

void shutdown()
{
    SDL_Quit();

    delete[]  g_game_state.enemies;
    delete    g_game_state.spit;
    delete    g_game_state.tear;
    delete    g_game_state.player;
    delete    g_game_state.background;
    delete    g_game_state.map;
    Mix_FreeChunk(g_game_state.jump_sfx);
    Mix_FreeMusic(g_game_state.bgm);
}

// ————— GAME LOOP ————— //
int main(int argc, char* argv[])
{
    initialise();

    while (g_game_is_running )
    {
        process_input();
        update();
        render();
    }

    shutdown();
    return 0;
}
