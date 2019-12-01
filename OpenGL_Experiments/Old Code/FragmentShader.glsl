#version 330 core
in float fragmentColor;
in float zPos;
out vec3 color;

void main(){
	float colAmount = fragmentColor + 0.13 * zPos - 0.5;
	if (abs(zPos - 5.0) < 0.001) {
		colAmount += 0.5;
	}
	color = vec3(colAmount, 0.1*colAmount, 0.7*colAmount);//fragmentColor;
}