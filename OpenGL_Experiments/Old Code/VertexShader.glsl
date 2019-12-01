#version 330 core
layout(location = 0) in vec3 vertexPosition_modelspace;
//layout(location = 1) in vec3 vertexColor;

uniform mat4 MVP;
uniform float drawCol;

out float fragmentColor;
out float zPos;

void main(){
  // Output position of the vertex, in clip space : MVP * position
  gl_Position =  MVP * vec4(vertexPosition_modelspace,1);
  fragmentColor = drawCol;
  zPos = vertexPosition_modelspace.y;
}