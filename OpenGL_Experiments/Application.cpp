#include <stdio.h>
#include <stdlib.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <vector>
#include <algorithm>
#include <time.h>
#include <thread>
#include <chrono>
#include <PerlinNoise.hpp>
#include <pybind11/pybind11.h>
using namespace glm;

const int noiseSize = 24;
const double windowWidth = 1920;//1024; 1920
const double windowHeight = 1080;// 576; 1080
const int chunks = 8;

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
		"out vec4 color;\n"

		"uniform float rAmt;\n"
		"uniform float gAmt;\n"
		"uniform float bAmt;\n"

		"void main() {\n"
		"	float colAmount = fragmentColor + 0.13 * zPos;\n"
		"	color = vec4(colAmount * rAmt, colAmount * gAmt, colAmount * bAmt, colAmount * 1.5);//fragmentColor;\n"
		"}\n";
}

GLuint LoadShaders() {

	// Create the shaders
	GLuint VertexShaderID = glCreateShader(GL_VERTEX_SHADER);
	GLuint FragmentShaderID = glCreateShader(GL_FRAGMENT_SHADER);

	// Read the Vertex Shader code
	std::string VertexShaderCode = getVertexShaderString();

	// Read the Fragment Shader code
	std::string FragmentShaderCode = getFragmentShaderString();

	GLint Result = GL_FALSE;
	int InfoLogLength;

	// Compile Vertex Shader
	printf("Compiling vertex shader\n");
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
	printf("Compiling fragment shader\n");
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

class OpenGLProgram {
private:
	std::uint32_t seed;
	double wavelength;
	int octaves;
	double shaderBrightness = 0.9; // 0.7
	double shaderR = 1.0;
	double shaderG = 0.1;
	double shaderB = 0.7;
	
	std::thread glThread;
	bool stopProgram = false;

	std::vector<float> noise1;
	std::vector<float> noise2;
	std::vector<float> noise3;
	GLfloat g_vertex_buffer_data[noiseSize * noiseSize * 3 * chunks];
	
	class heightPIDController {
	private:
		double kP = 0.16;
		double kI = 0.12;
		double kD = 0.08;
		double error = 0;
		double totalError = 0;
		double lastError = 0;
		double value = 0;
		double target = 0;

	public:
		heightPIDController(double initialValue) {
			value = initialValue;
		}
		void setTarget(double a) {
			target = a;
		}
		double getTarget() {
			return target;
		}
		double getValue() {
			return value;
		}
		void step() {
			error = target - value;
			totalError += error;
			double p = kP * error;
			double i = kI * totalError;
			double d = kD * (error - lastError);
			lastError = error;
			totalError *= 0.8;
			value += p + i + d;
		}
	};

	heightPIDController mLow = heightPIDController(0.0);
	heightPIDController mMid = heightPIDController(0.0);
	heightPIDController mHi = heightPIDController(0.0);

public:

	void run() {
		#pragma region Noise
		fprintf(stderr, "Random seed is %d\n", seed);

		double yoffset = 0.0;
		double yscrollspeed = 0.3;
		int ysteps = 0;

		double peaksArray[noiseSize];
		double pi = 3.141592653589;
		for (int i = 0; i < noiseSize; i++) {
			peaksArray[i] = 1.0 + sin(pi * i / (noiseSize - 1) * 3.0) - cos(pi * i / (noiseSize - 1) * 2.0) - sin(pi * i / (noiseSize - 1));
		}
		
		const siv::PerlinNoise perlin(seed);
		for (int k = 0; k < chunks; k++) {
			int koffset = k * (noiseSize - 1);
			for (int i = 0; i < noiseSize; i++) {
				for (int j = 0; j < noiseSize; j++) {
					noise1.push_back(max(perlin.octaveNoise0_1((i + koffset) / wavelength, j / wavelength, 1) * 10 - 4, 0.0) * 1.5);
					noise2.push_back(perlin.octaveNoise0_1((i + koffset) / (wavelength / 2), j / (wavelength / 2), 1) * 5.0 - 3.0);
					noise3.push_back(perlin.octaveNoise0_1((i + koffset) / (wavelength / 4), j / (wavelength / 4), 1) * 4.0 - 1.0);
				}
			}
		}
		#pragma endregion

		#pragma region Init
		// Initialise GLFW
		glewExperimental = true; // Needed for core profile
		if (!glfwInit())
		{
			fprintf(stderr, "Failed to initialize GLFW\n");
			return;
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
			return;
		}
		glfwMakeContextCurrent(window); // Initialize GLEW
		glewExperimental = true; // Needed in core profile
		if (glewInit() != GLEW_OK) {
			fprintf(stderr, "Failed to initialize GLEW\n");
			return;
		}
		#pragma endregion

		#pragma region Vertices
		GLuint VertexArrayID;
		glGenVertexArrays(1, &VertexArrayID);
		glBindVertexArray(VertexArrayID);

		// Vertices
		int index = 0;
		for (int k = 0; k < chunks; k++) {
			int koffset = k * (noiseSize - 1);
			for (int j = 0; j < noiseSize; j++) {
				for (int i = 0; i < noiseSize; i++) {
					g_vertex_buffer_data[index] = i - noiseSize / 2;
					index++;
					g_vertex_buffer_data[index] = 0;
					index++;
					g_vertex_buffer_data[index] = j - noiseSize / 2 + koffset;
					index++;
				}
			}
		}

		GLuint vertexbuffer;
		glGenBuffers(1, &vertexbuffer);
		glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
		glBufferStorage(GL_ARRAY_BUFFER, noiseSize * noiseSize * 3 * chunks * sizeof(GLfloat), g_vertex_buffer_data, GL_DYNAMIC_STORAGE_BIT);

		// Vertex indices
		std::vector<unsigned short> indices;
		for (int k = 0; k < chunks; k++) {
			int koffset = noiseSize * noiseSize * k;
			for (int j = 0; j < noiseSize - 1; j++) {
				int joffset = noiseSize * j;
				for (int i = 0; i < noiseSize - 1; i++) {
					index = i + joffset + koffset;
					indices.push_back(index);
					indices.push_back(index + 1);
					indices.push_back(index + noiseSize);
					indices.push_back(index + 1);
					indices.push_back(index + noiseSize);
					indices.push_back(index + noiseSize + 1);
				}
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
		float FoV = 100.0f; //45
		float speed = 6.0f; // 3 units / second
		float fovspeed = 50.0;
		float mouseSpeed = 0.1f;
		bool mouseControlsOn = false;
		bool rotatingCamera = true;

		// Graphics settings
		glLineWidth(2.0);
		glClearColor(0.05f, 0.0f, 0.15f, 0.0f);
		//glClearColor(0.35f, 0.4f, 0.9f, 0.0f); // windowx xp land background
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LESS);
		glEnable(GL_BLEND);
		// glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // normal blending
		glBlendFunc(GL_SRC_ALPHA, GL_ONE); // additive blending
		//glEnable(GL_CULL_FACE);

		// Init shaders
		GLuint programID = LoadShaders();
		#pragma endregion

		#pragma region Loop
		// Ensure we can capture the escape key being pressed below
		glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);

		//double lastTime = 0;
		double currentTime = 0;
		do {
			// Time
			float deltaTime = glfwGetTime();
			int sleepTime = std::max(16.666 - deltaTime*1000.0, 0.0);
			std::this_thread::sleep_for(std::chrono::milliseconds(sleepTime));
			deltaTime = glfwGetTime();
			currentTime += deltaTime;
			glfwSetTime(0);

			// Update position
			yoffset += yscrollspeed;
			if (yoffset > (ysteps + 1) * (noiseSize - 1)) {
				// Load a new chunk (update z coordinates and noise)
				int arrayPos = ysteps % chunks;
				int zOrigin = (ysteps + chunks) * (noiseSize - 1);
				index = 2 + arrayPos * noiseSize * noiseSize * 3;
				for (int j = 0; j < noiseSize; j++) {
					for (int i = 0; i < noiseSize; i++) {
						g_vertex_buffer_data[index] = j - noiseSize / 2 + zOrigin;
						index += 3;
					}
				}
				for (int i = 0; i < noiseSize; i++) {
					for (int j = 0; j < noiseSize; j++) {
						index = j + i * noiseSize + arrayPos * noiseSize * noiseSize;
						noise1[index] = max(perlin.octaveNoise0_1((i + zOrigin) / wavelength, j / wavelength, 1) * 10 - 4, 0.0) * 1.5;
						noise2[index] = perlin.octaveNoise0_1((i + zOrigin) / (wavelength / 2), j / (wavelength / 2), 1) * 5.0 - 3.0;
						noise3[index] = perlin.octaveNoise0_1((i + zOrigin) / (wavelength / 4), j / (wavelength / 4), 1) * 4.0 - 1.0;
					}
				}
				ysteps++;
			}

			// Update mountain heights
			int ind = 1;
			mLow.step();
			mMid.step();
			mHi.step();
			for (int k = 0; k < chunks; k++) {
				int koffset = k * noiseSize * noiseSize;
				for (int j = 0; j < noiseSize; j++) {
					int joffset = j * noiseSize;
					for (int i = 0; i < noiseSize; i++) {
						double iscale = peaksArray[i]; // abs(i - noiseSize / 2.0) / (noiseSize / 2.0) + 0.05;
						g_vertex_buffer_data[ind] = iscale * (
							mLow.getValue() * noise1.at(i + joffset + koffset) +
							mMid.getValue() * noise2.at(i + joffset + koffset) +
							mHi.getValue() * noise3.at(i + joffset + koffset));
						ind += 3;
					}
				}
			}
			glBufferSubData(GL_ARRAY_BUFFER, 0, noiseSize * noiseSize * 3 * chunks * sizeof(GLfloat), g_vertex_buffer_data);
			
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
			}
			else {
				if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) {
					mouseControlsOn = true;
					rotatingCamera = false;
				}
			}

			if (rotatingCamera) {
				double cameraRadius = noiseSize / 2.0;
				double cameraPeriod = 24.0;
				double cameraHeight = 2.0; // 15.0;
				verticalAngle = 0; // atan(-(cameraHeight - 4.0) / cameraRadius);
				horizontalAngle = 0; // 3.14 / cameraPeriod * currentTime;
				// position = vec3(-cameraRadius * sin(horizontalAngle), cameraHeight, -cameraRadius * cos(horizontalAngle));
				position = vec3(0, cameraHeight, yoffset);
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
			// FoV up
			if (glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS) {
				FoV += deltaTime * fovspeed;
			}
			// FoV down
			if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS) {
				FoV -= deltaTime * fovspeed;
			}
			FoV = clamp<float>(FoV, 45, 120);

			// Shader Controls
			if (glfwGetKey(window, GLFW_KEY_U) == GLFW_PRESS) shaderR += 0.01;
			if (glfwGetKey(window, GLFW_KEY_J) == GLFW_PRESS) shaderR -= 0.01;
			if (glfwGetKey(window, GLFW_KEY_I) == GLFW_PRESS) shaderG += 0.01;
			if (glfwGetKey(window, GLFW_KEY_K) == GLFW_PRESS) shaderG -= 0.01;
			if (glfwGetKey(window, GLFW_KEY_O) == GLFW_PRESS) shaderB += 0.01;
			if (glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS) shaderB -= 0.01;
			shaderR = clamp<float>(shaderR, 0.0, 1.0);
			shaderG = clamp<float>(shaderG, 0.0, 1.0);
			shaderB = clamp<float>(shaderB, 0.0, 1.0);

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
			GLuint rColID = glGetUniformLocation(programID, "rAmt");
			GLuint gColID = glGetUniformLocation(programID, "gAmt");
			GLuint bColID = glGetUniformLocation(programID, "bAmt");
			glUniform1f(DrawColID, shaderBrightness);
			glUniform1f(rColID, shaderR);
			glUniform1f(gColID, shaderG);
			glUniform1f(bColID, shaderB);
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
			glUniform1f(DrawColID, shaderBrightness + 0.4);
			glUniform1f(rColID, shaderR);
			glUniform1f(gColID, shaderG);
			glUniform1f(bColID, shaderB);
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
			glfwWindowShouldClose(window) == 0 && !stopProgram);
		#pragma endregion

		glfwTerminate();
		return;
	}

	void defineParams(std::uint32_t aSeed, double aWavelength, int aOctaves) {
		seed = aSeed;
		wavelength = aWavelength;
		octaves = aOctaves;
	}

	void startOpenGLThread() {
		glThread = std::thread(&OpenGLProgram::run, this);
	}

	void stopThreadGracefully() {
		stopProgram = true;
		if (glThread.joinable()) {
			glThread.join();
		}
	}

	void setShaderBrightness(double value) {
		shaderBrightness = value;
	}

	void setMountainHeight(double low, double mid, double high) {
		mLow.setTarget(low);
		mMid.setTarget(mid);
		mHi.setTarget(high);
	}

};

OpenGLProgram program = OpenGLProgram();

void runProgram() {
	srand(time(NULL));
	program.defineParams(rand() % 65536, /*wavelength*/ 8, /*octaves*/ 3); // 32, 3
	program.startOpenGLThread();
}

void stopProgram() {
	program.stopThreadGracefully();
}

void setShaderBrightness(double brightness) {
	program.setShaderBrightness(brightness);
}

void setMountainHeight(double low, double mid, double high) {
	program.setMountainHeight(low, mid, high);
}

namespace py = pybind11;

PYBIND11_MODULE(OpenGL_Experiments, m) {
	m.def("runProgram", &runProgram, R"pbdoc(
        Run the opengl program.
    )pbdoc")
	.def("stopProgram", &stopProgram, R"pbdoc(
        Stop the opengl program.
    )pbdoc")
	.def("setShaderBrightness", &setShaderBrightness, R"pbdoc(
        Set the brightness of mountain peaks.
    )pbdoc")
	.def("setMountainHeight", &setMountainHeight, R"pbdoc(
        Set the height of mountain peaks.
    )pbdoc");

#ifdef VERSION_INFO
	m.attr("__version__") = VERSION_INFO;
#else
	m.attr("__version__") = "dev";
#endif
}