 Henry Yau 
 April 16, 2018
 Submission for programming assignment, test 1.

 Imports an FBX mesh, loads textures, displays with OpenGL PBR shader using PBRmap to assign
 physical properties. interprets color channels as [R,G,B]->[metallic, roughness, ao]

 It's been awhile since I've done any graphics programming, so I used the Glitter Boilerplate
 https://github.com/Polytonic/Glitter as a starting point.

 The PBR fragment shader is based on learnopengl.com's example https://github.com/JoeyDeVries/LearnOpenGL

 assimp fails to load the provided .FBX file so I used FBXSDK to extract the mesh
 textures were assigned with absolute paths, so were manually loaded instead

 Video sample: OglTest1.flv