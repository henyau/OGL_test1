// oglTest1.cpp : Defines the entry point for the console application.
// Henry Yau 
// April 16, 2018
// Submission for programming assignment, test 1.

// Imports an FBX mesh, loads textures, displays with OpenGL PBR shader using PBRmap to assign
// physical properties. interprets color channels as [R,G,B]->[metallic, roughness, ao]

// It's been awhile since I've done any graphics programming, so I used the Glitter Boilerplate
// https://github.com/Polytonic/Glitter as a starting point.

// The PBR fragment shader is based on learnopengl.com's example https://github.com/JoeyDeVries/LearnOpenGL

// assimp fails to load the provided .FBX file so I used FBXSDK to extract the mesh
// textures were assigned with absolute paths, so were manually loaded instead

// Local Headers
#include "glitter.hpp"

// System Headers
#include <glad/glad.h>
#include <GLFW/glfw3.h>

// Standard Headers
#include <cstdio>
#include <cstdlib>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

//
#include "learnopengl/filesystem.h"
#include "learnopengl/shader_m.h"
#include "learnopengl/camera.h"
#include "learnopengl/model.h"

#include <iostream>

vector<Texture> textures_loaded;	// stores all the textures loaded so far, optimization to make sure textures aren't loaded more than once.
vector<Mesh> meshes;
string directory;
bool gammaCorrection;

void processInput(GLFWwindow *window);

/* Tab character ("\t") counter */
int numTabs = 0;

// timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;
/**

* Print the required number of tabs.
*/
void PrintTabs() {
	for (int i = 0; i < numTabs; i++)
		printf("\t");
}

/**
* Return a string-based representation based on the attribute type.
*/
FbxString GetAttributeTypeName(FbxNodeAttribute::EType type) {
	switch (type) {
	case FbxNodeAttribute::eUnknown: return "unidentified";
	case FbxNodeAttribute::eNull: return "null";
	case FbxNodeAttribute::eMarker: return "marker";
	case FbxNodeAttribute::eSkeleton: return "skeleton";
	case FbxNodeAttribute::eMesh: return "mesh";
	case FbxNodeAttribute::eNurbs: return "nurbs";
	case FbxNodeAttribute::ePatch: return "patch";
	case FbxNodeAttribute::eCamera: return "camera";
	case FbxNodeAttribute::eCameraStereo: return "stereo";
	case FbxNodeAttribute::eCameraSwitcher: return "camera switcher";
	case FbxNodeAttribute::eLight: return "light";
	case FbxNodeAttribute::eOpticalReference: return "optical reference";
	case FbxNodeAttribute::eOpticalMarker: return "marker";
	case FbxNodeAttribute::eNurbsCurve: return "nurbs curve";
	case FbxNodeAttribute::eTrimNurbsSurface: return "trim nurbs surface";
	case FbxNodeAttribute::eBoundary: return "boundary";
	case FbxNodeAttribute::eNurbsSurface: return "nurbs surface";
	case FbxNodeAttribute::eShape: return "shape";
	case FbxNodeAttribute::eLODGroup: return "lodgroup";
	case FbxNodeAttribute::eSubDiv: return "subdiv";
	default: return "unknown";
	}
}

/**
* Print an attribute.
*/
void PrintAttribute(FbxNodeAttribute* pAttribute) {
	if (!pAttribute) return;

	FbxString typeName = GetAttributeTypeName(pAttribute->GetAttributeType());
	FbxString attrName = pAttribute->GetName();
	PrintTabs();
	// Note: to retrieve the character array of a FbxString, use its Buffer() method.
	printf("<attribute type='%s' name='%s'/>\n", typeName.Buffer(), attrName.Buffer());
}

/**
* Print a node, its attributes, and all its children recursively.
*/
void PrintNode(FbxNode* pNode) {
	PrintTabs();
	const char* nodeName = pNode->GetName();
	FbxDouble3 translation = pNode->LclTranslation.Get();
	FbxDouble3 rotation = pNode->LclRotation.Get();
	FbxDouble3 scaling = pNode->LclScaling.Get();

	// Print the contents of the node.
	printf("<node name='%s' translation='(%f, %f, %f)' rotation='(%f, %f, %f)' scaling='(%f, %f, %f)'>\n",
		nodeName,
		translation[0], translation[1], translation[2],
		rotation[0], rotation[1], rotation[2],
		scaling[0], scaling[1], scaling[2]
	);
	numTabs++;

	// Print the node's attributes.
	for (int i = 0; i < pNode->GetNodeAttributeCount(); i++)
		PrintAttribute(pNode->GetNodeAttributeByIndex(i));

	// Recursively print the children.
	for (int j = 0; j < pNode->GetChildCount(); j++)
		PrintNode(pNode->GetChild(j));
	
	// Print UV set names
	fbxsdk::FbxStringList UVNames;
	pNode->GetMesh()->GetUVSetNames(UVNames);

	std::cout << "UV set names:";
	for (int i = 0; i < UVNames.GetCount(); i++)
		std::cout << UVNames.GetStringAt(i) << std::endl;

	numTabs--;
	PrintTabs();
	printf("</node>\n");
}

vector<Texture> loadMaterialTextures(aiString str, aiTextureType type, string typeName)
{
	vector<Texture> textures;
	//for (unsigned int i = 0; i < mat->GetTextureCount(type); i++)
	{
		//aiString str;
		//mat->GetTexture(type, i, &str);
		// check if texture was loaded before and if so, continue to next iteration: skip loading a new texture
		bool skip = false;
		for (unsigned int j = 0; j < textures_loaded.size(); j++)
		{
			if (std::strcmp(textures_loaded[j].path.data(), str.C_Str()) == 0)
			{
				textures.push_back(textures_loaded[j]);
				skip = true; // a texture with the same filepath has already been loaded, continue to next one. (optimization)
				break;
			}
		}
		if (!skip)
		{   // if texture hasn't been loaded already, load it
			Texture texture;
			texture.id = TextureFromFile(str.C_Str(), directory);
			texture.type = typeName;
			texture.path = str.C_Str();
			textures.push_back(texture);
			textures_loaded.push_back(texture);  // store it as texture loaded for entire model, to ensure we won't unnecesery load duplicate textures.
		}
	}
	return textures;
}

// copy fbx mesh data to our format
Mesh processMesh(FbxMesh *mesh, FbxNode *scene)
{
	// data to fill
	vector<Vertex> vertices;
	vector<unsigned int> indices;
	vector<Texture> textures;

	//loop through polygons, then vertices of those polygons
	unsigned int numPolys = mesh->mPolygons.Size();
	unsigned int numCP = mesh->mControlPoints.Size();
	unsigned int numPolyVert = mesh->mPolygonVertices.Size();

	//store all the UVs 
	fbxsdk::FbxArray<fbxsdk::FbxVector2> allUV;
	mesh->GetPolygonVertexUVs("map1", allUV);

	//fbxsdk::FbxVector4 *allCP;
	//allPolyVert = mesh->
	for (unsigned int i = 0; i <numPolys; i++)
		for (unsigned int j = 0; j < 3; j++)
	{
		Vertex vertex;
		glm::vec3 vector; // we declare a placeholder vector since assimp uses its own vector class that doesn't directly convert to glm's vec3 class so we transfer the data to this placeholder glm::vec3 first.
						  // positions

		int polyInd = mesh->GetPolygonVertex(i, j);
		fbxsdk::FbxVector4 pos= mesh->GetControlPointAt(polyInd);

		vector.x = pos[0];
		vector.y = pos[1];
		vector.z = pos[2]; // copy to our vector
		vertex.Position = vector;
		// normals
		fbxsdk::FbxVector4 normaltmp;
		mesh->GetPolygonVertexNormal(i, j, normaltmp);
		vector.x = normaltmp[0];
		vector.y = normaltmp[1];
		vector.z = normaltmp[2];
		vertex.Normal = vector;
		// texture coordinates


		if (mesh->GetUVLayerCount()>0) // does the mesh contain texture coordinates?
		{
			glm::vec2 vec;	
			FbxVector2 pUV;
			bool bUnmapped;
			
			mesh->GetPolygonVertexUV(i, j, "map1", pUV, bUnmapped);
			//mesh->GetPolygonVertexUV
			vec.x = pUV.mData[0];
			vec.y = -pUV.mData[1];// UV coords flipped

			vertex.TexCoords = vec;
		}
		else
			vertex.TexCoords = glm::vec2(0.0f, 0.0f);
		
		vertices.push_back(vertex);
		indices.push_back(i*3+j);
	}

	//Load the textures in manually. the FBX provided has absolute directory paths D:/.../...


	// 1. diffuse maps
	aiString diffName(FileSystem::getPath("example1/images/helios_enemyfighter_basecolor.png"));
	aiString emmName(FileSystem::getPath("/example1/images/helios_enemyfighter_emmisive.png"));
	aiString normalName(FileSystem::getPath("/example1/images/helios_enemyfighter_normal.png"));
	aiString pbrName(FileSystem::getPath("/example1/images/helios_enemyfighter_pbr.png"));

	vector<Texture> diffuseMaps = loadMaterialTextures(diffName, aiTextureType_DIFFUSE, "diffuseMap");
	textures.insert(textures.end(), diffuseMaps.begin(), diffuseMaps.end());
	// 2. specular maps
	vector<Texture> emissiveMaps = loadMaterialTextures(emmName, aiTextureType_EMISSIVE, "emissiveMap");
	textures.insert(textures.end(), emissiveMaps.begin(), emissiveMaps.end());
	// 3. normal maps
	std::vector<Texture> normalMaps = loadMaterialTextures(normalName, aiTextureType_NORMALS, "normalMap");
	textures.insert(textures.end(), normalMaps.begin(), normalMaps.end());
	// 4. pbr maps
	std::vector<Texture> pbrMaps = loadMaterialTextures(pbrName, aiTextureType_UNKNOWN, "pbrMap");
	textures.insert(textures.end(), pbrMaps.begin(), pbrMaps.end());
	
	// return a mesh object created from the extracted mesh data
	return Mesh(vertices, indices, textures);
}


Camera camera(glm::vec3(0.0f, 0.0f, 3.0f));
float lastX = mWidth / 2.0f;
float lastY = mHeight/ 2.0f;


void mouse_callback(GLFWwindow* window, double xpos, double ypos);
bool firstMouse = true;

int main(int argc, char * argv[]) 
{

	// Load GLFW and Create a Window
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	//glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	//glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
	auto mWindow = glfwCreateWindow(mWidth, mHeight, "OpenGL test", nullptr, nullptr);

	// Check for Valid Context
	if (mWindow == nullptr) {
		std::cout << "Failed to Create OpenGL Context" << std::endl;
		return EXIT_FAILURE;
	}

	// Create Context and Load OpenGL Functions
	glfwMakeContextCurrent(mWindow);
	glfwSetCursorPosCallback(mWindow, mouse_callback);
	//glfwSetInputMode(mWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	//gladLoadGL();
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		std::cout << "Failed to initialize GLAD" << std::endl;
		return EXIT_FAILURE;
	}

	fprintf(stderr, "OpenGL %s\n", glGetString(GL_VERSION));

	glEnable(GL_DEPTH_TEST);
	Shader ourShader("vertexshader.vs", "fragmentshader.fs");	
	
	//this doesn't work.
	Model ourModel(FileSystem::getPath("example1/models/helios_enemyFighter_geo.fbx"));
	
	// Use FBXSDK to load model/scene instead
	const char* lFilename = "./example1/models/helios_enemyFighter_geo.fbx";
	
	//broken fbx, to view can copy poly indices and vert list from mesh
	//const char* lFilename = "./example2/model/vanilleBroken_geo.fbx";


	// Initialize the SDK manager. This object handles memory management.
	FbxManager* lSdkManager = FbxManager::Create();

	// Create the IO settings object.
	FbxIOSettings *ios = FbxIOSettings::Create(lSdkManager, IOSROOT);
	lSdkManager->SetIOSettings(ios);

	// Create an importer using the SDK manager.
	FbxImporter* lImporter = FbxImporter::Create(lSdkManager, "");

	// Use the first argument as the filename for the importer.
	if (!lImporter->Initialize(lFilename, -1, lSdkManager->GetIOSettings())) {
		printf("Call to FbxImporter::Initialize() failed.\n");
		printf("Error returned: %s\n\n", lImporter->GetStatus().GetErrorString());
		exit(-1);
	}

	// Create a new scene so that it can be populated by the imported file.
	FbxScene* lScene = FbxScene::Create(lSdkManager, "myScene");

	// Import the contents of the file into the scene.
	lImporter->Import(lScene);

	// The file is imported, so get rid of the importer.
	lImporter->Destroy();


	FbxNode* lRootNode = lScene->GetRootNode();
	if (lRootNode) {
		for (int i = 0; i < lRootNode->GetChildCount(); i++)
			PrintNode(lRootNode->GetChild(i));
	}
	FbxMesh* lMesh = lRootNode->GetChild(0)->GetMesh();
	ourModel.meshes.push_back (processMesh(lMesh, lRootNode->GetChild(0)));
	//ourModel.meshes.


	
	// lights
	// ------
	glm::vec3 lightPos(-1000.5f, 1200.50f, -300.3f);
	glm::vec3 lightPositions[] = {
		glm::vec3(-10.0f,  10.0f, 10.0f),
		glm::vec3(10.0f,  10.0f, 10.0f),
		glm::vec3(-10.0f, -10.0f, 10.0f),
		glm::vec3(10.0f, -10.0f, 10.0f),
	};
	glm::vec3 lightColors[] = {
		glm::vec3(20.0f, 20.0f, 20.0f),
		glm::vec3(100.0f, 100.0f, 100.0f),
		glm::vec3(20.0f, 20.0f, 20.0f),
		glm::vec3(100.0f, 100.0f, 100.0f)
	};
	// Rendering Loop
	while (glfwWindowShouldClose(mWindow) == false) 
	{
		if (glfwGetKey(mWindow, GLFW_KEY_ESCAPE) == GLFW_PRESS)
			glfwSetWindowShouldClose(mWindow, true);

		float currentFrame = glfwGetTime();
		deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;
		processInput(mWindow);
		// Background Fill Color
		glClearColor(0.25f, 0.25f, 0.25f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		ourShader.use();

		// view/projection transformations
		glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)mWidth / (float)mHeight, 0.1f, 100.0f);
		glm::mat4 view = camera.GetViewMatrix();
		ourShader.setMat4("projection", projection);
		ourShader.setMat4("view", view);
		ourShader.setVec3("camPos", camera.Position);

		// render the loaded model
		glm::mat4 model = glm::mat4(1.0f);//0 4.0 seems to not initialize to identity anymore
		

		model = glm::translate(model, glm::vec3(0.0f, -15.75f, -6.0f)); // translate it down so it's at the center of the scene
		model = glm::scale(model, glm::vec3(0.02f, 0.02f, 0.02f));	// it's a bit too big for our scene, so scale it down

		//model = glm::translate(model, glm::vec3(0.0f, -1.75f, -6.0f)); // translate it down so it's at the center of the scene
		//model = glm::scale(model, glm::vec3(0.02f, 0.2f, 0.2f));	// it's a bit too big for our scene, so scale it down
		
	

		//add lights
		for (unsigned int i = 0; i < sizeof(lightPositions) / sizeof(lightPositions[0]); ++i)
		{
			glm::vec3 newPos = lightPositions[i] + glm::vec3(sin(glfwGetTime() * 5.0) * 5.0, 0.0, 0.0);
			newPos = lightPositions[i];
			ourShader.setVec3("lightPositions[" + std::to_string(i) + "]", newPos);
			ourShader.setVec3("lightColors[" + std::to_string(i) + "]", lightColors[i]);
		}

		ourShader.setMat4("model", model);
		ourShader.setVec3("lightPos", lightPos);

		ourModel.Draw(ourShader);
		
		// Flip Buffers and Draw
		glfwSwapBuffers(mWindow);
		glfwPollEvents();
	}   
	glfwTerminate();

	lSdkManager->Destroy();

	return EXIT_SUCCESS;
}

// glfw: whenever the mouse moves, this callback is called
// -------------------------------------------------------
void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
	if (firstMouse)
	{
		lastX = xpos;
		lastY = ypos;
		firstMouse = false;
	}

	float xoffset = xpos - lastX;
	float yoffset = lastY - ypos; // reversed since y-coordinates go from bottom to top

	lastX = xpos;
	lastY = ypos;

	camera.ProcessMouseMovement(xoffset, yoffset);
}

void processInput(GLFWwindow *window)
{
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		glfwSetWindowShouldClose(window, true);

	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
		camera.ProcessKeyboard(FORWARD, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
		camera.ProcessKeyboard(BACKWARD, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
		camera.ProcessKeyboard(LEFT, deltaTime);
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
		camera.ProcessKeyboard(RIGHT, deltaTime);
}
