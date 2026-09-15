# Sandtris

Tetris with some tweaks

https://github.com/user-attachments/assets/797edf6a-4960-42e1-a6ce-9f42a0d4e510

## Controls

| Key          | Action                 |
| ------------ | ---------------------- |
| Left / Right | Move the falling piece |
| Down         | Drop faster            |
| Esc          | Quit                   |

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
- A cellular-automaton sand simulation, with flood fill driving the clear condition.
