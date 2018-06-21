#include <glew.h>
#include <freeglut.h>
#define GLM_SWIZZLE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <tiny_obj_loader.h>
#include "fbxloader.h"
#include <IL/il.h>
#include <vector>

using namespace std;

#define MENU_TIMER_START 1
#define MENU_TIMER_STOP 2
#define MENU_EXIT 3

#ifndef max
# define max(a,b) (((a)>(b))?(a):(b))
# define min(a,b) (((a)<(b))?(a):(b))
#endif

int timer_cnt = 0;
bool timer_enabled = true;
unsigned int timer_speed = 16;

typedef struct
{
    GLuint vao;
    GLuint vbo;
    GLuint vboTex;
    GLuint ebo;
    int materialId;
    int indexCount;
} Shape;

typedef struct
{
    GLuint texId;
} Material;

vector<Shape> characterShapes;
vector<Material> characterMaterials;
fbx_handles characterFbx;

using namespace glm;

mat4 mvp;
GLint um4mvp;

float center[3];

void checkError(const char *functionName)
{
    GLenum error;
    while (( error = glGetError() ) != GL_NO_ERROR) {
        fprintf (stderr, "GL error 0x%X detected in %s\n", error, functionName);
    }
}

// Print OpenGL context related information.
void dumpInfo(void)
{
    printf("Vendor: %s\n", glGetString (GL_VENDOR));
    printf("Renderer: %s\n", glGetString (GL_RENDERER));
    printf("Version: %s\n", glGetString (GL_VERSION));
    printf("GLSL: %s\n", glGetString (GL_SHADING_LANGUAGE_VERSION));
}

char** loadShaderSource(const char* file)
{
	FILE* fp = fopen(file, "rb");
	fseek(fp, 0, SEEK_END);
	long sz = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	char *src = new char[sz + 1];
	fread(src, sizeof(char), sz, fp);
	src[sz] = '\0';
	char **srcp = new char*[1];
	srcp[0] = src;
	return srcp;
}

void freeShaderSource(char** srcp)
{
	delete srcp[0];
	delete srcp;
}

void shaderLog(GLuint shader)
{
	GLint isCompiled = 0;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &isCompiled);
	if(isCompiled == GL_FALSE)
	{
		GLint maxLength = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);

		// The maxLength includes the NULL character
		GLchar* errorLog = new GLchar[maxLength];
		glGetShaderInfoLog(shader, maxLength, &maxLength, &errorLog[0]);

		printf("%s\n", errorLog);
		delete errorLog;
	}
}

std::string convertPath(std::string origin)
{
	char *str = new char[origin.length() + 1];
	strcpy(str, origin.c_str());
	char *pch;
	char *pastpch;
	pch = strtok(str, "/");
	while (pch != NULL)
	{
		//printf("%s\n", pch);
		pastpch = pch;
		pch = strtok(NULL, "/");
	}
	pch = strtok(pastpch, ".");
	string tex("sophia\\Tex");
	string n(pch);
	string newPath = tex + "\\" + n + ".png";
	cout << "new path: " << newPath << '\n';
	return newPath;
}

void My_Init()
{
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);

    GLuint program = glCreateProgram();
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	char** vertexShaderSource = loadShaderSource("vertex.vs.glsl");
	char** fragmentShaderSource = loadShaderSource("fragment.fs.glsl");
    glShaderSource(vertexShader, 1, vertexShaderSource, NULL);
    glShaderSource(fragmentShader, 1, fragmentShaderSource, NULL);
	freeShaderSource(vertexShaderSource);
	freeShaderSource(fragmentShaderSource);
    glCompileShader(vertexShader);
    glCompileShader(fragmentShader);
	shaderLog(vertexShader);
    shaderLog(fragmentShader);
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);
	um4mvp = glGetUniformLocation(program, "um4mvp");
    glUseProgram(program);

    GLuint sampler;
    glGenSamplers(1, &sampler);
    glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindSampler(0, sampler);
}

void My_LoadModels()
{
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;

	std::string err;

    bool ret = LoadFbx(characterFbx, shapes, materials, err, "man/Old_Man_Fixed_Anim.fbx");
	//bool ret = LoadFbx(characterFbx, shapes, materials, err, "marker_man/Walking.fbx");

    if (ret)
    {
        // For Each Material
        for (int i = 0; i < materials.size(); i++)
        {
            ILuint ilTexName;
            ilGenImages(1, &ilTexName);
            ilBindImage(ilTexName);
            Material mat;
			string newPath;
			//newPath = convertPath(materials[i].diffuse_texname);
			newPath = materials[i].diffuse_texname;
			printf("Texture Name: %s\n", newPath.c_str());
            if (ilLoadImage(newPath.c_str()))
            {
                int width = ilGetInteger(IL_IMAGE_WIDTH);
                int height = ilGetInteger(IL_IMAGE_HEIGHT);
                unsigned char *data = new unsigned char[width * height * 4];
                ilCopyPixels(0, 0, 0, width, height, 1, IL_RGBA, IL_UNSIGNED_BYTE, data);

                glGenTextures(1, &mat.texId);
                glBindTexture(GL_TEXTURE_2D, mat.texId);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
                glGenerateMipmap(GL_TEXTURE_2D);

                delete[] data;
                ilDeleteImages(1, &ilTexName);
            }
            characterMaterials.push_back(mat);
        }

		float Vmax[3];
		float Vmin[3];
		Vmax[0] = shapes[0].mesh.positions[0];
		Vmax[1] = shapes[0].mesh.positions[1];
		Vmax[2] = shapes[0].mesh.positions[2];
		Vmin[0] = shapes[0].mesh.positions[0];
		Vmin[1] = shapes[0].mesh.positions[1];
		Vmin[2] = shapes[0].mesh.positions[2];

        // For Each Shape (or Mesh, Object)
        for (int i = 0; i < shapes.size(); i++)
        {
            Shape shape;
            glGenVertexArrays(1, &shape.vao);
            glBindVertexArray(shape.vao);

            glGenBuffers(3, &shape.vbo);
            glBindBuffer(GL_ARRAY_BUFFER, shape.vbo);
            glBufferData(GL_ARRAY_BUFFER, shapes[i].mesh.positions.size() * sizeof(float), shapes[i].mesh.positions.data(), GL_STATIC_DRAW);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
            glBindBuffer(GL_ARRAY_BUFFER, shape.vboTex);
            glBufferData(GL_ARRAY_BUFFER, shapes[i].mesh.texcoords.size() * sizeof(float), shapes[i].mesh.texcoords.data(), GL_STATIC_DRAW);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, 0);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, shape.ebo);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, shapes[i].mesh.indices.size() * sizeof(unsigned int), shapes[i].mesh.indices.data(), GL_STATIC_DRAW);
            shape.materialId = shapes[i].mesh.material_ids[0];
            shape.indexCount = shapes[i].mesh.indices.size();
            glEnableVertexAttribArray(0);
            glEnableVertexAttribArray(1);
            characterShapes.push_back(shape);

			for (int j = 0; j < shapes[i].mesh.positions.size(); j++)
			{
				int k = j % 3;
				Vmax[k] = max(Vmax[k], shapes[i].mesh.positions[j]);
				Vmin[k] = min(Vmin[k], shapes[i].mesh.positions[j]);
			}
        }
		for (int i = 0; i < 3; i++) center[i] = (Vmax[i] + Vmin[i]) / 2;
		printf("max = (%f, %f, %f)\n", Vmax[0], Vmax[1], Vmax[2]);
		printf("min = (%f, %f, %f)\n", Vmin[0], Vmin[1], Vmin[2]);
		printf("center = (%f, %f, %f)\n", center[0], center[1], center[2]);
    }
}

// GLUT callback. Called to draw the scene.
void My_Display()
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    std::vector<tinyobj::shape_t> new_shapes;
	int man_cnt = timer_cnt % 100;
    GetFbxAnimation(characterFbx, new_shapes, man_cnt / 100.0f);
	//int man_cnt = timer_cnt % 100 - 10;
	//GetFbxAnimation(characterFbx, new_shapes, man_cnt / 10.0f);

    for (unsigned int i = 0; i < characterShapes.size(); ++i)
    {
        glBindVertexArray(characterShapes[i].vao);
        glBindBuffer(GL_ARRAY_BUFFER, characterShapes[i].vbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, new_shapes[i].mesh.positions.size() * sizeof(float), new_shapes[i].mesh.positions.data());
        glBindTexture(GL_TEXTURE_2D, characterMaterials[characterShapes[i].materialId].texId);
        mat4 lmvp = mvp * rotate(mat4(), radians(90.0f), vec3(0, -1, 0)) * rotate(mat4(), radians(90.0f), vec3(-1, 0, 0)) * scale(mat4(), vec3(0.01f, 0.01f, 0.01f)) * translate(mat4(), vec3(-center[0], -center[1], -center[2]));
		//mat4 lmvp = mvp *  scale(mat4(), vec3(0.01f, 0.01f, 0.01f)) * translate(mat4(), vec3(-center[0], -center[1], -center[2]));
        glUniformMatrix4fv(um4mvp, 1, GL_FALSE, &lmvp[0][0]);
        glDrawElements(GL_TRIANGLES, characterShapes[i].indexCount, GL_UNSIGNED_INT, 0);
    }

    glutSwapBuffers();
}

void My_Reshape(int width, int height)
{
	glViewport(0, 0, width, height);

	float viewportAspect = (float)width / (float)height;
    mvp = perspective(radians(45.0f), viewportAspect, 0.1f, 100.0f);
	mvp = mvp * lookAt(vec3(-2.0f, 1.0f, 0.0f), vec3(0.0f, 0.0f, 0.0f), vec3(0.0f, 1.0f, 0.0f));
}

void My_Timer(int val)
{
	timer_cnt++;
	glutPostRedisplay();
	if(timer_enabled)
	{
		glutTimerFunc(timer_speed, My_Timer, val);
	}
}

void My_Mouse(int button, int state, int x, int y)
{
	if(state == GLUT_DOWN)
	{
		printf("Mouse %d is pressed at (%d, %d)\n", button, x, y);
	}
	else if(state == GLUT_UP)
	{
		printf("Mouse %d is released at (%d, %d)\n", button, x, y);
	}
}

void My_Keyboard(unsigned char key, int x, int y)
{
	printf("Key %c is pressed at (%d, %d)\n", key, x, y);
}

void My_SpecialKeys(int key, int x, int y)
{
	switch(key)
	{
	case GLUT_KEY_F1:
		printf("F1 is pressed at (%d, %d)\n", x, y);
		break;
	case GLUT_KEY_PAGE_UP:
		printf("Page up is pressed at (%d, %d)\n", x, y);
		break;
	case GLUT_KEY_LEFT:
		printf("Left arrow is pressed at (%d, %d)\n", x, y);
		break;
	default:
		printf("Other special key is pressed at (%d, %d)\n", x, y);
		break;
	}
}

void My_Menu(int id)
{
	switch(id)
	{
	case MENU_TIMER_START:
		if(!timer_enabled)
		{
			timer_enabled = true;
			glutTimerFunc(timer_speed, My_Timer, 0);
		}
		break;
	case MENU_TIMER_STOP:
		timer_enabled = false;
		break;
	case MENU_EXIT:
		exit(0);
		break;
	default:
		break;
	}
}

int main(int argc, char *argv[])
{
	// Initialize GLUT and GLEW, then create a window.
	////////////////////
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH);
	glutInitWindowPosition(100, 100);
	glutInitWindowSize(600, 600);
	glutCreateWindow("FBX Loader"); // You cannot use OpenGL functions before this line; The OpenGL context must be created first by glutCreateWindow()!
	glewInit();
	ilInit();
    ilEnable(IL_ORIGIN_SET);
    ilOriginFunc(IL_ORIGIN_LOWER_LEFT);
	dumpInfo();
	My_Init();
	My_LoadModels();
	////////////////////

	// Create a menu and bind it to mouse right button.
	////////////////////////////
	int menu_main = glutCreateMenu(My_Menu);
	int menu_timer = glutCreateMenu(My_Menu);

	glutSetMenu(menu_main);
	glutAddSubMenu("Timer", menu_timer);
	glutAddMenuEntry("Exit", MENU_EXIT);

	glutSetMenu(menu_timer);
	glutAddMenuEntry("Start", MENU_TIMER_START);
	glutAddMenuEntry("Stop", MENU_TIMER_STOP);

	glutSetMenu(menu_main);
	glutAttachMenu(GLUT_RIGHT_BUTTON);
	////////////////////////////

	// Register GLUT callback functions.
	///////////////////////////////
	glutDisplayFunc(My_Display);
	glutReshapeFunc(My_Reshape);
	glutMouseFunc(My_Mouse);
	glutKeyboardFunc(My_Keyboard);
	glutSpecialFunc(My_SpecialKeys);
	glutTimerFunc(timer_speed, My_Timer, 0); 
	///////////////////////////////

	// Enter main event loop.
	//////////////
	glutMainLoop();
	//////////////
	return 0;
}