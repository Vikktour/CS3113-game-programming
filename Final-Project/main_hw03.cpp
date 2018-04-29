/*
Victor Zheng vz365
hw03 Space Invaders
Controls:
up = shoot
left,right

questions and answers:
1) Class entity has no member x i.e. bullets[bulletIndex].x = -1.2;
ans: bullets[bulletIndex].position.x = -1.2;
2)signed/unsigned mismatch in for loop for i<entities.size()
ans: .size() returns a size_t type, so make i of that type
3) Does the screen resolution ratio have to be 16:9? Don't we want it to be more narrow?
4) Why position += velocity * elapsed; instead of postion += velocity? ans: need velocity for
5) Why do we need to change position along with translating the model matrix? ans: the position is for where the player would shoot bullets, and the model matrix is where you see the plan's actual position
6)Lives go to 0 and gameover to mainmenu, lives not resetting? Ans:
8)why do the same enemies fire the bullets every start? Is there a seed that I have to change? Ans: Because the seed for the rng is not set. call srand(time(NULL)) to modify rng of rand

7)Initialize game function? I still have to reset the enemy positions after gameover but this seems like a lot of work, especially when moving the things already set in main() to the initialize function; I need to convert everything.

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
#include <ctime>//for srand time
#include <SDL_mixer.h>


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
float playerSpeed = 1.5f; float playerBulletSpeed = 5.0f; int numLives = 1; //global variable so it's easier to adjust

void DrawText(ShaderProgram *program, int fontTexture, std::string text, float size, float spacing);
int enemyCount = 15; int enemiesPerRow = 5; float enemySpeed = 0.3f; float enemyBulletSpeed = 0.3f; float yRowGapRatio = 1.5f; float deadEnemyOffsetY = 10.0f;//where the dead enemy is located
SDL_Window* displayWindow;
const Uint8 *keys = SDL_GetKeyboardState(NULL);
ShaderProgram texturedProgram;
GLuint fontTexture;

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

class SheetSprite {//representing a nonuniform sprite sheet's sprite, where you have to get the uv coordinates of the png
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

//SheetSprite(unsigned int textureID, float u, float v, float width, float height, float size) : textureID(textureID), u(u), v(v), width(width), height(height), size(size) {}//custom constructor
//<SubTexture height="75" width="99" y="941" x="211" name="playerShip1_blue.png"/>
//Level.game.player.sprite = SheetSprite(spaceTexture, 211.0f / pixelWidth, 941.0f / pixelHeight, 99.0f / pixelWidth, 75.0f / pixelHeight, 0.5f);
void SheetSprite::Draw(ShaderProgram *program) const{
	glBindTexture(GL_TEXTURE_2D, textureID);
	GLfloat texCoords[] = {
		u, v + height,
		u + width, v,
		u, v,
		u + width, v,
		u, v + height,
		u + width, v + height
	};//comeback1

	float aspect = width / height;
	float vertices[] = {
		//note that the first vertex (bottom=left) defined by -0.5f*width,-0.5f*height can be redefined in terms of size*aspect,size to get the size scaling we want
		//note that halfWidth = size*aspect/2.0f, height = size/2.0f
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
	Vector3 size;//<width,height,N/A>
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

float* convertToUV(float pixelWidth, float pixelHeight, float height, float width, float y, float x) {//input the data for the spritesheet to get an array of
	float retArray[4];
	retArray[0] = x / pixelWidth;//u
	retArray[1] = y / pixelHeight;//v
	retArray[2] = width / pixelWidth;//entity width (uv units) i.e. from 0-1
	retArray[3] = height / pixelHeight;//entity height (uv units)
	return retArray;
}


/* Game State */
class GameState {
public:
	//contains: player, enemies, bullets, score

	Entity player;
	float playerYpos = -3.0f;


	//Object pooling (as opposed to dynamic object creation) for the bullets (if too too many bullets, destroy oldest one to make new)
	std::vector<Entity> bullets = std::vector<Entity>(MAX_BULLETS);//make vector of bullets with a cap for #bullets.
	float playerReloadTime = 1.0f; //this is the time before the next bullet can be fired, so that the bullets aren't sprayed on top of each other
	float playerNoShoot = playerReloadTime; int bulletIndex = 0; 
	
	

	//Enemy bullets
	std::vector<Entity> enemyBullets = std::vector<Entity>(MAX_BULLETS);
	float enemyReloadTime = 10.0f;
	float enemyNoShoot = enemyReloadTime; int enemyBulletIndex = 0;


	std::vector<Entity> enemies = std::vector<Entity>(enemyCount); //note that a vector would work better than an array in this case b/c it lets you use the variable enemyCount to initialize the size

	int score = 0; int lives = numLives;

};
//I didn't make a function for player shoot, but it's implementation is in Update(gamestate)
void enemyShoot(GameState &game, float elapsed) {
	srand(time(NULL));
	int shooter = rand() % enemyCount;
	if (game.enemyNoShoot >= game.enemyReloadTime) {
		game.enemyBullets[game.enemyBulletIndex].position.x = game.enemies[shooter].position.x;
		game.enemyBullets[game.enemyBulletIndex].position.y = game.enemies[shooter].position.y;
		game.enemyBullets[game.enemyBulletIndex].velocity.y = -enemyBulletSpeed; //not sure why enemy bullet speed needs to be really small????
		game.enemyNoShoot = 0;
		game.enemyBulletIndex++;
		if (game.enemyBulletIndex == MAX_BULLETS) {
			game.enemyBulletIndex = 0;
		}
	}
	//for (int i = 0; i < MAX_BULLETS; i++) {//debug
	//	game.enemyBullets[i].position.x = 0.0f+i/2;
	//	game.enemyBullets[i].position.y = 0.0f;
	//	game.enemyBullets[i].velocity.y = -0.5f;
	//}
	for (int i = 0; i < MAX_BULLETS; i++) {
		game.enemyBullets[i].position.y += game.enemyBullets[i].velocity.y * elapsed;
	}
	game.enemyNoShoot += elapsed;

}

void RenderGame(const GameState &game) {
	// render all the entities in the game
	glClear(GL_COLOR_BUFFER_BIT);

	//create matrix (can do this in GameState class, but that would require removal of const in this current function
	Matrix playerModelMatrix;
	std::vector<Matrix> bulletsModelMatrix = std::vector<Matrix>(MAX_BULLETS);
	std::vector<Matrix> enemyModelMatrix = std::vector<Matrix>(enemyCount);

	playerModelMatrix.Identity();//set player's position to identity
	//game.player.position = Vector3(-2.0f, 0.0f, 0.0f);//debug
	playerModelMatrix.Translate(game.player.position.x, game.playerYpos, 0.0f);//note we have to change the player's position whenever we change the player model matrix
	//playerModelMatrix.Translate(-1.0f, 0.0f, 0.0f); //debug
	texturedProgram.SetModelMatrix(playerModelMatrix);
	game.player.sprite.Draw(&texturedProgram);


	//bulletsModelMatrix[0].Identity(); //--debug
	//bulletsModelMatrix[0].Translate(0.0f, game.bullets[0].position.y, 0.0f);
	//texturedProgram.SetModelMatrix(bulletsModelMatrix[0]);
	//game.bullets[0].sprite.Draw(&texturedProgram);
	//game.bullets[0].velocity.y = 2.0f;

	for (int i = 0; i < MAX_BULLETS; i++) {//draw player bullets
		bulletsModelMatrix[i].Identity();
		bulletsModelMatrix[i].Translate(game.bullets[i].position.x, game.bullets[i].position.y, 0.0f);
		//bulletsModelMatrix[i].Translate(0.0f,i, 0.0f);
		texturedProgram.SetModelMatrix(bulletsModelMatrix[i]);
		game.bullets[i].sprite.Draw(&texturedProgram);
	}


	for (int i = 0; i < MAX_BULLETS; i++) {//draw enemy bullets
		bulletsModelMatrix[i].Identity();
		bulletsModelMatrix[i].Translate(game.enemyBullets[i].position.x, game.enemyBullets[i].position.y, 0.0f);
		texturedProgram.SetModelMatrix(bulletsModelMatrix[i]);
		game.enemyBullets[i].sprite.Draw(&texturedProgram);
	}


	for (int i = 0; i < enemyCount; i++) {//put 5 enemies in each row
		enemyModelMatrix[i].Identity();
		enemyModelMatrix[i].Translate(game.enemies[i].position.x, game.enemies[i].position.y, 0.0f);
		texturedProgram.SetModelMatrix(enemyModelMatrix[i]);
		game.enemies[i].sprite.Draw(&texturedProgram);
	}


	// render score and other UI elements

	//draw score
	float textsize = 0.3f; float textSpacing = 0.05f;

	Matrix textMatrix;
	textMatrix.Identity();
	textMatrix.Translate(leftScreen+0.1f, topScreen-0.3f, 0.0f);
	texturedProgram.SetModelMatrix(textMatrix);
	std::string scoreString = std::to_string(game.score);//convert int to string
	std::string displayScoreString = "Score:" + scoreString;
	DrawText(&texturedProgram, fontTexture, displayScoreString, textsize, textSpacing);//returnhere

	//draw lives
	textMatrix.Identity();
	textMatrix.Translate(leftScreen+0.1f, topScreen-0.1f, 0.0f);
	texturedProgram.SetModelMatrix(textMatrix);

	std::string lifeString = std::to_string(game.lives);//convert int to string
	std::string displayLifeString = "Lives:" + lifeString;
	DrawText(&texturedProgram, fontTexture, displayLifeString, textsize, textSpacing);//returnhere
	
}
void UpdateGame(GameState &game, float elapsed) {
	// move all the entities based on time elapsed and their velocity
	//update things that aren't effected by input (i.e. monsters moving left and right)
	//bullets keep flying up
	for (int i = 0; i < MAX_BULLETS; i++) {
		//bullets[i].position.y += bullets[i].velocity.y * elapsed; //this is the same as entity's update function
		game.bullets[i].Update(elapsed);
	}

	//make enemy bounce upon contact with wall
	int tempCount = enemyCount; //
	int lastInRow; int cycle = 0; bool enemyCollideWall = 0;
	while (tempCount > 0) {
		//get right most enemy in row
		if (tempCount >= enemiesPerRow) {
			lastInRow = enemiesPerRow;
		}
		else if (tempCount < enemiesPerRow) {
			lastInRow = tempCount;
		}

		//check if entities go out of bounds
		//float yRowGap = 1.0f / yRowGapRatio;
		//float yRowGap = 0.5f;
		float enemyAspect = game.enemies[0].sprite.width / game.enemies[0].sprite.height;
		float halfEnemyWidth = enemyAspect* game.enemies[0].sprite.size / 2.0f;//half of horizontal sprite length
		float halfEnemyHeight = game.enemies[0].sprite.size / 2.0f;//half of vertical sprite length
		for (int i = 0; i < lastInRow; i++) {//check each row for collision with left/right side of screen
			if (game.enemies[i + (cycle*enemiesPerRow)].position.x - halfEnemyWidth <= leftScreen) {//if leftmost visible enemy hits left wall, then the row moves right	
				for (int j = i; j < lastInRow; j++) {
					game.enemies[j + (cycle*enemiesPerRow)].velocity.x = enemySpeed;
					//game.enemies[j + (cycle*enemiesPerRow)].position.y -= yRowGap;
				}
				enemyCollideWall = 1;
			}
			else if (game.enemies[i + (cycle*enemiesPerRow)].position.x + halfEnemyWidth >= rightScreen) {//if enemy hits right wall, move left
				for (int j = i; j >= 0; j--) {
					game.enemies[j + (cycle*enemiesPerRow)].velocity.x = -enemySpeed;
					//game.enemies[j + (cycle*enemiesPerRow)].position.y -= yRowGap;
				}
				enemyCollideWall = 1;
			}
		}

		//This is to shift the enemies down everytime they hit they wall
		//float nRows = enemyCount / enemiesPerRow + 1;
		float yRowGap = 1.0f / yRowGapRatio / enemyCount;
		//float yRowGap = 0.1f; //debug
		if (enemyCollideWall == true) {
			for (int i = 0; i < enemyCount; i++) {
				game.enemies[i].position.y -= yRowGap;
			}
			//game.enemies[1].position.y -= 0.1f;//debug, the enemy shifts down too much. Ans: might be b/c each row of enemy collide w/ wall at different instance, causing it to jump down 5 times really quick downwards
		}
		
		//float playerLeft = game.player.position.x - game.player.sprite.width /2;
		//if (game.player.position.x < leftScreen) {//check for player collision with wall
		//	game.player.position.x = leftScreen;
		//}
		//if (game.player.position.x > rightScreen) {
		//	game.player.position.x = rightScreen;
		//}

		float playerAspect = game.player.sprite.width / game.player.sprite.height;//NOTE that aspect*size is the widthvertex coordinates for the width
		float playerLeft = game.player.position.x - playerAspect * game.player.sprite.size / 2.0f;//divide by 2.0f b/c the position is at the center and the entity's left is half a width away
		float playerRight = game.player.position.x + playerAspect * game.player.sprite.size / 2.0f;
		float playerTop = game.player.position.y + game.player.sprite.size / 2.0f;
		float playerBottom = game.player.position.y - game.player.sprite.size / 2.0f;
		if (playerLeft < leftScreen) {//check for player collision with wall
			game.player.position.x = leftScreen + playerAspect * game.player.sprite.size / 2.0f;
		}
		if (playerRight > rightScreen) {
			game.player.position.x = rightScreen - playerAspect * game.player.sprite.size / 2.0f;
		}


		//check if entities go out of bounds

		tempCount -= 5; cycle++;

		for (int i = 0; i < enemyCount; i++) {
			game.enemies[i].position.x += game.enemies[i].velocity.x * elapsed;
		}

		//update enemy bullets
		enemyShoot(game, elapsed);

		float enemyBulletAspect = game.enemyBullets[0].sprite.width / game.enemyBullets[0].sprite.height;
		float halfEnemyBulletW = enemyBulletAspect * game.enemyBullets[0].sprite.size / 2.0f;
		float halfEnemyBulletH = game.enemyBullets[0].sprite.size / 2.0f;
		//enemy bullet hits player, move the bullets
		for (int i = 0; i < MAX_BULLETS; i++) {
			if (game.enemyBullets[i].position.y + halfEnemyBulletH >= playerBottom && game.enemyBullets[i].position.y - halfEnemyBulletH <= playerTop ) {//bulletback >= playerbottom and bullettip <= playertop
				if (game.enemyBullets[i].position.x + halfEnemyBulletW >= playerLeft && game.enemyBullets[i].position.x - halfEnemyBulletW <= playerRight) {//bulletright >= playerleft, bulletleft <= playerright
					game.lives -= 1;
					game.enemyBullets[i].position.x = -2000.0f;
					game.enemyBullets[i].velocity.y = 0.0f;
				}
			}
		}

		float playerBulletAspect = game.bullets[0].sprite.width / game.bullets[0].sprite.height;
		float halfPlayerBulletW = playerBulletAspect * game.bullets[0].sprite.size / 2.0f;
		float halfPlayerBulletH = game.bullets[0].sprite.size / 2.0f;
		//player bullet hits enemy, move the enemy off the screen, update score
		for (int i = 0; i < MAX_BULLETS; i++) {
			for (int j = 0; j < enemyCount; j++) {
				if (game.bullets[i].position.y + halfPlayerBulletH >= game.enemies[j].position.y - halfEnemyHeight && game.bullets[i].position.y - halfPlayerBulletH <= game.enemies[j].position.y + halfEnemyHeight) {//bulletTip >= enemyBottom, bulletBottom <= enemyTop
					if (game.bullets[i].position.x + halfPlayerBulletW >= game.enemies[j].position.x - halfEnemyWidth && game.bullets[i].position.x - halfPlayerBulletW <= game.enemies[j].position.x + halfEnemyWidth) {//bulletRight>=enemyLeft, bulletLeft<=enemyRight
						game.enemies[j].position.y = - deadEnemyOffsetY;
						game.enemies[j].position.x = 0.0f;
						game.enemies[j].velocity.x = 0.0f;
						game.enemies[j].velocity.y = 0.0f;
						game.bullets[i].position.x = -2000.0f;
						game.bullets[i].velocity.y = 0;
						game.playerNoShoot = game.playerReloadTime;
						game.score++;
					}
				}
			}
		}
		//if any enemy crosses the bottom screen then game over
	}
}
void ProcessGameInput(GameState &game,float elapsed) {
	 
	//is there an input? what to do with it? Update things that use input
	if (keys[SDL_SCANCODE_LEFT]) {
		//player moves left
		game.player.velocity.x = -playerSpeed;
		game.player.Update(elapsed);//position.x += game.player.velocity.x * elapsed;
	}
	if (keys[SDL_SCANCODE_RIGHT]) {
		//player moves right
		game.player.velocity.x = playerSpeed;
		game.player.Update(elapsed);//position.x += game.player.velocity.x * elapsed;
	}

	//game.bullets[1].position.x = game.player.position.x;//debug, need to somehow prevent player from spray shooting
	//game.bullets[1].position.y = game.player.position.y + 1.0f;

	if (keys[SDL_SCANCODE_UP]) {
		//player shoots bullets upward
		if (game.playerNoShoot >= game.playerReloadTime) {
			game.bullets[game.bulletIndex].position.x = game.player.position.x;
			game.bullets[game.bulletIndex].position.y = game.player.position.y;
			game.bullets[game.bulletIndex].velocity.y = playerBulletSpeed;
			game.playerNoShoot = 0.0f;
			game.bulletIndex++;
			if (game.bulletIndex == MAX_BULLETS) {
				game.bulletIndex = 0; //if you've used up max bullets, go back to using the first bullet
			}
		}
	}

	if (keys[SDL_SCANCODE_P]) {
		//pause the game
	}
	game.playerNoShoot += elapsed;
}
/* Game State*/


/* Game Mode */
enum GameMode { STATE_MAIN_MENU, STATE_GAME_LEVEL, STATE_GAME_OVER }; //enumeration declaration (
GameMode mode = STATE_MAIN_MENU; //initiallize game mode to main menu

class MainMenu {
public:
	//std::vector<Entity> entities;
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
			//InitializeGame();//comeback
		}
	}
};

void InitializeGame(GameState &game) {

}

class GameLevel {
public:
	GameLevel(GameState* game):game(game) {}
	GameState* game;
	void Render() {
		RenderGame(*game);//comebackreturn
	}
	void Update(float elapsed){
		UpdateGame(*game, elapsed);
		if (game->lives == 0) {
			mode = STATE_GAME_OVER;
		}
	}
	void ProcessInput(float elapsed) {
	}
};

class GameOver {
public:
	GameOver(GameState* game):game(game) {}
	GameState* game;
	void Render() {
		float size = 0.5f; float spacing = 0.0f;
		glClear(GL_COLOR_BUFFER_BIT);
		Matrix textMatrix;
		textMatrix.Identity();
		textMatrix.Translate(-2.0f, 0.0f, 0.0f);
		texturedProgram.SetModelMatrix(textMatrix);
		DrawText(&texturedProgram, fontTexture, "GAMEOVER", size, spacing);
		size = 0.15f;
		textMatrix.Identity();
		textMatrix.Translate(-2.0f, -1.5f, 0.0f);
		texturedProgram.SetModelMatrix(textMatrix);
		DrawText(&texturedProgram, fontTexture, "Press Esc to return to Menu", size, spacing);
	}
	void Update(float elapsed) {

	}
	void ProcessInput() {
		if (keys[SDL_SCANCODE_ESCAPE]) {
			mode = STATE_MAIN_MENU;
			game->lives = numLives;	
		}
	}
};

//Render here has the same name as above???? Can I change the name? What are update/process input doing? Is there an efficient way to implement the functions instead of copy paste into diff classes?
MainMenu mainMenu;
GameState game;
GameLevel Level(&game);
GameOver Over(&game);

void checkEnemyWin() {//if enemy wins in gamelevel then switch to gameover
	float halfEnemyHeight = game.enemies[0].sprite.size / 2.0f;
	float enemyAspect = game.enemies[0].sprite.width / game.enemies[0].sprite.height;
	float halfEnemyWidth = enemyAspect * game.enemies[0].sprite.size / 2.0f;
	for (int i = 0; i < enemyCount; i++) {
		//if (game.enemies[i].position.y + halfEnemyHeight < bottomScreen) {
		//	mode = STATE_GAME_OVER;
		//}
		if (game.enemies[i].position.y < bottomScreen && game.enemies[i].position.y > bottomScreen - deadEnemyOffsetY/2.0f) {
			mode = STATE_GAME_OVER;
		}
	}
}

void Render() {//mode rendering (changes game state and draw the entities for that particular state)
	switch (mode) {
	case STATE_MAIN_MENU:
		mainMenu.Render();
		break;
	case STATE_GAME_LEVEL:
		Level.Render();
		break;
	case STATE_GAME_OVER:
		Over.Render();
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
		if (keys[SDL_SCANCODE_ESCAPE]) {
			//go to main menu
			//might want to pause game first then go to main menu
			mode = STATE_MAIN_MENU;
		}
		checkEnemyWin();
		break;
	case STATE_GAME_OVER:
		Over.Update(elapsed);
	}
	
}

void ProcessInput(float elapsed) {
	switch (mode) {
	case STATE_MAIN_MENU:
		break;
	case STATE_GAME_LEVEL:
		break;
	case STATE_GAME_OVER:
		Over.ProcessInput();
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
	projectionMatrix.SetOrthoProjection(leftScreen, rightScreen, bottomScreen, topScreen, -1.0f, 1.0f);//defining the bounds for the screen for the view matrix


	/* Draw text */
	fontTexture = LoadTexture(RESOURCE_FOLDER"1font.png");
	texturedProgram.Load(RESOURCE_FOLDER"vertex_textured.glsl", RESOURCE_FOLDER"fragment_textured.glsl");
	texturedProgram.SetProjectionMatrix(projectionMatrix);
	texturedProgram.SetViewMatrix(viewMatrix);
	//programUntextured.Load(RESOURCE_FOLDER"vertex.glsl", RESOURCE_FOLDER"fragment.glsl");
	
	
	
	
	//std::vector<float>* textVertices = vertexData.data();// returns a pointer to the beginning of the vector
	/* Draw text */


	float pixelWidth = 1024.0f; float pixelHeight = 1024.0f;
	/* Initialize player */
	GLuint spaceTexture = LoadTexture(RESOURCE_FOLDER"2SpaceInvaders.png");
	//<SubTexture height="75" width="99" y="941" x="211" name="playerShip1_blue.png"/>
	float* playerArray = convertToUV(pixelWidth, pixelHeight, 75.0f, 99.0f, 941.0f, 211.0f);//returns [u v width height]

	//(unsigned int textureID, float u, float v, float width, float height, float size)
	//Level.game.player.sprite = SheetSprite(spaceTexture, 211.0f / pixelWidth, 941.0f / pixelHeight, 99.0f / pixelWidth, 75.0f / pixelHeight, 0.5f);
	float playerWidth = Level.game->player.size.x = playerArray[2];//player width in uv size (0-1)
	float playerHeight = Level.game->player.size.y = playerArray[3];//player height in uv size (0-1)
	Level.game->player.sprite = SheetSprite(spaceTexture, playerArray[0], playerArray[1], playerWidth, playerHeight, 0.5f);//note that playerWidth,playerHeight are stored in sheetsprite class as width,height
	
																			
	Level.game->player.position.x = 0.0f;
	Level.game->player.position.y = Level.game->playerYpos;//default -3.0f
	/* Initialize player */


	//initialize bullets out of view
	for (int i = 0; i < MAX_BULLETS; i++) {
		Level.game->bullets[i].position.x = -2000.0f;
		Level.game->enemyBullets[i].position.x = -2000.0f;
	}
	/* Choose bullet texture */
	for (int i = 0; i < MAX_BULLETS; i++) {
		//Level1.bullets[i].sprite = SheetSprite(spaceTexture, 776.0f / pixelWidth, 895.0f / pixelHeight, 34.0f / pixelWidth, 33.0f / pixelHeight, 0.5f); //powerup blue
		//Level1.bullets[i].sprite = SheetSprite(spaceTexture, 811.0f / pixelWidth, 663.0f / pixelHeight, 16.0f / pixelWidth, 40.0f / pixelHeight, 0.5f); //fire09
		//Level1.bullets[i].sprite = SheetSprite(spaceTexture, 211.0f / pixelWidth, 941.0f / pixelHeight, 99.0f / pixelWidth, 75.0f / pixelHeight, 0.5f); //hero, test to see if it's shooting multiple units on top of each other
		Level.game->bullets[i].sprite = SheetSprite(spaceTexture, 829.0f / pixelWidth, 471.0f / pixelHeight, 14.0f / pixelWidth, 31.0f / pixelHeight, 0.5f); //fire15
		Level.game->enemyBullets[i].sprite = SheetSprite(spaceTexture, 811.0f / pixelWidth, 663.0f / pixelHeight, 16.0f / pixelWidth, 40.0f / pixelHeight, 0.5f); //fire09
		//Level.game.enemyBullets[i].sprite = SheetSprite(spaceTexture, 211.0f / pixelWidth, 941.0f / pixelHeight, 99.0f / pixelWidth, 75.0f / pixelHeight, 0.5f);
		//<SubTexture height = "31" width = "14" y = "471" x = "829" name = "fire15.png" / >
	}
	/* Choose bullet texture */



	//* Initialize enemies *//

	for (int i = 0; i < enemyCount; i++) {
		Level.game->enemies[i].sprite = SheetSprite(spaceTexture, 518.0f / pixelWidth, 325.0f / pixelHeight, 84.0f / pixelHeight, 82.0f / pixelWidth, 0.5f); //<SubTexture height="84" width="82" y="325" x="518" name="enemyBlack4.png"/>
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
	int tempCount = enemyCount; int numRows = 0; int lastInRow; int cycle = 0; /*float yRowGapRatio = 1.5f;*/ float yRowGap = 1 / yRowGapRatio;
	while (tempCount > 0) {
		if (tempCount >= enemiesPerRow) {
			lastInRow = enemiesPerRow;
		}
		else if (tempCount < enemiesPerRow) {
			lastInRow = tempCount;
		}
		numRows++;
		for (int i = 0; i < tempCount; i++) {//put 5 enemies in each row
			Level.game->enemies[i+(cycle*enemiesPerRow)].position.x = -2.0f + i;
			Level.game->enemies[i+(cycle*enemiesPerRow)].position.y = numRows/yRowGapRatio - 1.0f; //note that dividing by 2 instead of 2.0f here made it round into a int rather than a float... which caused some rows to stack
		}
		tempCount -= 5; cycle++;
	}
	//Level.game.enemies[1].position.y -= 0.1f;//debug, for some reason this shift is smaller than when doing it in Render()

	//initialize enemy speed
	for (int i = 0; i < enemyCount; i++) {
		Level.game->enemies[i].velocity.x = enemySpeed;
	}

	//* Initialize enemies *//


	SDL_Event event;
	bool done = false;
	while (!done) {
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) {
				done = true;
			}
		}
		glClear(GL_COLOR_BUFFER_BIT);

		float ticks = (float)SDL_GetTicks() / 1000.0f;//returns the number of millisecs/1000 elapsed since you compiled code
		float elapsed = ticks - lastFrameTicks;//returns the number of millisecs/1000 elapsed since the last loop of the event
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


/* Personal Debug and fixes

1) 1st bullet keeps getting replaced even though 2nd isn't used? The initialized variable was inside the function causing it to set to 0 again. Moved it to become the GameState class's member
1.1) Another thing that helped fix it is that I needed to do: if (game.bulletIndex == MAX_BULLETS) instead of if (game.bulletIndex = MAX_BULLETS)





*/