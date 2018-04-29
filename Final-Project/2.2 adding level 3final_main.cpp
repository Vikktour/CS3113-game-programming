/*
Johnathan Apuzen
Victor Zheng vz365

//debug notes
1)level not rendered properly, world tiles are mapped to the wrong tiles in the picture. Fix: !=-1 instead of !=0 for empty tiles
2)After loading level2, the player does not collide with the tiles on the bottom of the level. Fix: Change update2's map1 to map2

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
#include "FlareMap.h"
#include <iostream>
#include <vector>
#include <string>
#include <SDL_mixer.h>

#ifdef _WINDOWS
#define RESOURCE_FOLDER ""
#else
#define RESOURCE_FOLDER "NYUCodebase.app/Contents/Resources/"
#endif

//define timesteps to update based on FPS, e.g. 60fps means update every 1/60 second ~ 0.166666f
#define FIXED_TIMESTEP 0.0166666f
#define MAX_TIMESTEPS 6
#define LEVEL_HEIGHT1 20
#define LEVEL_WIDTH1 150
//Level2: 50hx150w
//Level3: 20hx150w
//note that this level will be 240*400 pixels

/* Adjustable global variables */
//screen
float screenWidth = 720;
float screenHeight = 480;
float topScreen = 7.0f;
float bottomScreen = -7.0f;
float aspectRatio = screenWidth / screenHeight;
float leftScreen = -aspectRatio * topScreen;
float rightScreen = aspectRatio * topScreen;

//time and FPS
float accumulator = 0.0f; float lastFrameTicks = 0.0f;

//game
float velocityMax = 3.0f;//excluding going up
float jumpSpeed = 10.0f;
float tileSize = 1.0f;//tile size is how big you want each block to be in the world (e.g. our sprite is 16x16pixels in png, but we can make it 1.0x1.0 in world)

FlareMap map1; FlareMap map2; FlareMap map3;
GLuint gameTexture1; GLuint gameTexture2; GLuint gameTexture3;//level texture
GLuint foxTexture;//player texture


//more global variables
ShaderProgram texturedProgram;
ShaderProgram textProgram;
SDL_Window* displayWindow;

float lerp(float v0, float v1, float t) {//used for friction. Linear Interpolation (LERP)
	return (1.0f - t)*v0 + t * v1;//note that everytime this function is used, if v1 is 0 then we're going to keep dividing v0 to eventually go to 0.
}

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

class SheetSprite {
public:
	SheetSprite(){//default constructor
		spriteCountX = 1;
		spriteCountY = 1;
		sizeX = 1.0f;
		sizeY = 1.0f;
	};
	void Draw(ShaderProgram* program) const;
	float sizeX; float sizeY;
	unsigned int textureID;
	int index;
	int spriteCountX;//horizontal #tile size of spritesheet
	int spriteCountY;
};

//note: the spritesheet I'm using for this hw is 20x20 blocks so spriteCountX = spriteCountY = 20
void SheetSprite::Draw(ShaderProgram* program) const {//drawing for uniform spritesheet. spriteCountX&spriteCountY are the z #rows&cols of 
	if (index >= 0) {//while there's something to draw. Note that index=-1 is a blank screen
		glBindTexture(GL_TEXTURE_2D, textureID);
		float u = (float)(((int)index) % spriteCountX) / (float)spriteCountX;
		float v = (float)(((int)index) / spriteCountX) / (float)spriteCountY;
		float spriteWidth = 1.0 / (float)spriteCountX;
		float spriteHeight = 1.0 / (float)spriteCountY;
		float texCoords[] = {
			u, v + spriteHeight,
			u + spriteWidth, v,
			u, v,
			u + spriteWidth, v,
			u, v + spriteHeight,
			u + spriteWidth, v + spriteHeight
		};
		float vertices[] = {//size to scale the image to the desired pixels
			-0.5f*sizeX, -0.5f*sizeY,
			0.5f*sizeX, 0.5f*sizeY,
			-0.5f*sizeX, 0.5f*sizeY,
			0.5f*sizeX, 0.5f*sizeY,
			-0.5f*sizeX,-0.5f*sizeY,
			0.5f*sizeX, -0.5f*sizeY };
		// draw this data
		glVertexAttribPointer(program->positionAttribute, 2, GL_FLOAT, false, 0, vertices);
		glEnableVertexAttribArray(program->positionAttribute);
		glVertexAttribPointer(program->texCoordAttribute, 2, GL_FLOAT, false, 0, texCoords);
		glEnableVertexAttribArray(program->texCoordAttribute);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		glDisableVertexAttribArray(program->positionAttribute);
		glDisableVertexAttribArray(program->texCoordAttribute);
	}
}

class Vector3 { //keep track of object coordinates
public:
	Vector3(float x = 0.0f, float y = 0.0f, float z = 0.0f) : x(x), y(y), z(z) {}
	float x;
	float y;
	float z;
};

enum EntityType {player};
class Entity {
public:
	Entity() : velocity(0.0f, 0.0f, 0.0f), acceleration(0.0f, 0.0f, 0.0f), friction(0.5f, 0.0f, 0.0f) {};
	void Update(float elapsed);
	void Render(ShaderProgram* program);
	bool CollidesWith(Entity *entity);
	SheetSprite sprite;
	Vector3 position;
	Vector3 size;
	Vector3 velocity;
	Vector3 acceleration;
	Vector3 friction;
	float entityMaxVelocity;
	bool isStatic;
	bool faceOtherWay;
	EntityType entityType;
	bool collidedTop;
	bool collidedBottom;
	bool collidedLeft;
	bool collidedRight;
};

enum GameMode {MAINMENU,LEVEL1,LEVEL2,LEVEL3,GAMEOVER,VICTORY};
class GameState {
public:
	Entity Player;
	std::vector<float> vertexData;
	std::vector<float> texCoordData;
	GameMode gamemode;
};

//place dyanmic object from flaremap
void PlaceEntity(std::string type, float x, float y, GameState* game, FlareMap* map) {

}

std::vector<int> SolidTile;//store the solid tiles depending on level
void InitializeLevel1(GameState* game){
	glClearColor(210.0f / 255.0f, 105.0f / 255.0f, 30.0f / 255.0f, 1.0f);
	int spriteCountX = 32; int spriteCountY = 40;
	//insert into textureCoord vector the uv-vertices of each mapdata to draw
	for (int y = 0; y < map1.mapHeight; y++) {
		for (int x = 0; x < map1.mapWidth; x++) {
			if (map1.mapData[y][x] != -1) {//don't draw the blank tiles. Change it to -1 to fix it
				// add vertices
				float u = (float)(((int)map1.mapData[y][x]) % spriteCountX) / (float)spriteCountX;
				float v = (float)(((int)map1.mapData[y][x]) / spriteCountX) / (float)spriteCountY;
				float spriteWidth = 1.0f / (float)spriteCountX;
				float spriteHeight = 1.0f / (float)spriteCountY;
				game->vertexData.insert(game->vertexData.end(), {//*x and *y means to draw at each location for the glvertexattributepointer
					tileSize * x, -tileSize * y,
					tileSize * x, (-tileSize * y) - tileSize,
					(tileSize * x) + tileSize, (-tileSize * y) - tileSize,
					tileSize * x, -tileSize * y,
					(tileSize * x) + tileSize, (-tileSize * y) - tileSize,
					(tileSize * x) + tileSize, -tileSize * y
					});
				game->texCoordData.insert(game->texCoordData.end(), {
					u, v,
					u, v + (spriteHeight),
					u + spriteWidth, v + (spriteHeight),
					u, v,
					u + spriteWidth, v + (spriteHeight),
					u + spriteWidth, v
					});
			}
		}
	}
	//indicate solid floor tiles for this stage. Note 128-139 are solid floor tiles
	//for (int i = 0; i < SolidTile.size(); i++) {
	//	SolidTile.pop_back();
	//}
	SolidTile.clear();
	for (int i = 128; i <= 139; i++) {
		SolidTile.push_back(i);
	}

	//20-27,52,84,116,59,91,123 are solid bolder tiles
	for (int i = 20; i <= 27; i++) {
		SolidTile.push_back(i);
	}
	std::string solidTiles = "52,84,116,59,91,123"; //int numTiles = 6;
	std::stringstream ss(solidTiles);
	std::string tileIndex;
	//for (int i = 0; i < numTiles; i++) {
	//	std::getline(ss, tileIndex, ',');//for every data value, convert to int-1 and then store it into mapdata's matrix (array of arrays: width x length)
	//	int val = atoi(tileIndex.c_str());//convert string to int
	//	SolidTile.push_back(val);
	//}
	while (ss) {
		std::getline(ss, tileIndex, ',');
		//for ever value obtained before each comma, add it into the SolidTile vector
		int val = atoi(tileIndex.c_str());//convert string to int
		SolidTile.push_back(val);
	}


	//fox player
	Entity player;
	player.position.x = 5.0f;
	player.position.y = -15.0f;
	game->Player = player;
	SheetSprite playerSpriteSheet;
	playerSpriteSheet.textureID = foxTexture;
	playerSpriteSheet.index = 0;
	playerSpriteSheet.spriteCountX = 3;
	playerSpriteSheet.spriteCountY = 9;
	playerSpriteSheet.sizeX = 2.0f;
	playerSpriteSheet.sizeY = 2.5f;
	game->Player.sprite = playerSpriteSheet;
}
void InitializeLevel2(GameState* game) {
	//glClearColor(102.0f/255.0f, 210.0f/255.0f, 255.0f/255.0f, 1.0f);//102,210,255 - skyblue
	glClearColor(76.0f / 255.0f, 185.0f / 255.0f, 229.0f / 255.0f, 1.0f);
	int spriteCountX = 32; int spriteCountY = 40;
	//insert into textureCoord vector the uv-vertices of each mapdata to draw
	for (int y = 0; y < map2.mapHeight; y++) {
		for (int x = 0; x < map2.mapWidth; x++) {
			if (map2.mapData[y][x] != -1) {//don't draw the blank tiles. Change it to -1 to fix it
				// add vertices
				float u = (float)(((int)map2.mapData[y][x]) % spriteCountX) / (float)spriteCountX;
				float v = (float)(((int)map2.mapData[y][x]) / spriteCountX) / (float)spriteCountY;
				float spriteWidth = 1.0f / (float)spriteCountX;
				float spriteHeight = 1.0f / (float)spriteCountY;
				game->vertexData.insert(game->vertexData.end(), {//*x and *y means to draw at each location for the glvertexattributepointer
					tileSize * x, -tileSize * y,
					tileSize * x, (-tileSize * y) - tileSize,
					(tileSize * x) + tileSize, (-tileSize * y) - tileSize,
					tileSize * x, -tileSize * y,
					(tileSize * x) + tileSize, (-tileSize * y) - tileSize,
					(tileSize * x) + tileSize, -tileSize * y
					});
				game->texCoordData.insert(game->texCoordData.end(), {
					u, v,
					u, v + (spriteHeight),
					u + spriteWidth, v + (spriteHeight),
					u, v,
					u + spriteWidth, v + (spriteHeight),
					u + spriteWidth, v
					});
			}
		}
	}
	//indicate solid floor tiles for this stage. Note "" are solid floor tiles
	//for (int i = 0; i < SolidTile.size(); i++) {
	//	SolidTile.pop_back();
	//}
	SolidTile.clear();
	for (int i = 128; i <= 767; i++) {
		SolidTile.push_back(i);
	}
	//for (int i = 128; i <= 1023; i++) {
	//	SolidTile.push_back(i);
	//}
	//std::string solidTiles = "52,84,116,59,91,123"; //int numTiles = 6;
	std::string solidTiles = "";
	std::stringstream ss(solidTiles);
	std::string tileIndex;
	while (ss) {
		std::getline(ss, tileIndex, ',');
		//for ever value obtained before each comma, add it into the SolidTile vector
		int val = atoi(tileIndex.c_str());//convert string to int
		SolidTile.push_back(val);
	}


	//fox player
	Entity player;
	player.position.x = 5.0f;
	player.position.y = -45.0f;
	game->Player = player;
	SheetSprite playerSpriteSheet;
	playerSpriteSheet.textureID = foxTexture;
	playerSpriteSheet.index = 0;
	playerSpriteSheet.spriteCountX = 3;
	playerSpriteSheet.spriteCountY = 9;
	playerSpriteSheet.sizeX = 2.0f;
	playerSpriteSheet.sizeY = 2.5f;
	game->Player.sprite = playerSpriteSheet;
}
void InitializeLevel3(GameState* game) {

}

void tilemapRender(GameState game, ShaderProgram* program, GLuint gameTexture) {//for rendering static entities
	// draw this data

	Matrix tilemapModelMatrix;
	program->SetModelMatrix(tilemapModelMatrix);

	glBindTexture(GL_TEXTURE_2D, gameTexture); //use game's texture to draw
	glVertexAttribPointer(program->positionAttribute, 2, GL_FLOAT, false, 0, game.vertexData.data());//the "2" stands for 2 coords per vertex
	glEnableVertexAttribArray(program->positionAttribute);

	glVertexAttribPointer(program->texCoordAttribute, 2, GL_FLOAT, false, 0, game.texCoordData.data());
	glEnableVertexAttribArray(program->texCoordAttribute);

	glUseProgram(program->programID);
	glDrawArrays(GL_TRIANGLES, 0, game.vertexData.size() / 2);//vertexData.size() gives the number of coordinates, so divide by 2 to get the number of vertices
	glDisableVertexAttribArray(program->positionAttribute);
	glDisableVertexAttribArray(program->texCoordAttribute);
}

void worldToTileCoordinates(float worldX, float worldY, int *gridX, int *gridY) {//divide worldSize/tileSize = gridSize (i.e. which block number along x and y)
	*gridX = (int)(worldX / tileSize);//note we're casting to int
	*gridY = (int)(-worldY / tileSize);//note gridY returned is (+)
}

//running animation for player
const int runIndices[] = { 1,2,3,4,5 };//leave out resting sprite
int counter = 0;
void running(Entity* player) {
	player->sprite.index = runIndices[counter];
	counter++;
	if (counter == 5) {
		counter = 0;
	}
}

const Uint8 *keys = SDL_GetKeyboardState(NULL);
float playerSpeed = 30.0f;
void ProcessInput(GameState* game, float elapsed) {
	//if (bossAnimation()) {
	//	//halt movement
	//}
	if (keys[SDL_SCANCODE_LEFT]) {
		game->Player.velocity.x = -playerSpeed;
		game->Player.faceOtherWay = true;
	}
	if (keys[SDL_SCANCODE_RIGHT]) {
		game->Player.velocity.x = playerSpeed;
		game->Player.faceOtherWay = false;
	}
	if (keys[SDL_SCANCODE_UP]) {
		game->Player.velocity.y = playerSpeed;
	}
	if (keys[SDL_SCANCODE_DOWN]) {
		game->Player.velocity.y = -playerSpeed;
	}
	//if not running then set velocity to 0
	if (!(keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_RIGHT])) {
		game->Player.velocity.x = 0.0f;
		game->Player.sprite.index = 0;
	}
	else {//going left or right, so do running animation
		running(&(game->Player));
	}
	if (!(keys[SDL_SCANCODE_UP] || keys[SDL_SCANCODE_DOWN])) {
		game->Player.velocity.y = 0.0f;
	}

	if (keys[SDL_SCANCODE_1]) {
		game->gamemode = LEVEL1;
		//clear the draw vertices data
		game->vertexData.clear();
		game->texCoordData.clear();
		//initialize game with new data
		InitializeLevel1(game);
	}
	if (keys[SDL_SCANCODE_2]) {
		game->gamemode = LEVEL2;
		game->vertexData.clear();
		game->texCoordData.clear();
		InitializeLevel2(game);
	}
	if (keys[SDL_SCANCODE_3]) {
		game->gamemode = LEVEL3;
		game->vertexData.clear();
		game->texCoordData.clear();
		InitializeLevel3(game);
	}

}

void Update1(GameState* game,float elapsed) {
	game->Player.position.x += game->Player.velocity.x * elapsed;
	game->Player.position.y += game->Player.velocity.y * elapsed;

	int gridX; int gridY;
	float playerHalfSizeY = 0.90f * game->Player.sprite.sizeY / 2.0f;//made it 0.90x b/c the player's sprite has some emptiness at the bottom
	float playerHalfSizeX = 0.70f * game->Player.sprite.sizeX / 2.0f;
	//find the bottom of the player and then convert that coord to tiled
	float playerLeft = game->Player.position.x - playerHalfSizeX;
	float playerRight = game->Player.position.x + playerHalfSizeX;
	float playerTop = game->Player.position.y + playerHalfSizeY;
	float playerBottom = game->Player.position.y - playerHalfSizeY;
	//My solid floor tiles indices: 128-139, which we will indicate in IsSolid vector.

	//collsion on the bottom of player
	worldToTileCoordinates(game->Player.position.x, playerBottom, &gridX, &gridY);//this is the center bottom of the player, but how do we check other tiles of player bottom??? question. Use for loop: -player.size.x/2 to player.size.x/2 in tile coords. nTiles = player.size.x/tileSize;
	//note the return value of gridX,gridY corresponds to which tile (defined in row=gridX,col=gridY) is in touch with the player's bottom.
	if (gridX < map1.mapWidth && gridX >= 0 && gridY < map1.mapHeight && gridY >= 0) {//if the player is still within Level bounds. This check is needed b/c otherwise map.mapData[y][x] would be accessing outside of it's array
		for (int i = 0; i < SolidTile.size(); i++) {
			if (map1.mapData[gridY][gridX] == SolidTile[i]) {//COLLISION on the bottom of player(if the playerBottom's grid position corresponds to the solid tile). Resolve it in terms of world coords
				float tileLeft = (float)gridX * tileSize;
				float tileRight = ((float)gridX + 1.0f) * tileSize;
				float tileTop = (float)(-gridY) * tileSize;
				float tileBottom = ((float)(-gridY) - 1.0f) * tileSize;
				//This check is useful if the player is larger than 1tilex1tile. ??? This actually is useless b/c we already saw a collision. But note that the player's size is based on 1 tile so no need for this.
				if (!(playerBottom > tileTop || playerTop < tileBottom || playerLeft > tileRight || playerRight < tileLeft)) {//collision!
					float penetrationY = fabs(playerBottom - tileTop);
					game->Player.position.y += penetrationY + 0.001f;
					if (game->Player.velocity.y <= 0) {//do this check to see if the player jumped, so we don't set the jump velocity to 0
						game->Player.velocity.y = 0.0f;//why do we need this???? Ans: So the player doesn't keep going down as you're moving and colliding. i.e. w/o this, the gravity's acceleration will keep making your velocity get bigger negatively so even with the penetration fix, the velocity*elapsed will make it go down more and more each tick.
					}
					game->Player.collidedBottom = true;
				}
			}
		}		
	}

	//collision on the top of player
	worldToTileCoordinates(game->Player.position.x, playerTop, &gridX, &gridY);
	if (gridX < map1.mapWidth && gridX >= 0 && gridY < map1.mapHeight && gridY >= 0) {//if player's top side is still in level bounds
		for (int i = 0; i < SolidTile.size(); i++) {
			if (map1.mapData[gridY][gridX] == SolidTile[i]) {
				//COLLISION on the top of player
				float tileLeft = (float)gridX * tileSize;
				float tileRight = ((float)gridX + 1.0f) * tileSize;
				float tileTop = (float)(-gridY) * tileSize;
				float tileBottom = ((float)(-gridY) - 1.0f) * tileSize;
				game->Player.velocity.y = 0.0f;
				float penetrationY = fabs(playerTop - tileBottom);
				//game->Player.position.x -= (penetrationY + 0.001f);
				game->Player.position.y -= (penetrationY + 0.00001f);//the player shakes because the "jump" has a big velocity so it moves up quite a bit at each frame.

				game->Player.collidedTop = true;
			}
		}
	}

	//collision on the right side of player
	worldToTileCoordinates(playerRight, game->Player.position.y, &gridX, &gridY);
	if (gridX < map1.mapWidth && gridX >= 0 && gridY < map1.mapHeight && gridY >= 0) {//if player's right side is still in level bounds
		for (int i = 0; i < SolidTile.size(); i++) {
			if (map1.mapData[gridY][gridX] == SolidTile[i]) {//COLLISION on the right of player
				float tileLeft = (float)gridX * tileSize;
				float tileRight = ((float)gridX + 1.0f) * tileSize;
				float tileTop = (float)(-gridY) * tileSize;
				float tileBottom = ((float)(-gridY) - 1.0f) * tileSize;

				float penetrationX = fabs(playerRight - tileLeft);
				game->Player.position.x -= (penetrationX + 0.001f);
				//game->Player.velocity.x = 0.0f;
				game->Player.collidedRight = true;
			}
		}
	}

	//collision on the left side of player
	worldToTileCoordinates(playerLeft, game->Player.position.y, &gridX, &gridY);
	if (gridX < map1.mapWidth && gridX >= 0 && gridY < map1.mapHeight && gridY >= 0) {//if player's left side is still in level bounds
		for (int i = 0; i < SolidTile.size(); i++) {
			if (map1.mapData[gridY][gridX] == SolidTile[i]) {//COLLISION on the left of player
				float tileLeft = (float)gridX * tileSize;
				float tileRight = ((float)gridX + 1.0f) * tileSize;
				float tileTop = (float)(-gridY) * tileSize;
				float tileBottom = ((float)(-gridY) - 1.0f) * tileSize;

				float penetrationX = fabs(playerLeft - tileRight);
				game->Player.position.x += penetrationX + 0.001f;
				//game->Player.velocity.x = 0.0f;
				game->Player.collidedLeft = true;
			}
		}
	}
}
void Update2(GameState* game, float elapsed) {
	game->Player.position.x += game->Player.velocity.x * elapsed;
	game->Player.position.y += game->Player.velocity.y * elapsed;

	int gridX; int gridY;
	float playerHalfSizeY = 0.90f * game->Player.sprite.sizeY / 2.0f;//made it 0.90x b/c the player's sprite has some emptiness at the bottom
	float playerHalfSizeX = 0.70f * game->Player.sprite.sizeX / 2.0f;
	//find the bottom of the player and then convert that coord to tiled
	float playerLeft = game->Player.position.x - playerHalfSizeX;
	float playerRight = game->Player.position.x + playerHalfSizeX;
	float playerTop = game->Player.position.y + playerHalfSizeY;
	float playerBottom = game->Player.position.y - playerHalfSizeY;
	//My solid floor tiles indices: "", which we will indicate in IsSolid vector.

	//collsion on the bottom of player
	worldToTileCoordinates(game->Player.position.x, playerBottom, &gridX, &gridY);//this is the center bottom of the player, but how do we check other tiles of player bottom??? question. Use for loop: -player.size.x/2 to player.size.x/2 in tile coords. nTiles = player.size.x/tileSize;
	//note the return value of gridX,gridY corresponds to which tile (defined in row=gridX,col=gridY) is in touch with the player's bottom.
	if (gridX < map2.mapWidth && gridX >= 0 && gridY < map2.mapHeight && gridY >= 0) {//if the player is still within Level bounds. This check is needed b/c otherwise map.mapData[y][x] would be accessing outside of it's array
		for (int i = 0; i < SolidTile.size(); i++) {
			if (map2.mapData[gridY][gridX] == SolidTile[i]) {//COLLISION on the bottom of player(if the playerBottom's grid position corresponds to the solid tile). Resolve it in terms of world coords
				float tileLeft = (float)gridX * tileSize;
				float tileRight = ((float)gridX + 1.0f) * tileSize;
				float tileTop = (float)(-gridY) * tileSize;
				float tileBottom = ((float)(-gridY) - 1.0f) * tileSize;
				//This check is useful if the player is larger than 1tilex1tile. ??? This actually is useless b/c we already saw a collision. But note that the player's size is based on 1 tile so no need for this.
				if (!(playerBottom > tileTop || playerTop < tileBottom || playerLeft > tileRight || playerRight < tileLeft)) {//collision!
					float penetrationY = fabs(playerBottom - tileTop);
					game->Player.position.y += penetrationY + 0.001f;
					if (game->Player.velocity.y <= 0) {//do this check to see if the player jumped, so we don't set the jump velocity to 0
						game->Player.velocity.y = 0.0f;//why do we need this???? Ans: So the player doesn't keep going down as you're moving and colliding. i.e. w/o this, the gravity's acceleration will keep making your velocity get bigger negatively so even with the penetration fix, the velocity*elapsed will make it go down more and more each tick.
					}
					game->Player.collidedBottom = true;
				}
			}
		}
	}

	//collision on the top of player
	worldToTileCoordinates(game->Player.position.x, playerTop, &gridX, &gridY);
	if (gridX < map2.mapWidth && gridX >= 0 && gridY < map2.mapHeight && gridY >= 0) {//if player's top side is still in level bounds
		for (int i = 0; i < SolidTile.size(); i++) {
			if (map2.mapData[gridY][gridX] == SolidTile[i]) {//COLLISION on the top of player
				float tileLeft = (float)gridX * tileSize;
				float tileRight = ((float)gridX + 1.0f) * tileSize;
				float tileTop = (float)(-gridY) * tileSize;
				float tileBottom = ((float)(-gridY) - 1.0f) * tileSize;
				game->Player.velocity.y = 0.0f;
				float penetrationY = fabs(playerTop - tileBottom);
				//game->Player.position.x -= (penetrationY + 0.001f);
				game->Player.position.y -= (penetrationY + 0.00001f);//the player shakes because the "jump" has a big velocity so it moves up quite a bit at each frame.

				game->Player.collidedTop = true;
			}
		}
	}

	//collision on the right side of player
	worldToTileCoordinates(playerRight, game->Player.position.y, &gridX, &gridY);
	if (gridX < map2.mapWidth && gridX >= 0 && gridY < map2.mapHeight && gridY >= 0) {//if player's right side is still in level bounds
		for (int i = 0; i < SolidTile.size(); i++) {
			if (map2.mapData[gridY][gridX] == SolidTile[i]) {//COLLISION on the right of player
				float tileLeft = (float)gridX * tileSize;
				float tileRight = ((float)gridX + 1.0f) * tileSize;
				float tileTop = (float)(-gridY) * tileSize;
				float tileBottom = ((float)(-gridY) - 1.0f) * tileSize;

				float penetrationX = fabs(playerRight - tileLeft);
				game->Player.position.x -= (penetrationX + 0.001f);
				//game->Player.velocity.x = 0.0f;
				game->Player.collidedRight = true;
			}
		}
	}

	//collision on the left side of player
	worldToTileCoordinates(playerLeft, game->Player.position.y, &gridX, &gridY);
	if (gridX < map2.mapWidth && gridX >= 0 && gridY < map2.mapHeight && gridY >= 0) {//if player's left side is still in level bounds
		for (int i = 0; i < SolidTile.size(); i++) {
			if (map2.mapData[gridY][gridX] == SolidTile[i]) {//COLLISION on the left of player
				float tileLeft = (float)gridX * tileSize;
				float tileRight = ((float)gridX + 1.0f) * tileSize;
				float tileTop = (float)(-gridY) * tileSize;
				float tileBottom = ((float)(-gridY) - 1.0f) * tileSize;

				float penetrationX = fabs(playerLeft - tileRight);
				game->Player.position.x += penetrationX + 0.001f;
				//game->Player.velocity.x = 0.0f;
				game->Player.collidedLeft = true;
			}
		}
	}
}
//MAINMENU, LEVEL1, LEVEL2, LEVEL3, GAMEOVER, VICTORY
void Update(GameState* game, float elapsed) {//mode rendering (changes game state and draw the entities for that particular state)
	switch (game->gamemode) {
	case MAINMENU:
		break;
	case LEVEL1:
		Update1(game, elapsed);
		break;
	case LEVEL2:
		Update2(game, elapsed);
		break;
	case LEVEL3:
		//Update3(game, elapsed);
		break;
	case GAMEOVER:
		break;
	case VICTORY:
		break;
	}
}


void Render1(GameState* game, ShaderProgram* program, float elapsed) {
	//render static entities
	glClear(GL_COLOR_BUFFER_BIT);//rendering happens once very frame, but there's no way to remove what's previously rendered, but glClear will make what's previously rendered blank.
	glEnable(GL_BLEND);//don't put this nor glBlendFunc in loop!
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); //gets rid of the clear (black) parts of the png
	tilemapRender(*game, program, gameTexture1);

	Matrix ModelMatrix;

	//draw player
	ModelMatrix.Identity();//set player's position to identity
	ModelMatrix.Translate(game->Player.position.x, game->Player.position.y, 0.0f);//note we have to change the player's position whenever we change the player model matrix
	if (game->Player.faceOtherWay == true) {
		ModelMatrix.Scale(-1.0f, 1.0f, 1.0f);
	}
	texturedProgram.SetModelMatrix(ModelMatrix);
	game->Player.sprite.Draw(&texturedProgram);
}
void Render2(GameState* game, ShaderProgram* program, float elapsed) {
	//render static entities
	glClear(GL_COLOR_BUFFER_BIT);//rendering happens once very frame, but there's no way to remove what's previously rendered, but glClear will make what's previously rendered blank.
	glEnable(GL_BLEND);//don't put this nor glBlendFunc in loop!
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); //gets rid of the clear (black) parts of the png
	tilemapRender(*game, program, gameTexture2);

	Matrix ModelMatrix;

	//draw player
	ModelMatrix.Identity();//set player's position to identity
	ModelMatrix.Translate(game->Player.position.x, game->Player.position.y, 0.0f);//note we have to change the player's position whenever we change the player model matrix
	if (game->Player.faceOtherWay == true) {
		ModelMatrix.Scale(-1.0f, 1.0f, 1.0f);
	}
	texturedProgram.SetModelMatrix(ModelMatrix);
	game->Player.sprite.Draw(&texturedProgram);
}
void Render3(GameState* game, ShaderProgram* program, float elapsed) {

}

//MAINMENU, LEVEL1, LEVEL2, LEVEL3, GAMEOVER, VICTORY
void Render(GameState* game, ShaderProgram* program, float elapsed) {//mode rendering (changes game state and draw the entities for that particular state)
	switch (game->gamemode) {
	case MAINMENU:
		break;
	case LEVEL1:
		Render1(game, program, elapsed);
		break;
	case LEVEL2:
		Render2(game, program, elapsed);
		break;
	case LEVEL3:
		Render3(game, program, elapsed);
		break;
	case GAMEOVER:
		break;
	case VICTORY:
		break;
	}
}




int main(int argc, char *argv[])
{
	SDL_Init(SDL_INIT_VIDEO);
	displayWindow = SDL_CreateWindow("Final Project Demo", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, screenWidth, screenHeight, SDL_WINDOW_OPENGL);
	SDL_GLContext context = SDL_GL_CreateContext(displayWindow);
	SDL_GL_MakeCurrent(displayWindow, context);
	#ifdef _WINDOWS
		glewInit();
	#endif

		Matrix projectionMatrix;
		Matrix viewMatrix; //where in the world you're looking at note that identity means your view is at (0,0)
		projectionMatrix.SetOrthoProjection(leftScreen, rightScreen, bottomScreen, topScreen, -1.0f, 1.0f);//defining the bounds for the screen for the view matrix

		texturedProgram.Load(RESOURCE_FOLDER"vertex_textured.glsl", RESOURCE_FOLDER"fragment_textured.glsl");
		texturedProgram.SetProjectionMatrix(projectionMatrix);
		texturedProgram.SetViewMatrix(viewMatrix);//I will need to have to set viewmatrix to follow player position

		map1.Load("Level1.2.txt");
		map2.Load("Level2.4.txt");
		map3.Load("Level3.0.txt");
		//map3.Load("Level1.2.txt");
		gameTexture1 = LoadTexture(RESOURCE_FOLDER"1FallStage.png");
		gameTexture2 = LoadTexture(RESOURCE_FOLDER"2SnowStage.png");
		gameTexture3 = LoadTexture(RESOURCE_FOLDER"3NightStage.png");
		foxTexture = LoadTexture(RESOURCE_FOLDER"0kit-fox.png");

		GameState game;
		game.gamemode = LEVEL2;
		//InitializeLevel1(&game);
		//InitializeLevel2(&game);

		//to view the map
		//Entity camera;
		//camera.position.x = camera.position.z = 0.0f;
		//camera.position.y = 0.0f;// -LEVEL_HEIGHT * tileSize;
		////camera.position.y = -1.0f;
		////game.Player = camera;
	SDL_Event event;
	bool done = false;
	while (!done) {
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) {
				done = true;
			}
		}
		//glClearColor(0.5f, 0.0, 0.5f, 1.0f);//clear color
		//glClear(GL_COLOR_BUFFER_BIT);//set screen to clear color

		float ticks = (float)SDL_GetTicks() / 1000.0f;//returns the number of millisecs/1000 elapsed since you compiled code
		float elapsed = ticks - lastFrameTicks;//returns the number of millisecs/1000 elapsed since the last loop of the event
		lastFrameTicks = ticks;

		elapsed += accumulator;

		if (elapsed > 6.0f * FIXED_TIMESTEP) {//initializing the game may take a lot of elapsed time so we don't care about the time in the pre-rendering moments
			elapsed = 6.0f * FIXED_TIMESTEP;
		}

		if (elapsed < FIXED_TIMESTEP) {//if not enough time elapsed yet for desired first frame then don't update
			accumulator = elapsed;
			continue;//if elapsed isn't big enough, then ignore what's below, go back to the top of the current while loop.
		}


		while (elapsed >= FIXED_TIMESTEP) {//if enough time for a frame, then update each frame until not enough time for single frame

			/* render */
			viewMatrix.Identity();
			viewMatrix.Translate(-game.Player.position.x, -game.Player.position.y, 0.0f);
			texturedProgram.SetViewMatrix(viewMatrix);
			ProcessInput(&game, FIXED_TIMESTEP);
			Update(&game, FIXED_TIMESTEP);
			Render(&game, &texturedProgram, FIXED_TIMESTEP);

			/* render */

			elapsed -= FIXED_TIMESTEP;
		}
		accumulator = elapsed;



		/* put rendering stuff here */
		//Update(elapsed);
		//Render();
		//ProcessInput(elapsed);

		SDL_GL_SwapWindow(displayWindow);
	}

	SDL_Quit();
	return 0;
}

/*------------------- Patch Notes --------------------*/
/*
1)Load Level1 tileworld
2)Draw fox player on screen
2.1)Created function to read in indices of solid tiles
3.0)Give running animation
3.1)Check for player collision with floor, keep him above ground.

4)Create Level2 and have update(),render() with their respective levels
5)Boss summon animation, shake screen and don't allow player to move until animation is over.



*/