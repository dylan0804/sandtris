# Sandtris

A falling-sand puzzle game inspired by Tetris. Pieces fall, crumble into individual grains of sand, and settle realistically instead of locking into rigid rows. Clear the board by connecting a single color's sand from the left wall to the right wall.

![gameplay screenshot]

## Controls

| Key          | Action                 |
| ------------ | ---------------------- |
| Left / Right | Move the falling piece |
| Down         | Drop faster            |
| Esc          | Quit                   |

## How it clears

Unlike classic Tetris, a full row doesn't clear anything. Instead, when a piece crumbles it becomes individual grains of colored sand that tumble and settle. Whenever one color's sand forms a connected path spanning the entire width of the board, left wall to right wall, that whole connected blob clears and adds to your score.

## Building

**Prerequisites** (macOS, via [Homebrew](https://brew.sh)):

```
brew install glfw freetype
```

**Build and run:**

```
make run
```

Or build only:

```
make
./engine
```

## Important things to note

- Raw OpenGL 3.3 core profile — no glm, no game engine. VAOs/VBOs, shaders, and projection matrices are all managed and written by hand.
- Text rendering built directly on FreeType: glyphs are loaded, uploaded as textures, and composited with the game's own renderer in the same frame.
- A from-scratch orthographic projection matrix implementation in plain C.
- A cellular-automaton sand simulation, with connected-component flood fill driving the clear condition.
