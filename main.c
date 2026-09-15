#include <OpenGL/gl3.h>
#include <stddef.h>
#include <stdlib.h>
#define GLFW_INCLUDE_NONE
#include "external/GLFW/glfw3.h"
#include <ft2build.h>
#include FT_FREETYPE_H
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define SCR_WIDTH 650
#define SCR_HEIGHT 800
#define GAME_SCR_WIDTH 400
#define CELL_SIZE 40
#define SAND_SIZE 4
#define GRID_W (400 / SAND_SIZE)
#define GRID_H (SCR_HEIGHT / SAND_SIZE)
#define MAX_VERTS (GRID_W * GRID_H * 6)
#define MAX_STACK (GRID_W * GRID_H * 4)
#define BLINK_DURATION 0.4
#define POINTS_PER_CELL 5

double clear_start_time = -1;

typedef enum { EMPTY, SAND } CellType;
typedef struct {
  CellType cell_type;
  float color[3];
  int piece_type;
  bool clearing;
} Cell;
Cell cells[GRID_H][GRID_W] = {0};

typedef struct {
  int row;
  int col;
} Coord;
Coord stack[MAX_STACK];
int stack_index = 0;

bool visited[GRID_H][GRID_W] = {false};
int visited_count = 0;

enum Color {
  CYAN,
  YELLOW,
  PURPLE,
  GREEN,
  RED,
  BLUE,
  ORANGE,
  BEZEL_DARK,
  BEZEL_LIGHT,
  HIGHLIGHT,
  ACCENT,
  COLOR_COUNT
};
float rgb_table[COLOR_COUNT][3] = {
    [CYAN] = {0.f, 1.f, 1.f},              // cyan
    [YELLOW] = {1.f, 1.f, 0.f},            // yellow
    [PURPLE] = {0.6f, 0.f, 0.8f},          // purple
    [GREEN] = {0.f, 1.f, 0.f},             // green
    [RED] = {1.f, 0.f, 0.f},               // red
    [BLUE] = {0.f, 0.f, 1.f},              // blue
    [ORANGE] = {1.f, 0.5f, 0.f},           // orange
    [BEZEL_DARK] = {0.08f, 0.09f, 0.14f},  // shadow/outline
    [BEZEL_LIGHT] = {0.92f, 0.89f, 0.78f}, // main bar
    [ACCENT] = {1.0f, 0.78f, 0.15f},       // reserved for later (score, labels)
    [HIGHLIGHT] = {1.0f, 1.0f, 1.0f},
};

typedef enum {
  PIECE_I,
  PIECE_O,
  PIECE_T,
  PIECE_S,
  // PIECE_Z,
  PIECE_J,
  // PIECE_L,
  PIECE_COUNT
} PieceType;

enum Color piece_to_color[PIECE_COUNT] = {
    [PIECE_I] = CYAN,
    [PIECE_O] = YELLOW,
    [PIECE_T] = PURPLE,
    [PIECE_S] = GREEN,
    // [PIECE_Z] = RED,
    [PIECE_J] = BLUE,
    // [PIECE_L] = ORANGE,
};

int piece_shape[PIECE_COUNT][3][2] = {
    [PIECE_I] = {{1, 0}, {1, 0}, {1, 0}},
    [PIECE_O] = {{1, 1}, {1, 1}, {0, 0}},
    [PIECE_T] = {{1, 0}, {1, 1}, {1, 0}},
    [PIECE_S] = {{1, 0}, {1, 1}, {0, 1}},
    // [PIECE_Z] = {{0, 1}, {1, 1}, {1, 0}},
    [PIECE_J] = {{0, 1}, {0, 1}, {1, 1}},
    // [PIECE_L] = {{1, 0}, {1, 0}, {1, 1}},
};
int dir[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};

typedef struct Piece {
  float x;
  float y;
  PieceType piece_type;
} Piece;

typedef struct State {
  int scr_width;
  int scr_height;
  int fb_width;
  int fb_height;
  int score;
  bool is_game_over;
  Piece current_piece;
  Piece next_piece;
} State;

typedef struct VertexRenderer {
  GLuint vao;
  GLuint vbo;
  GLuint shader_program;
} VertexRenderer;

typedef struct TextRenderer {
  GLuint vao;
  GLuint vbo;
  GLuint shader_program;
} TextRenderer;

typedef struct Renderer {
  TextRenderer text_renderer;
  VertexRenderer vertex_renderer;
  GLFWwindow *window;
} Renderer;

typedef struct Vertex {
  float pos[2];
  float col[3];
} Vertex;

Vertex batch[MAX_VERTS];
int batch_index = 0;

typedef struct Character {
  unsigned int texture_id;
  int size[2];
  int bearing[2];
  unsigned int advance;
} Character;
Character characters[128];

typedef struct TextVertex {
  float pos[2];
  float tex[2];
} TextVertex;

static const char *text_vertex_shader_source =
    "#version 330 core\n"
    "layout (location = 0) in vec2 aPos;\n"
    "layout (location = 1) in vec2 aTex;\n"

    "out vec2 texCoords;\n"
    "uniform mat4 projection;\n"

    "void main()\n"
    "{\n"
    "   gl_Position = projection * vec4(aPos, 0.0, 1.0);\n"
    "   texCoords = aTex;\n"
    "}\n";

static const char *text_fragment_shader_source =
    "#version 330 core\n"
    "in vec2 texCoords;\n"
    "out vec4 color;\n"

    "uniform sampler2D text;\n"
    "uniform vec3 text_color;\n"

    "void main()\n"
    "{\n"
    "   vec4 sampled = vec4(1.0, 1.0, 1.0, texture(text, texCoords).r);\n"
    "   color = vec4(text_color, 1.0) * sampled;"
    "}\n";

static const char *vertex_shader_source =
    "#version 330 core\n"
    "layout (location = 0) in vec2 aPos;\n"
    "layout (location = 1) in vec3 aCol;\n"
    "layout (location = 2) in vec4 vertex;\n"

    "out vec3 color;\n"
    "uniform mat4 projection;\n"

    "void main()\n"
    "{\n"
    "   gl_Position = projection * vec4(aPos.x, aPos.y, 0.0, 1.0);\n" // 2d not
                                                                      // 3d
    "   color = aCol;\n"
    "}\n";

static const char *fragment_shader_source = "#version 330 core\n"
                                            "in vec3 color;\n"
                                            "out vec4 FragColor;\n"
                                            "void main()\n"
                                            "{\n"
                                            "FragColor = vec4(color, 1.0f);\n"
                                            "}\n";

void error_callback(int error, const char *description) {
  fprintf(stderr, "Error: %s\n", description);
}

void key_callback(GLFWwindow *window, int key, int scancode, int action,
                  int mods) {
  State *state = glfwGetWindowUserPointer(window);
  if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
    glfwDestroyWindow(window);
    glfwTerminate();
    exit(EXIT_SUCCESS);
  }

  if (key == GLFW_KEY_DOWN && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
    state->current_piece.y += 12;
  }

  if (key == GLFW_KEY_LEFT && (action == GLFW_PRESS || action == GLFW_REPEAT)) {
    if (state->current_piece.x - 10 < 0) {
      state->current_piece.x = 0;
    } else {
      state->current_piece.x -= 10;
    }
  }

  if (key == GLFW_KEY_RIGHT &&
      (action == GLFW_PRESS || action == GLFW_REPEAT)) {
    if (state->current_piece.piece_type == PIECE_I &&
        state->current_piece.x + 10 >= GAME_SCR_WIDTH - CELL_SIZE) {
      state->current_piece.x = GAME_SCR_WIDTH - CELL_SIZE;
    } else if (state->current_piece.piece_type != PIECE_I &&
               state->current_piece.x + 10 >= GAME_SCR_WIDTH - CELL_SIZE * 2) {
      state->current_piece.x = GAME_SCR_WIDTH - CELL_SIZE * 2;
    } else {
      state->current_piece.x += 10;
    }
  }
}

typedef float Mat4Row[4];
Mat4Row *ortho(int left, int right, int bottom, int top) {
  float (*result)[4] = calloc(4, sizeof(Mat4Row));

  result[0][0] = 2.0 / (right - left);
  result[1][1] = 2.0 / (top - bottom);
  result[2][2] = -1.0;
  result[3][0] = -1.0 * (right + left) / (right - left);
  result[3][1] = -1.0 * (top + bottom) / (top - bottom);
  result[3][3] = 1.0;

  return result;
}

void push_quad(float width, float height, float x, float y, GLFWwindow *window,
               float rgb_color[3]) {
  Vertex vertices[] = {
      {{x, y}, {rgb_color[0], rgb_color[1], rgb_color[2]}},
      {{x + width, y}, {rgb_color[0], rgb_color[1], rgb_color[2]}},
      {{x, y + height}, {rgb_color[0], rgb_color[1], rgb_color[2]}},
      {{x + width, y}, {rgb_color[0], rgb_color[1], rgb_color[2]}},
      {{x + width, y + height}, {rgb_color[0], rgb_color[1], rgb_color[2]}},
      {{x, y + height}, {rgb_color[0], rgb_color[1], rgb_color[2]}}};

  memcpy(&batch[batch_index], vertices, sizeof(vertices));
  batch_index += 6;
}

void window_init(Renderer *renderer, State *state) {
  glfwSetErrorCallback(error_callback);

  if (!glfwInit())
    exit(EXIT_FAILURE);

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

  GLFWwindow *window =
      glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Cage", NULL, NULL);
  if (!window) {
    glfwTerminate();
    exit(EXIT_FAILURE);
  }

  glfwSetKeyCallback(window, key_callback);
  glfwMakeContextCurrent(window);
  glfwSwapInterval(1); // vsync -> prevents screen tearing

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  // framebuffer size is for the viewport (actual pixels, 2x on retina)
  int fb_width, fb_height;
  glfwGetFramebufferSize(window, &fb_width, &fb_height);

  // window size is the logical size (SCR_WIDTH x SCR_HEIGHT) used for all
  // game logic
  int width, height;
  glfwGetWindowSize(window, &width, &height);

  glfwSetWindowUserPointer(window, state);

  state->fb_height = fb_height;
  state->fb_width = fb_width;
  state->scr_height = height;
  state->scr_width = width;

  renderer->window = window;
}

void vertex_renderer_init(Renderer *renderer, State *state) {
  GLuint vao;
  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);
  renderer->vertex_renderer.vao = vao;

  // vertex buffer objects (vbo)
  // this is a chunk of memory that lives in the GPU. this is why VRAM is
  // important. more VRAM -> more vertex data, textures, etc
  GLuint vertex_buffer;
  glGenBuffers(1, &vertex_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
  glBufferData(GL_ARRAY_BUFFER, MAX_VERTS * sizeof(Vertex), NULL,
               GL_DYNAMIC_DRAW);
  renderer->vertex_renderer.vbo = vertex_buffer;

  const GLuint vertex_shader =
      glCreateShader(GL_VERTEX_SHADER); // create shader object
  glShaderSource(vertex_shader, 1, &vertex_shader_source, NULL);
  glCompileShader(vertex_shader);

  // fragment shader
  const GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fragment_shader, 1, &fragment_shader_source, NULL);
  glCompileShader(fragment_shader);

  // create shader program
  const GLuint shader_program = glCreateProgram();
  // attach and link the vertex and fragment shader
  glAttachShader(shader_program, vertex_shader);
  glAttachShader(shader_program, fragment_shader);
  glLinkProgram(shader_program);
  renderer->vertex_renderer.shader_program = shader_program;

  glUseProgram(shader_program);
  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);

  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)0);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                        (void *)offsetof(Vertex, col));
  glEnableVertexAttribArray(1);

  glBindVertexArray(renderer->vertex_renderer.vao);
}

void text_renderer_init(Renderer *renderer, State *state) {
  FT_Library ft;
  if (FT_Init_FreeType(&ft)) {
    printf("error init freetype\n");
    return;
  }

  FT_Face face;
  if (FT_New_Face(ft, "./PressStart2P-Regular.ttf", 0, &face)) {
    printf("failed to load font\n");
    return;
  }

  FT_Set_Pixel_Sizes(face, 0, 48);

  if (FT_Load_Char(face, 'X', FT_LOAD_RENDER)) {
    printf("failed to load glyph\n");
    return;
  }

  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

  for (unsigned char c = 0; c < 128; c++) {
    if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
      printf("failed to load glyph\n");
      continue;
    }

    unsigned int tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, face->glyph->bitmap.width,
                 face->glyph->bitmap.rows, 0, GL_RED, GL_UNSIGNED_BYTE,
                 face->glyph->bitmap.buffer);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    Character character = {
        .texture_id = tex,
        .advance = face->glyph->advance.x,
        .size = {face->glyph->bitmap.width, face->glyph->bitmap.rows},
        .bearing = {face->glyph->bitmap_left, face->glyph->bitmap_top},
    };
    characters[c] = character;
  }

  FT_Done_Face(face);
  FT_Done_FreeType(ft);

  GLuint vao;
  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);
  renderer->text_renderer.vao = vao;

  GLuint vbo;
  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, 128 * sizeof(TextVertex), NULL,
               GL_DYNAMIC_DRAW);
  renderer->text_renderer.vbo = vbo;

  const GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
  glShaderSource(vertex_shader, 1, &text_vertex_shader_source, NULL);
  glCompileShader(vertex_shader);

  const GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
  glShaderSource(fragment_shader, 1, &text_fragment_shader_source, NULL);
  glCompileShader(fragment_shader);

  const GLuint shader_program = glCreateProgram();
  // attach and link the vertex and fragment shader
  glAttachShader(shader_program, vertex_shader);
  glAttachShader(shader_program, fragment_shader);
  glLinkProgram(shader_program);
  renderer->text_renderer.shader_program = shader_program;

  glUseProgram(shader_program);

  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);

  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(TextVertex),
                        (void *)0);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(TextVertex),
                        (void *)offsetof(TextVertex, tex));
  glEnableVertexAttribArray(1);
}

void crumble(Piece *piece) {
  int cells_per_block = CELL_SIZE / SAND_SIZE;
  for (int row = 0; row < 3; row++) {
    for (int col = 0; col < 2; col++) {
      if (piece_shape[piece->piece_type][row][col] == 1) {
        int base_x = (piece->x + col * CELL_SIZE) / SAND_SIZE;
        int base_y = (piece->y + row * CELL_SIZE) / SAND_SIZE;
        for (int sy = 0; sy < cells_per_block; sy++) {
          for (int sx = 0; sx < cells_per_block; sx++) {
            int gx = base_x + sx;
            int gy = base_y + sy;
            if (gx >= 0 && gx < GRID_W && gy >= 0 && gy < GRID_H) {
              cells[gy][gx].cell_type = SAND;
              cells[gy][gx].piece_type = piece->piece_type;
              memcpy(&cells[gy][gx].color,
                     &rgb_table[piece_to_color[piece->piece_type]],
                     sizeof(rgb_table[0]));
            }
          }
        }
      }
    }
  }
}

bool collide(Piece *piece, State *state) {
  int row_index = ((int)piece->y + 3 * CELL_SIZE) / SAND_SIZE;
  if (row_index >= GRID_H)
    return true;

  Cell *last_row = cells[row_index];
  int start_col = (int)piece->x / SAND_SIZE;
  int last_col = ((int)piece->x + 2 * CELL_SIZE) / SAND_SIZE;
  for (int i = start_col; i < last_col; i++) {
    if (i >= 0 && i < GRID_W && last_row[i].cell_type == SAND)
      return true;
  }
  return false;
}

void draw_piece(Renderer *renderer, Piece *piece) {
  for (int row = 0; row < 3; row++) {
    for (int col = 0; col < 2; col++) {
      if (piece_shape[piece->piece_type][row][col] == 1) {
        push_quad(CELL_SIZE, CELL_SIZE, piece->x + col * CELL_SIZE,
                  piece->y + row * CELL_SIZE, renderer->window,
                  rgb_table[piece_to_color[piece->piece_type]]);
      }
    }
  }
}

float get_center_x(float pos1, float pos2, float width) {
  float center = pos1 + ((pos2 - pos1) / 2);
  return center - width / 2;
}

void spawn_piece(Renderer *renderer) {
  State *state = glfwGetWindowUserPointer(renderer->window);
  draw_piece(renderer, &state->current_piece);
  if (collide(&state->current_piece, state)) {
    crumble(&state->current_piece);
    state->current_piece.piece_type = state->next_piece.piece_type;
    state->current_piece.x = rand() % (GAME_SCR_WIDTH - CELL_SIZE * 2);
    state->current_piece.y = 0;
    if (collide(&state->current_piece, state)) {
      state->is_game_over = true;
      return;
    }

    state->next_piece.piece_type = rand() % PIECE_COUNT;
    state->next_piece.x = get_center_x(
        425, 625, state->next_piece.piece_type == PIECE_I ? 40 : 80);
    state->next_piece.y = 325;
  }
  if (state->current_piece.y + 3 * CELL_SIZE < state->scr_height) {
    state->current_piece.y += 1;
  }
}

void update_sand() {
  for (int row = GRID_H - 2; row >= 0; row--) {
    for (int col = 0; col < GRID_W; col++) {
      if (cells[row][col].cell_type == SAND) {
        if (cells[row + 1][col].cell_type == EMPTY) {
          cells[row + 1][col] = cells[row][col];
          cells[row][col].cell_type = EMPTY;
        } else {
          int dir = (rand() % 2) ? 1 : -1;
          if (col + dir >= 0 && col + dir < GRID_W &&
              cells[row + 1][col + dir].cell_type == EMPTY) {
            cells[row + 1][col + dir] = cells[row][col];
            cells[row][col].cell_type = EMPTY;
          } else if (col - dir >= 0 && col - dir < GRID_W &&
                     cells[row + 1][col - dir].cell_type == EMPTY) {
            cells[row + 1][col - dir] = cells[row][col];
            cells[row][col].cell_type = EMPTY;
          }
        }
      }
    }
  }
}

void draw_grid(Renderer *renderer, GLFWwindow *window) {
  for (int row = 0; row < GRID_H; row++) {
    for (int col = 0; col < GRID_W; col++) {
      if (cells[row][col].clearing) {
        bool flash_on = ((int)(glfwGetTime() * 10) % 2 == 0);
        float white[3] = {1.0f, 1.0f, 1.0f};
        push_quad(SAND_SIZE, SAND_SIZE, col * SAND_SIZE, row * SAND_SIZE,
                  window, flash_on ? white : cells[row][col].color);
      } else if (cells[row][col].cell_type == SAND) {
        push_quad(SAND_SIZE, SAND_SIZE, col * SAND_SIZE, row * SAND_SIZE,
                  window, cells[row][col].color);
      }
    }
  }
}

bool flood_fill(int row, int col, int piece_type) {
  bool reached_right = false;
  stack[stack_index++] = (Coord){row, col};

  visited[row][col] = true;
  visited_count += 1;

  while (stack_index > 0) {
    Coord c = stack[--stack_index];

    for (int i = 0; i < 4; i++) {
      int nx = c.row + dir[i][0];
      int ny = c.col + dir[i][1];

      if (nx < 0 || nx >= GRID_H || ny < 0 || ny >= GRID_W)
        continue;

      if (visited[nx][ny])
        continue;

      if (cells[nx][ny].piece_type != piece_type ||
          cells[nx][ny].cell_type != SAND || cells[nx][ny].clearing)
        continue;

      visited[nx][ny] = true;
      visited_count += 1;

      if (ny == GRID_W - 1)
        reached_right = true;

      stack[stack_index++] = (Coord){.row = nx, .col = ny};
    }
  }

  return reached_right;
}

int get_digit_count(int score) {
  if (score == 0)
    return 1;
  int count = 0;
  while (score > 0) {
    score /= 10;
    count++;
  }
  return count;
}

char *int_to_string(int score) {
  int n = get_digit_count(score);
  char *result = malloc((n + 1) * sizeof(char));

  int i = n - 1;
  if (!score) {
    result[i] = (score % 10) + '0';
  } else {
    while (score) {
      result[i] = (score % 10) + '0';
      score /= 10;
      i--;
    }
  }
  result[n] = '\0';
  return result;
}

void calculate_score(Renderer *renderer) {
  State *state = glfwGetWindowUserPointer(renderer->window);
  state->score += visited_count * POINTS_PER_CELL;
}

void clear_line(Renderer *renderer) {
  int left_row[PIECE_COUNT];
  for (int p = 0; p < PIECE_COUNT; p++)
    left_row[p] = -1;

  bool right_has[PIECE_COUNT] = {false};

  for (int row = GRID_H - 1; row >= 0; row--) {
    if (cells[row][0].cell_type == SAND && !cells[row][0].clearing) {
      left_row[cells[row][0].piece_type] = row;
    }
    if (cells[row][GRID_W - 1].cell_type == SAND && !cells[row][0].clearing) {
      right_has[cells[row][GRID_W - 1].piece_type] = true;
    }
  }

  for (int p = 0; p < PIECE_COUNT; p++) {
    if (left_row[p] != -1 && right_has[p]) {
      memset(visited, false, sizeof(visited));
      visited_count = 0;
      if (flood_fill(left_row[p], 0, p)) {
        calculate_score(renderer);
        clear_start_time = glfwGetTime();
        for (int row = 0; row < GRID_H; row++) {
          for (int col = 0; col < GRID_W; col++) {
            if (visited[row][col]) {
              cells[row][col].clearing = true;
            }
          }
        }
      }
    }
  }
}

void flush_batch_game(Renderer *renderer) {
  State *state = glfwGetWindowUserPointer(renderer->window);
  glViewport(0, 0, (float)state->fb_width / state->scr_width * 400,
             state->fb_height);
  glBindVertexArray(renderer->vertex_renderer.vao);
  glBindBuffer(GL_ARRAY_BUFFER, renderer->vertex_renderer.vbo);
  glBufferSubData(GL_ARRAY_BUFFER, 0, batch_index * sizeof(Vertex), batch);
  glUseProgram(renderer->vertex_renderer.shader_program);
  GLint loc = glGetUniformLocation(renderer->vertex_renderer.shader_program,
                                   "projection");
  Mat4Row *projection = ortho(0, GAME_SCR_WIDTH, state->scr_height, 0);
  glUniformMatrix4fv(loc, 1, GL_FALSE, (float *)projection);
  free(projection);
  glDrawArrays(GL_TRIANGLES, 0, batch_index);
}

void flush_batch_ui(Renderer *renderer) {
  State *state = glfwGetWindowUserPointer(renderer->window);
  glViewport((float)state->fb_width / state->scr_width * 400, 0,
             (float)state->fb_width / state->scr_width * 250, state->fb_height);
  glBindVertexArray(renderer->vertex_renderer.vao);
  glBindBuffer(GL_ARRAY_BUFFER, renderer->vertex_renderer.vbo);
  glBufferSubData(GL_ARRAY_BUFFER, 0, batch_index * sizeof(Vertex), batch);
  glUseProgram(renderer->vertex_renderer.shader_program);
  GLint loc = glGetUniformLocation(renderer->vertex_renderer.shader_program,
                                   "projection");
  Mat4Row *projection = ortho(400, 650, state->scr_height, 0);
  glUniformMatrix4fv(loc, 1, GL_FALSE, (float *)projection);
  free(projection);
  glDrawArrays(GL_TRIANGLES, 0, batch_index);
}

void render_text(Renderer *renderer, char *text, float x, float y, float scale,
                 enum Color color) {
  State *state = glfwGetWindowUserPointer(renderer->window);
  y = state->scr_height - y;
  glViewport((float)state->fb_width / state->scr_width * 400, 0,
             (float)state->fb_width / state->scr_width * 250, state->fb_height);
  glUseProgram(renderer->text_renderer.shader_program);
  GLint loc = glGetUniformLocation(renderer->text_renderer.shader_program,
                                   "projection");
  Mat4Row *projection = ortho(400, 650, 0, state->scr_height);
  glUniformMatrix4fv(loc, 1, GL_FALSE, (float *)projection);
  free(projection);

  glUniform3f(glGetUniformLocation(renderer->text_renderer.shader_program,
                                   "text_color"),
              rgb_table[color][0], rgb_table[color][1], rgb_table[color][2]);
  glActiveTexture(GL_TEXTURE0);
  glBindVertexArray(renderer->text_renderer.vao);

  for (int i = 0; text[i] != '\0'; i++) {
    Character ch = characters[text[i]];

    float xpos = x + ch.bearing[0] * scale;
    float ypos = y - (ch.size[1] - ch.bearing[1]) * scale;

    float w = ch.size[0] * scale;
    float h = ch.size[1] * scale;

    TextVertex vertices[6] = {
        {{xpos, ypos + h}, 0.0f, 0.0f},     {{xpos, ypos}, 0.0f, 1.0f},
        {{xpos + w, ypos}, 1.0f, 1.0f},

        {{xpos, ypos + h}, 0.0f, 0.0f},     {{xpos + w, ypos}, 1.0f, 1.0f},
        {{xpos + w, ypos + h}, 1.0f, 0.0f},
    };

    glBindTexture(GL_TEXTURE_2D, ch.texture_id);
    glBindBuffer(GL_ARRAY_BUFFER, renderer->text_renderer.vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glDrawArrays(GL_TRIANGLES, 0, 6);

    x += (ch.advance >> 6) * scale;
  }

  glBindVertexArray(0);
  glBindTexture(GL_TEXTURE_2D, 0);
}

void draw_next_piece(Renderer *renderer) {
  State *state = glfwGetWindowUserPointer(renderer->window);
  draw_piece(renderer, &state->next_piece);
}

void draw_divider(Renderer *renderer) {
  State *state = glfwGetWindowUserPointer(renderer->window);
  push_quad(12.0, state->scr_height, 400, 0, renderer->window,
            rgb_table[BEZEL_DARK]);
  push_quad(8.0, state->scr_height, 400, 0, renderer->window,
            rgb_table[BEZEL_LIGHT]);
  push_quad(2.0, state->scr_height, 400, 0, renderer->window,
            rgb_table[HIGHLIGHT]);
}

void draw_box(Renderer *renderer, float x1, float y1, float x2, float y2) {
  float w = x2 - x1;
  float h = y2 - y1;
  push_quad(w, 3, x1, y1, renderer->window, rgb_table[BEZEL_DARK]); // top
  push_quad(3, h, x1, y1, renderer->window, rgb_table[BEZEL_DARK]); // left
  push_quad(w, 3, x1, y2, renderer->window, rgb_table[BEZEL_DARK]); // bottom
  push_quad(3, h + 2, x2, y1, renderer->window, rgb_table[BEZEL_DARK]); // right
}

void draw_score_box(Renderer *renderer) {
  draw_box(renderer, 425, 50, 625, 150);
}

void draw_next_piece_box(Renderer *renderer) {
  draw_box(renderer, 425, 225, 625, 475);
}

float get_string_width(char *string, float scale) {
  float width = 0;
  for (int i = 0; string[i] != '\0'; i++) {
    Character c = characters[string[i]];
    width += (c.advance >> 6) * scale;
  }

  return width;
}

int main() {
  srand(time(NULL));
  State state = {0};
  Renderer renderer = {0};

  window_init(&renderer, &state);
  vertex_renderer_init(&renderer, &state);
  text_renderer_init(&renderer, &state);

  state.current_piece.x = rand() % (GAME_SCR_WIDTH - CELL_SIZE * 2);
  state.current_piece.piece_type = rand() % PIECE_COUNT;

  state.next_piece.piece_type = rand() % PIECE_COUNT;
  state.next_piece.x =
      get_center_x(425, 625, state.next_piece.piece_type == PIECE_I ? 40 : 80);
  state.next_piece.y = 325;

  while (!glfwWindowShouldClose(renderer.window)) {
    if (state.is_game_over)
      break;

    glClear(GL_COLOR_BUFFER_BIT);
    batch_index = 0;

    spawn_piece(&renderer);

    update_sand();

    clear_line(&renderer);
    if (clear_start_time >= 0 &&
        glfwGetTime() - clear_start_time > BLINK_DURATION) {
      for (int row = 0; row < GRID_H; row++) {
        for (int col = 0; col < GRID_W; col++) {
          if (cells[row][col].clearing) {
            cells[row][col].clearing = false;
            cells[row][col].cell_type = EMPTY;
          }
        }
      }
      clear_start_time = -1;
    }
    draw_grid(&renderer, renderer.window);

    flush_batch_game(&renderer);
    batch_index = 0;

    draw_divider(&renderer);
    draw_score_box(&renderer);
    draw_next_piece_box(&renderer);
    draw_next_piece(&renderer);

    flush_batch_ui(&renderer);

    render_text(&renderer, "SCORE",
                get_center_x(425, 625, get_string_width("SCORE", 0.5)), 90, 0.5,
                GREEN);

    char *score_str = int_to_string(state.score);
    render_text(&renderer, score_str,
                get_center_x(425, 625, get_string_width(score_str, 0.5)), 120,
                0.5, HIGHLIGHT);
    free(score_str);

    render_text(&renderer, "NEXT",
                get_center_x(425, 625, get_string_width("NEXT", 0.5)), 270, 0.5,
                GREEN);
    render_text(&renderer, "PIECE",
                get_center_x(425, 625, get_string_width("PIECE", 0.5)), 300,
                0.5, GREEN);

    glfwSwapBuffers(renderer.window);
    glfwPollEvents();
  }

  glfwDestroyWindow(renderer.window);
  glfwTerminate();
  exit(EXIT_SUCCESS);
}
