/*
Victor Zheng vz365
hw03 Space Invaders
Controls:

questions and answers:
1) Class entity has no member x i.e. bullets[bulletIndex].x = -1.2;
ans: bullets[bulletIndex].position.x = -1.2;
2)signed/unsigned mismatch in for loop for i<entities.size()
ans: .size() returns a size_t type, so make i of that type
3) Does the screen resolution ratio have to be 16:9? Don't we want it to be more narrow?
4) Why position += velocity * elapsed; instead of postion += velocity? ans: need velocity for 
5) Why do we need to change position along with translating the model matrix? ans: the position is for where the player would shoot bullets, and the model matrix is where you see the plan's actual position
*/


#define STB_IMAGE_IMPLEMENTATION //to allow assert(false)
#ifdef _WINDOWS
#include <GL/glew.h>
#endif
#include <SDL.h>
#include <SDL_opengl.h>
#include <SDL_image.h>
#include "ShaderProgram.h"
#include "Matrix.h"
#include "stb_image.h"
#include <iostream>
#include <vector>
#include <string>


#ifdef _WINDOWS
#define RESOURCE_FOLDER ""
#else
#define RESOURCE_FOLDER "NYUCodebase.app/Contents/Resources/"
#endif

#define MAX_BULLETS 5


/* Some global variables */
//screen
float screenWidth = 480;
float screenHeight = 720;
float topScreen = 4.0f;
float bottomScreen = -4.0f;
float aspectRatio = screenWidth / screenHeight;
float leftScreen = -aspectRatio * topScreen;
float rightScreen = aspectRatio * topScreen;

float lastFrameTicks = 0.0f;
float playerSpeed = 0.01f;
void DrawText(ShaderProgram *program, int fontTexture, std::string text, float size, float spacing);
int enemyCount = 21; int enemiesPerRow = 5; float enemySpeed = 0.3f;
SDL_Window* displayWindow;
const Uint8 *keys = SDL_GetKeyboardState(NULL);
ShaderProgram texturedProgram;

GLuint LoadTexture(const char *filePath) {
	int w, h, comp;
	unsigned char* image = stbi_load(filePath, &w, &h, &comp, STBI_rgb_alpha); //load pixel data from image file

	if (image == NULL) {
		std::cout << "Unable to load image. Make sure the path is correct\n";
		assert(false);
	}

	GLuint retTexture; //creating a new OpenGL textureID
	glGenTextures(1, &retTexture); //numtextures,GLuint *texture
	glBindTexture(GL_TEXTURE_2D, retTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, image); //sets the texture data of specified texture target (make sure to match the image format GL_RGBA or GL_RGB for their respective image)

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	stbi_image_free(image);//free the image???????
	return retTexture;
}



class SheetSprite { //representing a sprite sheet
public:
	SheetSprite() {}; //default constructor
	SheetSprite(unsigned int textureID, float u, float v, float width, float height, float
		size) : textureID(textureID), u(u), v(v), width(width), height(height), size(size) {}//custom constructor

	void Draw(ShaderProgram *program) const;

	float size;	
	unsigned int textureID;
	float u;
	float v;
	float width;
	float height;
};

void SheetSprite::Draw(ShaderProgram *program) const{
	glBindTexture(GL_TEXTURE_2D, textureID);
	GLfloat texCoords[] = {
		u, v + height,
		u + width, v,
		u, v,
		u + width, v,
		u, v + height,
		u + width, v + height
	};

	float aspect = width / height;
	float vertices[] = {
		-0.5f * size * aspect, -0.5f * size,
		0.5f * size * aspect, 0.5f * size,
		-0.5f * size * aspect, 0.5f * size,
		0.5f * size * aspect, 0.5f * size,
		-0.5f * size * aspect, -0.5f * size ,
		0.5f * size * aspect, -0.5f * size };
	// draw our arrays
	glVertexAttribPointer(program->positionAttribute, 2, GL_FLOAT, false, 0, vertices);
	glEnableVertexAttribArray(program->positionAttribute);
	glVertexAttribPointer(program->texCoordAttribute, 2, GL_FLOAT, false, 0, texCoords);
	glEnableVertexAttribArray(program->texCoordAttribute);
	glDrawArrays(GL_TRIANGLES, 0, 6);
	glDisableVertexAttribArray(program->positionAttribute);
	glDisableVertexAttribArray(program->texCoordAttribute);
}

class Vector3 { //keep track of object coordinates
public:
	Vector3(float x = 0.0f, float y = 0.0f, float z = 0.0f) : x(x), y(y), z(z) {}
	float x;
	float y;
	float z;
};

class Entity { //object
public:
	Entity() {};
	Entity(Vector3 position, Vector3 velocity, Vector3 size, float rotation, SheetSprite sprite) : position(position), velocity(velocity), size(size), rotation(rotation), sprite(sprite) {}
	Vector3 position;
	Vector3 velocity;
	Vector3 size;
	float rotation;
	SheetSprite sprite;
	//void Draw(ShaderProgram *program) {
	//	sprite.Draw(program);
	//}
	void Update(float elapsed);
};

void Entity::Update(float elapsed) {
	position.x += velocity.x * elapsed;
	position.y += velocity.y * elapsed;
}


GLuint fontTexture;


Matrix playerModelMatrix;
std::vector<Matrix> bulletsModelMatrix = std::vector<Matrix>(MAX_BULLETS);
std::vector<Matrix> enemyModelMatrix = std::vector<Matrix>(enemyCount);

/* Game State */
class GameState {
public:
	//contains: player, enemies, bullets, score

	Entity player;
	float playerYpos = -3.0f;

	//Object pooling (as opposed to dynamic object creation) for the bullets (if too too many bullets, destroy last one to make new)
	std::vector<Entity> bullets = std::vector<Entity>(MAX_BULLETS);//make vector of bullets with a cap for #bullets.
	float reloadTime = 0.5f; //this is the time before the next bullet can be fired, so that the bullets aren't sprayed on top of each other



	std::vector<Entity> enemies = std::vector<Entity>(enemyCount); //note that a vector would work better than an array in this case b/c it lets you use the variable enemyCount to initialize the size

	//initialize positions

	int score;

};
GameState state;
void RenderGame(GameState &game) {//--comeback and add const
	// render all the entities in the game
	glClear(GL_COLOR_BUFFER_BIT);

	playerModelMatrix.Identity();//set player's position to identity
	//game.player.position = Vector3(-2.0f, 0.0f, 0.0f);//debug
	playerModelMatrix.Translate(game.player.position.x, game.playerYpos, 0.0f);//note we have to change the player's position whenever we change the player model matrix
	//playerModelMatrix.Translate(-1.0f, 0.0f, 0.0f); //debug
	texturedProgram.SetModelMatrix(playerModelMatrix);
	game.player.sprite.Draw(&texturedProgram);


	//bulletsModelMatrix[0].Identity(); //--debug
	//bulletsModelMatrix[0].Translate(0.0f, bullets[0].position.y, 0.0f);
	//texturedProgram.SetModelMatrix(bulletsModelMatrix[0]);
	//bullets[0].sprite.Draw(&texturedProgram);
	//bullets[0].velocity.y = 2.0f;

	for (int i = 0; i < MAX_BULLETS; i++) {
		bulletsModelMatrix[i].Identity();
		bulletsModelMatrix[i].Translate(game.bullets[i].position.x, game.bullets[i].position.y, 0.0f);
		//bulletsModelMatrix[i].Translate(0.0f,i, 0.0f);
		texturedProgram.SetModelMatrix(bulletsModelMatrix[i]);
		game.bullets[i].sprite.Draw(&texturedProgram);
	}


	for (int i = 0; i < enemyCount; i++) {//put 5 enemies in each row
		enemyModelMatrix[i].Identity();
		enemyModelMatrix[i].Translate(game.enemies[i].position.x, game.enemies[i].position.y, 0.0f);
		texturedProgram.SetModelMatrix(enemyModelMatrix[i]);
		game.enemies[i].sprite.Draw(&texturedProgram);
	}

	// render score and other UI elements
}
void UpdateGame(GameState &game, float elapsed) {//need const for gamestate??????????
	// move all the entities based on time elapsed and their velocity
	//update things that aren't effected by input (i.e. monsters moving left and right)
	//bullets keep flying up
	for (int i = 0; i < MAX_BULLETS; i++) {
		//bullets[i].position.y += bullets[i].velocity.y * elapsed; //this is the same as entity's update function
		game.bullets[i].Update(elapsed);
	}

	//make enemy bounce upon contact with wall
	int tempCount = enemyCount; //
	int lastInRow; int cycle = 0;
	while (tempCount > 0) {
		//get right most enemy in row
		if (tempCount >= enemiesPerRow) {
			lastInRow = enemiesPerRow;
		}
		else if (tempCount < enemiesPerRow) {
			lastInRow = tempCount;
		}

		//check for collisions with left and right screen
		for (int i = 0; i < lastInRow; i++) {//check each row for collision with left/right side of screen
			if (game.enemies[i + (cycle*enemiesPerRow)].position.x <= leftScreen) {//if leftmost visible enemy hits left wall, then the row moves right	
				for (int j = i; j < lastInRow; j++) {
					game.enemies[j + (cycle*enemiesPerRow)].velocity.x = enemySpeed;
				}
			}
			else if (game.enemies[i + (cycle*enemiesPerRow)].position.x >= rightScreen) {//if enemy hits right wall, move left
				for (int j = i; j >= 0; j--) {
					game.enemies[j + (cycle*enemiesPerRow)].velocity.x = -enemySpeed;
				}

			}
		}

		tempCount -= 5; cycle++;

		for (int i = 0; i < enemyCount; i++) {
			game.enemies[i].position.x += game.enemies[i].velocity.x * elapsed;
		}
	}

	game.reloadTime += elapsed;
}
void ProcessGameInput(GameState &game,float elapsed) {
	int bulletIndex = 0;
	//is there an input? what to do with it? Update things that use input
	if (keys[SDL_SCANCODE_LEFT]) {
		//player moves left
		game.player.velocity.x = -1.5f;
		game.player.Update(elapsed);//position.x += game.player.velocity.x * elapsed;
	}
	if (keys[SDL_SCANCODE_RIGHT]) {
		//player moves right
		game.player.velocity.x = 1.5f;
		game.player.Update(elapsed);//position.x += game.player.velocity.x * elapsed;
	}
	if (keys[SDL_SCANCODE_UP]) {
		//player shoots bullets upward
		if (game.reloadTime >= 0.5f) {
			game.bullets[bulletIndex].position.x = game.player.position.x;
			game.bullets[bulletIndex].position.y = game.player.position.y;
			game.bullets[bulletIndex].velocity.y = 5.0f;
			bulletIndex++;
			game.reloadTime = 0.0f;
		}
		if (bulletIndex > MAX_BULLETS - 1) {
			bulletIndex = 0; //if you've used up max bullets, go back to using the first bullet
		}
	}
	if (keys[SDL_SCANCODE_P]) {
		//pause the game
	}
}
/* Game State*/


/* Game Mode */
enum GameMode { STATE_MAIN_MENU, STATE_GAME_LEVEL, STATE_GAME_OVER }; //enumeration declaration (
GameMode mode = STATE_MAIN_MENU; //initiallize game mode to main menu

class MainMenu {
public:
	std::vector<Entity> entities;
	void Render() { //draw text on screen
		float size = 0.5f; float spacing = 0.0f;
		glClear(GL_COLOR_BUFFER_BIT);
		/*for (size_t i = 0; i < entities.size(); i++) {
			entities[i].sprite.Draw(program);
		}*/
		Matrix textMatrix;
		textMatrix.Identity();
		textMatrix.Translate(-2.0f, 0.0f, 0.0f);
		texturedProgram.SetModelMatrix(textMatrix);
		DrawText(&texturedProgram, fontTexture, "Main Menu", size, spacing);
		textMatrix.Identity();
		textMatrix.Translate(-3.0f, -2.0f, 0.0f);
		texturedProgram.SetModelMatrix(textMatrix);
		size = 0.3f;
		DrawText(&texturedProgram, fontTexture, "Press Spacebar to play", size, spacing);
	}
	void Update(float elapsed) {// e.g. can make the text flash different colors with w.r.t. time

	}
	void ProcessInput() {//SDL_SCANCODE to switch states (e.g. menu to level)
		if (keys[SDL_SCANCODE_SPACE]) {
			mode = STATE_GAME_LEVEL; //switch from main menu to game level 1
			//std::cout << mode << std::endl;
		}
	}
};


class GameLevel {
public:
	GameLevel() {}
	GameState game;
	void Render() {
		RenderGame(game);
	}
	void Update(float elapsed){
		UpdateGame(game, elapsed);
	}
	void ProcessInput(float elapsed) {
		ProcessGameInput(game, elapsed);
	}
};

//Render here has the same name as above???? Can I change the name? What are update/process input doing? Is there an efficient way to implement the functions instead of copy paste into diff classes?
MainMenu mainMenu;
GameLevel Level;

void Render() {//mode rendering (changes game state and draw the entities for that particular state)
	switch (mode) {
	case STATE_MAIN_MENU:
		mainMenu.Render();
		break;
	case STATE_GAME_LEVEL:
		Level.Render();
		if (keys[SDL_SCANCODE_ESCAPE]) {
			//go to main menu
			//might want to pause game first then go to main menu
			mode = STATE_MAIN_MENU;
		}
		break;
	}
}
void Update(float elapsed) {
	switch (mode) {
	case STATE_MAIN_MENU:
		mainMenu.Update(elapsed);
		
		break;
	case STATE_GAME_LEVEL:
		Level.Update(elapsed);
		break;
	}
}

void ProcessInput(float elapsed) {
	switch (mode) {
	case STATE_MAIN_MENU:
		mainMenu.ProcessInput();
		break;
	case STATE_GAME_LEVEL:
		Level.ProcessInput(elapsed);
		break;
	}
}
/* Game Mode */


void DrawText(ShaderProgram *program, int fontTexture, std::string text, float size, float spacing) { //for uniform (text) sprites
	float texture_size = 1.0 / 16.0f; //insert 16x16 grid
	std::vector<float> vertexData;
	std::vector<float> texCoordData;
	for (int i = 0; i < text.length(); i++) {
		int spriteIndex = (int)text[i];
		float texture_x = (float)(spriteIndex % 16) / 16.0f;
		float texture_y = (float)(spriteIndex / 16) / 16.0f;
		vertexData.insert(vertexData.end(), {
			((size + spacing) * i) + (-0.5f * size), 0.5f * size,
			((size + spacing) * i) + (-0.5f * size), -0.5f * size,
			((size + spacing) * i) + (0.5f * size), 0.5f * size,
			((size + spacing) * i) + (0.5f * size), -0.5f * size,
			((size + spacing) * i) + (0.5f * size), 0.5f * size,
			((size + spacing) * i) + (-0.5f * size), -0.5f * size,
			});
		texCoordData.insert(texCoordData.end(), {
			texture_x, texture_y,
			texture_x, texture_y + texture_size,
			texture_x + texture_size, texture_y,
			texture_x + texture_size, texture_y + texture_size,
			texture_x + texture_size, texture_y,
			texture_x, texture_y + texture_size,
			});
	}
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); //gets rid of the clear (black) parts of the png
													   //program.Load(RESOURCE_FOLDER"vertex_textured.glsl", RESOURCE_FOLDER"fragment_textured.glsl");
	glBindTexture(GL_TEXTURE_2D, fontTexture); //use fontTexture to draw
	glVertexAttribPointer(program->positionAttribute, 2, GL_FLOAT, false, 0, vertexData.data());
	glEnableVertexAttribArray(program->positionAttribute);

	glVertexAttribPointer(program->texCoordAttribute, 2, GL_FLOAT, false, 0, texCoordData.data());
	glEnableVertexAttribArray(program->texCoordAttribute);

	glUseProgram(program->programID);
	glDrawArrays(GL_TRIANGLES, 0, vertexData.size()/2);//vertexData.size() gives the number of coordinates, so divide by 2 to get the number of vertices
	glDisableVertexAttribArray(program->positionAttribute);
	glDisableVertexAttribArray(program->texCoordAttribute);
}
	
	
	
	

int main(int argc, char *argv[])
{

	SDL_Init(SDL_INIT_VIDEO);
	displayWindow = SDL_CreateWindow("My Game", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, screenWidth, screenHeight, SDL_WINDOW_OPENGL);
	SDL_GLContext context = SDL_GL_CreateContext(displayWindow);
	SDL_GL_MakeCurrent(displayWindow, context);
#ifdef _WINDOWS
	glewInit();
#endif




	glViewport(0, 0, screenWidth, screenHeight);

	Matrix projectionMatrix;
	Matrix viewMatrix; //where in the world you're looking at
	projectionMatrix.SetOrthoProjection(leftScreen, rightScreen, bottomScreen, topScreen, -1.0f, 1.0f); //defining the bounds for the screen for the view matrix


	/* Draw text */
	texturedProgram.Load(RESOURCE_FOLDER"vertex_textured.glsl", RESOURCE_FOLDER"fragment_textured.glsl");
	texturedProgram.SetProjectionMatrix(projectionMatrix);
	texturedProgram.SetViewMatrix(viewMatrix);
	//programUntextured.Load(RESOURCE_FOLDER"vertex.glsl", RESOURCE_FOLDER"fragment.glsl");
	
	
	fontTexture = LoadTexture(RESOURCE_FOLDER"1font.png");
	GLuint spaceTexture = LoadTexture(RESOURCE_FOLDER"2SpaceInvaders.png");
	
	//std::vector<float>* textVertices = vertexData.data();// returns a pointer to the beginning of the vector
	/* Draw text */

	/* Initialize player */
	Entity hero;
	hero.velocity.x = 0.0f;
	Level.game.player = hero;
	float pixelWidth = 1024.0f; float pixelHeight = 1024.0f;
	//(unsigned int textureID, float u, float v, float width, float height, float size)
	Level.game.player.sprite = SheetSprite(spaceTexture, 211.0f / pixelWidth, 941.0f / pixelHeight, 99.0f / pixelWidth, 75.0f / pixelHeight, 0.5f);
	//<SubTexture height="75" width="99" y="941" x="211" name="playerShip1_blue.png"/>
	Level.game.player.position.x = 0.0f;
	Level.game.player.position.y = Level.game.playerYpos; //default -3.0f
	/* Initialize player */


	//initialize bullets out of view
	for (int i = 0; i < MAX_BULLETS; i++) {
		Level.game.bullets[i].position.x = -2000.0f;
	}
	/* Choose bullets */
	for (int i = 0; i < MAX_BULLETS; i++) {
		//Level1.bullets[i].sprite = SheetSprite(spaceTexture, 776.0f / pixelWidth, 895.0f / pixelHeight, 34.0f / pixelWidth, 33.0f / pixelHeight, 0.5f); //powerup blue
		//Level1.bullets[i].sprite = SheetSprite(spaceTexture, 811.0f / pixelWidth, 663.0f / pixelHeight, 16.0f / pixelWidth, 40.0f / pixelHeight, 0.5f); //fire09
		//Level1.bullets[i].sprite = SheetSprite(spaceTexture, 211.0f / pixelWidth, 941.0f / pixelHeight, 99.0f / pixelWidth, 75.0f / pixelHeight, 0.5f); //hero, test to see if it's shooting multiple units on top of each other
		Level.game.bullets[i].sprite = SheetSprite(spaceTexture, 829.0f / pixelWidth, 471.0f / pixelHeight, 14.0f / pixelWidth, 31.0f / pixelHeight, 0.5f); //fire15
		//<SubTexture height = "31" width = "14" y = "471" x = "829" name = "fire15.png" / >
	}
	/* Choose bullets */

	
	//SheetSprite mySprite = SheetSprite(fontTexture, 425.0f / 1024.0f, 468.0f / 1024.0f, 93.0f / 1024.0f, 84.0f /1024.0f, 0.2f); //for non-uniform sprite

	/* Initialize enemies */

	for (int i = 0; i < enemyCount; i++) {
		Level.game.enemies[i].sprite = SheetSprite(spaceTexture, 518.0f / pixelWidth, 325.0f / pixelHeight, 84.0f / pixelHeight, 82.0f / pixelWidth, 0.5f); //<SubTexture height="84" width="82" y="325" x="518" name="enemyBlack4.png"/>
	}


	////used for debugging to tell which enemy is which
	//Level1.enemies[0].sprite = SheetSprite(spaceTexture, 811.0f / pixelWidth, 663.0f / pixelHeight, 16.0f / pixelWidth, 40.0f / pixelHeight, 1.0f);//1st firebullet
	//Level1.enemies[1].sprite = SheetSprite(spaceTexture, 518.0f / pixelWidth, 325.0f / pixelHeight, 84.0f / pixelHeight, 82.0f / pixelWidth, 0.5f);//2nd enemy
	//Level1.enemies[2].sprite = SheetSprite(spaceTexture, 211.0f / pixelWidth, 941.0f / pixelHeight, 99.0f / pixelWidth, 75.0f / pixelHeight, 0.5f);//3rd hero
	//Level1.enemies[3].sprite = SheetSprite(spaceTexture, 776.0f / pixelWidth, 895.0f / pixelHeight, 34.0f / pixelWidth, 33.0f / pixelHeight, 0.5f);//4th powerup
	//Level1.enemies[4].sprite = SheetSprite(spaceTexture, 518.0f / pixelWidth, 325.0f / pixelHeight, 84.0f / pixelHeight, 82.0f / pixelWidth, 0.5f);//5th enemy
	//Level1.enemies[5].sprite = SheetSprite(spaceTexture, 829.0f / pixelWidth, 471.0f / pixelHeight, 14.0f / pixelWidth, 31.0f / pixelHeight, 0.5f);//6th bullet

	////used for debugging to tell which enemy is which
	//for (int i = 0; i < enemyCount; i++) {
	//	Level1.enemies[i].sprite = SheetSprite(spaceTexture, 518.0f / pixelWidth, 325.0f / pixelHeight, 84.0f / pixelHeight, 82.0f / pixelWidth, 0.25f + i / 25.0f); //<SubTexture height="84" width="82" y="325" x="518" name="enemyBlack4.png"/>
	//}

	//initialize the positions of the enemies
	int tempCount = enemyCount; int numRows = 0; int lastInRow; int cycle = 0;
	while (tempCount > 0) {
		if (tempCount >= enemiesPerRow) {
			lastInRow = enemiesPerRow;
		}
		else if (tempCount < enemiesPerRow) {
			lastInRow = tempCount;
		}
		numRows++;
		for (int i = 0; i < tempCount; i++) {//put 5 enemies in each row
			Level.game.enemies[i+(cycle*enemiesPerRow)].position.x = -2.0f + i;
			Level.game.enemies[i+(cycle*enemiesPerRow)].position.y = numRows/1.5f - 1.0f; //note that dividing by 2 instead of 2.0f here made it round into a int rather than a float... which caused some rows to stack
		}
		tempCount -= 5; cycle++;
	}

	//initialize enemy speed
	for (int i = 0; i < enemyCount; i++) {
		Level.game.enemies[i].velocity.x = enemySpeed;
	}

	/* Initialize enemies */


	SDL_Event event;
	bool done = false;
	while (!done) {
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) {
				done = true;
			}
		}
		glClear(GL_COLOR_BUFFER_BIT);

		float ticks = (float)SDL_GetTicks() / 1000.0f;
		float elapsed = ticks - lastFrameTicks;
		lastFrameTicks = ticks;

		/* put rendering stuff here */
		Update(elapsed);
		Render();
		ProcessInput(elapsed);

		SDL_GL_SwapWindow(displayWindow);
	}
	SDL_Quit();
	return 0;
}