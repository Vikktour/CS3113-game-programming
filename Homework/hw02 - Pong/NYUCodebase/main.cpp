/*
Victor Zheng vz365
hw02 Pong Game
Controls:
p1 blue paddle (left)
w = paddle up
s = paddle down
p2 red paddle (right)
up arrow = paddle up
down arrown = paddle down

Concerns: [resolved 1-5, unresolved 6]
1) can't get color of untextured polgyons to work (fixed: need 2 shader programs)
2) how to print to screen?
3) how to get rid of blank spots for the png (i.e. change the empty space into the background color)? (fixed: use glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
4) after scoring and placed into center, ball doesn't go to opposite direction (fixed)
5) wasn't able to avoid the ball reset going up and down vertically situation (will learn in the future with elapsed time)
6) Line 238,251 not sure why the velocity needs to be + to go left and - to go right.
*/

#define STB_IMAGE_IMPLEMENTATION
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

#ifdef _WINDOWS
	#define RESOURCE_FOLDER ""
#else
	#define RESOURCE_FOLDER "NYUCodebase.app/Contents/Resources/"
#endif

SDL_Window* displayWindow;

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


int main(int argc, char *argv[])
{
	SDL_Init(SDL_INIT_VIDEO);
	displayWindow = SDL_CreateWindow("My Game", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640*2, 360*2, SDL_WINDOW_OPENGL);
	SDL_GLContext context = SDL_GL_CreateContext(displayWindow);
	SDL_GL_MakeCurrent(displayWindow, context);
	#ifdef _WINDOWS
		glewInit();
	#endif

		glViewport(0, 0, 640 * 2, 360 * 2);
		ShaderProgram programUntextured;
		ShaderProgram programTextured;
		programTextured.Load(RESOURCE_FOLDER"vertex_textured.glsl", RESOURCE_FOLDER"fragment_textured.glsl");
		programUntextured.Load(RESOURCE_FOLDER"vertex.glsl", RESOURCE_FOLDER"fragment.glsl");
		GLuint greyBallTexture = LoadTexture(RESOURCE_FOLDER"ballGrey.png");
		Matrix projectionMatrix;
		Matrix viewMatrix;
		float leftScreen = -3.55f * 2;
		float rightScreen = 3.55f * 2;
		float topScreen = 2.0f * 2;
		float bottomScreen = -2.0f * 2;
		projectionMatrix.SetOrthoProjection(leftScreen, rightScreen, bottomScreen, topScreen, -1.0f * 2, 1.0f * 2);
		Matrix bPaddleMtx;
		Matrix rPaddleMtx;
		Matrix greyBallMtx;
		


		float lastFrameTicks = 0.0f;
		float ballAngle = 0.0f;

		//background color of the screen
		//glClearColor(0.4f, 0.2f, 0.8f, 0.5f); //purple background
		//glClearColor(1.0f, 1.0f, 1.0f, 1.0f); //white background
	    glClearColor(0.0f, 0.0f, 0.0f, 1.0f); //black background

		float bPaddlexpos = -6.0f;
		float bPaddleypos = 0.0f;
		float rPaddlexpos = 6.0f;
		float rPaddleypos = 0.0f;
		
		float paddleHeight = 1.0f;
		float paddleWidth = 0.2f;
		float paddleSpeed = 0.01f;

		float ballxpos = 0;
		float ballypos = 0;
		float ballRadius = 11.0f/180; //converting radius from 11 pixels to OpenGL units. Note the image is 22x22 pixels.
		float ballSpeed = 5.0;
		float ballxVelocity = ballSpeed;
		float ballyVelocity = 0;
		int randomUpDown;

	SDL_Event event;
	bool done = false;
	while (!done) {
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) {
				done = true;
			}
		}

		float ticks = (float)SDL_GetTicks() / 1000.0f;
		float elapsed = ticks - lastFrameTicks;
		lastFrameTicks = ticks;
		
		float ballTop = ballypos + ballRadius;
		float ballBottom = ballypos - ballRadius;
		float ballLeft = ballxpos - ballRadius;
		float ballRight = ballxpos + ballRadius;

		glClear(GL_COLOR_BUFFER_BIT);
		programUntextured.SetProjectionMatrix(projectionMatrix);
		programTextured.SetProjectionMatrix(projectionMatrix);
		programUntextured.SetViewMatrix(viewMatrix);
		programTextured.SetViewMatrix(viewMatrix);
		

		bPaddleMtx.Identity();
		programUntextured.SetColor(0.0, 0.0, 1.0, 1.0);
		//program.SetColor(0.4f, 0.2f, 0.8f, 0.5f);
		//program.SetColor(0.2f, 0.8f, 0.4f, 1.0f); //set color not working
		bPaddleMtx.Translate(bPaddlexpos, bPaddleypos, 0.0f);
		//program.Load(RESOURCE_FOLDER"vertex.glsl", RESOURCE_FOLDER"fragment.glsl");
		programUntextured.SetModelMatrix(bPaddleMtx);
		float bPaddleVertices[] = { 
			-paddleWidth / 2, -paddleHeight / 2,
			paddleWidth / 2, -paddleHeight / 2,
			paddleWidth / 2, paddleHeight / 2,
			-paddleWidth / 2, -paddleHeight / 2,
			paddleWidth / 2, paddleHeight / 2,
			-paddleWidth / 2, paddleHeight / 2 };
		glVertexAttribPointer(programUntextured.positionAttribute, 2, GL_FLOAT, false, 0, bPaddleVertices);
		glEnableVertexAttribArray(programUntextured.positionAttribute);
		glUseProgram(programUntextured.programID);//use the program to draw untextured polygon
		glDrawArrays(GL_TRIANGLES, 0, 6); //writes to the screen (so set everything before this)
		glDisableVertexAttribArray(programUntextured.positionAttribute);

		rPaddleMtx.Identity();
		programUntextured.SetColor(1.0f, 0.0f, 0.0f, 1.0f); //set color red not working
		rPaddleMtx.Translate(rPaddlexpos, rPaddleypos, 0.0f);
		programUntextured.SetModelMatrix(rPaddleMtx);
		float rPaddleVertices[] = {
			-paddleWidth / 2, -paddleHeight / 2,
			paddleWidth / 2, -paddleHeight / 2,
			paddleWidth / 2, paddleHeight / 2,
			-paddleWidth / 2, -paddleHeight / 2,
			paddleWidth / 2, paddleHeight / 2,
			-paddleWidth / 2, paddleHeight / 2 };
		glVertexAttribPointer(programUntextured.positionAttribute, 2, GL_FLOAT, false, 0, rPaddleVertices);
		glEnableVertexAttribArray(programUntextured.positionAttribute);
		glUseProgram(programUntextured.programID);
		glDrawArrays(GL_TRIANGLES, 0, 6); //writes to the screen (so set everything before this)
		glDisableVertexAttribArray(programUntextured.positionAttribute);


		/* draw grey ball */
		greyBallMtx.Identity(); //start at center
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		greyBallMtx.Translate(ballxpos, ballypos, 0.0f);
		greyBallMtx.Scale(0.5f, 0.5f, 1.0f);
		//program.Load(RESOURCE_FOLDER"vertex_textured.glsl", RESOURCE_FOLDER"fragment_textured.glsl");
		programTextured.SetModelMatrix(greyBallMtx);

		glBindTexture(GL_TEXTURE_2D, greyBallTexture);
		float greyBallVertices[] = { -0.5, -0.5, 0.5, -0.5, 0.5, 0.5, -0.5, -0.5, 0.5, 0.5, -0.5, 0.5 };
		glVertexAttribPointer(programTextured.positionAttribute, 2, GL_FLOAT, false, 0, greyBallVertices);
		glEnableVertexAttribArray(programTextured.positionAttribute);

		float greyBallTexCoords[] = { 0.0, 1.0, 1.0, 1.0, 1.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0, 0.0 };
		glVertexAttribPointer(programTextured.texCoordAttribute, 2, GL_FLOAT, false, 0, greyBallTexCoords);
		glEnableVertexAttribArray(programTextured.texCoordAttribute);

		glUseProgram(programTextured.programID);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		glDisableVertexAttribArray(programTextured.positionAttribute);
		glDisableVertexAttribArray(programTextured.texCoordAttribute);
		/* draw grey ball */

		const Uint8 *keys = SDL_GetKeyboardState(NULL);
		

		if (keys[SDL_SCANCODE_W]) {
			bPaddleypos += paddleSpeed;
			if (bPaddleypos + paddleHeight / 2 > topScreen) //don't let the paddle go off bounds
				bPaddleypos -= paddleSpeed;
		}
		else if (keys[SDL_SCANCODE_S]) {
			bPaddleypos -= paddleSpeed;
			if (bPaddleypos - paddleHeight / 2 < bottomScreen)
				bPaddleypos += paddleSpeed;
		}
		else if (keys[SDL_SCANCODE_UP]) {
			rPaddleypos += paddleSpeed;
			if (rPaddleypos + paddleHeight / 2 > topScreen)
				rPaddleypos -= paddleSpeed;
		}
		else if (keys[SDL_SCANCODE_DOWN]) {
			rPaddleypos -= paddleSpeed;
			if (rPaddleypos - paddleHeight / 2 < bottomScreen)
				rPaddleypos += paddleSpeed;
		}

		/*can't do this because ball will keep changing direction when it hit the wall making it stuck. Fix: just implement -1 or 1 hardcode after each condition*/
		if (ballTop >= topScreen)
			ballyVelocity = -sin(ballAngle)*ballSpeed; //if ball hits the top, then make it go down
		else if(ballBottom <= bottomScreen)
			ballyVelocity = sin(ballAngle)*ballSpeed; //if ball hits the bottom, then make it go up

		/* Ball leaving screen to left/right */
		if (ballRight < leftScreen) { //player 2 scores, reposition ball to center and make it move left
			ballxpos = 0.0f;
			ballypos = 0.0f;
			ballAngle = rand() % 21 *3.14/180;
			ballxVelocity = cos(ballAngle)*ballSpeed; //not sure why I need +cos to make the ball go left
			if (rand() % 2 == 0)
				randomUpDown = -1;
			else
				randomUpDown = 1;
			ballyVelocity = randomUpDown * sin(ballAngle)*ballSpeed;
			glClearColor(1.0, 0.5f, 0.0f, 1.0f);//change background color to orange indicating player 2 (red) wins
			//std::cout << "Player 2 scores!";
		}
		if (ballLeft > rightScreen) { //player 1 scores, resposition ball to center and make it move right
			ballxpos = 0.0f;
			ballypos = 0.0f;
			ballAngle = rand() % 21*3.14/180;
			ballxVelocity = -cos(ballAngle)*ballSpeed; //not sure why I need -cos to make the ball move right
			if (rand() % 2 == 0)
				randomUpDown = -1;
			else
				randomUpDown = 1;
			ballyVelocity = randomUpDown * sin(ballAngle)*ballSpeed;
			glClearColor(0.0, 1.0f, 1.0f, 1.0f); //change background color to skyblue indicating player 1 (blue) wins
			//std::cout << "Player 1 scores!";
		}

		/* Ball colliding with paddle */
		bool noCollisionWithBluePaddle = (ballLeft > bPaddlexpos + paddleWidth / 2) || (ballBottom > bPaddleypos + paddleHeight/2) || (ballTop < bPaddleypos - paddleHeight/2);
		if (!noCollisionWithBluePaddle) {//if there's a collision with the ball and blue paddle
			if (ballxpos >= bPaddlexpos) {//ball collides and still on the right part of the blue paddle, change the direction to right and change angle
				ballAngle = (rand() % 46) * 3.14/180;
				ballxVelocity = cos(ballAngle)*ballSpeed;
				if (ballyVelocity > 0)
					ballyVelocity = -sin(ballAngle)*ballSpeed;
				else if (ballyVelocity <= 0)
					ballyVelocity = sin(ballAngle)*ballSpeed;
			}
			else if (ballxpos < bPaddlexpos) {//ball collides but on the left side of the blue paddle, let it exit
				ballAngle = (rand() % 46) *3.14/180;
				ballxVelocity = cos(ballAngle)*ballSpeed; //no change in x-direction
				if (ballyVelocity > 0)
					ballyVelocity = -sin(ballAngle)*ballSpeed;
				else if (ballyVelocity <= 0)
					ballyVelocity = sin(ballAngle)*ballSpeed;
			}
			
		}
		bool noCollisionWithRedPaddle = (ballRight < rPaddlexpos - paddleWidth / 2) || (ballBottom > rPaddleypos + paddleHeight/2) || (ballTop < rPaddleypos - paddleHeight/2);
		if (!noCollisionWithRedPaddle) {//if there's a collision with the ball and red paddle
			if (ballxpos <= rPaddlexpos) {//ball collides and still on the left part of the red paddle, change direction to left and change angle
				ballAngle = (rand() % 46)*3.14/180;
				ballxVelocity = -cos(ballAngle)*ballSpeed;
				if (ballyVelocity > 0)
					ballyVelocity = -sin(ballAngle)*ballSpeed;
				else if (ballyVelocity <= 0)
					ballyVelocity = sin(ballAngle)*ballSpeed;
				
			}
			else if (ballxpos > rPaddlexpos) {//ball collides but on the right side of the red paddle, let it exit
				ballAngle = (rand() % 46)*3.14/180;
				ballxVelocity = cos(ballAngle)*ballSpeed;//no change in x-direction
				if (ballyVelocity > 0)
					ballyVelocity = -sin(ballAngle)*ballSpeed;
				else if (ballyVelocity <= 0)
					ballyVelocity = sin(ballAngle)*ballSpeed;
			}
		}
		
		ballxpos += ballxVelocity * elapsed;
		ballypos += ballyVelocity * elapsed;

		SDL_GL_SwapWindow(displayWindow);
	}

	SDL_Quit();
	return 0;
}


/*Notes
1)I didn't use a variable for direction because the ball will get stuck (i.e. changing directions infinite times)
*/
