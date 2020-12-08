/*
Victor Zheng vz365
hw04 Scrolling platformer demo
Controls:

questions and answers:
What is bool collidedBottom for? Ans: It's to apply friction (only if the dynamic entity is colliding with something under it)
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
float topScreen = 7.0f;
float bottomScreen = -7.0f;
float aspectRatio = screenWidth / screenHeight;
float leftScreen = -aspectRatio * topScreen;
float rightScreen = aspectRatio * topScreen;

//time and FPS
float accumulator = 0.0f; float lastFrameTicks = 0.0f;

//game
float velocityMax = 3.0f;
float tileSize = 1.0f;//tile size is how big you want each block to be in the world (e.g. our sprite is 16x16pixels in png, but we can make it 1.0x1.0 in world)
FlareMap map;


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

enum EntityType { ENTITY_PLAYER, ENTITY_ENEMY, ENTITY_COIN, ENTITY_POWERUP };
class Entity {
public:
	Entity() : velocity(0.0f,0.0f,0.0f), acceleration(0.0f,0.0f,0.0f), friction(0.5f,0.0f,0.0f) {};
	void Update(float elapsed);
	void Render(ShaderProgram* program);
	bool CollidesWith(Entity *entity);
	SheetSprite sprite;
	Vector3 position;
	Vector3 size;
	Vector3 velocity;
	Vector3 acceleration;
	Vector3 friction;
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
	//Set a speed limit
	if (velocity.x > velocityMax) {
		velocity.x = velocityMax;
	}
	else if (velocity.x < -velocityMax) {
		velocity.x = -velocityMax;
	}
	if (velocity.y > velocityMax) {
		velocity.y = velocityMax;
	}
	else if (velocity.y < -velocityMax) {
		velocity.y = -velocityMax;
	}
	position.x += velocity.x * elapsed;
	position.y += velocity.y * elapsed;

	//friction if touching ground
	if (velocity.y == 0 && acceleration.x == 0) {
		velocity.x = lerp(velocity.x, 0.0f, elapsed * friction.x);
		//velocity.y = lerp(velocity.y, 0.0f, elapsed * friction.y);
	}

	//gravity for dynamic entities
	float gravity = -1.0f;
	velocity.y += gravity * elapsed;

}

void worldToTileCoordinates(float worldX, float worldY, int *gridX, int *gridY) {//divide worldSize/tileSize = gridSize (i.e. which block number along x and y)
	*gridX = (int)(worldX / tileSize);//note we're casting to int
	*gridY = (int)(-worldY / tileSize);
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
	//std::vector<std::vector<Entity>> LevelMap;//vector of [layer] static entities
	//static entity tilemap stored in terms of vertices
	std::vector<float> vertexData;
	std::vector<float> texCoordData;
	//solid tiles in Tiled: 121, 122, 123, 142(black floor)
	//solid tiles after  flaremap conversion (minus 1): 120,121,122;141(this block doesn't really matter)
};


void PlaceEntity(std::string type, float x, float y, GameState* game, FlareMap* map) {//this is for initalizing positions of entities
	//note float x&y can be in pixel coords by inputting x*tileSize where tileSize = 16.0f
	GLuint gameTexture = LoadTexture(RESOURCE_FOLDER"CdH_TILES.png");//don't put this in loop!
	if (type == "DuckPlayer") {
			Entity player;
			//note the duckplayer texture is at (12blocks,3blocks) of the png
			player.sprite.index = 223-1;//our definition of sprite indices is based on the first tile being index=0, so subtract 1 from the n-th sprite in this case n=223th sprite is the first duck, minus 1 to n
			player.sprite.spriteCountX = 20;
			player.sprite.spriteCountY = 20;
			player.position.x = x;//note that this x is 16.0f*tileLocation ~ pixels = 176.0f	//30.0f;//x;//debug
			player.position.y = y;//NOTE THAT THIS GOES DOWN, since we entered a negatiive value for y in PlaceEntity() //0.0f;//y;
			//player.position.x = 10.0f * 16.0f;
			//player.position.y = 20.0f * 16.0f;
			player.sprite.textureID = gameTexture;
			player.sprite.size = tileSize;
			game->Player = player;
	}
	else if (type == "DuckPrincess") {//
		Entity duckPrincess;
		//note the duck texture is at (12blocks,4blocks) of the png
		duckPrincess.sprite.index = 224-1;
		duckPrincess.sprite.spriteCountX = 20;
		duckPrincess.sprite.spriteCountY = 20;
		duckPrincess.position.x = x;
		duckPrincess.position.y = y;
		duckPrincess.sprite.textureID = gameTexture;
		duckPrincess.sprite.size = tileSize;
		game->DuckPrincess = duckPrincess;
	}
	else if (type == "Water") {//I want the water to make the duck be able to fly
		Entity water;
		//note the water texture is at (11blocks,8blocks) of the png
		water.sprite.index = 208-1;
		water.sprite.spriteCountX = 20;
		water.sprite.spriteCountY = 20;
		water.position.x = x;
		water.position.y = y;
		water.sprite.textureID = gameTexture;
		water.sprite.size = tileSize;
		game->WaterVec.push_back(water);

	}
	else if (type == "Boost") {//I want the boost to make the duck go faster
		Entity boost;
		//note the boost texture is at (8blocks,4blocks) of the png
		boost.sprite.index = 144-1;
		boost.sprite.spriteCountX = 20;
		boost.sprite.spriteCountY = 20;
		boost.position.x = x;
		boost.position.y = y;
		boost.sprite.textureID = gameTexture;
		boost.sprite.size = tileSize;
		game->BoostVec.push_back(boost);
	}

}


void InitializeGame(GameState *game) {//set the positions of all the entities
	map.Load("MyLevel1.4.txt");

	float halfSide = tileSize / 2.0f;
	for (int i = 0; i < map.entities.size(); i++) {//place Dynamic entities
		PlaceEntity(map.entities[i].type, map.entities[i].x * tileSize + halfSide, map.entities[i].y * -tileSize + halfSide, game, &map);//push into gamestate the initial positions of DYNAMIMC ENTITIES;
	}

	int sprintCountX = 20; int sprintCountY = 20;
	for (int y = 0; y < map.mapHeight; y++) {
		for (int x = 0; x < map.mapWidth; x++) {
			if (map.mapData[y][x] != 0) {//don't draw the blank tiles
				// add vertices
				float u = (float)(((int)map.mapData[y][x]) % sprintCountX) / (float)sprintCountX;
				float v = (float)(((int)map.mapData[y][x]) / sprintCountX) / (float)sprintCountY;
				float spriteWidth = 1.0f / (float)sprintCountX;
				float spriteHeight = 1.0f / (float)sprintCountY;
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

}


void tilemapRender(GameState game, ShaderProgram* program) {//for rendering static entities
	// draw this data
	GLuint gameTexture = LoadTexture(RESOURCE_FOLDER"CdH_TILES.png");
	//GLuint gameTexture = LoadTexture(RESOURCE_FOLDER"CdH_TILES.png");
	Matrix tilemapModelMatrix;
	program->SetModelMatrix(tilemapModelMatrix);
	//glEnable(GL_BLEND);//don't put this nor glBlendFunc in loop!
	//glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); //gets rid of the clear (black) parts of the png
	//												   //program.Load(RESOURCE_FOLDER"vertex_textured.glsl", RESOURCE_FOLDER"fragment_textured.glsl");
	glBindTexture(GL_TEXTURE_2D, gameTexture); //use fontTexture to draw
	glVertexAttribPointer(program->positionAttribute, 2, GL_FLOAT, false, 0, game.vertexData.data());//the "2" stands for 2 coords per vertex
	glEnableVertexAttribArray(program->positionAttribute);

	glVertexAttribPointer(program->texCoordAttribute, 2, GL_FLOAT, false, 0, game.texCoordData.data());
	glEnableVertexAttribArray(program->texCoordAttribute);

	glUseProgram(program->programID);
	glDrawArrays(GL_TRIANGLES, 0, game.vertexData.size() / 2);//vertexData.size() gives the number of coordinates, so divide by 2 to get the number of vertices
	glDisableVertexAttribArray(program->positionAttribute);
	glDisableVertexAttribArray(program->texCoordAttribute);
}


const Uint8 *keys = SDL_GetKeyboardState(NULL);


void Update(GameState* game, float elapsed) {
	game->Player.Update(elapsed);

	//if entity collides with floor tile, push it back up
	//solid tiles after  flaremap conversion (minus 1): 120,121,122;141(this block doesn't really matter)
	//comeback1

	int gridX; int gridY;
	float halfLength = tileSize / 2.0f;
	for (int y = 0; y < map.mapHeight; y++) {//comeback2
		for (int x = 0; x < map.mapWidth; x++) {
			if (map.mapData[y][x] == 120 || map.mapData[y][x]==121 || map.mapData[y][x]==122 || map.mapData[y][x]==141 ) {
			//if (map.mapData[y][x] == 121) {
				float staticLeft = (float) x * tileSize;
				float staticRight = (float)(x + 1) * tileSize;
				float staticTop = (float)y * tileSize;
				float staticBottom = (float)(y - 1) * tileSize;

				float playerLeft = game->Player.position.x - halfLength;
				float playerRight = game->Player.position.x + halfLength;
				float playerTop = game->Player.position.y + halfLength;
				float playerBottom = game->Player.position.y - halfLength;

				//player
				//worldToTileCoordinates(game->Player.position.x, game->Player.position.y, &gridX, &gridY);
				//if ((gridY - 1 == y) && (gridX))

				if (staticBottom<playerTop && staticTop > playerBottom && staticLeft < playerRight && staticRight > playerLeft) {//collision: bottom of player hit top of static entity
					game->Player.collidedBottom = true;//what to do with this boolean????
					float penetrationY = fabs(staticTop * game->Player.position.y - (game->Player.position.y - halfLength));
					//game->Player.position.y += penetrationY + 0.001f;
					game->Player.position.y += 1.0f + 0.001f;
					game->Player.velocity.y = 0.0f;
				}

				//maybe choosing indices for y,x to declare solids manually will make it easier
				//19th row,12th column ~ 19+14=33 row in .txt -> ~121 is the tile the .txt block value for the tile the player starts above.
				//if (fabs((game->Player.position.y - halfLength) - staticTop) <= halfLength) {
				//	game->Player.collidedBottom = true;
				//	float penetrationY = fabs(staticTop * game->Player.position.y - (game->Player.position.y - halfLength));
				//	game->Player.velocity.y = 0.0f;
				//	game->Player.position.y += 0.000005f;//penetrationY + 0.001f;

				//}
			}
			//game->Player.position.y += 1.0f;
			//game->Player.position.y -= 1.0f;
		}
	}


}

void ProcessInput(GameState* game, float elapsed) {//make a copy of game so that it only applies acceleration for that instance of pressing
	if (keys[SDL_SCANCODE_LEFT]) {
		//player moves left
		game->Player.acceleration.x = -5.0f;
	}
	else if (keys[SDL_SCANCODE_RIGHT]) {
		//player moves left
		game->Player.acceleration.x = 5.0f;
	}
	else if (keys[SDL_SCANCODE_UP]) {
		game->Player.acceleration.y = 5.0f;
	}
	else {//no keys are pressed, have acceleration reset to 0.
		game->Player.acceleration.x = 0.0f;
		game->Player.acceleration.y = 0.0f;
	}
	//Update(&game, elapsed);//this will update the player velocity but acceleration will remain 0 if no key is pressed
}

void Render(GameState game, ShaderProgram* program) {//display the entities on screen
	//render static entities
	glClear(GL_COLOR_BUFFER_BIT);//rendering happens once very frame, but there's no way to remove what's previously rendered, but glClear will make what's previously rendered blank.
	glEnable(GL_BLEND);//don't put this nor glBlendFunc in loop!
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); //gets rid of the clear (black) parts of the png
	tilemapRender(game,program);

	//render dynamic entities (note this is rendered after the static entities b/c it needs to be in the frontmost view)
	game.Player.Render(program);
	game.DuckPrincess.Render(program);
	for (int i = 0; i < game.BoostVec.size(); i++ ) {
		game.BoostVec[i].Render(program);
	}
	for (int i = 0; i < game.WaterVec.size(); i++) {
		game.WaterVec[i].Render(program);
	} //comeback doesn't render

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
			continue;//if elapsed isn't big enough, then ignore what's below, go back to the top of the current while loop.
		}


		while (elapsed >= FIXED_TIMESTEP) {//if enough time for a frame, then update each frame until not enough time for single frame
			//Update(FIXED_TIMESTEP);//uncomment
			
			/* render */
			//viewMatrix.Translate(30.0f, 0.0f, 0.0f);
			viewMatrix.Identity();
			viewMatrix.Translate(-game.Player.position.x, -game.Player.position.y, 0.0f);
			texturedProgram.SetViewMatrix(viewMatrix);
			ProcessInput(&game, elapsed);
			Update(&game,FIXED_TIMESTEP);
			Render(game,&texturedProgram);
			
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
4)Doing glClear(GL_COLOR_BUFFER_BIT) before every render will help get rid of the distortion of outside the stage level map
*/

/*Edit log
1)Edit size of texture
2)Get the player to translate properly
3)Get the view matrix to follow the player
4)Get the tiled background to render properly. Use the tilemap render function (vector of vertices) from 3-05 slides
5)The player is at the top-left of the stage-rendered-screen for some reason. Fixed: utilize a modelmatrix for the static entities
6)After rendering the static entities, it seems that the top-left of the level-layer is at the origin. But the player doesn't seem to be on the 16x16 tile. Also it seems that somehow the waterbottle is also nearly at the right place despite not having coordinates assigned.
fix: offset the dynamic entities by halfSide (i.e. 6.0f) to the right and down.
7.0)Change the scaling
7.1)Allow the player to move (i.e. walk & fly). Apply friction. Make a cap for velocity.

8)Apply gravity & Make some of the static entities physical (e.g. player can't fall under the floor)
9)Apply effects of dynamic entities
*/