// Standard and OpenGL headers
#include <iostream>
#include <vector>
#include <cmath>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define M_PI 3.14159265358979323846 // Define PI if not already defined

// Structure to store vertex coordinates
struct Vertex {
	float x, y, z;
};

// Structure to represent a physical object with mass and radius
struct objects {
	float mass, radius;
} MyObjects;

// Global containers and variables
std::vector<Vertex> vertices;
std::vector<unsigned int> indices;
float zakriveniFaktor = 1e8f;
GLuint VAO, VBO, EBO;
GLuint shaderProgram;

glm::mat4 view, projection;
glm::vec3 cameraPos = glm::vec3(0.0f, 0.0f, 1.0f);     // Initial camera position
glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, 1.0f);   // Direction camera is facing
glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);      // Up direction

float yaw;           // Horizontal rotation
float lastX, lastY;  // Mouse position tracking
float pitch;         // Vertical rotation
bool firstMouse = true;
bool iKeyPressedLastFrame = false;

// Ellipse parameters
float a = 3.0f;
float b = 2.0f;
glm::vec3 center = glm::vec3(0.0f, 0.0f, 0.0f);
glm::vec3 spherePos = glm::vec3(0.0f, 0.0f, 0.0f);            // Sphere's current position
glm::vec3 sphereVelocity = glm::vec3(0.05f, 0.0f, 0.0f);      // Sphere's movement speed

// Grid rendering variables
GLuint gridVAO, gridVBO;
std::vector<glm::vec3> gridVertices;

// Simple fragment shader for the floor (white color)
const char* floorFragmentShaderSource = "#version 330 core\n"
"out vec4 FragColor;\n"
"void main() {\n"
"   FragColor = vec4(1.0, 1.0, 1.0, 1.0); // White floor\n"
"}\0";

// Vertex shader for all geometry
const char* vertexShaderSource = "#version 330 core\n"
"layout (location = 0) in vec3 aPos;\n"
"uniform mat4 model;\n"
"uniform mat4 view;\n"
"uniform mat4 projection;\n"
"void main() {\n"
"   gl_Position = projection * view * model * vec4(aPos, 1.0);\n"
"}\0";

// Fragment shader for the sphere (white color)
const char* fragmentShaderSource = "#version 330 core\n"
"out vec4 FragColor;\n"
"void main() {\n"
"   FragColor = vec4(1.0, 1.0, 1.0, 1.0);\n"
"}\0";

// Generate a deformable grid surface based on curvature
void generateGrid(float size, int divisions, glm::vec3 center, float mass) {
	gridVertices.clear();
	float G = 6.674e-11f;
	float c = 3.0e8f;
	float step = size / divisions;
	float half = size / 2.0f;
	
	for (int i = 0; i <= divisions; i++) {
		for (int j = 0; j <= divisions; j++) {
			float x = -half + j * step;
			float z = -half + i * step;
			
			glm::vec2 gridPoint = glm::vec2(x, z);
			glm::vec2 objPos = glm::vec2(center.x, center.z);
			float dist = glm::distance(gridPoint, objPos) + 0.001f;
			
			float bend = (G * mass) / (dist * c * c);
			float y = -bend * 50; // Scale curvature visually
			
			if (dist < MyObjects.radius) y = 0.0f; // Flatten under the sphere
			
			gridVertices.push_back(glm::vec3(x, y, z));
		}
	}
}

// Setup VBO/VAO for the deformable grid floor
void setupFloorGrid(glm::vec3 center, float mass) {
	generateGrid(10.0f, 100, center, mass);
	
	if (gridVAO == 0) {
		glGenVertexArrays(1, &gridVAO);
		glGenBuffers(1, &gridVBO); 
	}
	
	glBindVertexArray(gridVAO);
	glBindBuffer(GL_ARRAY_BUFFER, gridVBO);
	glBufferData(GL_ARRAY_BUFFER, gridVertices.size() * sizeof(glm::vec3), gridVertices.data(), GL_DYNAMIC_DRAW);
	
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
	glEnableVertexAttribArray(0);
	glBindVertexArray(0);
}

// Render the grid floor
void drawFloorGrid(GLuint shader) {
	glUseProgram(shader);
	glm::mat4 model = glm::mat4(1.0f);
	glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
	glBindVertexArray(gridVAO);
	glDrawArrays(GL_POINTS, 0, gridVertices.size());
	glBindVertexArray(0);
}

// Generate a 3D sphere mesh
void generateSphere(float radius, int sectors, int stacks) {
	float sectorStep = 2 * M_PI / sectors;
	float stackStep = M_PI / stacks;
	
	for (int i = 0; i <= stacks; ++i) {
		float stackAngle = M_PI / 2 - i * stackStep;
		float xy = radius * cos(stackAngle);
		float z = radius * sin(stackAngle);
		
		for (int j = 0; j <= sectors; ++j) {
			float sectorAngle = j * sectorStep;
			float x = xy * cos(sectorAngle);
			float y = xy * sin(sectorAngle);
			vertices.push_back({x, y, z});
		}
	}
	
	for (int i = 0; i < stacks; ++i) {
		for (int j = 0; j < sectors; ++j) {
			int first = i * (sectors + 1) + j;
			int second = first + sectors + 1;
			indices.push_back(first);
			indices.push_back(second);
			indices.push_back(first + 1);
			indices.push_back(second);
			indices.push_back(second + 1);
			indices.push_back(first + 1);
		}
	}
}

// Draw the 3D sphere
void drawSphere() {
	glBindVertexArray(VAO);
	glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);
	glBindVertexArray(0);
}

// Setup VAO/VBO/EBO buffers for the sphere
void setupBuffers() {
	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);
	glGenBuffers(1, &EBO);
	
	glBindVertexArray(VAO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);
	
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
	
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
	glEnableVertexAttribArray(0);
	glBindVertexArray(0);
}

// Compile shader from source
GLuint compileShader(const char* source, GLenum type) {
	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, &source, NULL);
	glCompileShader(shader);
	return shader;
}

// Compile and link shaders
void setupShaders() {
	GLuint vertexShader = compileShader(vertexShaderSource, GL_VERTEX_SHADER);
	GLuint fragmentShader = compileShader(fragmentShaderSource, GL_FRAGMENT_SHADER);
	shaderProgram = glCreateProgram();
	glAttachShader(shaderProgram, vertexShader);
	glAttachShader(shaderProgram, fragmentShader);
	glLinkProgram(shaderProgram);
	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);
	
	// Optional: Floor shader
	GLuint floorShaderProgram;
	GLuint floorFragmentShader = compileShader(floorFragmentShaderSource, GL_FRAGMENT_SHADER);
	floorShaderProgram = glCreateProgram();
	glAttachShader(floorShaderProgram, vertexShader);
	glAttachShader(floorShaderProgram, floorFragmentShader);
	glLinkProgram(floorShaderProgram);
	glDeleteShader(floorFragmentShader);
}

// Compute curvature based on Einstein's formula
float zakriveni(float mass, float radius) {
	const float G = 6.674e-11f;
	const float c = 3.0e8f;     	
	return (G * mass) / (radius * c * c);
}

// Handle keyboard input
void processInput(GLFWwindow* window) {
	float cameraSpeed = 0.05f;
	if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
		cameraSpeed *= 2;
	
	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
		cameraPos += cameraSpeed * cameraFront;
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
		cameraPos -= cameraSpeed * cameraFront;
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
		cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
		cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * cameraSpeed;
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		glfwSetWindowShouldClose(window, true);
	
	// Show curvature info when pressing 'I'
	bool currentlyPressed = glfwGetKey(window, GLFW_KEY_I) == GLFW_PRESS;
	if (currentlyPressed && !iKeyPressedLastFrame) {
		float curvature = zakriveni(MyObjects.mass, MyObjects.radius);
		printf("Object mass: %.3e kg\n", MyObjects.mass);
		printf("Object radius: %.3f m\n", MyObjects.radius);
		printf("Curvature: %.6e\n", curvature);
	}
	iKeyPressedLastFrame = currentlyPressed;
}

// Mouse movement for controlling camera rotation
void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
	if (firstMouse) {
		lastX = xpos;
		lastY = ypos;
		firstMouse = false;
	}
	
	float xoffset = xpos - lastX;
	float yoffset = lastY - ypos;
	lastX = xpos;
	lastY = ypos;
	
	float sensitivity = 0.1f;
	xoffset *= sensitivity;
	yoffset *= sensitivity;
	
	yaw += xoffset;
	pitch += yoffset;
	
	if (pitch > 89.0f) pitch = 89.0f;
	if (pitch < -89.0f) pitch = -89.0f;
	
	glm::vec3 front;
	front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
	front.y = sin(glm::radians(pitch));
	front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
	cameraFront = glm::normalize(front);
}

// Main program
int main() {
	if (!glfwInit()) return -1;
	GLFWwindow* window = glfwCreateWindow(800, 600, "3D Moving Sphere", NULL, NULL);
	if (!window) { glfwTerminate(); return -1; }
	glfwMakeContextCurrent(window);
	glewInit();
	glEnable(GL_DEPTH_TEST);
	
	// Set object properties
	MyObjects.mass = 10e25f;
	MyObjects.radius = 1.0f;
	
	generateSphere(1.0f, 40, 40);
	setupBuffers();
	setupShaders();
	setupFloorGrid(spherePos, MyObjects.mass);
	glfwSetCursorPosCallback(window, mouse_callback);
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	
	while (!glfwWindowShouldClose(window)) {
		processInput(window);
		
		// Animate the sphere along an ellipse
		static float angle = 0.0f;
		angle += 0.002f;
		glm::vec3 ellipsePos = glm::vec3(
			center.x + a * cos(angle),
			center.y,
			center.z + b * sin(angle)
			);
		spherePos = ellipsePos;
		
		// Recalculate floor grid curvature
		setupFloorGrid(spherePos, MyObjects.mass);
		
		// Rendering
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		
		glm::mat4 model = glm::mat4(1.0f);
		model = glm::translate(model, spherePos);
		
		view = glm::lookAt(cameraPos, cameraPos + cameraFront, cameraUp);
		projection = glm::perspective(glm::radians(45.0f), 800.0f / 600.0f, 0.1f, 100.0f);
		
		// Pass matrices to shader
		glUseProgram(shaderProgram);
		glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
		glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
		glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
		
		drawSphere();             // Draw the moving sphere
		drawFloorGrid(shaderProgram); // Draw deformable floor
		
		glfwSwapBuffers(window);
		glfwPollEvents();
	}
	glfwTerminate();
	return 0;
}

