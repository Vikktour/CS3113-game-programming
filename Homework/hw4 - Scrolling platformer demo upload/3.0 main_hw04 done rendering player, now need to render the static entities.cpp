/*
Victor Zheng vz365
hw04 Scrolling platformer demo
Controls:

questions and answers:

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

#ifdef _WINDOWS
	#define RESOURCE_FOLDER ""
#else
	#define RESOURCE_FOLDER "NYUCodebase.app/Contents/Resources/"
#endif

//define timesteps to update based on FPS, e.g. 60fps means update every 1/60 second ~ 0.166666f
#define FIXED_TIMESTEP 0.0166666f
#define MAX_TIMESTEPS 6
#define LEVEL_HEIGHT 25 //each level is at most 25 tiles tall
#define LEVEL_WIDTH 50 //each level is at most 50 tiles wide
//note that this level will be 240*400 pixels

/* Adjustable global variables */
//screen
float screenWidth = 720;
float screenHeight = 480;
float topScreen = 100.0f;
float bottomScreen = -100.0f;
float aspectRatio = screenWidth / screenHeight;
float leftScreen = -aspectRatio * topScreen;
float rightScreen = aspectRatio * topScreen;

//time and FPS
float accumulator = 0.0f; float lastFrameTicks = 0.0f;

//more global variables
ShaderProgram texturedProgram;
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
	//SheetSprite() {};//divide by 0 error - crash at start
	SheetSprite() : spriteCountX(20),spriteCountY(20) {};//set spriteCountX & Y as hardcode 20(current png size) to avoid divide by 0 possibility
	//SheetSprite(float size, unsigned int textureID, int index, int spriteCountX, int spritCountY) : {}
	void Draw(ShaderProgram* program) const;
	float size;
	unsigned int textureID;
	int index;
	int spriteCountX;//horizontal #tile size of spritesheet
	int spriteCountY;
};

//note: the spritesheet I'm using for this hw is 20x20 blocks so spriteCountX = spriteCountY = 20
void SheetSprite::Draw(ShaderProgram* program) const {//drawing for uniform spritesheet. spriteCountX&spriteCountY are the z #rows&cols of 
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

class Vector3 { //keep track of object coordinates
public:
	Vector3(float x = 0.0f, float y = 0.0f, float z = 0.0f) : x(x), y(y), z(z) {}
	float x;
	float y;
	float z;
};

enum EntityType { ENTITY_PLAYER, ENTITY_ENEMY, ENTITY_COIN, ENTITY_POWERUP };
class Entity {
public:
	Entity() {};
	void Update(float elapsed);
	void Render(ShaderProgram* program);
	bool CollidesWith(Entity *entity);
	SheetSprite sprite;
	Vector3 position;
	Vector3 size;
	Vector3 velocity;
	Vector3 acceleration;
	bool isStatic;
	EntityType entityType;
	bool collidedTop;
	bool collidedBottom;
	bool collidedLeft;
	bool collidedRight;
};


void Entity::Update(float elapsed) {
	velocity.x += acceleration.x * elapsed;
	velocity.y += acceleration.y * elapsed;
	position.x += velocity.x * elapsed;
	position.y += velocity.y * elapsed;

}
void Entity::Render(ShaderProgram* program) {
	Matrix modelMatrix;
	modelMatrix.Identity();
	modelMatrix.Translate(position.x, position.y, position.z);
	//modelMatrix.Translate(30.0f, 30.0f, position.z);
	program->SetModelMatrix(modelMatrix);
	sprite.Draw(program);
}
bool Entity::CollidesWith(Entity *entity) {
	//boxbox
	//circlecircle
	//comeback
	return 0;
}

class GameState {
public:
	Entity Player;
	Entity DuckPrincess;
	std::vector<Entity> WaterVec;
	std::vector<Entity> BoostVec;
	//Entity LevelMap[LEVEL_HEIGHT][LEVEL_WIDTH];//2D array of static entities. Don't want to use this anymore b/c vector is more convenient to get size
	std::vector<std::vector<Entity>> LevelMap;
};


void PlaceEntity(std::string type, float x, float y, GameState* game, FlareMap* map) {//this is for initalizing positions of entities
	//note float x&y can be in pixel coords by inputting x*tileSize where tileSize = 16.0f
	GLuint gameTexture = LoadTexture(RESOURCE_FOLDER"CdH_TILES.png");
	if (type == "DuckPlayer") {
			Entity player;
			//note the duckplayer texture is at (12blocks,3blocks) of the png
			player.sprite.index = 223;
			player.sprite.spriteCountX = 20;
			player.sprite.spriteCountY = 20;
			player.position.x = x;//note that this x is 16.0f*tileLocation ~ pixels	//30.0f;//x;//debug
			player.position.y = y;//0.0f;//y;
			player.sprite.textureID = gameTexture;
			player.sprite.size = 16.0f;
			game->Player = player;
	}
	//else if (type == "DuckPrincess") {//
	//	Entity duckPrincess;
	//	//note the duck texture is at (12blocks,4blocks) of the png
	//	duckPrincess.sprite.index = 224;
	//	duckPrincess.sprite.spriteCountX = 20;
	//	duckPrincess.sprite.spriteCountY = 20;
	//}
	//else if (type == "Water") {//I want the water to make the duck jump higher
	//	Entity water;
	//	//note the water texture is at (11blocks,8blocks) of the png
	//	water.sprite.index = 208;
	//	water.sprite.spriteCountX = 20;
	//	water.sprite.spriteCountY = 20;

	//}
	//else if (type == "Boost") {//I want the boost to make the duck go faster
	//	Entity boost;
	//	//note the boost texture is at (8blocks,4blocks) of the png
	//	boost.sprite.index = 144;
	//	boost.sprite.spriteCountX = 20;
	//	boost.sprite.spriteCountY = 20;
	//}

}

float tileSize = 16.0f;//each tile size is 16 pixels
void InitializeGame(GameState *game) {//set the positions of all the entities
	FlareMap map;
	map.Load("MyLevel1.4.txt");
	
	std::vector<std::vector<Entity>> vectorWithSize(map.mapHeight, std::vector<Entity> (map.mapWidth));
	game->LevelMap = vectorWithSize; //this is to give the LevelMap 2D vector size so I can loop

	for (int i = 0; i < map.entities.size(); i++) {//place Dynamic entities
		PlaceEntity(map.entities[i].type, map.entities[i].x * tileSize, map.entities[i].y * -tileSize, game, &map);//push into gamestate the initial positions of DYNAMIMC ENTITIES;
	}

	for (int y = 0; y < map.mapHeight; y++) {
		for (int x = 0; x < map.mapWidth; x++) {
			//comeback: do something with map.mapData[y][x]
			game->LevelMap[y][x].sprite.index = map.mapData[y][x];//comeback2
			game->LevelMap[y][x].position.x = x;// * 16.0f;
			game->LevelMap[y][x].position.y = y;// * 16.0f;
		}
	}
	//game->LevelMap[10][5].sprite.index = 1;//debug
	//game->LevelMap[1][0];
}

void Render(GameState* game, ShaderProgram* program) {//display the entities on screen
	//render static entities
	for (int y = 0; y < game->LevelMap.size(); y++) {
		for (int x = 0; x < game->LevelMap[0].size(); x++) {
			game->LevelMap[y][x].Render(program);
		}
	}


	//render dynamic entities (note this is rendered after the static entities b/c it needs to be in the frontmost view)
	game->Player.Render(program);//not working, maybe placeEntity's gameTexture or something else needs to be in the loop rather than one time use.


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

		glViewport(0, 0, screenWidth, screenHeight);//sets size and offset of rendering area

		Matrix projectionMatrix;
		Matrix viewMatrix; //where in the world you're looking at note that identity means your view is at (0,0)
		projectionMatrix.SetOrthoProjection(leftScreen, rightScreen, bottomScreen, topScreen, -1.0f, 1.0f);//defining the bounds for the screen for the view matrix
		//projectionMatrix.SetOrthoProjection(-720.0f, 720.0f, -480.0f, 480.0f, -1.0f, 1.0f);//debug
		//projectionMatrix.SetOrthoProjection(-72.0f, 72.0f, -48.0f, 48.0f, -1.0f, 1.0f);
		//projectionMatrix.SetOrthoProjection(-7.2f, 7.2f, -4.8f, 4.8f, -1.0f, 1.0f);

		texturedProgram.Load(RESOURCE_FOLDER"vertex_textured.glsl", RESOURCE_FOLDER"fragment_textured.glsl");
		texturedProgram.SetProjectionMatrix(projectionMatrix);
		texturedProgram.SetViewMatrix(viewMatrix);//I will nened to have to set viewmatrix to follow player position

		GameState game;
		InitializeGame(&game);

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
			continue;//It makes the program crash if I use glClear above it
		}


		while (elapsed >= FIXED_TIMESTEP) {//if enough time for a frame, then update each frame until not enough time for single frame
			//Update(FIXED_TIMESTEP);//uncomment
			
			/* render */

			viewMatrix.Translate(-game.Player.position.x, -game.Player.position.y, 0.0f);
			//viewMatrix.Translate(30.0f, 0.0f, 0.0f);
			texturedProgram.SetViewMatrix(viewMatrix);
			Render(&game,&texturedProgram);
			viewMatrix.Identity();

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


/*Debug Notes

1)Switched the non-uniform sprite class & draw() to a uniform sprite class & draw()
2)Flaremap.cpp modify the flags to fit the title of my types (e.g. had to change: else if(line == "[ObjectsLayer]") to [Object Layer 1] to match my type name, due to how the Tiled software saved the type as 
3)avoid having glCLear() in the same loop as the accumulator FPS checker, that along with "continue" will cause the game to crash. A fix i did was comment out glClear() and then uncommented "continue"






*/

/*Edit log
1)Edit size of texture
2)Get the player to translate properly
3)Get the view matrix to follow the player
4)Get the tiled background to render properly


*/