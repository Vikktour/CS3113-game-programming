/*
Victor Zheng
vz365
hw01
untextured?
*/

#ifdef _WINDOWS
	#include <GL/glew.h>
#endif

#include <SDL.h>
#include <SDL_opengl.h>
#include <SDL_image.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <iostream>
#include "ShaderProgram.h"

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

	//glClearColor(0.4f, 0.2f, 0.4f, 1.0f); //setting clear color of the screen
	//glClear(GL_COLOR_BUFFER_BIT); //clear the screen with the above set clear value
	


	SDL_Init(SDL_INIT_VIDEO);
	displayWindow = SDL_CreateWindow("My Game", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640*2, 360*2, SDL_WINDOW_OPENGL); //16:9 display window ratio
	SDL_GLContext context = SDL_GL_CreateContext(displayWindow);
	SDL_GL_MakeCurrent(displayWindow, context);
	#ifdef _WINDOWS
		glewInit();
	#endif

		glViewport(0, 0, 640*2, 360*2); //16:9 viewport ratio
		ShaderProgram program;
		program.Load(RESOURCE_FOLDER"vertex_textured.glsl", RESOURCE_FOLDER"fragment_textured.glsl");
		GLuint snailTexture = LoadTexture(RESOURCE_FOLDER"snail.png");
		//program.SetColor(0.2f, 0.8f, 0.4f, 1.0f); //color of rednered polygon
		
		Matrix projectionMatrix;
		projectionMatrix.SetOrthoProjection(-3.55, 3.55, -2.0f, 2.0f, -1.0f, 1.0f);
		//Matrix modelMatrix;
		Matrix snailModelMatrix;
		Matrix snail2ModelMatrix;
		Matrix alienYModelMatrix;
		Matrix viewMatrix;
		
		
		GLuint snail2Texture = LoadTexture(RESOURCE_FOLDER"snail.png");
		GLuint alienYTexture = LoadTexture(RESOURCE_FOLDER"alienYellow.png");

		float lastFrameTicks = 0.0f;



		

	SDL_Event event;
	bool done = false;
	while (!done) {
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT || event.type == SDL_WINDOWEVENT_CLOSE) {
				done = true;
			}
		}
		glClear(GL_COLOR_BUFFER_BIT);

		glUseProgram(program.programID);
		program.SetModelMatrix(snailModelMatrix);
		program.SetModelMatrix(snail2ModelMatrix);
		program.SetModelMatrix(alienYModelMatrix);
		program.SetProjectionMatrix(projectionMatrix);
		program.SetViewMatrix(viewMatrix);

		
		//snailModelMatrix.Identity(); //create snail
		glBindTexture(GL_TEXTURE_2D, snailTexture);
		//float snailVertices[] = { -0.5, -0.5, 0.5, -0.5, 0.5, 0.5, -0.5, -0.5, 0.5, 0.5, -0.5, 0.5 };
		float snailVertices[] = { -2.0,-0.5,-1.0,-0.5,-1.0,0.5,-2.0,-0.5,-1.0,0.5,-2.0,0.5};
		glVertexAttribPointer(program.positionAttribute, 2, GL_FLOAT, false, 0, snailVertices);
		glEnableVertexAttribArray(program.positionAttribute);

		//float snailTexCoords[] = { 0.0, 1.0, 1.0, 1.0, 1.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0, 0.0 };
		float snailTexCoords[] = { 0.0, 1.0, 1.0, 1.0, 1.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0, 0.0 };
		glVertexAttribPointer(program.texCoordAttribute, 2, GL_FLOAT, false, 0, snailTexCoords);
		glEnableVertexAttribArray(program.texCoordAttribute);

		glDrawArrays(GL_TRIANGLES, 0, 6);
		glDisableVertexAttribArray(program.positionAttribute);
		glDisableVertexAttribArray(program.positionAttribute);



	/*	float ticks = (float)SDL_GetTicks() / 1000.0f;
		float elapsed = ticks - lastFrameTicks;
		lastFrameTicks = ticks;
		float angle = 0;
		angle += elapsed;*/

		//snailModelMatrix.Rotate( 45.0f * (3.1415926f / 180.0f)); //doesn't work ever since I changed modelMatrix --> snailModelMartrix
		//snailModelMatrix.Translate(2.0f, 0.0f, 0.0f);


		//snail2ModelMatrix.Identity(); //create snail2
		glBindTexture(GL_TEXTURE_2D, snail2Texture);
		float snail2Vertices[] = { -0.5, -0.5, 0.5, -0.5, 0.5, 0.5, -0.5, -0.5, 0.5, 0.5, -0.5, 0.5 };
		glVertexAttribPointer(program.positionAttribute, 2, GL_FLOAT, false, 0, snail2Vertices);
		glEnableVertexAttribArray(program.positionAttribute);

		float snail2TexCoords[] = { 0.0, 1.0, 1.0, 1.0, 1.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0, 0.0 };
		glVertexAttribPointer(program.texCoordAttribute, 2, GL_FLOAT, false, 0, snail2TexCoords);
		glEnableVertexAttribArray(program.texCoordAttribute);

		glDrawArrays(GL_TRIANGLES, 0, 6);
		glDisableVertexAttribArray(program.positionAttribute);
		glDisableVertexAttribArray(program.positionAttribute);

		//alienYModelMatrix.Identity(); //create alienY
		glBindTexture(GL_TEXTURE_2D, alienYTexture);
		float alienYVertices[] = { -0.5, -2.0, 0.5, -2.0, 0.5, -1.0, -0.5, -2.0, 0.5, -1.0, -0.5, -1.0 };
		glVertexAttribPointer(program.positionAttribute, 2, GL_FLOAT, false, 0, alienYVertices);
		glEnableVertexAttribArray(program.positionAttribute);

		float alienYTexCoords[] = { 0.0, 1.0, 1.0, 1.0, 1.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0, 0.0 };
		glVertexAttribPointer(program.texCoordAttribute, 2, GL_FLOAT, false, 0, alienYTexCoords);
		glEnableVertexAttribArray(program.texCoordAttribute);

		glDrawArrays(GL_TRIANGLES, 0, 6);
		glDisableVertexAttribArray(program.positionAttribute);
		glDisableVertexAttribArray(program.positionAttribute);

		SDL_GL_SwapWindow(displayWindow);
		snailModelMatrix.Rotate(45.0f * (3.1415926f / 180.0f));

		//glClear(GL_COLOR_BUFFER_BIT); //clears out the snail&alien
		//SDL_GL_SwapWindow(displayWindow);
	}

	SDL_Quit();
	return 0;
}
