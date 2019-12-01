#include <stdio.h>
#include <stdlib.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <time.h>
#include <PerlinNoise.hpp>
using namespace glm;

const double windowWidth = 1024;
const double windowHeight = 768;

std::string getVertexShaderString() {
	return
		"#version 330 core\n"
		"layout(location = 0) in vec3 vertexPosition_modelspace;\n"
		"//layout(location = 1) in vec3 vertexColor;\n"

		"uniform mat4 MVP;\n"
		"uniform float drawCol;\n"

		"out float fragmentColor;\n"
		"out float zPos;\n"

		"void main() {\n"
		"	// Output position of the vertex, in clip space : MVP * position\n"
		"	gl_Position = MVP * vec4(vertexPosition_modelspace, 1);\n"
		"	fragmentColor = drawCol;\n"
		"	zPos = vertexPosition_modelspace.y;\n"
		"}";
}

std::string getFragmentShaderString() {
	return
		"#version 330 core\n"
		"in float fragmentColor;\n"
		"in float zPos;\n"
		"out vec3 color;\n"

		"void main() {\n"
		"	float colAmount = fragmentColor + 0.13 * zPos - 0.5;\n"
		"	if (abs(zPos - 5.0) < 0.001) {\n"
		"		colAmount += 0.5;\n"
		"	}\n"
		"	color = vec3(colAmount, 0.1 * colAmount, 0.7 * colAmount);//fragmentColor;\n"
		"}\n";
}

GLuint LoadShaders(const char* vertex_file_path, const char* fragment_file_path) {

	// Create the shaders
	GLuint VertexShaderID = glCreateShader(GL_VERTEX_SHADER);
	GLuint FragmentShaderID = glCreateShader(GL_FRAGMENT_SHADER);

	// Read the Vertex Shader code from the file
	std::string VertexShaderCode = getVertexShaderString();
	/*std::ifstream VertexShaderStream(vertex_file_path, std::ios::in);
	if (VertexShaderStream.is_open()) {
		std::stringstream sstr;
		sstr << VertexShaderStream.rdbuf();
		VertexShaderCode = sstr.str();
		VertexShaderStream.close();
	}
	else {
		printf("Impossible to open %s. Are you in the right directory ? Don't forget to read the FAQ !\n", vertex_file_path);
		getchar();
		return 0;
	}*/

	// Read the Fragment Shader code from the file
	std::string FragmentShaderCode = getFragmentShaderString();
	/*std::ifstream FragmentShaderStream(fragment_file_path, std::ios::in);
	if (FragmentShaderStream.is_open()) {
		std::stringstream sstr;
		sstr << FragmentShaderStream.rdbuf();
		FragmentShaderCode = sstr.str();
		FragmentShaderStream.close();
	}*/

	GLint Result = GL_FALSE;
	int InfoLogLength;

	// Compile Vertex Shader
	printf("Compiling shader : %s\n", vertex_file_path);
	char const* VertexSourcePointer = VertexShaderCode.c_str();
	glShaderSource(VertexShaderID, 1, &VertexSourcePointer, NULL);
	glCompileShader(VertexShaderID);

	// Check Vertex Shader
	glGetShaderiv(VertexShaderID, GL_COMPILE_STATUS, &Result);
	glGetShaderiv(VertexShaderID, GL_INFO_LOG_LENGTH, &InfoLogLength);
	if (InfoLogLength > 0) {
		std::vector<char> VertexShaderErrorMessage(InfoLogLength + 1);
		glGetShaderInfoLog(VertexShaderID, InfoLogLength, NULL, &VertexShaderErrorMessage[0]);
		printf("%s\n", &VertexShaderErrorMessage[0]);
	}

	// Compile Fragment Shader
	printf("Compiling shader : %s\n", fragment_file_path);
	char const* FragmentSourcePointer = FragmentShaderCode.c_str();
	glShaderSource(FragmentShaderID, 1, &FragmentSourcePointer, NULL);
	glCompileShader(FragmentShaderID);

	// Check Fragment Shader
	glGetShaderiv(FragmentShaderID, GL_COMPILE_STATUS, &Result);
	glGetShaderiv(FragmentShaderID, GL_INFO_LOG_LENGTH, &InfoLogLength);
	if (InfoLogLength > 0) {
		std::vector<char> FragmentShaderErrorMessage(InfoLogLength + 1);
		glGetShaderInfoLog(FragmentShaderID, InfoLogLength, NULL, &FragmentShaderErrorMessage[0]);
		printf("%s\n", &FragmentShaderErrorMessage[0]);
	}

	// Link the program
	printf("Linking program\n");
	GLuint ProgramID = glCreateProgram();
	glAttachShader(ProgramID, VertexShaderID);
	glAttachShader(ProgramID, FragmentShaderID);
	glLinkProgram(ProgramID);

	// Check the program
	glGetProgramiv(ProgramID, GL_LINK_STATUS, &Result);
	glGetProgramiv(ProgramID, GL_INFO_LOG_LENGTH, &InfoLogLength);
	if (InfoLogLength > 0) {
		std::vector<char> ProgramErrorMessage(InfoLogLength + 1);
		glGetProgramInfoLog(ProgramID, InfoLogLength, NULL, &ProgramErrorMessage[0]);
		printf("%s\n", &ProgramErrorMessage[0]);
	}

	glDetachShader(ProgramID, VertexShaderID);
	glDetachShader(ProgramID, FragmentShaderID);

	glDeleteShader(VertexShaderID);
	glDeleteShader(FragmentShaderID);

	return ProgramID;
}

int main(void)
{
	#pragma region Noise
	srand(time(NULL));
	std::uint32_t seed = rand() % 65535;
	fprintf(stderr, "Random seed is %d\n", seed);
	double frequency = 4; // between 0.1 and 64.0
	int octaves = 4; // between 1 and 16
	const int noiseSize = 128;
	double fstep = noiseSize / frequency;
	//float noise[noiseSize][noiseSize];
	std::vector<float> noise;

	const siv::PerlinNoise perlin(seed);
	for (int i = 0; i < noiseSize; i++) {
		for (int j = 0; j < noiseSize; j++) {
			noise.push_back( max(perlin.octaveNoise0_1(i / fstep, j / fstep, octaves) * 10 - 4, 0.0) * 2.0 );
		}
	}
	#pragma endregion

	#pragma region Init
	// Initialise GLFW
	glewExperimental = true; // Needed for core profile
	if (!glfwInit())
	{
		fprintf(stderr, "Failed to initialize GLFW\n");
		return -1;
	}

	glfwWindowHint(GLFW_SAMPLES, 4); // 4x antialiasing
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3); // We want OpenGL 3.3
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); // We don't want the old OpenGL 

	// Open a window and create its OpenGL context
	GLFWwindow* window;
	window = glfwCreateWindow(windowWidth, windowHeight, "Hello World", NULL, NULL);
	if (window == NULL) {
		fprintf(stderr, "Failed to open GLFW window. If you have an Intel GPU, they are not 3.3 compatible.\n");
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window); // Initialize GLEW
	glewExperimental = true; // Needed in core profile
	if (glewInit() != GLEW_OK) {
		fprintf(stderr, "Failed to initialize GLEW\n");
		return -1;
	}
	#pragma endregion

	#pragma region Vertices
	GLuint VertexArrayID;
	glGenVertexArrays(1, &VertexArrayID);
	glBindVertexArray(VertexArrayID);

	// Vertices
	static GLfloat g_vertex_buffer_data[noiseSize * noiseSize * 3];
	int index = 0;
	for (int j = 0; j < noiseSize; j++) {
		for (int i = 0; i < noiseSize; i++) {
			g_vertex_buffer_data[index] = i - noiseSize / 2;
			index++;
			g_vertex_buffer_data[index] = noise.at(i + noiseSize * j);//noise[i][j];
			index++;
			g_vertex_buffer_data[index] = j - noiseSize / 2;
			index++;
		}
	}

	GLuint vertexbuffer;
	glGenBuffers(1, &vertexbuffer);
	glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
	glBufferData(GL_ARRAY_BUFFER, noiseSize * noiseSize * 3 * sizeof(GLfloat), g_vertex_buffer_data, GL_STATIC_DRAW);

	// Vertex indices
	std::vector<unsigned short> indices;
	for (int j = 0; j < noiseSize-1; j++) {
		for (int i = 0; i < noiseSize-1; i++) {
			index = i + noiseSize * j;
			indices.push_back(index);
			indices.push_back(index + 1);
			indices.push_back(index + noiseSize);
			indices.push_back(index + 1);
			indices.push_back(index + noiseSize);
			indices.push_back(index + noiseSize + 1);
		}
	}

	// Generate a buffer for the indices
	GLuint elementbuffer;
	glGenBuffers(1, &elementbuffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementbuffer);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned short), &indices[0], GL_STATIC_DRAW);
	#pragma endregion

	#pragma region Settings
	// Camera settings
	glm::vec3 position = glm::vec3(0, 10, 16);
	float horizontalAngle = 3.14f; // horizontal angle : toward -Z
	float verticalAngle = 0.0f; // vertical angle : 0, look at the horizon
	float initialFoV = 45.0f;
	float speed = 6.0f; // 3 units / second
	float mouseSpeed = 1.0f;
	bool mouseControlsOn = false;
	bool rotatingCamera = true;

	// Graphics settings
	glLineWidth(1.0);
	glClearColor(0.05f, 0.0f, 0.15f, 0.0f);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	//glEnable(GL_CULL_FACE);

	// Init shaders
	GLuint programID = LoadShaders("VertexShader.glsl", "FragmentShader.glsl");
	#pragma endregion

	#pragma region Loop
	// Ensure we can capture the escape key being pressed below
	glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);

	double lastTime = glfwGetTime();

	do {
		// Time
		double currentTime = glfwGetTime();
		float deltaTime = float(currentTime - lastTime);
		lastTime = currentTime;

		// Clear the screen.
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		// 1st attribute buffer : vertices
		glEnableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
		glVertexAttribPointer(
			0,                  // attribute 0. No particular reason for 0, but must match the layout in the shader.
			3,                  // size
			GL_FLOAT,			// type
			GL_FALSE,           // normalized?
			0,                  // stride
			(void*)0            // array buffer offset
		);
		
		
		// Camera
		if (mouseControlsOn) {
			double xpos, ypos;
			glfwGetCursorPos(window, &xpos, &ypos);
			glfwSetCursorPos(window, windowWidth / 2, windowHeight / 2);
			horizontalAngle += mouseSpeed * deltaTime * float(windowWidth / 2 - xpos);
			verticalAngle += mouseSpeed * deltaTime * float(windowHeight / 2 - ypos);
			if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
				mouseControlsOn = false;
				rotatingCamera = true;
			}
		} else {
			if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) {
				mouseControlsOn = true;
				rotatingCamera = false;
			}
		}

		if (rotatingCamera) {
			double cameraRadius = noiseSize / 2.0;
			double cameraPeriod = 20.0;
			double cameraHeight = 14.0;
			verticalAngle = atan(-cameraHeight / cameraRadius);
			horizontalAngle = 3.14 / cameraPeriod * currentTime;
			position = vec3(-cameraRadius * sin(horizontalAngle), cameraHeight, -cameraRadius * cos(horizontalAngle));
		}
		
		glm::vec3 direction(
			cos(verticalAngle) * sin(horizontalAngle),
			sin(verticalAngle),
			cos(verticalAngle) * cos(horizontalAngle)
		);
		glm::vec3 right = glm::vec3(
			sin(horizontalAngle - 3.14f / 2.0f),
			0,
			cos(horizontalAngle - 3.14f / 2.0f)
		);
		glm::vec3 up = glm::cross(right, direction);

		// Move forward
		if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
			position += direction * deltaTime * speed;
		}
		// Move backward
		if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
			position -= direction * deltaTime * speed;
		}
		// Strafe right
		if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
			position += right * deltaTime * speed;
		}
		// Strafe left
		if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
			position -= right * deltaTime * speed;
		}
		// Move up
		if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
			position.y += deltaTime * speed;
		}
		// Move down
		if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
			position.y -= deltaTime * speed;
		}

		float FoV = initialFoV; // -5 * glfwGetMouseWheel(); // THIS DOESN'T WORK
		
		glm::mat4 ProjectionMatrix = glm::perspective(glm::radians(FoV), 4.0f / 3.0f, 0.1f, 120.0f); // aspect ratio 4:3, draw distance 120
		glm::mat4 ViewMatrix = glm::lookAt(
			position,           // Camera is here
			position + direction, // and looks here : at the same position, plus "direction"
			up                  // Head is up (set to 0,-1,0 to look upside-down)
		);
		glm::mat4 ModelMatrix = glm::mat4(1.0);
		glm::mat4 MVP = ProjectionMatrix * ViewMatrix * ModelMatrix;

		GLuint MatrixID = glGetUniformLocation(programID, "MVP");
		glUniformMatrix4fv(MatrixID, 1, GL_FALSE, &MVP[0][0]);

		// Use shader
		GLuint DrawColID = glGetUniformLocation(programID, "drawCol");
		glUniform1f(DrawColID, 0.0);
		glUseProgram(programID);

		// Index buffer
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementbuffer);
		
		// Draw triangles
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		glDrawElements(
			GL_TRIANGLES,      // mode
			indices.size(),    // count
			GL_UNSIGNED_SHORT,   // type
			(void*)0           // element array buffer offset
		);

		// Draw triangles again
		glUniform1f(DrawColID, 0.7);
		glPolygonOffset(-1, -1);
		glEnable(GL_POLYGON_OFFSET_LINE);
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
		glDrawElements(
			GL_TRIANGLES,      // mode
			indices.size(),    // count
			GL_UNSIGNED_SHORT,   // type
			(void*)0           // element array buffer offset
		);
		glDisable(GL_POLYGON_OFFSET_LINE);

		glDisableVertexAttribArray(0);

		// Swap buffers
		glfwSwapBuffers(window);
		glfwPollEvents();

	} // Check if the ESC key was pressed or the window was closed
	while (glfwGetKey(window, GLFW_KEY_ESCAPE) != GLFW_PRESS &&
		glfwWindowShouldClose(window) == 0);
	#pragma endregion

	glfwTerminate();
	return 0;
}