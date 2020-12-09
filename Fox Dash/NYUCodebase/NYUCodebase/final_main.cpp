/* Final Project CS3113
Johnathan Apuzen
Victor Zheng

Controls:
Movement: up,down,left,right
Attack: Spacebar
Switch Levels: 1,2,3 (need to implement other ways to switch levels e.g. press up while colliding with cave.
Suicide: d
Main menu: m
Pause Game: Esc
Mainmenu->CloseWindow: Press Esc while on mainmenu

//debug notes
1)level not rendered properly, world tiles are mapped to the wrong tiles in the picture. Fix: !=-1 instead of !=0 for empty tiles
2)After loading level2, the player does not collide with the tiles on the bottom of the level. Fix: Change update2's map1 to map2
3)Level 3 player is stuck at starting point (always colliding?). Fix:Forgot to uncomment Update3() in Update() case
4)When pausing, gamelevel doesn't render. Fix:I accidentally put the glclear after renderlevel which cleared the screen before adding the pausescreen. Move the glclear before renderlevel.
5)When player dies, the level turns blank. Fix:update the currentLevel variable to be the current level to render.
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
//Level1: 20hx150w
//Level2: 50hx150w
//Level3: 20hx150w
//note that this level will be 240*400 pixels

/* Adjustable global variables */
//screen
float screenWidth = 720 * 1.7;
float screenHeight = 480 * 1.5;
float topScreen = 7.0f + 1.5f;
float bottomScreen = -7.0f - 1.5f;
float aspectRatio = screenWidth / screenHeight;
float leftScreen = -aspectRatio * topScreen;
float rightScreen = aspectRatio * topScreen;
float screenShakeValue; float screenShakeSpeed; float screenShakeIntensity;

//time and FPS
float accumulator = 0.0f; float lastFrameTicks = 0.0f;

//game
bool cheats = false;//Note: can also press 0 to toggle on/off 
//bool cheats = true;
int skeletonBossHealth = 12; int golemHealth = 20;
float jumpSpeed = 40.0f;
float playerSpeed = 6.0f;
float playerAcceleration = 130.0f;
float tileSize = 1.0f;//tile size is how big you want each block to be in the world (e.g. our sprite is 16x16pixels in png, but we can make it 1.0x1.0 in world)
float cameraOffSetY = 4.4f;//to keep player's view above the floor tiles

//Player Movement
float friction_x = 12.0f;
float friction_y = 2.0f;
float gravity = -80.0f;

float playerAttackTimer = 0.60f;
float playerLastShot = 0.0f;

//other global storage
float collidedCaveCounter; float MoveTo3Counter;
float viewX; float viewY;//for viewmatrix spots. This is used for pause-screen location
float viewXFinal;//for smooth transition of screen sliding

//level2 global variables
bool openPortal = false; float earthquakeTimer = 2.0f;
//level3 global variables
float golemDrawRatio = 1.5f;

FlareMap map1; FlareMap map2; FlareMap map3;
GLuint menuTexture; GLuint pauseTexture; GLuint gameOverTexture;
GLuint gameTexture1; GLuint gameTexture2; GLuint gameTexture3;//level texture
GLuint arrowIndicatorTexture; GLuint portalTexture;

float closeWindow;

//music
Mix_Music *music;
Mix_Music *musicLevel1;
Mix_Music *musicLevel2;
Mix_Music *musicLevel3;


//sound
//mushroom sound
Mix_Chunk* mushroomThrow;
Mix_Chunk* mushroomHit;
Mix_Chunk* mushroomHit2; //collision with entity
//player sounds
Mix_Chunk* playerHit;
Mix_Chunk* playerJump;
//Enemy sounds
Mix_Chunk* vampireAttackSound;
Mix_Chunk* skeletonAttackSound;
Mix_Chunk* golemAttackSound;
Mix_Chunk* golemSpawn;
//Misc
Mix_Chunk* levelVictory;
Mix_Chunk* gameVictory;


//player
GLuint foxTexture;//player texture
GLuint mushroomTextures[3];


//Skeleton textures
GLuint skeletonTextureIdle[6]; 
GLuint skeletonTextureWalking[8];
GLuint skeletonTextureAttacking[8];
GLuint skeletonTextureDying[8];

const int maxSkeletons = 6;

//Vampire textures
GLuint vampireTextureIdle[8];
GLuint vampireTextureAttack[13];
GLuint vampireTextureDying[12];
GLuint vampireTextureProjectile;

const int maxVampires = 5;

//Golem textures
GLuint golemTextureWalk[6];
GLuint golemTextureAppear[15];
GLuint golemTextureAttack[6];
GLuint golemTextureDie[7];
GLuint golemTextureProjectile;
GLuint golemTextureRed;

bool golemActivated = false;
bool golemCutscene = false;


//more global variables
ShaderProgram texturedProgram;
ShaderProgram textProgram;
SDL_Window* displayWindow;
int i = 0;
Matrix viewMatrix;

float lerp(float v0, float v1, float t) {//used for friction. Linear Interpolation (LERP)
	return (1.0f - t)*v0 + t * v1;//note that everytime this function is used, if v1 is 0 then we're going to keep dividing v0 to eventually go to 0.
}

const Uint8 *keys = SDL_GetKeyboardState(NULL);

bool cheatIsOn = false; float timerBeforeToggle = 0.2f;
void Cheats(float elapsed) {
	if (keys[SDL_SCANCODE_0] && cheats && timerBeforeToggle>=0.2f) {//turn on cheats
		cheats = false;
		timerBeforeToggle = 0.0f;
	}
	if (keys[SDL_SCANCODE_0] && !cheats && timerBeforeToggle>=0.2f) {//turn off cheats
		cheats = true;
		timerBeforeToggle = 0.0f;
	}
	timerBeforeToggle += elapsed;
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

void Draw(ShaderProgram* program, float sizeX, float sizeY, GLuint texture) {
	glBindTexture(GL_TEXTURE_2D, texture);

	float vertices[] = { -sizeX / 2, sizeY / 2, -sizeX / 2, -sizeY / 2, sizeX / 2, sizeY / 2,      sizeX / 2, sizeY / 2, -sizeX / 2, -sizeY / 2, sizeX / 2, -sizeY / 2 };
	glVertexAttribPointer(program->positionAttribute, 2, GL_FLOAT, false, 0, vertices);

	float texCoords[] = { 0.0, 0.0, 0.0, 1.0, 1.0, 0.0,			1.0, 0.0, 0.0, 1.0, 1.0, 1.0 };
	glVertexAttribPointer(program->texCoordAttribute, 2, GL_FLOAT, false, 0, texCoords);

	glEnableVertexAttribArray(program->texCoordAttribute);
	glEnableVertexAttribArray(program->positionAttribute);

	glDrawArrays(GL_TRIANGLES, 0, 6);

	glDisableVertexAttribArray(program->positionAttribute);
	glDisableVertexAttribArray(program->texCoordAttribute);
}

enum EntityType {player};
class Entity {
public:
	Entity() : velocity(0.0f, 0.0f, 0.0f), acceleration(0.0f, 0.0f, 0.0f), friction(friction_x, friction_y, 0.0f), attacking(false), drawOffset(0.0f,0.0f,0.0f){
		collidedCave = false; collidedCaveArrow = false; MoveTo3 = false;
		skeletonBossAttackRange = 0.0f;
	};
	void Update(float elapsed);
	void Render(ShaderProgram* program);
	bool CollidesWith(Entity *entity);
	SheetSprite sprite;
	Vector3 position;
	Vector3 size;
	Vector3 drawSize;
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
	bool collidedCave; bool collidedCaveArrow;

	GLuint texture;
	int texCounter;
	float rotation;
	float rotationFactor;
	int dyingCounter;
	int attackCounter;
	bool dead;
	bool dying;
	bool attacking;
	bool playerDeathRight;
	float drawRatio;
	bool MoveTo3;

	Vector3 drawOffset;
	int health = 1;

	//specific enemies
	float skeletonBossAttackRange;
};

enum GameMode {MAINMENU,LEVEL1,LEVEL2,LEVEL3,PAUSE,GAMEOVER,VICTORY};
GameMode currentLevel;
class GameState {
public:
	Entity Player;
	Entity Mushroom;
	Entity Skeleton[maxSkeletons];
	Entity Vampire[maxVampires];
	Entity VampireProjectile[maxVampires * 3];
	Entity Golem;
	Entity GolemProjectile;

	//toggle music/sound on/off
	bool music;
	bool sound;


	std::vector<float> vertexData;
	std::vector<float> texCoordData;
	GameMode gamemode;
};

//place dyanmic object from flaremap
void PlaceEntity(std::string type, float x, float y, GameState* game, FlareMap* map) {

}

std::vector<int> SolidTile;//store the solid tiles depending on level
std::vector<int> caveTiles;//this is so the player can move on to level2

//Initialize each entity
void InitializeFox(GameState* game) {
	//fox player
	game->Player.position.x = 5.0f;
	game->Player.position.y = -15.0f;
	SheetSprite playerSpriteSheet;
	playerSpriteSheet.textureID = foxTexture;
	playerSpriteSheet.index = 0;
	playerSpriteSheet.spriteCountX = 3;
	playerSpriteSheet.spriteCountY = 9;
	playerSpriteSheet.sizeX = 2.0f;
	playerSpriteSheet.sizeY = 2.5f;

	game->Player.size.x = 2.0f * 0.65f;
	game->Player.size.y = 2.5f * 0.90f; //made it 0.90x b/c the player's sprite has some emptiness at the bottom

	game->Player.sprite = playerSpriteSheet;
	game->Player.dead = false;

	//Mushroom init
	game->Mushroom.position.x = game->Mushroom.position.y = -500.0f;

	game->Mushroom.drawSize.x = 70.0f * 0.025f; //Size of sprite
	game->Mushroom.drawSize.y = 70.0f * 0.025f;

	game->Mushroom.size.x = 70.0f * 0.025f * 0.45f; //Size of collision box
	game->Mushroom.size.y = 70.0f * 0.025f * 0.1f;

	game->Mushroom.texture = mushroomTextures[0];

	game->Mushroom.friction.x = 1.8f;
	game->Mushroom.friction.y = 1.0f;

	game->Mushroom.rotation = -45.0f * (3.141592f / 180.0f);
	game->Mushroom.rotationFactor = 2.0f * (3.141592f / 180.0f);

	game->Mushroom.dead = true;

	collidedCaveCounter = 0.0f; MoveTo3Counter = 0.0f;
}
void InitializeSkeletons(GameState* game) {
	//Skeletons init
	for (int j = 0; j < maxSkeletons; j++) {//note 0 is for the subboss if there is one
		game->Skeleton[j].drawRatio = 0.01f;
		game->Skeleton[j].position.x = -500.0f;
		game->Skeleton[j].position.y = -500.0f;

		game->Skeleton[j].drawSize.x = 224.0f * game->Skeleton[j].drawRatio; //Size of sprite
		game->Skeleton[j].drawSize.y = 364.0f * game->Skeleton[j].drawRatio;

		game->Skeleton[j].size.x = 224.0f * game->Skeleton[j].drawRatio * 0.70f; //Size of collision box
		game->Skeleton[j].size.y = 364.0f * game->Skeleton[j].drawRatio * 0.95f;

		game->Skeleton[j].texture = skeletonTextureIdle[0];

		game->Skeleton[j].attacking = false;
		game->Skeleton[j].dying = false;
		game->Skeleton[j].dead = true;
	}
}
void InitializeSkeletonBoss(GameState* game) {
	game->Skeleton[0].health = skeletonBossHealth; game->Skeleton[0].skeletonBossAttackRange = 4.5f;
	game->Skeleton[0].drawRatio = 0.02f;
	game->Skeleton[0].position.x = 0.0f;
	game->Skeleton[0].position.y = -10.0f;
	game->Skeleton[0].velocity.x = 0.0f;
	game->Skeleton[0].acceleration.x = 0.0f;

	game->Skeleton[0].drawSize.x = 224.0f * 0.02f; //Size of sprite
	game->Skeleton[0].drawSize.y = 364.0f * 0.02f;

	game->Skeleton[0].size.x = 224.0f * 0.02f * 0.70f; //Size of collision box
	game->Skeleton[0].size.y = 364.0f * 0.02f * 0.95f;

	game->Skeleton[0].texture = skeletonTextureIdle[0];

	game->Skeleton[0].attacking = false;
	game->Skeleton[0].dying = false;
	game->Skeleton[0].dead = false;
}
void InitializeVampires(GameState* game) {
	//Vampires init
	for (int j = 0; j < maxVampires; j++) {
		game->Vampire[j].position.x = -500.0f;
		game->Vampire[j].position.y = -500.0f;

		game->Vampire[j].drawSize.x = 188.0f * 0.01f; //Size of sprite
		game->Vampire[j].drawSize.y = 336.0f * 0.01f;

		game->Vampire[j].size.x = 188.0f * 0.01f * 0.70f; //Size of collision box
		game->Vampire[j].size.y = 336.0f * 0.01f * 0.95f;

		game->Vampire[j].drawOffset.x = game->Vampire[j].drawOffset.y = 0.0f;

		game->Vampire[j].texture = vampireTextureIdle[0];

		game->Vampire[j].attacking = false;
		game->Vampire[j].dying = false;
		game->Vampire[j].dead = true;
	}

	//Vampire Projectiles init (3 projectiles per vamp)
	for (int j = 0; j < maxVampires * 3; j++) {
		game->VampireProjectile[j].position.x = -500.0f;
		game->VampireProjectile[j].position.y = -500.0f;

		game->VampireProjectile[j].drawSize.x = 20.0f * 0.06f; //Size of sprite
		game->VampireProjectile[j].drawSize.y = 20.0f * 0.06f;

		game->VampireProjectile[j].size.x = 20.0f * 0.06f * 0.5f; //Size of collision box
		game->VampireProjectile[j].size.y = 20.0f * 0.06f * 0.5f;

		game->VampireProjectile[j].texture = vampireTextureProjectile;

		game->VampireProjectile[j].friction.x = 0.5f;
		game->VampireProjectile[j].friction.y = 0.5f;

		game->VampireProjectile[j].attacking = false;
		game->VampireProjectile[j].dying = false;
		game->VampireProjectile[j].dead = true;
	}
}
void InitializeGolem(GameState* game) {
	//Golem init

	game->Golem.position.x = -500.0f;
	game->Golem.position.y = -500.0f;

	game->Golem.health = golemHealth;
	
	game->Golem.drawSize.x = 460.0f * 0.025f; //Size of sprite
	game->Golem.drawSize.y = 352.0f * 0.025f;

	game->Golem.size.x = 460.0f * 0.025f * 0.70f; //Size of collision box
	game->Golem.size.y = 352.0f * 0.025f * 0.95f;
	
	game->Golem.drawOffset.x = game->Golem.drawOffset.y = 0.0f;

	game->Golem.texture = golemTextureWalk[0];

	game->Golem.attacking = false;
	game->Golem.dying = false;
	game->Golem.dead = true;
	
	//Golem Projectile init
	game->GolemProjectile.position.x = -500.0f;
	game->GolemProjectile.position.y = -500.0f;

	game->GolemProjectile.drawSize.x = 176.0f * 0.021f; //Size of sprite
	game->GolemProjectile.drawSize.y = 225.0f * 0.021f;

	game->GolemProjectile.size.x = 176.0f * 0.021f * 0.70f; //Size of collision box
	game->GolemProjectile.size.y = 225.0f * 0.021f * 0.80f;

	game->GolemProjectile.drawOffset.x = game->Golem.drawOffset.y = 0.0f;

	game->GolemProjectile.friction.x = 0.5f;
	game->GolemProjectile.friction.y = 0.5f;

	game->GolemProjectile.texture = golemTextureProjectile;

	game->GolemProjectile.attacking = false;
	game->GolemProjectile.dying = false;
	game->GolemProjectile.dead = true;
}


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

	//indicate cave tiles
	caveTiles.clear();
	//1146-1149,1178-1181,...,1274-1277
	int i = 1146;
	while (i <= 1274) {
		for (int j = 0; j < 4; j++) {
			caveTiles.push_back(i + j);
		}
		i += 32;//index width of spritesheet
	}


	//Initialize all entities
	InitializeFox(game);
	InitializeSkeletons(game);
	InitializeVampires(game);
	InitializeGolem(game);

	golemActivated = false;
	golemCutscene = false;

	Mix_PlayMusic(musicLevel1, -1);
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
	for (int i = 128; i <= 511; i++) {
		SolidTile.push_back(i);
	}
	//note the top row of tree is solid for enemies to stand on (512-523)
	for (int i = 512; i <= 523; i++) {
		SolidTile.push_back(i);
	}
	//524-543,556-575,...,876-895. platform tiles
	int i = 524;
	while (i <= 876) {
		for (int j = 0; j < 20; j++) {//20 solid tiles to select in each row of spritesheet
			SolidTile.push_back(i + j);
		}
		i += 32;//index width of spritesheet
	}
	//912-927,944-959,...,1008-1023
	i = 912;
	while (i <= 1008) {
		for (int j = 0; j < 16; j++) {//16 solid tiles to select in each row of spritesheet
			SolidTile.push_back(i + j);
		}
		i += 32;//index width of spritesheet
	}
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

	//Initialize all entities
	InitializeFox(game);
	game->Player.position.x = 5.0f;
	game->Player.position.y = -45.0f;
	InitializeSkeletons(game);
	InitializeSkeletonBoss(game);
	InitializeVampires(game);
	InitializeGolem(game);

	golemActivated = false;
	golemCutscene = false;

	earthquakeTimer = 2.0f; openPortal = false;

	Mix_PlayMusic(musicLevel2, -1);
}
void InitializeLevel3(GameState* game) {
	glClearColor(25.0f / 255.0f, 25.0f / 255.0f, 112.0f / 255.0f, 1.0f);//25,25,112
	int spriteCountX = 32; int spriteCountY = 40;
	//insert into textureCoord vector the uv-vertices of each mapdata to draw
	for (int y = 0; y < map3.mapHeight; y++) {
		for (int x = 0; x < map3.mapWidth; x++) {
			if (map3.mapData[y][x] != -1) {//don't draw the blank tiles. Change it to -1 to fix it
										   // add vertices
				float u = (float)(((int)map3.mapData[y][x]) % spriteCountX) / (float)spriteCountX;
				float v = (float)(((int)map3.mapData[y][x]) / spriteCountX) / (float)spriteCountY;
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
	for (int i = 128; i <= 511; i++) {
		SolidTile.push_back(i);
	}

	//20-27,52,84,116,59,91,123 are solid bolder tiles
	//for (int i = 20; i <= 27; i++) {
	//	SolidTile.push_back(i);
	//}
	std::string solidTiles = "";
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

	//Initialize all entities
	InitializeFox(game);
	InitializeSkeletons(game);
	InitializeVampires(game);
	InitializeGolem(game);

	golemActivated = false;
	golemCutscene = false;

	Mix_PlayMusic(musicLevel3, -1);
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

//Animation Functions~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

int counter = 0;
//const int runIndices[] = {2, 3, 2, 5};//leave out rest sprite
const int runIndices[] = { 1, 2, 3, 4, 5};//leave out rest sprite
//running animation for player
void running(Entity* player) {
	player->sprite.index = runIndices[counter/2];
	counter++;
	if (counter >= 10) {
		counter = 0;
	}
}

//Idle animation for player
const int idleIndices[] = { 0, 1, 2, 1 };
void idle(Entity* player) {
	player->sprite.index = idleIndices[counter / 14];
	counter++;
	if (counter >= 56) {
		counter = 0;
	}
}

int plAttackCounter = 0;
const int attackIndices[] = {12, 13, 14};
void playerAttack(Entity* player) {
	player->sprite.index = attackIndices[plAttackCounter / 3];
	plAttackCounter++;
}

bool playerDeathBool = true;
const int deathIndices[] = { 9, 10, 10, 11, 11, 11 }; //9, 10, 11 = knock down
void playerDie(GameState* game, Entity* player) {
	if (playerDeathBool) {
		playerDeathBool = false;

		//player->velocity.x = -30.0f;
		player->velocity.y = 15.0f;
		player->acceleration.x = -30.0f;//so the player doesn't keep moving right if the right key is pressed (after being pushed left)

		if (!player->playerDeathRight) {
			player->acceleration.x *= -1;
		}
		//if (player->faceOtherWay) {//can't do this b/c if ur facing right when enemy hit from left then you fall left
		//	player->acceleration.x *= -1;
		//}

		counter = 0;
	}

	player->sprite.index = deathIndices[counter/ 10];
	counter++;

	if (counter >= 6 * 10) {
		playerDeathBool = true;
		counter = 0;

		//open gamveover screen
		currentLevel = game->gamemode;
		game->gamemode = GAMEOVER;
	}
}

//Skeleton animations
//idle, 6 frames total
void skeletonIdle(Entity* entity) {
	entity->texture = skeletonTextureIdle[entity->texCounter/10];
	entity->texCounter++;
	if (entity->texCounter >= 60) entity->texCounter = 0;
}
//Walking, 8 frames total
void skeletonWalk(Entity* entity) {
	entity->texture = skeletonTextureWalking[entity->texCounter / 8];
	entity->texCounter++;
	if (entity->texCounter >= 64) entity->texCounter = 0;
}
//Attack, 8 frames 
bool skeletonAttackSoundBool = true;
void skeletonAttack(Entity* entity) {
	//Increase draw size
	entity->drawSize.x = 328.0f * entity->drawRatio;
	entity->drawSize.y = 384.0f * entity->drawRatio;

	entity->size.x = 224.0f * entity->drawRatio * 0.20f;
	//Increase collision for frames with sword
	if (entity->attackCounter > 3 * 8 && entity->attackCounter < 7 * 8) {
		if (skeletonAttackSoundBool) {
			Mix_PlayChannel(-1, skeletonAttackSound, 0);
			skeletonAttackSoundBool = false;
		}
		entity->size.x = 328.0f * entity->drawRatio * 0.85f;
	}

	entity->texture = skeletonTextureAttacking[entity->attackCounter / 8];
	entity->attackCounter++;
	
	//end of attack, reset
	if (entity->attackCounter >= 64) {
		entity->attackCounter = 0;
		entity->attacking = false;
		skeletonAttackSoundBool = true;
		entity->drawSize.x = 224.0f * entity->drawRatio;
		entity->drawSize.y = 364.0f * entity->drawRatio;
		entity->size.x = 224.0f * entity->drawRatio * 0.70f;
	}
}
//Dying, 8 frames total
void skeletonDie(Entity* entity) {
	entity->drawSize.x = 456.0f * entity->drawRatio;
	entity->drawSize.y = 364.0f * entity->drawRatio;

	entity->texture = skeletonTextureDying[entity->dyingCounter / 4];
	entity->dyingCounter++;
	if (entity->dyingCounter >= 32) {
		entity->drawSize.x = 224.0f * entity->drawRatio;
		entity->drawSize.y = 364.0f * entity->drawRatio;
		entity->dead = true;
	}
}


void VampireAttack(GameState* game, Entity* entity);
bool vampAttackBool = true;
//Vampire animations

//attack, 13 frames total
bool vampireAttackSoundBool = true;
void vampireAttack(GameState* game, Entity* entity) {
	//Move to match attack
	if (vampAttackBool) {
		vampAttackBool = false;
		entity->drawOffset.x = -1.56f;
		entity->drawOffset.y = 0.24f;

		if (entity->faceOtherWay) entity->drawOffset.x *= -1;
	}

	//Increase draw size
	entity->drawSize.x = 512.0f * 0.01f;
	entity->drawSize.y = 384.0f * 0.01f;

	entity->texture = vampireTextureAttack[entity->attackCounter / 6];
	entity->attackCounter++;

	if (entity->attackCounter == 4 * 6) {
		//Vampire throws projectile
		if (vampireAttackSoundBool) {
			Mix_PlayChannel(-1, vampireAttackSound, 0);
			vampireAttackSoundBool = false;
		}
		VampireAttack(game, entity);
	}

	//end of attack, reset
	if (entity->attackCounter >= 13 * 6) {
		entity->attackCounter = 0;
		entity->attacking = false;

		entity->texture = vampireTextureIdle[0];

		entity->drawSize.x = 188.0f * 0.01f;
		entity->drawSize.y = 336.0f * 0.01f;

		entity->drawOffset.x = 0.0f;
		entity->drawOffset.y = 0.0f;

		vampAttackBool = true;
		vampireAttackSoundBool = true;
	}
}
//idle, 8 frames total
void vampireIdle(Entity* entity) {
	entity->texture = vampireTextureIdle[entity->texCounter / 10];
	entity->texCounter++;
	if (entity->texCounter >= 8 * 10) entity->texCounter = 0;
}
//death, 12 frames total 
bool vampDeathBool = true;
void vampireDie(Entity* entity) {
	if (vampDeathBool) {
		vampDeathBool = false;
		entity->drawOffset.y = 0.24f;
		entity->drawOffset.x = -0.1f;

		if (entity->faceOtherWay) entity->drawOffset.x *= -1;
	}

	entity->drawSize.x = 332.0f * 0.01f;
	entity->drawSize.y = 380.0f * 0.01f;

	entity->texture = vampireTextureDying[entity->dyingCounter / 3];
	entity->dyingCounter++;

	//Dead, reset
	if (entity->dyingCounter >= 12 * 3) {
		entity->drawSize.x = 188.0f * 0.01f;
		entity->drawSize.y = 336.0f * 0.01f;
		entity->dyingCounter = 0;

		entity->drawOffset.y = 0.0f;
		entity->drawOffset.x = 0.0f;

		vampDeathBool = true;

		entity->dead = true;
	}
}

//Golem Animations
//idle, 6 frames total
void golemIdle(Entity* entity) {
	entity->texture = golemTextureWalk[entity->texCounter / 10];
	entity->texCounter++;
	if (entity->texCounter >= 6 * 10) entity->texCounter = 0;
}

//attack, 6 frames total
bool golemAttackBool = true;
bool golemAttackSoundBool = true;
float golemLastAttack = 0;
void golemAttack(GameState* game, Entity* entity) {
	
	//Move to match attack
	if (golemAttackBool) {
		golemAttackBool = false;
		entity->drawOffset.x = -0.81f;
		entity->drawOffset.y = 0.28f;

		if (entity->faceOtherWay) entity->drawOffset.x *= -1;
	}

	//Increase draw size
	entity->drawSize.x = 528.0f * 0.025f;
	entity->drawSize.y = 372.0f * 0.025f;

	entity->texture = golemTextureAttack[entity->attackCounter / 10];
	entity->attackCounter++;

	//golem throws projectile
	//x velocity based on distance to player
	float difference = game->Player.position.x - entity->position.x;
	//Throwing
	if (entity->attackCounter == 3 * 10) {
		if (golemAttackSoundBool) {
			Mix_PlayChannel(-1, golemAttackSound, 0);
			golemAttackSoundBool = false;
		}

		game->GolemProjectile.dead = false;
		
		if (entity->faceOtherWay) game->GolemProjectile.position.x = entity->position.x + 5.0f;
		else game->GolemProjectile.position.x = entity->position.x - 5.0f;
		game->GolemProjectile.position.y = entity->position.y + 1.5f;

		//choose velocity based on distance
		if(abs(difference) > 15.0f) game->GolemProjectile.velocity.x = -17.0f;
		else if (abs(difference) > 10.0f) game->GolemProjectile.velocity.x = -10.0f;
		else game->GolemProjectile.velocity.x = -5.0f;
		game->GolemProjectile.velocity.y = 1.8f;
		game->GolemProjectile.acceleration.y = -gravity * 0.85f;

		if (entity->faceOtherWay) {
			game->GolemProjectile.velocity.x *= -1;
		}
	}

	//end of attack, reset
	if (entity->attackCounter >= 6 * 10) {
		entity->attackCounter = 0;
		entity->attacking = false;

		entity->texture = golemTextureWalk[0];

		entity->drawSize.x = 460.0f * 0.025f; //Size of sprite
		entity->drawSize.y = 352.0f * 0.025f;

		entity->drawOffset.x = 0.0f;
		entity->drawOffset.y = 0.0f;

		golemLastAttack = 0;
		golemAttackBool = true;
		golemAttackSoundBool = true;
	}
}

//jump if player is too far
bool jumpToLeftSide = false; bool jumpToRightSide; bool golemRight = true; bool golemLeft = false;
void golemJump(GameState* game, Entity* entity) {

	//jump if player is too far left of golem
	float golemRightDist = entity->position.x - game->Player.position.x;

	if (golemRightDist >= 20.0f) {//use 20.0f
		jumpToLeftSide = true;
	}
	if (jumpToLeftSide) {
		//player is too far left of the golem, the golem jumps left.
		if (entity->collidedBottom) {
			entity->velocity.y = 60.0f;
			entity->collidedBottom = false;
		}
		
		if((golemRightDist >= -1.0f - game->Golem.size.x/2.0f) && (game->Golem.position.x - game->Golem.size.x/2.0f>= 0.0f)) {//keep moving left until golem position is 2.0f to the left of player. The right side is to make sure the golem doesn't fall of stage
			entity->acceleration.x = -500.0f;
		}
		else {
			entity->acceleration.x = 0.0f;
			jumpToLeftSide = false;
		}
	}

	//jump if player is too far right of golem
	float golemLeftDist = game->Player.position.x - entity->position.x;
	if (golemLeftDist >= 20.0f) {//use 20.0f
		jumpToRightSide = true;
	}
	if (jumpToRightSide) {
		//player is too far left of the golem, the golem jumps left.
		if (entity->collidedBottom) {
			entity->velocity.y = 60.0f;
			entity->collidedBottom = false;
		}

		if ((golemLeftDist >= -1.0f - game->Golem.size.x / 2.0f) && (game->Golem.position.x + game->Golem.size.x / 2.0f <= 150.0f)) {//keep moving left until golem position is 2.0f to the left of player
			entity->acceleration.x = 500.0f;
		}
		else {
			entity->acceleration.x = 0.0f;
			jumpToRightSide = false;
		}
	}

	//adjust bool for camera angle
	if (golemRightDist >= 0) {
		golemRight = true;
		golemLeft = false;
	}
	else {
		golemRight = false;
		golemLeft = true;
	}

}

//appear, 15 frames total
bool golemAppearDeathBool = true;
void golemAppear(Entity* entity) {
	//Move to match animation
	if (golemAppearDeathBool) {

		golemAppearDeathBool = false;
		entity->drawOffset.x = 0.0f;
		entity->drawOffset.y = 0.0f;

		if (entity->faceOtherWay) entity->drawOffset.x *= -1;
	}

	Mix_PlayChannel(-1, golemSpawn, 0);

	//Increase draw size
	entity->drawSize.x = 456.0f * 0.025f;
	entity->drawSize.y = 348.0f * 0.025f;

	entity->texture = golemTextureAppear[entity->dyingCounter / 12];
	entity->dyingCounter++;

	//end of appear animation, reset
	if (entity->dyingCounter >= 15 * 12) {
		entity->dyingCounter = 0;

		entity->texture = golemTextureWalk[0];

		entity->drawSize.x = 460.0f * 0.025f; //Size of sprite
		entity->drawSize.y = 352.0f * 0.025f;

		entity->drawOffset.x = 0.0f;
		entity->drawOffset.y = 0.0f;

		golemAppearDeathBool = true;

		golemCutscene = false;
	}
}

//death, 7 frames total
void golemDie(Entity* entity) {
	//Move to match animation
	if (golemAppearDeathBool) {
		golemAppearDeathBool = false;
		entity->drawOffset.x = 0.0f;
		entity->drawOffset.y = 1.9f;

		Mix_HaltMusic();
		Mix_PlayChannel(-1, gameVictory, 0);

		if (entity->faceOtherWay) entity->drawOffset.x *= -1;
	}


	//Increase draw size
	entity->drawSize.x = 456.0f * 0.025f;
	entity->drawSize.y = 504.0f * 0.025f;

	entity->texture = golemTextureDie[entity->dyingCounter / 15];
	entity->dyingCounter++;

	//end of appear animation, reset
	if (entity->dyingCounter >= 7 * 15) {
		entity->dyingCounter = 0;

		entity->texture = golemTextureWalk[0];

		entity->drawSize.x = 460.0f * 0.025f; //Size of sprite
		entity->drawSize.y = 352.0f * 0.025f;

		entity->drawOffset.x = 0.0f;
		entity->drawOffset.y = 0.0f;

		golemAppearDeathBool = true;

		entity->dead = true;
	}
}
//Animation Functions END~~~~~~~~~~~~~~~~~~~~~~~~~~

void ThrowMushroom(GameState* game);

void ProcessInputMenu(GameState* game, float elapsed) {
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
	if (keys[SDL_SCANCODE_SPACE]) {
		game->gamemode = LEVEL1;
		//clear the draw vertices data
		game->vertexData.clear();
		game->texCoordData.clear();
		//initialize game with new data
		InitializeLevel1(game);
	}
	if (keys[SDL_SCANCODE_ESCAPE]) {
		closeWindow = true;
	}
}
void ProcessInputGame(GameState* game, float elapsed) {
	if (keys[SDL_SCANCODE_D]) {
		game->Player.dead = true;
		game->Player.playerDeathRight = !(game->Player.faceOtherWay);//player falls backwards instead of always going right
	}
	if (keys[SDL_SCANCODE_M]) game->gamemode = MAINMENU;

	//If player not currently attacking
	if (!game->Player.attacking && !game->Player.dead && !golemCutscene) {
		//Can only attack when attack timer is up
		if (keys[SDL_SCANCODE_SPACE] && playerLastShot >= playerAttackTimer) {
			game->Player.attacking = true;
			playerLastShot = 0.0f;
			Mix_PlayChannel(-1, mushroomThrow, 0);
		}

		if (keys[SDL_SCANCODE_LEFT]) {
			//game->Player.velocity.x = -playerSpeed;
			game->Player.acceleration.x = -playerAcceleration;
			game->Player.faceOtherWay = true;
		}
		if (keys[SDL_SCANCODE_RIGHT]) {
			//game->Player.velocity.x = playerSpeed;
			game->Player.acceleration.x = playerAcceleration;
			game->Player.faceOtherWay = false;
			if ((game->gamemode == LEVEL2) && (game->Player.MoveTo3 == true) && (game->Skeleton[0].dead)) {
				game->Player.MoveTo3 = false;
				game->gamemode = LEVEL3;

				Mix_PlayChannel(-1, levelVictory, 0);

				game->vertexData.clear();
				game->texCoordData.clear();
				InitializeLevel3(game);
			}
		}
		//If player is standing on block (collidedBottom)
		if (keys[SDL_SCANCODE_UP]) {
			if ((game->gamemode == LEVEL1) && game->Player.collidedCave) {//move on to level 2
				game->gamemode = LEVEL2;

				Mix_PlayChannel(-1, levelVictory, 0);

				game->vertexData.clear();
				game->texCoordData.clear();
				InitializeLevel2(game);
				game->Player.collidedBottom = false;
			}
			else if (game->Player.collidedBottom) {//player jumps
				if (cheats) {
					game->Player.velocity.y = 10.0f;
				}
				else {
					Mix_PlayChannel(-1, playerJump, 0);
					game->Player.velocity.y = jumpSpeed;
					game->Player.collidedBottom = false;
				}
			}
		}
		if (keys[SDL_SCANCODE_DOWN]) {
			if (cheats) {
				game->Player.velocity.y = -10.0f;
			}
		}

		//if standing still play idle animation, set acceleration = 0
		if (!(keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_UP])) {
			game->Player.acceleration.x = 0;
			idle(&(game->Player));
		}
		else {//going left or right, so do running animation
			running(&(game->Player));
		}
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
	if (keys[SDL_SCANCODE_ESCAPE]) {
		currentLevel = game->gamemode;
		game->gamemode = PAUSE;
	}
}

void ProcessInputPause(GameState* game, float elapsed) {
	if (keys[SDL_SCANCODE_M]) {//return to menu 
		game->gamemode = MAINMENU; 
		game->vertexData.clear();
		game->texCoordData.clear();
	}
	if (keys[SDL_SCANCODE_R]) {//resume game
		game->gamemode = currentLevel;
	}
}

void ProcessInputGameOver(GameState* game, float elapsed) {
	if (keys[SDL_SCANCODE_M]) {//return to menu 
		game->gamemode = MAINMENU;
		game->vertexData.clear();
		game->texCoordData.clear();
	}
	if (keys[SDL_SCANCODE_R]) {//restart level
		game->gamemode = currentLevel;
		if (currentLevel == LEVEL1) { InitializeLevel1(game); }
		if (currentLevel == LEVEL2) { InitializeLevel2(game); }
		if (currentLevel == LEVEL3) { InitializeLevel3(game); }
	}
}

void ProcessInput(GameState* game, float elapsed) {
	switch (game->gamemode) {
	case MAINMENU:
		ProcessInputMenu(game, elapsed);
		break;
	case LEVEL1:
		ProcessInputGame(game, elapsed);
		break;
	case LEVEL2:
		ProcessInputGame(game, elapsed);
		break;
	case LEVEL3:
		ProcessInputGame(game, elapsed);
		break;
	case PAUSE:
		ProcessInputPause(game, elapsed);
		break;
	case GAMEOVER:
		ProcessInputGameOver(game, elapsed);
	}
}

//Update player position, velocity, & acceleration
//Added gravity + friction, also has jump and fall sprite changes
//Added attack animation
void UpdatePlayerMovement(GameState* game, float elapsed) {
	if (cheats) {
		playerAcceleration = 150.0f;
		friction_x = 2.0f;
		//friction_y = 2.0f;
	}
	else {
		playerAcceleration = 130.0f;
		friction_x = 12.0f;

	}
	game->Player.position.x += game->Player.velocity.x * elapsed;
	game->Player.position.y += game->Player.velocity.y * elapsed;

	game->Player.velocity.x = lerp(game->Player.velocity.x, 0.0f, elapsed * friction_x);
	game->Player.velocity.y = lerp(game->Player.velocity.y, 0.0f, elapsed * friction_y);

	game->Player.velocity.x += game->Player.acceleration.x * elapsed;
	game->Player.velocity.y += game->Player.acceleration.y * elapsed;

	if (!cheats) {
		game->Player.velocity.y += gravity * elapsed;
	}
	
	//Jump up and fall down sprites
	if (game->Player.velocity.y > 10.0 && !game->Player.collidedBottom) game->Player.sprite.index = 7;
	else if (game->Player.velocity.y > 2.5 && !game->Player.collidedBottom) game->Player.sprite.index = 6;
	else if (game->Player.velocity.y < -6.0 && !game->Player.collidedBottom) game->Player.sprite.index = 8;
	else if (!game->Player.collidedBottom) game->Player.sprite.index = 4;

	//attacking
	if (game->Player.attacking) {
		//If player attacking, slow down
		game->Player.acceleration.x *= 0.7;

		//attack animation
		playerAttack(&(game->Player));

		//Finished attack animation
		if (plAttackCounter >= 9) {
			plAttackCounter = 0;
			game->Player.attacking = false;

			//throw mushroom
			ThrowMushroom(game);

			//Cycle mushroom textures
			game->Mushroom.texture = mushroomTextures[game->Mushroom.texCounter];
			game->Mushroom.texCounter++;
			if (game->Mushroom.texCounter >= 3) game->Mushroom.texCounter = 0;
		}
	}
}

void UpdateEntityMovement(Entity* entity, float elapsed) {
	entity->position.x += entity->velocity.x * elapsed;
	entity->position.y += entity->velocity.y * elapsed;

	entity->velocity.x = lerp(entity->velocity.x, 0.0f, elapsed * entity->friction.x);
	entity->velocity.y = lerp(entity->velocity.y, 0.0f, elapsed * entity->friction.y);

	entity->velocity.x += entity->acceleration.x * elapsed;
	entity->velocity.y += entity->acceleration.y * elapsed;

	entity->velocity.y += gravity * elapsed;
}


//Skeleton behavior
void ChasingEntityBehavior(GameState* game, Entity* entity) {
	//Range the enemy will detect and chase player, and attack
	float detectRange = 14.0f; float detectRangeY = 5.0f;
	float attackRange = 3.0f; float attackRangeY = 5.0f;
	if (entity->skeletonBossAttackRange) { attackRange = entity->skeletonBossAttackRange; }//give the skeletonboss a bigger range

	float difference = game->Player.position.x - entity->position.x;
	float differenceY = abs(game->Player.position.y - entity->position.y);
	//If close enough to attack
	if (differenceY <= attackRangeY) {
		if ((difference > 0 && difference < attackRange) || (difference < 0 && -difference < attackRange)) {
			entity->attacking = true;
		}
		//comeback
		//attacking

		else {//chasing
			//If player on right
			if (difference > 0 && difference < detectRange && differenceY < detectRangeY) {
				entity->acceleration.x = 100.0f;
				entity->faceOtherWay = true;

				skeletonWalk(entity);
			}
			//If player on left
			else if (difference < 0 && -difference < detectRange && differenceY < detectRangeY) {
				entity->acceleration.x = -100.0f;
				entity->faceOtherWay = false;

				skeletonWalk(entity);
			}
			else {
				entity->acceleration.x = 0.0f;
				skeletonIdle(entity);
			}
		}
	}
}

//Spawn skeleton certain distance away from player
float skeletonLastSpawn = 4.0f;
float skeletonSpawnTimer = 4.0f;

void SpawnSkeleton(GameState* game, float elapsed) {
	if (game->gamemode != LEVEL3) {
		skeletonLastSpawn += elapsed;
		if (skeletonLastSpawn >= skeletonSpawnTimer) {
			skeletonLastSpawn = 0.0f;
			for (i = 1; i < maxSkeletons; i++) {
				if (game->Skeleton[i].dead) {
					game->Skeleton[i].position.x = game->Player.position.x + 20.0f;
					game->Skeleton[i].position.y = game->Player.position.y + 3.5f;
					game->Skeleton[i].attacking = false;
					game->Skeleton[i].dead = false;

					break;
				}
			}
		}
	}
	else {
		//level 3 skeletons spawn from boss
		skeletonLastSpawn += elapsed;
		if ((skeletonLastSpawn >= skeletonSpawnTimer)&&(!golemCutscene)&&(game->Golem.collidedBottom)) {
			skeletonLastSpawn = 0.0f;
			for (i = 1; i < maxSkeletons; i++) {
				if (game->Skeleton[i].dead) {
					game->Skeleton[i].position.x = game->Golem.position.x;
					game->Skeleton[i].position.y = game->Golem.position.y;
					game->Skeleton[i].attacking = false;
					game->Skeleton[i].dead = false;
					break;
				}
			}
		}
	}
}

float vampAttackRange = 12.0f;
//Vampire behavior
void VampireBehavior(GameState* game, Entity* entity) {
	float difference = game->Player.position.x - entity->position.x;

	//If player on right and close enough to attack
	if (difference > 0 && difference < vampAttackRange) {
		entity->attacking = true;
		entity->faceOtherWay = true;
	}
	//If player on left
	else if (difference < 0 && -difference < vampAttackRange) {
		entity->attacking = true;
		entity->faceOtherWay = false;
	}
	else {
		vampireIdle(entity);
	}
}
//Vampire throw projectile, pick an unused projectile
void VampireAttack(GameState* game, Entity* entity) {
	for (i = 0; i < maxVampires * 3; i++) {
		if (game->VampireProjectile[i].dead) {
			game->VampireProjectile[i].dead = false;
			game->VampireProjectile[i].position.x = entity->position.x;
			game->VampireProjectile[i].position.y = entity->position.y;

			game->VampireProjectile[i].velocity.x = -15.0f;
			game->VampireProjectile[i].velocity.y = 2.5f;
			game->VampireProjectile[i].acceleration.y = -gravity * 0.9f;

			if (entity->faceOtherWay) {
				game->VampireProjectile[i].velocity.x *= -1;
			}

			break;
		}
	}
}

//Spawn Vampire every set time
float vampireLastSpawn = 0.0f;
float vampireSpawnTimer = 8.0f;

void SpawnVampire(GameState* game, float elapsed) {
	vampireLastSpawn += elapsed;
	if (vampireLastSpawn >= vampireSpawnTimer) {
		vampireLastSpawn = 0.0f;
		for (i = 0; i < maxVampires; i++) {
			if (game->Vampire[i].dead) {
				game->Vampire[i].position.x = game->Player.position.x + 20.0f;
				game->Vampire[i].position.y = game->Player.position.y + 3.5f;
				game->Vampire[i].attacking = false;
				game->Vampire[i].dead = false;

				break;
			}
		}
	}
}


//Golem behavior
//Always goes towards player slowly
//attacks are time, velocity based on how far away player is
void GolemBehavior(GameState* game, Entity* entity) {
	float difference = game->Player.position.x - entity->position.x;

	//Golem attack based on timer
	if (!golemCutscene) {
		if (golemLastAttack > 2.5f) {
			entity->attacking = true;
			golemAttack(game, entity);
		}

		else {
			//If player on right
			if (difference > 0) {
				entity->acceleration.x = 10.0f;
				entity->faceOtherWay = true;

				golemIdle(entity);
			}
			//If player on left
			else if (difference < 0) {
				entity->acceleration.x = -10.0f;
				entity->faceOtherWay = false;

				golemIdle(entity);
			}
			else golemIdle(entity);
		}
	}
	else {
		entity->acceleration = 0.0f;
	}
}

void ThrowMushroom(GameState* game) {
	game->Mushroom.dead = false;
	game->Mushroom.position.x = game->Player.position.x;
	game->Mushroom.position.y = game->Player.position.y;

	game->Mushroom.rotation = -45.0f * (3.141592f / 180.0f);
	game->Mushroom.velocity.x = 35.0f;
	game->Mushroom.velocity.y = 18.0f;

	if (game->Player.faceOtherWay) { 
		game->Mushroom.velocity.x *= -1; 
		game->Mushroom.rotation *= -1;
		game->Mushroom.rotationFactor = 2.0f * (3.141592f / 180.0f);
	}
	else {
		game->Mushroom.rotationFactor = -2.0f * (3.141592f / 180.0f);
	}
}

//Handles collision with level map
bool EntityTileMapCollision(Entity* entity, FlareMap* map) {
	bool ret = false;
	int gridX; int gridY;
	float playerHalfSizeY = entity->size.y / 2.0f;
	float playerHalfSizeX = entity->size.x / 2.0f;
	//find the bottom of the player and then convert that coord to tiled
	float playerLeft = entity->position.x - playerHalfSizeX;
	float playerRight = entity->position.x + playerHalfSizeX;
	float playerTop = entity->position.y + playerHalfSizeY;
	float playerBottom = entity->position.y - playerHalfSizeY;
	//My solid floor tiles indices: 128-139, which we will indicate in IsSolid vector.

	//Level 2 going to the right end while above the top platform = move on to level 3
	worldToTileCoordinates(playerRight, entity->position.y, &gridX, &gridY);
	if (gridX > map->mapWidth - 1 && gridY < 30) {
		entity->MoveTo3 = true;//set flag for transition to level 3
	}

	//collsion on the bottom of player
	worldToTileCoordinates(entity->position.x, playerBottom, &gridX, &gridY);//this is the center bottom of the player, but how do we check other tiles of player bottom??? question. Use for loop: -player.size.x/2 to player.size.x/2 in tile coords. nTiles = player.size.x/tileSize;
	//note the return value of gridX,gridY corresponds to which tile (defined in row=gridX,col=gridY) is in touch with the player's bottom.
	if (gridX < map->mapWidth && gridX >= 0 && gridY < map->mapHeight && gridY >= 0) {//if the player is still within Level bounds. This check is needed b/c otherwise map.mapData[y][x] would be accessing outside of it's array
		for (int i = 0; i < SolidTile.size(); i++) {
			if (map->mapData[gridY][gridX] == SolidTile[i]) {//COLLISION on the bottom of player(if the playerBottom's grid position corresponds to the solid tile). Resolve it in terms of world coords
				float tileLeft = (float)gridX * tileSize;
				float tileRight = ((float)gridX + 1.0f) * tileSize;
				float tileTop = (float)(-gridY) * tileSize;
				float tileBottom = ((float)(-gridY) - 1.0f) * tileSize;
				//This check is useful if the player is larger than 1tilex1tile. ??? This actually is useless b/c we already saw a collision. But note that the player's size is based on 1 tile so no need for this.
				if (!(playerBottom > tileTop || playerTop < tileBottom || playerLeft > tileRight || playerRight < tileLeft)) {//collision!
					float penetrationY = fabs(playerBottom - tileTop);
					entity->position.y += penetrationY + 0.001f;
					if (entity->velocity.y <= 0) {//do this check to see if the player jumped, so we don't set the jump velocity to 0
						entity->velocity.y = 0.0f;//why do we need this???? Ans: So the player doesn't keep going down as you're moving and colliding. i.e. w/o this, the gravity's acceleration will keep making your velocity get bigger negatively so even with the penetration fix, the velocity*elapsed will make it go down more and more each tick.
					}
					entity->collidedBottom = true;
					ret =  true;
				}
			}
		}
	}

	//collision on the top of player
	worldToTileCoordinates(entity->position.x, playerTop, &gridX, &gridY);
	if (gridX < map->mapWidth && gridX >= 0 && gridY < map->mapHeight && gridY >= 0) {//if player's top side is still in level bounds
		for (int i = 0; i < SolidTile.size(); i++) {
			if (map->mapData[gridY][gridX] == SolidTile[i]) {
				//COLLISION on the top of player
				float tileLeft = (float)gridX * tileSize;
				float tileRight = ((float)gridX + 1.0f) * tileSize;
				float tileTop = (float)(-gridY) * tileSize;
				float tileBottom = ((float)(-gridY) - 1.0f) * tileSize;
				entity->velocity.y = 0.0f;
				float penetrationY = fabs(playerTop - tileBottom);
				//entity->position.x -= (penetrationY + 0.001f);
				entity->position.y -= (penetrationY + 0.00001f);//the player shakes because the "jump" has a big velocity so it moves up quite a bit at each frame.

				entity->collidedTop = true;

				ret = true;
			}
		}
	}

	//collision on the right side of player
	worldToTileCoordinates(playerRight, entity->position.y, &gridX, &gridY);
	if (gridX < map->mapWidth && gridX >= 0 && gridY < map->mapHeight && gridY >= 0) {//if player's right side is still in level bounds
		for (int i = 0; i < SolidTile.size(); i++) {
			if (map->mapData[gridY][gridX] == SolidTile[i]) {//COLLISION on the right of player
				float tileLeft = (float)gridX * tileSize;
				float tileRight = ((float)gridX + 1.0f) * tileSize;
				float tileTop = (float)(-gridY) * tileSize;
				float tileBottom = ((float)(-gridY) - 1.0f) * tileSize;

				float penetrationX = fabs(playerRight - tileLeft);
				entity->position.x -= (penetrationX + 0.001f);
				//entity->velocity.x = 0.0f;
				entity->collidedRight = true;

				ret = true;
			}
		}
	}

	//collision on the left side of player
	worldToTileCoordinates(playerLeft, entity->position.y, &gridX, &gridY);
	if (gridX < map->mapWidth && gridX >= 0 && gridY < map->mapHeight && gridY >= 0) {//if player's left side is still in level bounds
		for (int i = 0; i < SolidTile.size(); i++) {
			if (map->mapData[gridY][gridX] == SolidTile[i]) {//COLLISION on the left of player
				float tileLeft = (float)gridX * tileSize;
				float tileRight = ((float)gridX + 1.0f) * tileSize;
				float tileTop = (float)(-gridY) * tileSize;
				float tileBottom = ((float)(-gridY) - 1.0f) * tileSize;

				float penetrationX = fabs(playerLeft - tileRight);
				entity->position.x += penetrationX + 0.001f;
				//entity->velocity.x = 0.0f;
				entity->collidedLeft = true;

				ret = true;

			}
		}
	}

	//collision with cave
	worldToTileCoordinates(entity->position.x, entity->position.y, &gridX, &gridY);
	if (gridX < map->mapWidth && gridX >= 0 && gridY < map->mapHeight && gridY >= 0) {//if player's left side is still in level bounds
		for (int i = 0; i < caveTiles.size(); i++) {
			if (map->mapData[gridY][gridX] == caveTiles[i]) {//in front of cave
				entity->collidedCave = true;
				entity->collidedCaveArrow = true;
			}
			//else {
			//	entity->collidedCave = false;//doesn't work b/c there are other cave tiles that you are not colliding with.
			//}
		}
	}
	//entity->collidedCave = true;

	return ret;
}

//Returns true if 2 entities collide
bool EntityCollision(Entity* entity1, Entity* entity2) {
	//Entity1 boundaries
	float e1Left = entity1->position.x - (entity1->size.x / 2);
	float e1Right = entity1->position.x + (entity1->size.x / 2);
	float e1Top = entity1->position.y + (entity1->size.y / 2);
	float e1Bot = entity1->position.y - (entity1->size.y / 2);

	//Entity2 boundaries
	float e2Left = entity2->position.x - (entity2->size.x / 2);
	float e2Right = entity2->position.x + (entity2->size.x / 2);
	float e2Top = entity2->position.y + (entity2->size.y / 2);
	float e2Bot = entity2->position.y - (entity2->size.y / 2);

	//Collision check
	if (!(e1Bot > e2Top) && !(e1Top < e2Bot) && !(e1Left > e2Right) && !(e1Right < e2Left)) {
		return true;
	}
	return false;
}

//Individual entity update functions
void UpdatePlayer(GameState* game, float elapsed, FlareMap& map) {
	//Update Player
	UpdatePlayerMovement(game, elapsed);
	EntityTileMapCollision(&(game->Player), &map);

	//player fell off
	if (game->Player.position.y <= -30.0f && (game->gamemode == LEVEL1 || game->gamemode == LEVEL3)) {
		game->Player.dead = true;
	}
	if (game->Player.position.y <= -60.0f && game->gamemode == LEVEL2) {
		game->Player.dead = true;
	}

	playerLastShot += elapsed;

	if (cheats) {
		game->Player.dead = false;
		game->Player.collidedBottom = true;//unlimited jump
	}
	if (game->Player.dead) {
		playerDie(game, &(game->Player));
	}


	//Update Mushroom
	if (!game->Mushroom.dead) {
		UpdateEntityMovement(&(game->Mushroom), elapsed);
		game->Mushroom.rotation += game->Mushroom.rotationFactor;

		if (EntityTileMapCollision(&(game->Mushroom), &map)) {
			game->Mushroom.position.x = game->Mushroom.position.y = -500.0f;
			game->Mushroom.velocity.x = game->Mushroom.velocity.y = 0.0f;

			game->Mushroom.dead = true;
			//play Mushroom hit sound
			Mix_PlayChannel(-1, mushroomHit, 0);
		}
	}

	//Can't press up to enter next level until touching cave again
	if (game->Player.collidedCave == true) {
		collidedCaveCounter += elapsed;
		if (collidedCaveCounter >= 0.1f) {
			game->Player.collidedCave = false;
			collidedCaveCounter = 0.0f;
		}
	}

	//if you crossed right exit but go back to kill skeleton, then you have to go back to the right exit to move to level 3 (i.e. initially if u go to right exit then kill skeleton you move on to level 3
	if (game->Player.MoveTo3 == true) {
		MoveTo3Counter += elapsed;
		if (MoveTo3Counter >= 0.1f) {
			game->Player.MoveTo3 = false;
			MoveTo3Counter = 0.0f;
		}
	}

	if (game->Player.position.x < (150.0f - 10.0f)*tileSize) {
		game->Player.collidedCaveArrow = false;
	}

}
void UpdateSkeletons(GameState* game, float elapsed, FlareMap& map) {
	//Update Skeleton
	for (i = 0; i < maxSkeletons; i++) {
		//for every live skeleton
		if (!game->Skeleton[i].dying && !game->Skeleton[i].dead) {
			UpdateEntityMovement(&(game->Skeleton[i]), elapsed);
			EntityTileMapCollision(&(game->Skeleton[i]), &map);

			//If not attacking, see if can chase
			if (!game->Skeleton[i].attacking) ChasingEntityBehavior(game, (&game->Skeleton[i]));
			//Attacking
			else {
				//Slows down
				game->Skeleton[i].acceleration.x *= 0.95f;
				skeletonAttack(&(game->Skeleton[i]));
			}

			//Check Mushroom Skeleton collision
			if (EntityCollision(&(game->Mushroom), &(game->Skeleton[i]))) {
				Mix_PlayChannel(-1, mushroomHit2, 0);
				game->Mushroom.position.x = game->Mushroom.position.y = -500.0f;
				game->Mushroom.velocity.x = game->Mushroom.velocity.y = 0.0f;
				if (i == 0) {
					game->Skeleton[0].health -= 1;
					if (game->Skeleton[0].health == 0) {
						game->Skeleton[0].dying = true;
					}

					//Knockback
					game->Skeleton[0].velocity.x = 5.0f;
					game->Skeleton[0].velocity.y = 7.0f;
					if (game->Skeleton[0].faceOtherWay) game->Skeleton[0].velocity.x *= -1;

					break;
				}
				game->Skeleton[i].dying = true;

			}

			//Check Player Skeleton collision
			if (EntityCollision(&(game->Player), &(game->Skeleton[i]))) {
				Mix_PlayChannel(-1, playerHit, 0);
				game->Player.dead = true;
				//if (cheats) game->Player.dead = false;
				if (game->Player.position.x < game->Skeleton[i].position.x) game->Player.playerDeathRight = true;
				else game->Player.playerDeathRight = false;
			}

			//Check if skeleton fell off
			if (game->Skeleton[i].position.y <= -30.0f && (game->gamemode == LEVEL1 || game->gamemode == LEVEL3)) {
				game->Skeleton[i].dead = true;
			}
			if (game->Skeleton[i].position.y <= -60.0f && game->gamemode == LEVEL2) {
				game->Skeleton[i].dead = true;
			}
		}

		//Check dying
		if (game->Skeleton[i].dying) {
			game->Skeleton[i].velocity.x = game->Skeleton[i].velocity.y = 0.0f;
			game->Skeleton[i].acceleration.x = game->Skeleton[i].acceleration.y = 0.0f;

			skeletonDie(&(game->Skeleton[i]));
		}

		//Check dead
		if (game->Skeleton[i].dead) {
			game->Skeleton[i].attacking = false;
			game->Skeleton[i].dying = false;
			game->Skeleton[i].position.x = game->Skeleton[i].position.y = -500.0f;

			game->Skeleton[i].drawSize.x = 188.0f * 0.01f; //Size of sprite
			game->Skeleton[i].drawSize.y = 336.0f * 0.01f;

			game->Skeleton[i].size.x = 188.0f * 0.01f * 0.70f; //Size of collision box
			game->Skeleton[i].size.y = 336.0f * 0.01f * 0.95f;

			game->Skeleton[i].dyingCounter = 0;
		}
	}
}
void UpdateVampires(GameState* game, float elapsed, FlareMap& map) {
	//Update Vampires
	for (i = 0; i < maxVampires; i++) {
		if (!game->Vampire[i].dying && !game->Vampire[i].dead) {
			UpdateEntityMovement(&(game->Vampire[i]), elapsed);
			EntityTileMapCollision(&(game->Vampire[i]), &map);

			//If not attacking, see if in range to attack
			if (!game->Vampire[i].attacking) VampireBehavior(game, (&game->Vampire[i]));
			//Attacking
			else vampireAttack(game, &(game->Vampire[i]));

			//Check Player Vampire collision
			if (EntityCollision(&(game->Player), &(game->Vampire[i]))) {
				Mix_PlayChannel(-1, playerHit, 0);
				game->Player.dead = true;
				if (game->Player.position.x < game->Vampire[i].position.x) game->Player.playerDeathRight = true;
				else game->Player.playerDeathRight = false;
			}

			//Check Mushroom Vampire collision
			if (EntityCollision(&(game->Mushroom), &(game->Vampire[i]))) {
				Mix_PlayChannel(-1, mushroomHit2, 0);
				game->Vampire[i].dying = true;

				game->Mushroom.position.x = game->Mushroom.position.y = -500.0f;
				game->Mushroom.velocity.x = game->Mushroom.velocity.y = 0.0f;
			}

			//Check if Vampire fell off
			if (game->Vampire[i].position.y <= -30.0f && (game->gamemode == LEVEL1 || game->gamemode == LEVEL3)) {
				game->Vampire[i].dead = true;
			}
			if (game->Vampire[i].position.y <= -60.0f && game->gamemode == LEVEL2) {
				game->Vampire[i].dead = true;
			}
		}

		//Check dying
		if (game->Vampire[i].dying) {
			vampireDie(&(game->Vampire[i]));
		}

		//Check dead
		if (game->Vampire[i].dead) {
			game->Vampire[i].attacking = false;
			game->Vampire[i].dying = false;
			game->Vampire[i].position.x = game->Vampire[i].position.y = -500.0f;

			game->Vampire[i].drawSize.x = 188.0f * 0.01f;
			game->Vampire[i].drawSize.y = 336.0f * 0.01f;

			game->Vampire[i].size.x = 188.0f * 0.01f * 0.70f; //Size of collision box
			game->Vampire[i].size.y = 336.0f * 0.01f * 0.95f;

			game->Vampire[i].dyingCounter = 0;
		}
	}

	//Update vampire projectile
	for (i = 0; i < maxVampires; i++) {
		if (!game->VampireProjectile[i].dead) {
			UpdateEntityMovement(&(game->VampireProjectile[i]), elapsed);

			//Check collision with ground
			if (EntityTileMapCollision(&(game->VampireProjectile[i]), &map)) {
				game->VampireProjectile[i].position.x = game->VampireProjectile[i].position.y = -500.0f;
				game->VampireProjectile[i].velocity.x = game->VampireProjectile[i].velocity.y = 0.0f;
				game->VampireProjectile[i].acceleration.y = 0.0f;

				game->VampireProjectile[i].dead = true;
			}

			//Check collision with player
			if (EntityCollision(&(game->Player), &(game->VampireProjectile[i]))) {
				Mix_PlayChannel(-1, playerHit, 0);
				game->Player.dead = true;
				if (game->Player.position.x < game->VampireProjectile[i].position.x) game->Player.playerDeathRight = true;
				else game->Player.playerDeathRight = false;
			}
		}
	}
}
void UpdateGolem(GameState* game, float elapsed, FlareMap& map) {

	//Update golem
	if (!game->Golem.dead && !game->Golem.dying) {
		golemLastAttack += elapsed;
		UpdateEntityMovement(&(game->Golem), elapsed);
		EntityTileMapCollision(&(game->Golem), &map);

		GolemBehavior(game, &(game->Golem));

		//Check collision with player
		if (EntityCollision(&(game->Player), &(game->Golem))) {
			Mix_PlayChannel(-1, playerHit, 0);
			game->Player.dead = true;
			if (game->Player.position.x < game->Golem.position.x) game->Player.playerDeathRight = true;
			else game->Player.playerDeathRight = false;
		}

		//Check collision with mushroom
		if (EntityCollision(&(game->Mushroom), &(game->Golem))) {
			Mix_PlayChannel(-1, mushroomHit2, 0);
			game->Golem.health--;
			if (game->Golem.health == 0) game->Golem.dying = true;

			//Knockback
			game->Golem.velocity.x = 5.0f;
			game->Golem.velocity.y = 7.0f;
			if (game->Golem.faceOtherWay) game->Golem.velocity.x *= -1;

			//red texture
			game->Golem.texture = golemTextureRed;

			game->Mushroom.position.x = game->Mushroom.position.y = -500.0f;
			game->Mushroom.velocity.x = game->Mushroom.velocity.y = 0.0f;
		}
		
		//golem jumps if player is too far
		golemJump(game, &game->Golem);
	}
	//check dying
	if (game->Golem.dying) golemDie(&(game->Golem));
	//check dead
	if (game->Golem.dead) {
		game->Golem.attacking = false;
		game->Golem.dying = false;
		game->Golem.position.x = game->Golem.position.y = -500.0f;

		game->Golem.drawSize.x = 460.0f * 0.025f; //Size of sprite
		game->Golem.drawSize.y = 352.0f * 0.025f;

		game->Golem.size.x = 460.0f * 0.025f * 0.70f * golemDrawRatio; //Size of collision box
		game->Golem.size.y = 352.0f * 0.025f * 0.95f * golemDrawRatio;

		game->Golem.dyingCounter = 0;
	}


	//Update golem projectile
	if (!game->GolemProjectile.dead) {
		UpdateEntityMovement(&(game->GolemProjectile), elapsed);

		if (game->GolemProjectile.position.y <= -30.0f) game->GolemProjectile.dead = true;

		//Check collision with player
		if (EntityCollision(&(game->Player), &(game->GolemProjectile))) {
			Mix_PlayChannel(-1, playerHit, 0);
			game->Player.dead = true;
			if (game->Player.position.x < game->GolemProjectile.position.x) game->Player.playerDeathRight = true;
			else game->Player.playerDeathRight = false;
		}
	}
	else {
		game->GolemProjectile.position.x = game->GolemProjectile.position.y = -500.0f;
		game->GolemProjectile.velocity.x = game->GolemProjectile.velocity.y = 0.0f;
	}
}

void Update1(GameState* game,float elapsed) {
	UpdatePlayer(game, elapsed, map1);
	
	if (!game->Player.dead) {
		//Spawn skeletons and vampires every time interval
		SpawnSkeleton(game, elapsed);
		SpawnVampire(game, elapsed);

		UpdateSkeletons(game, elapsed, map1);
		UpdateVampires(game, elapsed, map1);
		UpdateGolem(game, elapsed, map1);
	}
}
void Update2(GameState* game, float elapsed) {
	UpdatePlayer(game, elapsed, map2);

	if (!game->Player.dead) {
		//Spawn skeletons and vampires every time interval
		SpawnSkeleton(game, elapsed);
		SpawnVampire(game, elapsed);

		UpdateSkeletons(game, elapsed, map2);
		UpdateVampires(game, elapsed, map2);
		UpdateGolem(game, elapsed, map2);
	}
}

void Update3(GameState* game, float elapsed) {
	//prevents player from running into golem during cutscene
	if(!golemCutscene) UpdatePlayer(game, elapsed, map3);

	if (!game->Player.dead) {
		SpawnSkeleton(game, elapsed);
		//SpawnVampire(game, elapsed);

		//Check if player passes certain point in map for the first time
		if (!golemActivated && game->Player.position.x >= 40.0f) {
			golemActivated = true;
			golemCutscene = true;

			//Spawn golem
			game->Golem.position.x = game->Player.position.x + 10.0f;
			game->Golem.position.y = game->Player.position.y + game->Golem.size.y/2.0f;
			game->Golem.dead = false;
		}

		if (golemCutscene && !game->Golem.dying && !game->Golem.dead) golemAppear(&(game->Golem));

		UpdateSkeletons(game, elapsed, map3);
		UpdateVampires(game, elapsed, map3);
		UpdateGolem(game, elapsed, map3);
	}
}

void UpdatePause(GameState* game, float elapsed) {
	//don't do anything (i.e. the level stays still
}

void UpdateGameOver(GameState* game, float elapsed) {
	//don't do anything (i.e. the level stays still
	//Update1(game, elapsed);//optional: have the enemies keep attacking
}

//MAINMENU, LEVEL1, LEVEL2, LEVEL3, GAMEOVER, VICTORY
void Update(GameState* game, float elapsed) {//mode rendering (changes game state and draw the entities for that particular state)
	switch (game->gamemode) {
	case MAINMENU:
		Mix_HaltMusic();
		break;
	case LEVEL1:
		Update1(game, elapsed);
		if (cheats) { game->Player.dead = false; }
		break;
	case LEVEL2:
		Update2(game, elapsed);
		if (cheats) { game->Player.dead = false; }
		break;
	case LEVEL3:
		Update3(game, elapsed);
		if (cheats) { game->Player.dead = false; }
		break;
	case PAUSE:
		UpdatePause(game, elapsed);
	case GAMEOVER:
		UpdateGameOver(game, elapsed);
		break;
	case VICTORY:
		break;
	}
}

void RenderPlayer(GameState* game, ShaderProgram* program, float elapsed) {
	Matrix ModelMatrix;
	//draw player
	ModelMatrix.Identity();//set player's position to identity
	ModelMatrix.Translate(game->Player.position.x, game->Player.position.y, 0.0f);//note we have to change the player's position whenever we change the player model matrix
	if (game->Player.faceOtherWay == true) {
		ModelMatrix.Scale(-1.0f, 1.0f, 1.0f);
	}
	texturedProgram.SetModelMatrix(ModelMatrix);
	game->Player.sprite.Draw(&texturedProgram);

	//draw mushroom
	ModelMatrix.Identity();
	ModelMatrix.Translate(game->Mushroom.position.x, game->Mushroom.position.y, 0.0f);
	ModelMatrix.Rotate(game->Mushroom.rotation);
	if (game->Mushroom.faceOtherWay == true) {
		ModelMatrix.Scale(-1.0f, 1.0f, 1.0f);
	}
	texturedProgram.SetModelMatrix(ModelMatrix);
	Draw(&texturedProgram, game->Mushroom.drawSize.x, game->Mushroom.drawSize.y, game->Mushroom.texture);
}
void RenderSkeletons(GameState* game, ShaderProgram* program, float elapsed) {
	Matrix ModelMatrix;
	//draw Skeletons
	for (i = 0; i < maxSkeletons; i++) {
		ModelMatrix.Identity();
		ModelMatrix.Translate(game->Skeleton[i].position.x, game->Skeleton[i].position.y, 0.0f);
		if (game->Skeleton[i].faceOtherWay == true) {
			ModelMatrix.Scale(-1.0f, 1.0f, 1.0f);
		}
		texturedProgram.SetModelMatrix(ModelMatrix);
		Draw(&texturedProgram, game->Skeleton[i].drawSize.x, game->Skeleton[i].drawSize.y, game->Skeleton[i].texture);
	}
}
void RenderVampires(GameState* game, ShaderProgram* program, float elapsed) {
	Matrix ModelMatrix;
	//draw Vampires
	for (i = 0; i < maxVampires; i++) {
		ModelMatrix.Identity();
		//position + draw offset to fix animations
		ModelMatrix.Translate(game->Vampire[i].position.x + game->Vampire[i].drawOffset.x,
			game->Vampire[i].position.y + game->Vampire[i].drawOffset.y, 0.0f);
		if (game->Vampire[i].faceOtherWay == true) {
			ModelMatrix.Scale(-1.0f, 1.0f, 1.0f);
		}
		texturedProgram.SetModelMatrix(ModelMatrix);

		Draw(&texturedProgram, game->Vampire[i].drawSize.x, game->Vampire[i].drawSize.y, game->Vampire[i].texture);
	}
	//draw Vampire Projectiles
	for (i = 0; i < maxVampires * 3; i++) {
		ModelMatrix.Identity();
		ModelMatrix.Translate(game->VampireProjectile[i].position.x, game->VampireProjectile[i].position.y, 0.0f);
		texturedProgram.SetModelMatrix(ModelMatrix);

		Draw(&texturedProgram, game->VampireProjectile[i].drawSize.x, game->VampireProjectile[i].drawSize.y, game->VampireProjectile[i].texture);
	}
}
void RenderGolem(GameState* game, ShaderProgram* program, float elapsed) {
	Matrix ModelMatrix;
	//draw golem
	ModelMatrix.Identity();
	ModelMatrix.Translate(game->Golem.position.x + game->Golem.drawOffset.x,
		game->Golem.position.y + game->Golem.drawOffset.y, 0.0f);
	if (game->Golem.faceOtherWay == true) {
		ModelMatrix.Scale(-1.0f, 1.0f, 1.0f);
	}

	texturedProgram.SetModelMatrix(ModelMatrix);
	Draw(&texturedProgram, game->Golem.drawSize.x * golemDrawRatio, game->Golem.drawSize.y * golemDrawRatio, game->Golem.texture);

	//draw golem projectile
	ModelMatrix.Identity();
	ModelMatrix.Translate(game->GolemProjectile.position.x, game->GolemProjectile.position.y, 0.0f);
	texturedProgram.SetModelMatrix(ModelMatrix);
	Draw(&texturedProgram, game->GolemProjectile.drawSize.x, game->GolemProjectile.drawSize.y, game->GolemProjectile.texture);


}

void RenderArrow(GameState* game) {
	if (game->Player.collidedCaveArrow) {
		Matrix ModelMatrix;
		ModelMatrix.Identity();
		//ModelMatrix.Translate(150.0f-4.0f, 50.0f-6.0f, 0.0f);
		//ModelMatrix.Translate(game->Player.position.x, -50.0f + 6.0f, 0.0f);
		ModelMatrix.Translate(150.0f - 4.75f, -20.0f + 6.0f, 0.0f);
		ModelMatrix.Scale(-1.0f, 1.0f, 1.0f);
		ModelMatrix.Rotate(-3.1416f / 2);
		texturedProgram.SetModelMatrix(ModelMatrix);
		Draw(&texturedProgram, 15.0f / 5.0f, 10.0f / 5.0f, arrowIndicatorTexture);
	}
}

bool screenShake;
void RenderPortal(GameState* game, float elapsed) {
	if (game->Skeleton[0].dead) {//if skeleton boss is dead, open portal
		Matrix ModelMatrix;
		ModelMatrix.Identity();
		ModelMatrix.Translate(150.0f, -20.0f, 0.0f);
		//ModelMatrix.Translate(game->Player.position.x, -50.0f + 6.0f, 0.0f);
		//ModelMatrix.Translate(150.0f - 4.75f, -20.0f + 6.0f, 0.0f);
		ModelMatrix.Rotate(-3.1416f / 2);
		texturedProgram.SetModelMatrix(ModelMatrix);
		Draw(&texturedProgram, 15.0f, 15.0f, portalTexture);
	}
	if (earthquakeTimer > 0.0f && openPortal) {
		//screenshake + sound(with lower volume)
		screenShake = true;
		earthquakeTimer -= elapsed;
	}
	else {
		screenShake = false;
	}

	//skeleton boss dies, portal opens
	if (game->Skeleton[0].dead) { openPortal = true; }
}

void Render1(GameState* game, ShaderProgram* program, float elapsed) {
	//render static entities
	glClear(GL_COLOR_BUFFER_BIT);//rendering happens once very frame, but there's no way to remove what's previously rendered, but glClear will make what's previously rendered blank.
	glEnable(GL_BLEND);//don't put this nor glBlendFunc in loop!
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); //gets rid of the clear (black) parts of the png
	tilemapRender(*game, program, gameTexture1);

	RenderPlayer(game, program, elapsed);
	RenderSkeletons(game, program, elapsed);
	RenderVampires(game, program, elapsed);
	RenderGolem(game, program, elapsed);

	RenderArrow(game);

}
void Render2(GameState* game, ShaderProgram* program, float elapsed) {
	//render static entities
	glClear(GL_COLOR_BUFFER_BIT);//rendering happens once very frame, but there's no way to remove what's previously rendered, but glClear will make what's previously rendered blank.
	glEnable(GL_BLEND);//don't put this nor glBlendFunc in loop!
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); //gets rid of the clear (black) parts of the png
	tilemapRender(*game, program, gameTexture2);

	RenderPlayer(game, program, elapsed);
	RenderSkeletons(game, program, elapsed);
	RenderVampires(game, program, elapsed);
	RenderGolem(game, program, elapsed);
	RenderPortal(game, elapsed);
}
void Render3(GameState* game, ShaderProgram* program, float elapsed) {
	//render static entities
	glClear(GL_COLOR_BUFFER_BIT);//rendering happens once very frame, but there's no way to remove what's previously rendered, but glClear will make what's previously rendered blank.
	glEnable(GL_BLEND);//don't put this nor glBlendFunc in loop!
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); //gets rid of the clear (black) parts of the png
	tilemapRender(*game, program, gameTexture3);

	RenderPlayer(game, program, elapsed);
	RenderSkeletons(game, program, elapsed);
	RenderVampires(game, program, elapsed);
	RenderGolem(game, program, elapsed);
}

void RenderMainMenu(ShaderProgram* program, float elapsed) {
	glClear(GL_COLOR_BUFFER_BIT);//rendering happens once very frame, but there's no way to remove what's previously rendered, but glClear will make what's previously rendered blank.
	glEnable(GL_BLEND);//don't put this nor glBlendFunc in loop!
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); //gets rid of the clear (black) parts of the png

	Matrix ModelMatrix;
	ModelMatrix.Identity();
	texturedProgram.SetModelMatrix(ModelMatrix);
	Draw(&texturedProgram, 37.0f/1.25f, 20.0f/1.25f, menuTexture);
}

void RenderPause(GameState* game, ShaderProgram* program, float elapsed) {
	glClear(GL_COLOR_BUFFER_BIT);//rendering happens once very frame, but there's no way to remove what's previously rendered, but glClear will make what's previously rendered blank.
	glEnable(GL_BLEND);//don't put this nor glBlendFunc in loop!
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); //gets rid of the clear (black) parts of the png
	//keep the level rendered behind the pause screen
	if (currentLevel == LEVEL1) { Render1(game, program, elapsed); }
	else if (currentLevel == LEVEL2) { Render2(game, program, elapsed); }
	else if (currentLevel == LEVEL3) { Render3(game, program, elapsed); }

	//render pause screen
	Matrix ModelMatrix;
	ModelMatrix.Identity();
	ModelMatrix.Translate(-viewX, -viewY, 0.0f);
	texturedProgram.SetModelMatrix(ModelMatrix);
	Draw(&texturedProgram, 736.0f / 50.0f, 420.0f / 50.0f, pauseTexture);
}

void RenderGameOver(GameState* game, ShaderProgram* program, float elapsed) {
	glClear(GL_COLOR_BUFFER_BIT);//rendering happens once very frame, but there's no way to remove what's previously rendered, but glClear will make what's previously rendered blank.
	glEnable(GL_BLEND);//don't put this nor glBlendFunc in loop!
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); //gets rid of the clear (black) parts of the png
	//keep the level rendered behind the gameover screen
	if (currentLevel == LEVEL1) { Render1(game, program, elapsed); }
	else if (currentLevel == LEVEL2) { Render2(game, program, elapsed); }
	else if (currentLevel == LEVEL3) { Render3(game, program, elapsed); }

	//render gameover screen
	Matrix ModelMatrix;
	ModelMatrix.Identity();
	ModelMatrix.Translate(-viewX, -viewY, 0.0f);
	texturedProgram.SetModelMatrix(ModelMatrix);
	Draw(&texturedProgram, 736.0f / 50.0f, 420.0f / 50.0f, gameOverTexture);
}

//MAINMENU, LEVEL1, LEVEL2, LEVEL3, GAMEOVER, VICTORY
void Render(GameState* game, ShaderProgram* program, float elapsed) {//mode rendering (changes game state and draw the entities for that particular state)
	switch (game->gamemode) {
	case MAINMENU:
		RenderMainMenu(program, elapsed);
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
	case PAUSE:
		RenderPause(game, program, elapsed);
		break;
	case GAMEOVER:
		RenderGameOver(game, program, elapsed);
		break;
	case VICTORY:
		break;
	}
}

void Quit(void) {
	//Cleanup sound

	SDL_Quit();
	return;
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
		viewMatrix; //where in the world you're looking at note that identity means your view is at (0,0)
		projectionMatrix.SetOrthoProjection(leftScreen, rightScreen, bottomScreen, topScreen, -1.0f, 1.0f);//defining the bounds for the screen for the view matrix

		texturedProgram.Load(RESOURCE_FOLDER"vertex_textured.glsl", RESOURCE_FOLDER"fragment_textured.glsl");
		texturedProgram.SetProjectionMatrix(projectionMatrix);
		texturedProgram.SetViewMatrix(viewMatrix);//I will need to have to set viewmatrix to follow player position

		//screen
		screenShakeValue = 1.0f; screenShakeSpeed = 1000.0f; screenShakeIntensity = 1.2f;

		//mainmenu
		menuTexture = LoadTexture(RESOURCE_FOLDER"0-Mainmenu2.png");
		//pause
		pauseTexture = LoadTexture(RESOURCE_FOLDER"0-Paused.png");
		//gameover
		gameOverTexture = LoadTexture(RESOURCE_FOLDER"0-GameOverScreen.png");

		map1.Load("Level1.2.txt");
		map2.Load("Level2.5.txt");
		map3.Load("Level3.0.txt");
		//map3.Load("Level1.2.txt");
		gameTexture1 = LoadTexture(RESOURCE_FOLDER"Level1/FallStage.png");
		gameTexture2 = LoadTexture(RESOURCE_FOLDER"Level2/SnowStage.png");
		gameTexture3 = LoadTexture(RESOURCE_FOLDER"Level3/NightStage.png");
		foxTexture = LoadTexture(RESOURCE_FOLDER"0-kit-fox.png");
		arrowIndicatorTexture = LoadTexture(RESOURCE_FOLDER"Level1/greenArrow.png");
		portalTexture = LoadTexture(RESOURCE_FOLDER"Level2/portal.png");

		//Mushroom projectiles
		mushroomTextures[0] = LoadTexture(RESOURCE_FOLDER"MushroomProjectiles/tinyShroom_brown.png");
		mushroomTextures[1] = LoadTexture(RESOURCE_FOLDER"MushroomProjectiles/tinyShroom_red.png");
		mushroomTextures[2] = LoadTexture(RESOURCE_FOLDER"MushroomProjectiles/tinyShroom_tan.png");

		//Skeleton Idle
		skeletonTextureIdle[0] = LoadTexture(RESOURCE_FOLDER"ArchiveSkeleton/idle/idle_1.png");
		skeletonTextureIdle[1] = LoadTexture(RESOURCE_FOLDER"ArchiveSkeleton/idle/idle_2.png");
		skeletonTextureIdle[2] = LoadTexture(RESOURCE_FOLDER"ArchiveSkeleton/idle/idle_3.png");
		skeletonTextureIdle[3] = LoadTexture(RESOURCE_FOLDER"ArchiveSkeleton/idle/idle_4.png");
		skeletonTextureIdle[4] = LoadTexture(RESOURCE_FOLDER"ArchiveSkeleton/idle/idle_5.png");
		skeletonTextureIdle[5] = LoadTexture(RESOURCE_FOLDER"ArchiveSkeleton/idle/idle_6.png");

		//Skeleton Walking
		skeletonTextureWalking[0] = LoadTexture(RESOURCE_FOLDER"ArchiveSkeleton/walk/go_1.png");
		skeletonTextureWalking[1] = LoadTexture(RESOURCE_FOLDER"ArchiveSkeleton/walk/go_2.png");
		skeletonTextureWalking[2] = LoadTexture(RESOURCE_FOLDER"ArchiveSkeleton/walk/go_3.png");
		skeletonTextureWalking[3] = LoadTexture(RESOURCE_FOLDER"ArchiveSkeleton/walk/go_4.png");
		skeletonTextureWalking[4] = LoadTexture(RESOURCE_FOLDER"ArchiveSkeleton/walk/go_5.png");
		skeletonTextureWalking[5] = LoadTexture(RESOURCE_FOLDER"ArchiveSkeleton/walk/go_6.png");
		skeletonTextureWalking[6] = LoadTexture(RESOURCE_FOLDER"ArchiveSkeleton/walk/go_7.png");
		skeletonTextureWalking[7] = LoadTexture(RESOURCE_FOLDER"ArchiveSkeleton/walk/go_8.png");

		//Skeleton Attacking
		skeletonTextureAttacking[0] = LoadTexture(RESOURCE_FOLDER"ArchiveSkeleton/attack/hit_1.png");
		skeletonTextureAttacking[1] = LoadTexture(RESOURCE_FOLDER"ArchiveSkeleton/attack/hit_2.png");
		skeletonTextureAttacking[2] = LoadTexture(RESOURCE_FOLDER"ArchiveSkeleton/attack/hit_3.png");
		skeletonTextureAttacking[3] = LoadTexture(RESOURCE_FOLDER"ArchiveSkeleton/attack/hit_4.png");
		skeletonTextureAttacking[4] = LoadTexture(RESOURCE_FOLDER"ArchiveSkeleton/attack/hit_5.png");
		skeletonTextureAttacking[5] = LoadTexture(RESOURCE_FOLDER"ArchiveSkeleton/attack/hit_6.png");
		skeletonTextureAttacking[6] = LoadTexture(RESOURCE_FOLDER"ArchiveSkeleton/attack/hit_7.png");
		skeletonTextureAttacking[7] = LoadTexture(RESOURCE_FOLDER"ArchiveSkeleton/attack/hit_8.png");

		//Skeleton Dying
		skeletonTextureDying[0] = LoadTexture(RESOURCE_FOLDER"ArchiveSkeleton/die/die_1.png");
		skeletonTextureDying[1] = LoadTexture(RESOURCE_FOLDER"ArchiveSkeleton/die/die_2.png");
		skeletonTextureDying[2] = LoadTexture(RESOURCE_FOLDER"ArchiveSkeleton/die/die_3.png");
		skeletonTextureDying[3] = LoadTexture(RESOURCE_FOLDER"ArchiveSkeleton/die/die_4.png");
		skeletonTextureDying[4] = LoadTexture(RESOURCE_FOLDER"ArchiveSkeleton/die/die_5.png");
		skeletonTextureDying[5] = LoadTexture(RESOURCE_FOLDER"ArchiveSkeleton/die/die_6.png");
		skeletonTextureDying[6] = LoadTexture(RESOURCE_FOLDER"ArchiveSkeleton/die/die_7.png");
		skeletonTextureDying[7] = LoadTexture(RESOURCE_FOLDER"ArchiveSkeleton/die/die_8.png");

		
		//Vampire idle
		vampireTextureIdle[0] = LoadTexture(RESOURCE_FOLDER"ArchiveVampire/idle/go_1.png");
		vampireTextureIdle[1] = LoadTexture(RESOURCE_FOLDER"ArchiveVampire/idle/go_2.png");
		vampireTextureIdle[2] = LoadTexture(RESOURCE_FOLDER"ArchiveVampire/idle/go_3.png");
		vampireTextureIdle[3] = LoadTexture(RESOURCE_FOLDER"ArchiveVampire/idle/go_4.png");
		vampireTextureIdle[4] = LoadTexture(RESOURCE_FOLDER"ArchiveVampire/idle/go_5.png");
		vampireTextureIdle[5] = LoadTexture(RESOURCE_FOLDER"ArchiveVampire/idle/go_6.png");
		vampireTextureIdle[6] = LoadTexture(RESOURCE_FOLDER"ArchiveVampire/idle/go_7.png");
		vampireTextureIdle[7] = LoadTexture(RESOURCE_FOLDER"ArchiveVampire/idle/go_8.png");

		//Vampire attack
		vampireTextureAttack[0] = LoadTexture(RESOURCE_FOLDER"ArchiveVampire/attack/hit_1.png");
		vampireTextureAttack[1] = LoadTexture(RESOURCE_FOLDER"ArchiveVampire/attack/hit_2.png");
		vampireTextureAttack[2] = LoadTexture(RESOURCE_FOLDER"ArchiveVampire/attack/hit_3.png");
		vampireTextureAttack[3] = LoadTexture(RESOURCE_FOLDER"ArchiveVampire/attack/hit_4.png");
		vampireTextureAttack[4] = LoadTexture(RESOURCE_FOLDER"ArchiveVampire/attack/hit_5.png");
		vampireTextureAttack[5] = LoadTexture(RESOURCE_FOLDER"ArchiveVampire/attack/hit_6.png");
		vampireTextureAttack[6] = LoadTexture(RESOURCE_FOLDER"ArchiveVampire/attack/hit_7.png");
		vampireTextureAttack[7] = LoadTexture(RESOURCE_FOLDER"ArchiveVampire/attack/hit_8.png");
		vampireTextureAttack[8] = LoadTexture(RESOURCE_FOLDER"ArchiveVampire/attack/hit_9.png");
		vampireTextureAttack[9] = LoadTexture(RESOURCE_FOLDER"ArchiveVampire/attack/hit_10.png");
		vampireTextureAttack[10] = LoadTexture(RESOURCE_FOLDER"ArchiveVampire/attack/hit_11.png");
		vampireTextureAttack[11] = LoadTexture(RESOURCE_FOLDER"ArchiveVampire/attack/hit_12.png");
		vampireTextureAttack[12] = LoadTexture(RESOURCE_FOLDER"ArchiveVampire/attack/hit_13.png");
		//projectile
		vampireTextureProjectile = LoadTexture(RESOURCE_FOLDER"ArchiveVampire/projectile.png");

		//Vampire disappear
		vampireTextureDying[0] = LoadTexture(RESOURCE_FOLDER"ArchiveVampire/disappear/appear_1.png");
		vampireTextureDying[1] = LoadTexture(RESOURCE_FOLDER"ArchiveVampire/disappear/appear_2.png");
		vampireTextureDying[2] = LoadTexture(RESOURCE_FOLDER"ArchiveVampire/disappear/appear_3.png");
		vampireTextureDying[3] = LoadTexture(RESOURCE_FOLDER"ArchiveVampire/disappear/appear_4.png");
		vampireTextureDying[4] = LoadTexture(RESOURCE_FOLDER"ArchiveVampire/disappear/appear_5.png");
		vampireTextureDying[5] = LoadTexture(RESOURCE_FOLDER"ArchiveVampire/disappear/appear_6.png");
		vampireTextureDying[6] = LoadTexture(RESOURCE_FOLDER"ArchiveVampire/disappear/appear_7.png");
		vampireTextureDying[7] = LoadTexture(RESOURCE_FOLDER"ArchiveVampire/disappear/appear_8.png");
		vampireTextureDying[8] = LoadTexture(RESOURCE_FOLDER"ArchiveVampire/disappear/appear_9.png");
		vampireTextureDying[9] = LoadTexture(RESOURCE_FOLDER"ArchiveVampire/disappear/appear_10.png");
		vampireTextureDying[10] = LoadTexture(RESOURCE_FOLDER"ArchiveVampire/disappear/appear_11.png");
		vampireTextureDying[11] = LoadTexture(RESOURCE_FOLDER"ArchiveVampire/disappear/appear_12.png");

		//Golem Appear
		golemTextureAppear[0] = LoadTexture(RESOURCE_FOLDER"ArchiveGolem/appear/appear_1.png");
		golemTextureAppear[1] = LoadTexture(RESOURCE_FOLDER"ArchiveGolem/appear/appear_2.png");
		golemTextureAppear[2] = LoadTexture(RESOURCE_FOLDER"ArchiveGolem/appear/appear_3.png");
		golemTextureAppear[3] = LoadTexture(RESOURCE_FOLDER"ArchiveGolem/appear/appear_4.png");
		golemTextureAppear[4] = LoadTexture(RESOURCE_FOLDER"ArchiveGolem/appear/appear_5.png");
		golemTextureAppear[5] = LoadTexture(RESOURCE_FOLDER"ArchiveGolem/appear/appear_6.png");
		golemTextureAppear[6] = LoadTexture(RESOURCE_FOLDER"ArchiveGolem/appear/appear_7.png");
		golemTextureAppear[7] = LoadTexture(RESOURCE_FOLDER"ArchiveGolem/appear/appear_8.png");
		golemTextureAppear[8] = LoadTexture(RESOURCE_FOLDER"ArchiveGolem/appear/appear_9.png");
		golemTextureAppear[9] = LoadTexture(RESOURCE_FOLDER"ArchiveGolem/appear/appear_10.png");
		golemTextureAppear[10] = LoadTexture(RESOURCE_FOLDER"ArchiveGolem/appear/appear_11.png");
		golemTextureAppear[11] = LoadTexture(RESOURCE_FOLDER"ArchiveGolem/appear/appear_12.png");
		golemTextureAppear[12] = LoadTexture(RESOURCE_FOLDER"ArchiveGolem/appear/appear_13.png");
		golemTextureAppear[13] = LoadTexture(RESOURCE_FOLDER"ArchiveGolem/appear/appear_14.png");
		golemTextureAppear[14] = LoadTexture(RESOURCE_FOLDER"ArchiveGolem/appear/appear_15.png");

		//Golem Walk/Idle
		golemTextureWalk[0] = LoadTexture(RESOURCE_FOLDER"ArchiveGolem/idle-walk/idle_1.png");
		golemTextureWalk[1] = LoadTexture(RESOURCE_FOLDER"ArchiveGolem/idle-walk/idle_2.png");
		golemTextureWalk[2] = LoadTexture(RESOURCE_FOLDER"ArchiveGolem/idle-walk/idle_3.png");
		golemTextureWalk[3] = LoadTexture(RESOURCE_FOLDER"ArchiveGolem/idle-walk/idle_4.png");
		golemTextureWalk[4] = LoadTexture(RESOURCE_FOLDER"ArchiveGolem/idle-walk/idle_5.png");
		golemTextureWalk[5] = LoadTexture(RESOURCE_FOLDER"ArchiveGolem/idle-walk/idle_6.png");
		golemTextureRed = LoadTexture(RESOURCE_FOLDER"ArchiveGolem/idle-walk/idle_1_red.png");

		//Golem Attack
		golemTextureAttack[0] = LoadTexture(RESOURCE_FOLDER"ArchiveGolem/attack/hit_1.png");
		golemTextureAttack[1] = LoadTexture(RESOURCE_FOLDER"ArchiveGolem/attack/hit_2.png");
		golemTextureAttack[2] = LoadTexture(RESOURCE_FOLDER"ArchiveGolem/attack/hit_3.png");
		golemTextureAttack[3] = LoadTexture(RESOURCE_FOLDER"ArchiveGolem/attack/hit_4.png");
		golemTextureAttack[4] = LoadTexture(RESOURCE_FOLDER"ArchiveGolem/attack/hit_5.png");
		golemTextureAttack[5] = LoadTexture(RESOURCE_FOLDER"ArchiveGolem/attack/hit_6.png");
		//projectile
		golemTextureProjectile = LoadTexture(RESOURCE_FOLDER"ArchiveGolem/projectile.png");

		//Golem Die
		golemTextureDie[0] = LoadTexture(RESOURCE_FOLDER"ArchiveGolem/die/die_1.png");
		golemTextureDie[1] = LoadTexture(RESOURCE_FOLDER"ArchiveGolem/die/die_2.png");
		golemTextureDie[2] = LoadTexture(RESOURCE_FOLDER"ArchiveGolem/die/die_3.png");
		golemTextureDie[3] = LoadTexture(RESOURCE_FOLDER"ArchiveGolem/die/die_4.png");
		golemTextureDie[4] = LoadTexture(RESOURCE_FOLDER"ArchiveGolem/die/die_5.png");
		golemTextureDie[5] = LoadTexture(RESOURCE_FOLDER"ArchiveGolem/die/die_6.png");
		golemTextureDie[6] = LoadTexture(RESOURCE_FOLDER"ArchiveGolem/die/die_7.png");

		//sounds setup
		Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 4096);

		//music
		musicLevel1 = Mix_LoadMUS(RESOURCE_FOLDER"ArchiveSounds/Music/musicLevel1.mp3");
		musicLevel2 = Mix_LoadMUS(RESOURCE_FOLDER"ArchiveSounds/Music/musicLevel2.mp3");
		musicLevel3 = Mix_LoadMUS(RESOURCE_FOLDER"ArchiveSounds/Music/musicLevel3.mp3");

		Mix_VolumeMusic(40);
		//Mix_PlayMusic(musicLevel1, -1);

		//Mushroom sounds
		mushroomThrow = Mix_LoadWAV(RESOURCE_FOLDER"ArchiveSounds/Mushrooms/song349-mushroom-throw1.wav"); 
		Mix_VolumeChunk(mushroomThrow, 40); // set sound volume (from 0 to 128)

		mushroomHit = Mix_LoadWAV(RESOURCE_FOLDER"ArchiveSounds/Mushrooms/mushroomHit.wav");
		Mix_VolumeChunk(mushroomHit, 30);

		mushroomHit2 = Mix_LoadWAV(RESOURCE_FOLDER"ArchiveSounds/Mushrooms/mushroomHit2.wav");
		Mix_VolumeChunk(mushroomHit2, 30);

		//player sounds
		playerHit = Mix_LoadWAV(RESOURCE_FOLDER"ArchiveSounds/Player/playerHit.wav");
		playerJump = Mix_LoadWAV(RESOURCE_FOLDER"ArchiveSounds/Player/playerJump.wav");

		//enemy sounds
		skeletonAttackSound = Mix_LoadWAV(RESOURCE_FOLDER"ArchiveSounds/Enemies/skeletonAttack.wav");
		vampireAttackSound = Mix_LoadWAV(RESOURCE_FOLDER"ArchiveSounds/Enemies/vampireAttack.wav");
		golemAttackSound = Mix_LoadWAV(RESOURCE_FOLDER"ArchiveSounds/Enemies/golemAttack.wav");
		golemSpawn = Mix_LoadWAV(RESOURCE_FOLDER"ArchiveSounds/Enemies/golemSpawn.wav");
		Mix_VolumeChunk(golemSpawn, 50);

		//misc
		levelVictory = Mix_LoadWAV(RESOURCE_FOLDER"ArchiveSounds/Misc/levelVictory.wav");
		gameVictory = Mix_LoadWAV(RESOURCE_FOLDER"ArchiveSounds/Misc/gameVictory.wav");

		GameState game;
		game.gamemode = MAINMENU;
		float soundCounter = 1.0f;


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
			soundCounter += FIXED_TIMESTEP;
			if (soundCounter >= 1.0f) {
				//loop for playing sound
				soundCounter = 0.0f;
			}
			
			/* render */
			viewMatrix.Identity();

			//ViewMatrix translations
			//For golem cutscenes

			if (game.gamemode == LEVEL1 || game.gamemode == LEVEL2 || game.gamemode == LEVEL3 || game.gamemode == PAUSE || game.gamemode == GAMEOVER) {
				//zoom out for stage 3
				if (game.gamemode == LEVEL3) {
					//viewMatrix.Scale(1.5f, 1.5f, 1.0f);
					viewMatrix.Scale(0.75f, 0.75f, 0.75f);
				}

				if (golemCutscene) {
					if (game.gamemode == PAUSE) {
						viewX = -game.Golem.position.x; float viewY = -game.Golem.position.y -1.1f;
						viewMatrix.Translate(viewX, viewY, 0.0f);
					}
					else {
						viewX = -game.Golem.position.x; float viewY = -game.Golem.position.y + sin(screenShakeValue * screenShakeSpeed * elapsed)* screenShakeIntensity*2;
						viewMatrix.Translate(viewX, viewY, 0.0f);
					}
					
				}
				//for golem boss fight
				
				else if (golemActivated && !golemCutscene && golemRight) {
					viewXFinal = -game.Player.position.x - 10.0f;
					viewX = lerp(viewX,viewXFinal,FIXED_TIMESTEP*10); viewY = -game.Player.position.y - 3.0f;
					viewMatrix.Translate(viewX, viewY, 0.0f);
				}
				
				//else if (golemActivated && !golemCutscene && golemLeft) {//no lerping would make a camera jump, which is too random
				//	viewX = -game.Player.position.x + 10.0f; viewY = -game.Player.position.y - 3.0f;
				//	viewMatrix.Translate(viewX, viewY, 0.0f);
				//}

				else if (golemActivated && !golemCutscene && golemLeft) {
					viewXFinal = -game.Player.position.x + 10.0f;
					viewX = lerp(viewX, viewXFinal, FIXED_TIMESTEP * 10); viewY = -game.Player.position.y - 3.0f;
					viewMatrix.Translate(viewX, viewY, 0.0f);
				}

				
				else if (screenShake) {
					viewX = -game.Player.position.x; viewY = -cameraOffSetY - game.Player.position.y + sin(screenShakeValue * screenShakeSpeed * elapsed)* screenShakeIntensity + 1.1f;
					viewMatrix.Translate(viewX, viewY, 0.0f);
				}
				//normal
				else {
					viewX = -game.Player.position.x; viewY = -game.Player.position.y - cameraOffSetY;
					viewMatrix.Translate(viewX, viewY, 0.0f);
				}

			}
			//viewMatrix.Translate(0.0f, 20.0f, 0.0f);//debug
			//viewMatrix.Translate(-game.Player.position.x, 20.0f, 0.0f);

			texturedProgram.SetViewMatrix(viewMatrix);

			ProcessInput(&game, FIXED_TIMESTEP);

			Update(&game, FIXED_TIMESTEP);
			if (closeWindow) { done = true; }

			Render(&game, &texturedProgram, FIXED_TIMESTEP);

			Cheats(FIXED_TIMESTEP);

			/* render */

			elapsed -= FIXED_TIMESTEP;
		}
		accumulator = elapsed;

		SDL_GL_SwapWindow(displayWindow);
	}

	Quit();
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