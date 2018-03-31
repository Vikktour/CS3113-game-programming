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
#define LEVEL_HEIGHT 15 //each level is 15 tiles tall
#define LEVEL_WIDTH 25 //each levle is 25 tiles wide

/* Adjustable global variables */
//screen
float screenWidth = 720;
float screenHeight = 480;
float topScreen = 2.0f;
float bottomScreen = -2.0f;
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

void SheetSprite::Draw(ShaderProgram *program) const {
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
	float vertices[] = {//note that halfWidth = size*aspect/2.0f, height = size/2.0f
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

enum EntityType { ENTITY_PLAYER, ENTITY_ENEMY, ENTITY_COIN, ENTITY_POWERUP };
class Entity {
public:
	Entity() {};
	void Update(float elapsed);
	void Render(ShaderProgram *program);
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
	//modelMatrix.Translate(position.x, position.y, position.z);
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
};


void PlaceEntity(std::string type, float x, float y, GameState* game, FlareMap* map) {//comebackreturn unresolved external
	//note float x&y can be in pixel coords by inputting x*tileSize where tileSize = 16.0f
	GLuint gameTexture = LoadTexture(RESOURCE_FOLDER"CdH_TILES.png");
	//note the png is 20x20 blocks
	float pngHeight = 20.0f * 16.0f;//pngHeight in pixels ~ 20blocks and 16 pixels per block
	float pngWidth = 20.0f * 16.0f;

	float stageHeight = map->mapHeight; float stageWidth = map->mapWidth;
	if (type == "DuckPrincess") {//I want these ducks to just walk back and forth, no collision
		Entity duckPrincess;
		float u = 12.0f * 16.0f; float v = 4.0f * 16.0f;//note the duck texture is at (12blocks,4blocks) of the png
		duckPrincess.sprite = SheetSprite(gameTexture, u, v, 16.0f / pngWidth, 16.0f / pngHeight, 1.0f);//(unsigned int textureID, float u, float v, float width, float height, float size)
		//now the world coordinates:
		duckPrincess.position.x = x;//NOTE: we're going to input x*16.0f so no need to multiply here
		duckPrincess.position.y = y;
		game->DuckPrincess = duckPrincess;
	}
	else if (type == "Water") {//I want the water to make the duck jump higher
		Entity water;
		float u = 11.0f * 16.0f; float v = 8.0f * 16.0f;//note the water texture is at (11blocks,8blocks) of the png
		water.sprite = SheetSprite(gameTexture, u, v, 16.0f / pngWidth, 16.0f / pngHeight, 1.0f);
		water.position.x = x;
		water.position.y = y;
		game->WaterVec.push_back(water);
	}
	else if (type == "Boost") {//I want the boost to make the duck go faster
		Entity boost;
		float u = 8.0f * 16.0f; float v = 4.0f * 16.0f;//note the boost texture is at (8blocks,4blocks) of the png
		boost.sprite = SheetSprite(gameTexture, u, v, 16.0f / pngWidth, 16.0f / pngHeight, 1.0f);
		boost.position.x = x;
		boost.position.y = y;
		game->BoostVec.push_back(boost);
	}
	else if (type == "DuckPlayer") {
		Entity player;
		float u = 12.0f * 16.0f; float v = 3.0f * 16.0f;//note the duckplayer texture is at (12blocks,3blocks) of the png
		player.sprite = SheetSprite(gameTexture, u, v, 16.0f / pngWidth, 16.0f / pngHeight, 1.0f);
		game->Player.position.x = 0;//x;//debug
		game->Player.position.y = 0;//y;
		game->Player = player;
	}
}

float tileSize = 16.0f;//each tile size is 16 pixels
void InitializeGame(GameState *game) {//set the positions of all the entities
	FlareMap map;
	map.Load("MyLevel1.4.txt");

	for (int i = 0; i < map.entities.size(); i++) {//place Dynamic entities
		PlaceEntity(map.entities[i].type, map.entities[i].x * tileSize, map.entities[i].y * -tileSize, game, &map);//push into gamestate the initial positions of DYNAMIMC ENTITIES;
	}

	for (int y = 0; y < map.mapHeight; y++) {
		for (int x = 0; x < map.mapWidth; x++) {
			//comeback: do something with map.mapData[y][x]

		}
	}

	

}

void Render(GameState* game, ShaderProgram* program) {//display the entities on screen
	//render static entities

	//render dynamic entities
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
		Matrix viewMatrix; //where in the world you're looking at
		projectionMatrix.SetOrthoProjection(leftScreen, rightScreen, bottomScreen, topScreen, -1.0f, 1.0f);//defining the bounds for the screen for the view matrix

		texturedProgram.Load(RESOURCE_FOLDER"vertex_textured.glsl", RESOURCE_FOLDER"fragment_textured.glsl");
		texturedProgram.SetProjectionMatrix(projectionMatrix);
		texturedProgram.SetViewMatrix(viewMatrix);//comeback, have to set viewmatrix to follow player position

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
		glClear(GL_COLOR_BUFFER_BIT);//set screen to clear color

		float ticks = (float)SDL_GetTicks() / 1000.0f;//returns the number of millisecs/1000 elapsed since you compiled code
		float elapsed = ticks - lastFrameTicks;//returns the number of millisecs/1000 elapsed since the last loop of the event
		lastFrameTicks = ticks;

		elapsed += accumulator;
		 
		if (elapsed > 6.0f * FIXED_TIMESTEP) {//initializing the game may take a lot of elapsed time so we don't care about the time in the pre-rendering moments
			elapsed = 6.0f * FIXED_TIMESTEP;
		}

		if (elapsed < FIXED_TIMESTEP) {//if not enough time elapsed yet for desired first frame then don't update
			accumulator = elapsed;
			//continue;//why's there a continue here???? It makes the program crash.
		}


		while (elapsed >= FIXED_TIMESTEP) {//if enough time for a frame, then update each frame until not enough time for single frame
			//Update(FIXED_TIMESTEP);//uncomment
			Render(&game,&texturedProgram);//uncomment
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

