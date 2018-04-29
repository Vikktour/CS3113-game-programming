/*
Johnathan Apuzen
Victor Zheng vz365

//debug notes
1)level not rendered properly, world tiles are mapped to the wrong tiles in the picture. Fix:

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
#define LEVEL_HEIGHT 20 //each level is at most 20 tiles tall
#define LEVEL_WIDTH 150 //each level is at most 150 tiles wide
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
		size = 2.0f;
		//sizeX = 1;
		//sizeY = 1;
	};
	void Draw(ShaderProgram* program) const;
	float size;
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
			-0.5f*size, -0.5f*size,
			0.5f*size, 0.5f*size,
			-0.5f*size, 0.5f*size,
			0.5f*size, 0.5f*size,
			-0.5f*size,-0.5f*size,
			0.5f*size, -0.5f*size };
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

void InitializeLevel1(GameState* game){
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
	game->Player.sprite = playerSpriteSheet;
	game->Player.size = 2.0f;

}
void InitializeLevel2(GameState* game) {

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

/*switch(game->gamemode){
case LEVEL1:
	
	break;
case LEVEL2:
	break;
case LEVEL3:
	break;
}*/

const Uint8 *keys = SDL_GetKeyboardState(NULL);
float playerSpeed = 8.0f;
void ProcessInput(GameState* game, float elapsed) {
	if (keys[SDL_SCANCODE_LEFT]) {
		game->Player.velocity.x = -playerSpeed;
	}
	if (keys[SDL_SCANCODE_RIGHT]) {
		game->Player.velocity.x = playerSpeed;
	}
	if (keys[SDL_SCANCODE_UP]) {
		game->Player.velocity.y = playerSpeed;
	}
	if (keys[SDL_SCANCODE_DOWN]) {
		game->Player.velocity.y = -playerSpeed;
	}
	if (!(keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_RIGHT])) {
		game->Player.velocity.x = 0.0f;
	}
	if (!(keys[SDL_SCANCODE_UP] || keys[SDL_SCANCODE_DOWN])) {
		game->Player.velocity.y = 0.0f;
	}

}
void Update(GameState* game,float elapsed) {
	game->Player.position.x += game->Player.velocity.x * elapsed;
	game->Player.position.y += game->Player.velocity.y * elapsed;

}
void Render(GameState* game, ShaderProgram* program) {
	//render static entities
	glClear(GL_COLOR_BUFFER_BIT);//rendering happens once very frame, but there's no way to remove what's previously rendered, but glClear will make what's previously rendered blank.
	glEnable(GL_BLEND);//don't put this nor glBlendFunc in loop!
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); //gets rid of the clear (black) parts of the png
	tilemapRender(*game, program, gameTexture1);

	Matrix ModelMatrix;

	//draw player
	ModelMatrix.Identity();//set player's position to identity
	ModelMatrix.Translate(game->Player.position.x, game->Player.position.y, 0.0f);//note we have to change the player's position whenever we change the player model matrix
	texturedProgram.SetModelMatrix(ModelMatrix);
	game->Player.sprite.Draw(&texturedProgram);

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
		//map2.Load("Level1.2.txt");
		//map3.Load("Level1.2.txt");
		gameTexture1 = LoadTexture(RESOURCE_FOLDER"FallStage.png");
		foxTexture = LoadTexture(RESOURCE_FOLDER"kit-fox.png");

		GameState game;
		InitializeLevel1(&game);

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
		glClearColor(0.5f, 0.0, 0.5f, 1.0f);//clear color
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
			Render(&game, &texturedProgram);

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
3.0)Give running animation
3.1)Check for player collision with floor, keep him above ground.
4)



*/