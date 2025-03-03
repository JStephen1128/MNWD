#include "raylib.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

#if defined(PLATFORM_WEB)
    #include <emscripten/emscripten.h>
#endif

//----------------------------------------------------------------------------------
// Some Defines
//----------------------------------------------------------------------------------
#define SQUARE_SIZE             30

#define GRID_HORIZONTAL_SIZE    12
#define GRID_VERTICAL_SIZE      20

#define LATERAL_SPEED           15
#define TURNING_SPEED           12
#define FAST_FALL_AWAIT_COUNTER 30

#define FADING_TIME             33

//----------------------------------------------------------------------------------
// Types and Structures Definition
//----------------------------------------------------------------------------------
typedef enum GridSquare { EMPTY, MOVING, FULL, BLOCK, FADING } GridSquare;
typedef enum GameState { TITLE_SCREEN, TUTORIAL, PLAYING, GAME_OVER } GameState;

//------------------------------------------------------------------------------------
// Global Variables Declaration
//------------------------------------------------------------------------------------
static const int screenWidth = 840;
static const int screenHeight = 620;

static Texture2D Titull;
static Texture2D GameScreen;
static Texture2D GameOvers;
static Texture2D TLC;

static bool gameOver = false;
static bool pause = false;

static int gameOverTimer = 0;
static bool gameOverTriggered = false;

static GameState currentGameState = TITLE_SCREEN;

//Block Sprites
Texture2D RedTexture;
Texture2D BlueTexture;
Texture2D YellowTexture;

// Matrices
static GridSquare grid [GRID_HORIZONTAL_SIZE][GRID_VERTICAL_SIZE];
static GridSquare piece [4][4];
static GridSquare incomingPiece [4][4];
Color boardColors[GRID_HORIZONTAL_SIZE][GRID_VERTICAL_SIZE] = {0};

// Theese variables keep track of the active piece position
static int piecePositionX = 0;
static int piecePositionY = 0;

// Game parameters
static Color fadingColor;
static bool beginPlay = true;      
static bool pieceActive = false;
static bool detection = false;
static bool lineToDelete = false;
static Color pieceColor;
static Color gridColors[GRID_HORIZONTAL_SIZE][GRID_VERTICAL_SIZE]; 


// Statistics
static int level = 1;
static int lines = 0;
static int score = 0;
static int hiscore = 0;

//music
static Music music;

// Counters
static int gravityMovementCounter = 0;
static int lateralMovementCounter = 0;
static int turnMovementCounter = 0;
static int fastFallMovementCounter = 0;

static int fadeLineCounter = 0;

static int gravitySpeed = 15;

//------------------------------------------------------------------------------------
// Module Functions Declaration (local)
//------------------------------------------------------------------------------------
static void InitGame(void);         // Initialize game
static void UpdateGame(void);       // Update game (one frame)
static void DrawGame(void);         // Draw game (one frame)
static void UnloadGame(void);       // Unload game
static void UpdateDrawFrame(void);  // Update and Draw (one frame)

// Additional module functions
static bool Createpiece();
static void GetRandompiece();
static void ResolveFallingMovement(bool *detection, bool *pieceActive);
static bool ResolveLateralMovement();
static bool ResolveTurnMovement();
static void CheckDetection(bool *detection);
static void CheckCompletion(bool *lineToDelete);
static int DeleteCompleteLines();

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
int main(void)
{
    // Initialization (Note windowTitle is unused on Android)
    //---------------------------------------------------------
    InitWindow(screenWidth, screenHeight, "Mega's Nuclear Waste Dump");
    InitAudioDevice(); 

    
    InitGame();

#if defined(PLATFORM_WEB)
    emscripten_set_main_loop(UpdateDrawFrame, 60, 1);
#else
    SetTargetFPS(60);
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        
        // Update and Draw
        //----------------------------------------------------------------------------------
        UpdateDrawFrame();
        //----------------------------------------------------------------------------------
    }
#endif
    // De-Initialization
    //--------------------------------------------------------------------------------------
    UnloadGame();         // Unload loaded data (textures, sounds, models...)

    CloseWindow();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}

//--------------------------------------------------------------------------------------
// Game Module Functions Definition
//--------------------------------------------------------------------------------------

// Initialize game variables
void InitGame(void)
{
    // Initialize game statistics
    level = 1;
    lines = 0;
    score = 0;

    // Initialize the audio system
    music = LoadMusicStream("theme.mp3"); 
    
    fadingColor = GRAY;

    piecePositionX = 0;
    piecePositionY = 0;

    pause = false;

    beginPlay = true;
    pieceActive = false;
    detection = false;
    lineToDelete = false;

    RedTexture = LoadTexture("RedBarrell.png");
    BlueTexture = LoadTexture("BlueBarrell.png");
    YellowTexture = LoadTexture("YellowBarrell.png");
    GameScreen = LoadTexture("GameScreen.png");
    GameOvers = LoadTexture("GameOver.png");
    TLC = LoadTexture("Tut.png");
    Titull = LoadTexture("TitleProbably.png");


    // Counters
    gravityMovementCounter = 0;
    lateralMovementCounter = 0;
    turnMovementCounter = 0;
    fastFallMovementCounter = 0;

    fadeLineCounter = 0;
    gravitySpeed = 15;

    // Initialize grid matrices
    for (int i = 0; i < GRID_HORIZONTAL_SIZE; i++)
    {
        for (int j = 0; j < GRID_VERTICAL_SIZE; j++)
        {
            if ((j == GRID_VERTICAL_SIZE - 1) || (i == 0) || (i == GRID_HORIZONTAL_SIZE - 1)) grid[i][j] = BLOCK;
            else grid[i][j] = EMPTY;
        }
    }
    
    
    // Initialize incoming piece matrices
    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j< 4; j++)
        {
            incomingPiece[i][j] = EMPTY;
        }
    }
    

}

// Update game (one frame)
void UpdateGame(void)
{
    
    if (currentGameState == TITLE_SCREEN)
    {
        if (IsKeyPressed(KEY_ENTER)) 
        {
            currentGameState = TUTORIAL;
        }
    }
    
    else if (currentGameState == TUTORIAL)
    {
        if (IsKeyPressed(KEY_ENTER)) 
        {
            currentGameState = PLAYING;
            InitGame();
        }
    }
    

    else if (currentGameState == PLAYING)
    {

        UpdateMusicStream(music);
            PlayMusicStream(music);
        
            if (gameOverTriggered)
            {
                gameOverTimer--;
                if (gameOverTimer <= 0)
                {
                    
                    currentGameState = GAME_OVER;
                }
            }
            else
            {

                if (!pause)
                {
                    if (!lineToDelete)
                    {
                        if (!pieceActive)
                        {
                            pieceActive = Createpiece();
                            fastFallMovementCounter = 0;
                        }
                        else
                        {
                            fastFallMovementCounter++;
                            gravityMovementCounter++;
                            lateralMovementCounter++;
                            turnMovementCounter++;

                            if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_RIGHT)) lateralMovementCounter = LATERAL_SPEED;
                            if (IsKeyPressed(KEY_UP)) turnMovementCounter = TURNING_SPEED;

                            if (IsKeyDown(KEY_DOWN) && (fastFallMovementCounter >= FAST_FALL_AWAIT_COUNTER))
                            {
                                gravityMovementCounter += gravitySpeed;
                            }

                            if (gravityMovementCounter >= gravitySpeed)
                            {
                                CheckDetection(&detection);
                                ResolveFallingMovement(&detection, &pieceActive);
                                CheckCompletion(&lineToDelete);
                                gravityMovementCounter = 0;
                            }

                            if (lateralMovementCounter >= LATERAL_SPEED)
                            {
                                if (!ResolveLateralMovement()) lateralMovementCounter = 0;
                            }

                            if (turnMovementCounter >= TURNING_SPEED)
                            {
                                if (ResolveTurnMovement()) turnMovementCounter = 0;
                            }
                        }

                        for (int j = 0; j < 2; j++)
                        {
                            for (int i = 1; i < GRID_HORIZONTAL_SIZE - 1; i++)
                            {
                                if (grid[i][j] == FULL)
                                {
                                    gameOver = true;
                                    currentGameState = GAME_OVER;
                                }
                            }
                        }
                    }
                    else
                    {
                        fadeLineCounter++;

                        if (fadeLineCounter % 8 < 4) fadingColor = WHITE;
                        else fadingColor = GRAY;

                        if (fadeLineCounter >= FADING_TIME)
                        {
                            int deletedLines = DeleteCompleteLines();
                            fadeLineCounter = 0;
                            lineToDelete = false;
                            lines += deletedLines;
                            score += (56 + (lines * 98));
                        }
                    }
                }
            }
        }
        
    else if (currentGameState == GAME_OVER)
    {
    
        if (IsKeyPressed(KEY_ENTER))
        {
            if (score > hiscore){hiscore = score;}
            gameOverTimer = 120;
            gameOverTriggered = false;
            gameOver = false;
            currentGameState = TITLE_SCREEN;
            
            StopMusicStream(music);   
           
        }
    }
}

// Draw game (one frame)
void DrawGame(void)
{
    BeginDrawing();
    ClearBackground(RAYWHITE);

    if (currentGameState == TITLE_SCREEN)
    {
        DrawTexture(Titull, 0, 0, WHITE);
        DrawText("High Score", screenWidth/2 - MeasureText("High Score", 20)/2, 5, 20, RED);
        DrawText(TextFormat("%05i", hiscore), screenWidth/2 - MeasureText("00000", 20)/2, 25, 20, WHITE);        
        DrawText("Press [Enter] to Start", screenWidth/2 - MeasureText("Press [Enter] to Start", 30)/2, screenHeight/2 + 62, 30, WHITE);
        DrawText("© 2025 MegaKoopa255", screenWidth/2 - MeasureText("© 2025 MegaKoopa255", 20)/2, screenHeight-20, 20, WHITE);

    }
    
    else if (currentGameState == TUTORIAL)
    {
        DrawTexture(TLC, 0, 0, WHITE);
        DrawText("After 39 long years, Uncle Henry has retired from his job at the", 35, 30, 20, WHITE);
        DrawText("nuclear waste dump and has sold the land to me!", 35, 50, 20, WHITE);
        DrawText("As my newest employee, you are now tasked with taking up the duties", 35, 70, 20, WHITE);
        DrawText("of handling the toxic waste.", 35, 90, 20, WHITE);
        DrawText("Basically, you must dispose of the waste by making rows, but avoid", 35, 110, 20, WHITE);
        DrawText("matching the same colors. If you try to stack a container of waste on", 35, 130, 20, WHITE);
        DrawText("top of another, it will fall to the side (it prioritizes the left, then,", 35, 150, 20, WHITE);
        DrawText("the right) so be mindful of that! Additionally, clearing a row can sometimes", 35, 170, 20, WHITE);
        DrawText("mutate the remaining containers on the board into different types.", 35, 190, 20, WHITE);
        DrawText("I hope you've got insurance!", 35, 210, 20, WHITE);
        

    }
    
    else if (currentGameState == PLAYING)
    {

        
        DrawTexture(GameScreen, 0, 0, WHITE);
        
            // Draw gameplay area
            Vector2 offset;
            offset.x = screenWidth/2 - (GRID_HORIZONTAL_SIZE*SQUARE_SIZE/2) - 50;
            offset.y = screenHeight/2 - ((GRID_VERTICAL_SIZE - 1)*SQUARE_SIZE/2) + SQUARE_SIZE*2;

            offset.y -= 50;     

            int controller = offset.x; 

            for (int j = 0; j < GRID_VERTICAL_SIZE; j++)
            {
                for (int i = 0; i < GRID_HORIZONTAL_SIZE; i++)
                {
                    // Draw each square of the grid
                    if (grid[i][j] == EMPTY)
                    {
                        DrawLine(offset.x, offset.y, offset.x + SQUARE_SIZE, offset.y, GRAY );
                        DrawLine(offset.x, offset.y, offset.x, offset.y + SQUARE_SIZE, GRAY );
                        DrawLine(offset.x + SQUARE_SIZE, offset.y, offset.x + SQUARE_SIZE, offset.y + SQUARE_SIZE, GRAY );
                        DrawLine(offset.x, offset.y + SQUARE_SIZE, offset.x + SQUARE_SIZE, offset.y + SQUARE_SIZE, GRAY );
                        offset.x += SQUARE_SIZE;
                    }
                    else if (grid[i][j] == FULL)
                    {
                        if (grid[i][j] == FULL)
                        {
                            if (gridColors[i][j].r == RED.r && gridColors[i][j].g == RED.g && gridColors[i][j].b == RED.b)
                            {
                                DrawTexture(RedTexture, offset.x, offset.y, WHITE);
                            }
                            else if (gridColors[i][j].r == BLUE.r && gridColors[i][j].g == BLUE.g && gridColors[i][j].b == BLUE.b)
                            {
                                DrawTexture(BlueTexture, offset.x, offset.y, WHITE);
                            }
                            else if (gridColors[i][j].r == YELLOW.r && gridColors[i][j].g == YELLOW.g && gridColors[i][j].b == YELLOW.b)
                            {
                                DrawTexture(YellowTexture, offset.x, offset.y, WHITE);
                            }
                        }
                        offset.x += SQUARE_SIZE;
                    }
                    else if (grid[i][j] == MOVING)
                    {
                        
                        Color movingColor = pieceColor; 

                        if (movingColor.r == RED.r && movingColor.g == RED.g && movingColor.b == RED.b)
                        {
                            DrawTexture(RedTexture, offset.x, offset.y, WHITE);
                        }
                        else if (movingColor.r == BLUE.r && movingColor.g == BLUE.g && movingColor.b == BLUE.b)
                        {
                            DrawTexture(BlueTexture, offset.x, offset.y, WHITE);
                        }
                        else if (movingColor.r == YELLOW.r && movingColor.g == YELLOW.g && movingColor.b == YELLOW.b)
                        {
                            DrawTexture(YellowTexture, offset.x, offset.y, WHITE);
                        }
                        else
                        {
                            DrawRectangle(offset.x, offset.y, SQUARE_SIZE, SQUARE_SIZE, GRAY);
                        }

                        offset.x += SQUARE_SIZE;
                    }
                    else if (grid[i][j] == BLOCK)
                    {
                        
                        offset.x += SQUARE_SIZE;
                    }
                    else if (grid[i][j] == FADING)
                    {
                        DrawRectangle(offset.x, offset.y, SQUARE_SIZE, SQUARE_SIZE, fadingColor);
                        offset.x += SQUARE_SIZE;
                    }
                }

                offset.x = controller;
                offset.y += SQUARE_SIZE;
            }

            // Draw incoming piece (hardcoded)
            offset.x = 600;
            offset.y = 45;

            int controler = offset.x;

            for (int j = 0; j < 4; j++)
            {
                for (int i = 0; i < 4; i++)
                {
                    if (incomingPiece[i][j] == EMPTY)
                    {

                    }
                    else if (incomingPiece[i][j] == MOVING)
                    {
                     
                        offset.x += SQUARE_SIZE;
                    }
                }

                offset.x = controler;
                offset.y += SQUARE_SIZE;
            }

            DrawText("!!!  DANGER  !!!", offset.x-20, offset.y - 100, 30, BLACK);
            DrawText(TextFormat("  Lines:   %04i", lines), offset.x-25, offset.y + 0, 30, WHITE);
            DrawText(TextFormat(" Score:   %05i", score), offset.x-25, offset.y + 40, 30, WHITE);
            DrawText(TextFormat("Hi Score: %05i", hiscore), offset.x-25, offset.y + 80, 30, WHITE);           
            

            if (pause) DrawText("GAME PAUSED", screenWidth/2 - MeasureText("GAME PAUSED", 40)/2, screenHeight/2 - 40, 40, WHITE);
        }
        else if (currentGameState == GAME_OVER) {         DrawTexture(GameOvers, 0, 0, WHITE);
        DrawText("Good help is so hard to find...", GetScreenWidth()/2 - MeasureText("Good help is so hard to find...", 50)/2, GetScreenHeight()/2 - 130, 50, RED);
             DrawText(TextFormat("Final Score:   %05i", score), GetScreenWidth()/2 - MeasureText("Final Score:   00000", 30)/2, GetScreenHeight()/2 - 70, 30, WHITE);
             DrawText(TextFormat("Previous High Score:   %05i", hiscore), GetScreenWidth()/2 - MeasureText("Previous High Score:   00000", 30)/2, GetScreenHeight()/2 - 30, 30, WHITE);
        DrawText("Press [Enter] to Play Again", GetScreenWidth()/2 - MeasureText("Press [Enter] to Play Again", 30)/2, GetScreenHeight()/2 + 10, 30, WHITE); }

    

    EndDrawing();
}

// Unload game variables
void UnloadGame(void)
{
    // TODO: Unload all dynamic loaded data (textures, sounds, models...)
}

// Update and Draw (one frame)
void UpdateDrawFrame(void)
{
    UpdateGame();
    DrawGame();
}

//--------------------------------------------------------------------------------------
// Additional module functions
//--------------------------------------------------------------------------------------
static bool Createpiece()
{
    piecePositionX = (int)((GRID_HORIZONTAL_SIZE - 4)/2);
    piecePositionY = -4;

    // If the game is starting and you are going to create the first piece, we create an extra one
    if (beginPlay)
    {
        GetRandompiece();
        beginPlay = false;
    }

    // We assign the incoming piece to the actual piece
    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j< 4; j++)
        {
            piece[i][j] = incomingPiece[i][j];
        }
    }

    // We assign a random piece to the incoming one
    GetRandompiece();

    // Assign the piece to the grid
    for (int i = piecePositionX; i < piecePositionX + 4; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            if (piece[i - (int)piecePositionX][j] == MOVING) grid[i][j] = MOVING;
        }
    }

    return true;
}

static void GetRandompiece()
{
    int random = GetRandomValue(0, 6);

    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            incomingPiece[i][j] = EMPTY;
        }
    }

    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            incomingPiece[i][j] = EMPTY;
        }
    }

    // Generate a single block in a random position within the 4x4 grid
    int x = GetRandomValue(0, 3);
    int y = GetRandomValue(0, 3);
    incomingPiece[x][y] = MOVING;
    
    int colorChoice = GetRandomValue(0, 2);
    switch (colorChoice)
{
    case 0: pieceColor = RED; break;
    case 1: pieceColor = BLUE; break;
    case 2: pieceColor = YELLOW; break;
}

}

static void ResolveFallingMovement(bool *detection, bool *pieceActive)
{
    // If we finished moving this piece, we stop it
    if (*detection)
    {
        for (int j = GRID_VERTICAL_SIZE - 2; j >= 0; j--)
        {
            for (int i = 1; i < GRID_HORIZONTAL_SIZE - 1; i++)
            {                
                if (grid[i][j] == MOVING)
                    {
                        grid[i][j] = FULL;
                        score += (1+(abs(19-((2*lines)+1)))/4);;
                        *detection = false;
                        *pieceActive = false;
                        boardColors[i][j] = pieceColor;
                        gridColors[i][j] = pieceColor;

                        // Variables to check if movement is possible
                        bool canMoveDownLeft = false;
                        bool canMoveDownRight = false;

                        // Check if the block can move diagonally down-left
                        if (i < GRID_HORIZONTAL_SIZE - 1 && j < GRID_VERTICAL_SIZE - 1 && grid[i-1][j+1] == EMPTY) 
                        {
                            canMoveDownLeft = true;
                        }

                        // Check if the block can move diagonally down-right
                        if (i < GRID_HORIZONTAL_SIZE - 1 && j < GRID_VERTICAL_SIZE - 1 && grid[i+1][j+1] == EMPTY) 
                        {
                            canMoveDownRight = true;
                        }

                        // Move Down-Left continuously
                        while (canMoveDownLeft)
                        {
                            grid[i][j] = EMPTY;  
                            grid[i-1][j+1] = FULL; 
                            gridColors[i-1][j+1] = pieceColor;

                            j++;   
                            i--;
                            score++;
                         
                            if (j >= GRID_VERTICAL_SIZE - 1 || i <= 0 || grid[i-1][j+1] != EMPTY)
                                break;
                        }

                        // Move Down-Right continuously 
                        while (!canMoveDownLeft && canMoveDownRight)
                        {
                            grid[i][j] = EMPTY;  
                            grid[i+1][j+1] = FULL; 
                            gridColors[i+1][j+1] = pieceColor;

                            j++;   
                            i++;  
                            score++;

                            if (j >= GRID_VERTICAL_SIZE - 1 || i >= GRID_HORIZONTAL_SIZE - 1 || grid[i+1][j+1] != EMPTY)
                                break;
                        }


                        // Game Over Condition: Check for adjacent same-color blocks
                        if ((i > 0 && grid[i-1][j] == FULL && gridColors[i-1][j].r == pieceColor.r &&
                                       gridColors[i-1][j].g == pieceColor.g &&
                                       gridColors[i-1][j].b == pieceColor.b) ||
                            (i < GRID_HORIZONTAL_SIZE - 1 && grid[i+1][j] == FULL && gridColors[i+1][j].r == pieceColor.r &&
                                                           gridColors[i+1][j].g == pieceColor.g &&
                                                           gridColors[i+1][j].b == pieceColor.b) ||
                            (j > 0 && grid[i][j-1] == FULL && gridColors[i][j-1].r == pieceColor.r &&
                                                         gridColors[i][j-1].g == pieceColor.g &&
                                                         gridColors[i][j-1].b == pieceColor.b) ||
                            (j < GRID_VERTICAL_SIZE - 1 && grid[i][j+1] == FULL && gridColors[i][j+1].r == pieceColor.r &&
                                                                   gridColors[i][j+1].g == pieceColor.g &&
                                                                   gridColors[i][j+1].b == pieceColor.b))
                        {

                            if (!gameOverTriggered)  
                            {  
                                gameOverTriggered = true;
                                    score -= 200;
                                
                                gameOverTimer = 120;   
                            }
                        }
                    }
            }
        }
    }
    else    
    {
        for (int j = GRID_VERTICAL_SIZE - 2; j >= 0; j--)
        {
            for (int i = 1; i < GRID_HORIZONTAL_SIZE - 1; i++)
            {
                if (grid[i][j] == MOVING)
                {
                    grid[i][j+1] = MOVING;
                    grid[i][j] = EMPTY;
                }
            }
        }

        piecePositionY++;
    }
}

static bool ResolveLateralMovement()
{
    bool collision = false;

    // Piece movement
    if (IsKeyDown(KEY_LEFT))        // Move left
    {
        // Check if is possible to move to left
        for (int j = GRID_VERTICAL_SIZE - 2; j >= 0; j--)
        {
            for (int i = 1; i < GRID_HORIZONTAL_SIZE - 1; i++)
            {
                if (grid[i][j] == MOVING)
                {
                    // Check if we are touching the left wall or we have a full square at the left
                    if ((i-1 == 0) || (grid[i-1][j] == FULL)) collision = true;
                }
            }
        }

        // If able, move left
        if (!collision)
        {
            for (int j = GRID_VERTICAL_SIZE - 2; j >= 0; j--)
            {
                for (int i = 1; i < GRID_HORIZONTAL_SIZE - 1; i++)            
                {
                    
                    if (grid[i][j] == MOVING)
                    {
                        grid[i-1][j] = MOVING;
                        grid[i][j] = EMPTY;
                    }
                }
            }

            piecePositionX--;
        }
    }
    else if (IsKeyDown(KEY_RIGHT))  // Move right
    {
        // Check if is possible to move to right
        for (int j = GRID_VERTICAL_SIZE - 2; j >= 0; j--)
        {
            for (int i = 1; i < GRID_HORIZONTAL_SIZE - 1; i++)
            {
                if (grid[i][j] == MOVING)
                {
                    // Check if we are touching the right wall or we have a full square at the right
                    if ((i+1 == GRID_HORIZONTAL_SIZE - 1) || (grid[i+1][j] == FULL))
                    {
                        collision = true;

                    }
                }
            }
        }

        // If able move right
        if (!collision)
        {
            for (int j = GRID_VERTICAL_SIZE - 2; j >= 0; j--)
            {
                for (int i = GRID_HORIZONTAL_SIZE - 1; i >= 1; i--)             // We check the matrix from right to left
                {
                    // Move everything to the right
                    if (grid[i][j] == MOVING)
                    {
                        grid[i+1][j] = MOVING;
                        grid[i][j] = EMPTY;
                    }
                }
            }

            piecePositionX++;
        }
    }

    return collision;
}

static bool ResolveTurnMovement()
{
    // Input for turning the piece

    return false;
}

static void CheckDetection(bool *detection)
{
    for (int j = GRID_VERTICAL_SIZE - 2; j >= 0; j--)
    {
        for (int i = 1; i < GRID_HORIZONTAL_SIZE - 1; i++)
        {
            if ((grid[i][j] == MOVING) && ((grid[i][j+1] == FULL) || (grid[i][j+1] == BLOCK))) *detection = true;
        }
    }
}

static void CheckCompletion(bool *lineToDelete)
{
    int calculator = 0;

    for (int j = GRID_VERTICAL_SIZE - 2; j >= 0; j--)
    {
        calculator = 0;
        for (int i = 1; i < GRID_HORIZONTAL_SIZE - 1; i++)
        {
            // Count each square of the line
            if (grid[i][j] == FULL)
            {
                calculator++;
            }

            // Check if we completed the whole line
            if (calculator == GRID_HORIZONTAL_SIZE - 2)
            {
                *lineToDelete = true;
                calculator = 0;
                

                // Mark the completed line
                for (int z = 1; z < GRID_HORIZONTAL_SIZE - 1; z++)
                {
                    grid[z][j] = FADING;
                }
            }
        }
    }
}

static int DeleteCompleteLines()
{
    int deletedLines = 0;

    // Erase the completed line
    for (int j = GRID_VERTICAL_SIZE - 2; j >= 0; j--)
    {
        while (grid[1][j] == FADING)
        {
            for (int i = 1; i < GRID_HORIZONTAL_SIZE - 1; i++)
            {
                grid[i][j] = EMPTY;
            }
            for (int j2 = j-1; j2 >= 0; j2--)
            {
                for (int i2 = 1; i2 < GRID_HORIZONTAL_SIZE - 1; i2++)
                {
                    if (grid[i2][j2] == FULL)
                    {
                        grid[i2][j2+1] = FULL;
                        grid[i2][j2] = EMPTY;
                    }
                    else if (grid[i2][j2] == FADING)
                    {
                        grid[i2][j2+1] = FADING;
                        grid[i2][j2] = EMPTY;
                    }
                }
            }
             deletedLines++;
        }
    }
    
    if (deletedLines > 0) { 
        gravitySpeed -= deletedLines;
        
    if (gravitySpeed < 4) gravitySpeed = 4; 
}
    return deletedLines;
}