/*
Victor Zheng
hw05 SAT collision
Controls: up down left right keys to move

Debug fixes:
0)Got rid of accleration aspect of the game for easier movement
1)objects were in constant collision, penetration fix makes them pushed farther and farther away from the center. Fix:

I want to have an object revolve relative to another
*/


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
#include "SatCollision.h"

#define STB_IMAGE_IMPLEMENTATION //to allow assert(false)

#ifdef _WINDOWS
	#define RESOURCE_FOLDER ""
#else
	#define RESOURCE_FOLDER "NYUCodebase.app/Contents/Resources/"
#endif

//define timesteps to update based on FPS, e.g. 60fps means update every 1/60 second ~ 0.166666f
#define FIXED_TIMESTEP 0.0166666f
#define MAX_TIMESTEPS 6

/* Adjustable global variables */
//screen
float screenWidth = 720.0f;
float screenHeight = 480.0f;
float topScreen = 7.0f;
float bottomScreen = -7.0f;
float aspectRatio = screenWidth / screenHeight;
float leftScreen = -aspectRatio * topScreen;
float rightScreen = aspectRatio * topScreen;

//time and FPS
float accumulator = 0.0f; float lastFrameTicks = 0.0f;

//game
float friction = 0.5f;

//more global variables
ShaderProgram untexturedProgram;
SDL_Window* displayWindow;
const Uint8 *keys = SDL_GetKeyboardState(NULL);


//float lerp(float v0, float v1, float t) {//used for friction. Linear Interpolation (LERP)
//	return (1.0f - t)*v0 + t * v1;//note that everytime this function is used, if v1 is 0 then we're going to keep dividing v0 to eventually go to 0.
//}

class Vector3 { //keep track of object coordinates
public:
	Vector3(float x = 0.0f, float y = 0.0f, float z = 0.0f) : x(x), y(y), z(z) {}
	float x;
	float y;
	float z;
};
Vector3 operator*(Matrix matrix, std::pair<float, float> point) {//matrix*vector. Note matrix is indexed as a column major so matrix[3][2] is column3,row2. Also assume z is 0.
	Vector3 retVec3(0, 0, 0);
	retVec3.x = matrix.m[0][0] * point.first + matrix.m[1][0] * point.second + matrix.m[2][0]*0.0f + matrix.m[3][0]*1.0f;
	retVec3.y = matrix.m[0][1] * point.first + matrix.m[1][1] * point.second + matrix.m[2][1] * 0.0f + matrix.m[3][1] * 1.0f;
	retVec3.z = matrix.m[0][2] * point.first + matrix.m[1][2] * point.second + matrix.m[2][2] * 0.0f + matrix.m[3][2] * 1.0f;
	//4th row dot product is omitted since we only need it for the translation of x,y,z
	return retVec3;
}

//class GameState;
GameState* game;//instantiate class for use in entity

class Entity {//untextured for the purpose of this assignment
public:
	//Entity() {}//default constructor
	Entity() : velocity(0.0f, 0.0f, 0.0f), acceleration(0.0f, 0.0f, 0.0f) {//default constructor with initialized values
		//make the color of each entity black, until assigned a new color
		for (int i = 0; i < 3; i++) {
			color.push_back(0.0f);
		}
		color.push_back(1.0f);
		//initialize rotation
		rotation = 0.0f;
		rotationSpeed = 0.0f; translationSpeed = 0.0f;
		rotateTime = false;
		revolve = false; relativeToPlayer = false;
	};
	//void Draw(ShaderProgram* program);
	void Update(float elapsed);
	void Render(ShaderProgram* program, float elapsed, GameState player);
	bool CollidesWith(Entity *entity);
	Vector3 position;
	Vector3 size;
	Vector3 velocity;
	Vector3 acceleration;
	float entityMaxVelocity;
	bool collidedTop;
	bool collidedBottom;
	bool collidedLeft;
	bool collidedRight;
	std::vector<float> EntityTriangleVertices;//vertices for draw
	std::vector<std::pair<float,float>> EntityVertices;//without the repeated points from triangles
	std::vector<float> color;//rgba
	bool rotateTime;//rotation depends on time
	float rotationSpeed; float translationSpeed;
	bool revolve; bool relativeToPlayer;
	float rotation;
	Matrix modelMatrixMember;
};



void Entity::Update(float elapsed) {
	//velocity.x += acceleration.x * elapsed;
	//velocity.y += acceleration.y * elapsed;
	//speed limit
	/*if (velocity.x > entityMaxVelocity) {
		velocity.x = entityMaxVelocity;
	}
	else if (velocity.x < -entityMaxVelocity) {
		velocity.x = -entityMaxVelocity;
	}
	if (velocity.y > entityMaxVelocity) {
		velocity.y = entityMaxVelocity;
	}
	else if (velocity.y < -entityMaxVelocity) {
		velocity.y = -entityMaxVelocity;
	}
	*/

	//acceleration.x = 5.0f;//debug
	position.x += velocity.x * elapsed;
	position.y += velocity.y * elapsed;
	
	if (rotateTime == true) {
		rotation += elapsed*rotationSpeed;
	}
}

void Entity::Render(ShaderProgram* program, float elapsed, GameState player) {
	modelMatrixMember.Identity();
	if (relativeToPlayer == true) {
		//modelMatrixMember.Translate()
	}
	if (revolve == true) {//rotate first then translate
		modelMatrixMember.Rotate(rotation);
	}
	modelMatrixMember.Translate(position.x, position.y, position.z);
	modelMatrixMember.Rotate(rotation);
	//untexturedProgram.SetColor(0.0f, 1.0f, 0.0f, 1.0f);
	untexturedProgram.SetColor(color[0], color[1], color[2], color[3]);
	untexturedProgram.SetModelMatrix(modelMatrixMember);

	glVertexAttribPointer(program->positionAttribute, 2, GL_FLOAT, false, 0, EntityTriangleVertices.data());//the "2" stands for 2 coords per vertex
	glEnableVertexAttribArray(program->positionAttribute);

	glUseProgram(program->programID);
	glDrawArrays(GL_TRIANGLES, 0, EntityTriangleVertices.size() / 2);//vertexData.size() gives the number of coordinates, so divide by 2 to get the number of vertices
	glDisableVertexAttribArray(program->positionAttribute);
}

class GameState {
public:
	Entity Player;
	std::vector<Entity> Object;
};

void ProcessInput(GameState* game) {
	if (keys[SDL_SCANCODE_LEFT]) {
		//player moves left
		//game->Player.position.x -= 0.1f;//debugg
		//game->Player.acceleration.x = -5.0f;
		game->Player.velocity.x = -3.0f;
	}
	if (keys[SDL_SCANCODE_RIGHT]) {
		//game->Player.acceleration.x = 5.0f;
		game->Player.velocity.x = 3.0f;
	}
	if (keys[SDL_SCANCODE_UP]) {
		//game->Player.acceleration.y = 5.0f;
		game->Player.velocity.y = 3.0f;
	}
	if (keys[SDL_SCANCODE_DOWN]) {
		//game->Player.acceleration.y = -5.0f;
		game->Player.velocity.y = -3.0f;
	}
	//no keys are pressed in axis, set corresponding axis speed to 0
	if (!(keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_RIGHT])){
		game->Player.velocity.x = 0.0f;
	}
	if (!(keys[SDL_SCANCODE_UP] || keys[SDL_SCANCODE_DOWN])) {
		game->Player.velocity.y = 0.0f;
	}
}

void Update(GameState* game, float elapsed) {
	game->Player.Update(elapsed);
	//game->Player.velocity.x = lerp(game->Player.velocity.x, 0.0f, elapsed*friction);
	//game->Player.velocity.y = lerp(game->Player.velocity.y, 0.0f, elapsed*friction);
	for (int i = 0; i < game->Object.size(); i++) {
		game->Object[i].Update(elapsed);
	}

	/* Check for Collision */

	//check for player colliding with other objects
	//loop throught objects in the game to test for collision with player
	for (int m = 0; m < game->Object.size(); m++) {
		std::pair<float, float> penetration;
		std::vector<std::pair<float, float>> e1Points;// = game->Player.EntityVertices;
		std::vector<std::pair<float, float>> e2Points;// = game->Object[i].EntityVertices;


		for (int p = 0; p < game->Player.EntityVertices.size(); p++) {
			Vector3 point = game->Player.modelMatrixMember * game->Player.EntityVertices[p];
			e1Points.push_back(std::make_pair(point.x, point.y));
		}

		for (int p = 0; p < game->Object[m].EntityVertices.size(); p++) {
			Vector3 point = game->Object[m].modelMatrixMember * game->Object[m].EntityVertices[p];
			e2Points.push_back(std::make_pair(point.x, point.y));
		}

		bool collided = CheckSATCollision(e1Points, e2Points, penetration);

		if (collided) {
			game->Player.position.x += (penetration.first * 0.5f);
			game->Player.position.y += (penetration.second * 0.5f);

			game->Object[m].position.x -= (penetration.first * 0.5f);
			game->Object[m].position.y -= (penetration.second * 0.5f);
			collided = false;
		}
	}

	//check for objects colliding with each other
	for (int i = 0; i < game->Object.size(); i++) {//object 1
		for (int j = i + 1; j < game->Object.size(); j++) {//object 2
			std::pair<float, float> penetration;
			std::vector<std::pair<float, float>> e1Points;
			std::vector<std::pair<float, float>> e2Points;

			for (int p = 0; p < game->Object[i].EntityVertices.size(); p++) {
				Vector3 point = game->Object[i].modelMatrixMember * game->Object[i].EntityVertices[p];
				e1Points.push_back(std::make_pair(point.x, point.y));
			}
			for (int p = 0; p < game->Object[j].EntityVertices.size(); p++) {
				Vector3 point = game->Object[j].modelMatrixMember * game->Object[j].EntityVertices[p];
				e2Points.push_back(std::make_pair(point.x, point.y));
			}

			bool collided = CheckSATCollision(e1Points, e2Points, penetration);

			if (collided) {
				game->Object[i].position.x += (penetration.first * 0.5f);
				game->Object[i].position.y += (penetration.second * 0.5f);

				game->Object[j].position.x -= (penetration.first * 0.5f);
				game->Object[j].position.y -= (penetration.second * 0.5f);
				collided = false;
			}
		}
	}




	/* check for collision */
}
void Render(GameState* game, ShaderProgram* program, float elapsed, GameState player) {
	glClear(GL_COLOR_BUFFER_BIT);//create a full screen of the clear color (e.g. use this to cover up the objects previously rendered)
	game->Player.Render(program,elapsed);
	for (int i = 0; i < game->Object.size(); i++) {
		game->Object[i].Render(program,elapsed, player);
	}
}

int main(int argc, char *argv[])
{
	SDL_Init(SDL_INIT_VIDEO);
	displayWindow = SDL_CreateWindow("SAT Collision", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, screenWidth, screenHeight, SDL_WINDOW_OPENGL);//gives the screen it's name, width, and height
	SDL_GLContext context = SDL_GL_CreateContext(displayWindow);
	SDL_GL_MakeCurrent(displayWindow, context);
	#ifdef _WINDOWS
		glewInit();
	#endif

		glViewport(0, 0, screenWidth, screenHeight);//sets size and offset of rendering area
		Matrix projectionMatrix;
		Matrix viewMatrix; //where in the world you're looking at note that identity means your view is at (0,0)
		Matrix modelMatrix;
		projectionMatrix.SetOrthoProjection(leftScreen, rightScreen, bottomScreen, topScreen, -1.0f, 1.0f);//defining the bounds for the screen for the view matrix

		untexturedProgram.Load(RESOURCE_FOLDER"vertex.glsl", RESOURCE_FOLDER"fragment.glsl");
		glClearColor(1.0f, 0.5f, 0.5f, 1.0f);//set color of rendered screen

		GameState game;

		/* Create Square player */
		Entity square;
		square.EntityTriangleVertices = {//vertices for draw
			-0.5f, 0.5f,
			-0.5f, -0.5f,
			0.5f,-0.5f,
			-0.5f,0.5f,
			0.5f,-0.5f,
			0.5f,0.5f
		};

		square.EntityVertices = {//pair vertices for collision detection
			{-0.5f, 0.5f},{-0.5f,-0.5f},{0.5f, -0.5f},{0.5f,0.5f}	
		};


		square.color = { 0.0f, 1.0f, 1.0f,1.0f };

		game.Player = square;
		game.Player.entityMaxVelocity = 5.0f;
		//game.Player.velocity.x = 0; game.Player.velocity.y = 0; game.Player.acceleration.x = 0; game.
		/* Create Square play */

		/* Create Triangle */
		Entity triangle;
		triangle.EntityTriangleVertices = {
			-0.5f,-0.5f,
			0.5f,-0.5f,
			0.0f,0.5f,
		};
		triangle.EntityVertices = {
			{-0.5f,-0.5f}, {0.5f, -0.5f}, {0.0f,0.5f}
		};
		triangle.position.x = 2.0f;
		triangle.position.y = 2.0f;

		triangle.rotateTime = true;
		triangle.revolve = true; triangle.relativeToPlayer = true;
		triangle.rotationSpeed = 2.0f;
		triangle.translationSpeed = 2.0f;

		game.Object.push_back(triangle);
		/* Create Triangle */

		/* Create Rectangle */
		Entity rectangle;
		rectangle.EntityTriangleVertices = {
			-0.5f, 1.0f,
			-0.5f, -1.0f,
			0.5f,-1.0f,
			-0.5f,1.0f,
			0.5f,-1.0f,
			0.5f,1.0f
		};
		rectangle.EntityVertices = {
			{-0.5f, 1.0f}, {-0.5f,-1.0f}, {0.5f,-1.0f},{0.5f,1.0f}
		};
		rectangle.position.x = -2.0f;
		rectangle.position.y = -2.0f;
		rectangle.color = { 1.0f, 1.0f, 1.0f,1.0f };
		game.Object.push_back(rectangle);
		/* Create Rectangle */

		/* Create Diamond */
		Entity diamond;
		diamond.EntityTriangleVertices = {//diamond
			-0.5f,0.0f,
			0.5f,0.0f,
			0.0f,1.0f,

			-0.5f,0.0f,
			0.0f,-1.0f,
			0.5f,0.0f
		};
		diamond.EntityVertices = {
			{-0.5f,0.0f}, {0.0f,-1.0f}, {0.5f,0.0f}, {0.0f,1.0f}
		};
		diamond.position.x = leftScreen + 1.0f; diamond.position.y = 0.0f;
		diamond.velocity.x = 2.0f;
		diamond.rotation = 45.0f;
		diamond.rotateTime = true; diamond.rotationSpeed = 5.0f;
		game.Object.push_back(diamond);
		/* Create Diamond */

		/* Create triangle2 */
		Entity triangle2;
		triangle2.EntityTriangleVertices = {
			-0.5f,-0.5f,
			0.5f,-0.5f,
			0.0f,1.0f,
		};
		triangle2.EntityVertices = {
			{ -0.5f,-0.5f },{ 0.5f, -0.5f },{ 0.0f,1.0f }
		};
		triangle2.position.x = 0.0f;
		triangle2.position.y = 3.0f;

		triangle2.rotateTime = true;
		triangle2.rotationSpeed = 2.0f;

		game.Object.push_back(triangle2);
		/* Create triangle2 */


	SDL_Event event;
	bool done = false;
	while (!done) {
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) {
				done = true;
			}
		}


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

			untexturedProgram.SetViewMatrix(viewMatrix);
			untexturedProgram.SetProjectionMatrix(projectionMatrix);
			//viewMatrix.Identity();//comeback
			//viewMatrix.Translate(-game.Player.position.x, -game.Player.position.y, 0.0f);
			//untexturedProgram.SetViewMatrix(viewMatrix);
			ProcessInput(&game);
			Update(&game, FIXED_TIMESTEP);
			//game.Player.position.x = 1.0f;//debug
			Render(&game, &untexturedProgram, FIXED_TIMESTEP, game.Player);

			/* render */

			elapsed -= FIXED_TIMESTEP;
		}
		accumulator = elapsed;

		
		SDL_GL_SwapWindow(displayWindow);
	}

	SDL_Quit();
	return 0;
}
