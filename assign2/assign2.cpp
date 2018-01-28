// assign2.cpp : Defines the entry point for the console application.
//

/*
	CSCI 480 Computer Graphics
	Assignment 2: Simulating a Roller Coaster
	C++ starter code
*/

#include "stdafx.h"
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <iostream>
#include <GL/glu.h>
#include <GL/glut.h>

#include "opencv2/core/core.hpp"
#include "opencv2/core/utility.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/imgcodecs/imgcodecs.hpp"

int g_vMousePos[2] = { 0, 0 };
int g_iLeftMouseButton = 0;    /* 1 if pressed, 0 if not */
int g_iMiddleMouseButton = 0;
int g_iRightMouseButton = 0;

typedef enum { ROTATE, TRANSLATE, SCALE } CONTROLSTATE;

CONTROLSTATE g_ControlState = ROTATE;

/* state of the world */
float g_vLandRotate[3] = { 0.0, 0.0, 0.0 };
float g_vLandTranslate[3] = { 0.0, 0.0, 0.0 };
float g_vLandScale[3] = { 1.0, 1.0, 1.0 };

/* Object where you can load an image */
cv::Mat3b imageBGR;

/* represents one control point along the spline */
struct point {
	double x;
	double y;
	double z;
};

/* spline struct which contains how many control points, and an array of control points */
struct spline {
	int numControlPoints;
	struct point *points;
};

/* the spline array */
struct spline *g_Splines;

/* total number of splines */
int g_iNumOfSplines;
int g_iNumOfPoints;

point * points;
point * tangents;
point * normals;
point * binormals;

int currentLocation = 0;

int updateEveryXDisplays = 1;
int displayNumber = 0;

const double s = 0.5;
const double alpha = 0.1;

GLuint texture[6];

void saveScreenshot(char *filename);

/* Read an image into memory.
Set argument displayOn to true to make sure images are loaded correctly.
One image loaded, set to false so it doesn't interfere with OpenGL window.*/
int readImage(char *filename, cv::Mat3b& image, bool displayOn)
{
	std::cout << "reading image: " << filename << std::endl;
	image = cv::imread(filename);
	if (!image.data) // Check for invalid input                    
	{
		std::cout << "Could not open or find the image." << std::endl;
		return 1;
	}

	if (displayOn)
	{
		cv::imshow("TestWindow", image);
		cv::waitKey(0); // Press any key to enter. 
	}
	return 0;
}

void texload(int i, char *filename)
{
	cv::Mat3b bufferBGR;
	readImage(filename, bufferBGR, false);

	int outputImageWidth = 256;
	int outputImageHeight = 256;

	// Resize image with bilinear interpolation. Note this does not maintain aspect ratio:
	// To automatically resize image to outputImageWidth x outputImageHeight upon loading.
	// Uncomment the next line below:
	cv::resize(bufferBGR, bufferBGR,
		cv::Size(outputImageWidth, outputImageHeight), 0.0, 0.0, CV_INTER_LINEAR);

	// OR, crop image with below:
	// Crop from upper left of image [pixel (0,0) in OpenCV], at width x height.
	cv::Rect cropRegion = cv::Rect(0, 0, outputImageWidth, outputImageHeight);
	if (bufferBGR.rows >= outputImageHeight && bufferBGR.cols >= outputImageWidth) {
		bufferBGR = bufferBGR(cropRegion);
	}
	else {
		// If crop fails, default to resize the image.
		cv::resize(bufferBGR, bufferBGR,
			cv::Size(outputImageWidth, outputImageHeight), 0.0, 0.0, CV_INTER_LINEAR);
	}

	// Flip up-down to account for CV / OpenGL 0,0 pixel location.
	// This is why the row traversal is in reverse.
	unsigned char* rgb_buffer = new unsigned char[bufferBGR.rows*bufferBGR.cols * 3]();
	int pixlocation = 0;
	for (int r = bufferBGR.rows - 1; r >= 0; r--) {
		for (int c = 0; c < bufferBGR.cols; c++) {
			rgb_buffer[pixlocation] = bufferBGR.at<cv::Vec3b>(r, c)[2];		// R
			rgb_buffer[pixlocation + 1] = bufferBGR.at<cv::Vec3b>(r, c)[1]; // G
			rgb_buffer[pixlocation + 2] = bufferBGR.at<cv::Vec3b>(r, c)[0]; // B
			pixlocation += 3;
		}
	}

	// Gl texture calls
	glBindTexture(GL_TEXTURE_2D, texture[i]);
	glTexImage2D(GL_TEXTURE_2D,
		0,
		GL_RGB,
		bufferBGR.cols,
		bufferBGR.rows,
		0,
		GL_RGB,
		GL_UNSIGNED_BYTE,
		rgb_buffer);

	// Free memory
	delete[] rgb_buffer;
}

point unitCross(point u, point v) {
	point cross;
	cross.x = u.y * v.z - u.z * v.y;
	cross.y = u.z * v.x - u.x * v.z;
	cross.z = u.x * v.y - u.y * v.x;
	double magnitude = sqrt(cross.x * cross.x + cross.y * cross.y + cross.z * cross.z);
	cross.x /= magnitude;
	cross.y /= magnitude;
	cross.z /= magnitude;
	return cross;
}

point add(point a, point b) {
	point c;
	c.x = a.x + b.x;
	c.y = a.y + b.y;
	c.z = a.z + b.z;
	return c;
}

point subtract(point a, point b) {
	point c;
	c.x = a.x - b.x;
	c.y = a.y - b.y;
	c.z = a.z - b.z;
	return c;
}

point negate(point a) {
	point c;
	c.x = 0 - a.x;
	c.y = 0 - a.y;
	c.z = 0 - a.z;
	return c;
}

point scale(point a, double scalar) {
	point c;
	c.x = a.x * scalar;
	c.y = a.y * scalar;
	c.z = a.z * scalar;
	return c;
}

void splineCalc() {
	g_iNumOfPoints = 0;
	for (int i = 0; i < g_iNumOfSplines; ++i) {
		g_iNumOfPoints += ((g_Splines[i].numControlPoints - 3) * 1000);
	}
	points = new point[g_iNumOfPoints];
	tangents = new point[g_iNumOfPoints];
	normals = new point[g_iNumOfPoints];
	binormals = new point[g_iNumOfPoints];

	int uTotal = 0;
	for (int i = 0; i < g_iNumOfSplines; ++i) {
		spline sp = g_Splines[i];
		int controlIndex = 0;
		while (controlIndex < sp.numControlPoints - 3) {
			double x1 = (sp.points[controlIndex]).x;
			double x2 = (sp.points[controlIndex + 1]).x;
			double x3 = (sp.points[controlIndex + 2]).x;
			double x4 = (sp.points[controlIndex + 3]).x;
			double y1 = (sp.points[controlIndex]).y;
			double y2 = (sp.points[controlIndex + 1]).y;
			double y3 = (sp.points[controlIndex + 2]).y;
			double y4 = (sp.points[controlIndex + 3]).y;
			double z1 = (sp.points[controlIndex]).z;
			double z2 = (sp.points[controlIndex + 1]).z;
			double z3 = (sp.points[controlIndex + 2]).z;
			double z4 = (sp.points[controlIndex + 3]).z;
			double u = 0.0;

			while (u < 1) {
				//multiply u with the basis matrix
				double uCubed = u * u * u;
				double uSquared = u * u;
				double a = uCubed * (0 - s) + uSquared * (2 * s) + u * (0 - s);
				double b = uCubed * (2 - s) + uSquared * (s - 3) + 1;
				double c = uCubed * (s - 2) + uSquared * (3 - 2 * s) + u * s;
				double d = uCubed * s + uSquared * (0 - s);

				//multiply result with control matrix
				double x = a * x1 + b * x2 + c * x3 + d * x4;
				double y = a * y1 + b * y2 + c * y3 + d * y4;
				double z = a * z1 + b * z2 + c * z3 + d * z4;

				point p;
				p.x = x;
				p.y = y;
				p.z = z;
				points[uTotal] = p;
					
				a = uSquared * 3 * (0 - s) + 2 * u * (2 * s) - s;
				b = uSquared * 3 * (2 - s) + 2 * u * (s - 3);
				c = uSquared * 3 * (s - 2) + 2 * u * (3 - 2 * s) + s;
				d = uSquared * 3 * s + 2 * u * (0 - s);

				double xtan = a * x1 + b * x2 + c * x3 + d * x4;
				double ytan = a * y1 + b * y2 + c * y3 + d * y4;
				double ztan = a * z1 + b * z2 + c * z3 + d * z4;
				double tanMagnitude = sqrt(xtan * xtan + ytan * ytan + ztan * ztan);

				point t;
				t.x = xtan / tanMagnitude;
				t.y = ytan / tanMagnitude;
				t.z = ztan / tanMagnitude;
				tangents[uTotal] = t;

				if (uTotal == 0) {
					point arbitrary;
					arbitrary.x = 1;
					arbitrary.y = 0;
					arbitrary.z = 0;
					normals[uTotal] = unitCross(arbitrary, t);
				}
				else {
					normals[uTotal] = unitCross(binormals[uTotal - 1], t);
				}
				binormals[uTotal] = unitCross(t, normals[uTotal]);

				u += 0.001;
				uTotal++;
			}
			controlIndex++;
		}
	}
}

void displayTextureMaps() {
	glEnable(GL_TEXTURE_2D);
	// CUBE BOTTOM TEXTURE MAP
	glBindTexture(GL_TEXTURE_2D, texture[0]);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glBegin(GL_POLYGON);
	glTexCoord2f(1.0, 0.0);
	glVertex3f(50.0, -50.0, -50.0);
	glTexCoord2f(0.0, 0.0);
	glVertex3f(-50.0, -50.0, -50.0);
	glTexCoord2f(0.0, 1.0);
	glVertex3f(-50.0, 50.0, -50.0);
	glTexCoord2f(1.0, 1.0);
	glVertex3f(50.0, 50.0, -50.0);
	glEnd();

	// CUBE TOP TEXTURE MAP
	glBindTexture(GL_TEXTURE_2D, texture[1]);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glBegin(GL_POLYGON);
	glTexCoord2f(1.0, 0.0);
	glVertex3f(50.0, 50.0, 50.0);
	glTexCoord2f(0.0, 0.0);
	glVertex3f(-50.0, 50.0, 50.0);
	glTexCoord2f(0.0, 1.0);
	glVertex3f(-50.0, -50.0, 50.0);
	glTexCoord2f(1.0, 1.0);
	glVertex3f(50.0, -50.0, 50.0);
	glEnd();

	// CUBE LEFT TEXTURE MAP
	glBindTexture(GL_TEXTURE_2D, texture[4]);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glBegin(GL_POLYGON);
	glTexCoord2f(1.0, 0.0);
	glVertex3f(-50.0, 50.0, -50.0);
	glTexCoord2f(0.0, 0.0);
	glVertex3f(-50.0, -50.0, -50.0);
	glTexCoord2f(0.0, 1.0);
	glVertex3f(-50.0, -50.0, 50.0);
	glTexCoord2f(1.0, 1.0);
	glVertex3f(-50.0, 50.0, 50.0);
	glEnd();

	// CUBE AHEAD TEXTURE MAP
	glBindTexture(GL_TEXTURE_2D, texture[5]);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glBegin(GL_POLYGON);
	glTexCoord2f(1.0, 0.0);
	glVertex3f(50.0, 50.0, -50.0);
	glTexCoord2f(0.0, 0.0);
	glVertex3f(-50.0, 50.0, -50.0);
	glTexCoord2f(0.0, 1.0);
	glVertex3f(-50.0, 50.0, 50.0);
	glTexCoord2f(1.0, 1.0);
	glVertex3f(50.0, 50.0, 50.0);
	glEnd();

	// CUBE RIGHT TEXTURE MAP
	glBindTexture(GL_TEXTURE_2D, texture[2]);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glBegin(GL_POLYGON);
	glTexCoord2f(1.0, 0.0);
	glVertex3f(50.0, -50.0, -50.0);
	glTexCoord2f(0.0, 0.0);
	glVertex3f(50.0, 50.0, -50.0);
	glTexCoord2f(0.0, 1.0);
	glVertex3f(50.0, 50.0, 50.0);
	glTexCoord2f(1.0, 1.0);
	glVertex3f(50.0, -50.0, 50.0);
	glEnd();

	// CUBE BEHIND TEXTURE MAP
	glBindTexture(GL_TEXTURE_2D, texture[3]);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glBegin(GL_POLYGON);
	glTexCoord2f(1.0, 0.0);
	glVertex3f(-50.0, -50.0, -50.0);
	glTexCoord2f(0.0, 0.0);
	glVertex3f(50.0, -50.0, -50.0);
	glTexCoord2f(0.0, 1.0);
	glVertex3f(50.0, -50.0, 50.0);
	glTexCoord2f(1.0, 1.0);
	glVertex3f(-50.0, -50.0, 50.0);
	glEnd();

	glDisable(GL_TEXTURE_2D);
}

void displaySplines() {
	for (int i = 0; i < g_iNumOfPoints - 100; i += 100) {
		point a = points[i];
		point b = points[i + 100];
		point an = normals[i];
		point bn = normals[i + 100];
		point abn = binormals[i];
		point bbn = binormals[i + 100];
		point v0 = add(a, scale(subtract(an, abn), alpha));
		point v1 = add(a, scale(add(an, abn), alpha));
		point v2 = add(a, scale(subtract(abn, an), alpha));
		point v3 = add(a, scale(subtract(negate(an), abn), alpha));
		point v4 = add(b, scale(subtract(bn, bbn), alpha));
		point v5 = add(b, scale(add(bn, bbn), alpha));
		point v6 = add(b, scale(subtract(bbn, bn), alpha));
		point v7 = add(b, scale(subtract(negate(bn), bbn), alpha));

		glBegin(GL_QUADS);
		glVertex3d(v1.x, v1.y, v1.z);
		glVertex3d(v2.x, v2.y, v2.z);
		glVertex3d(v6.x, v6.y, v6.z);
		glVertex3d(v5.x, v5.y, v5.z);

		glVertex3d(v6.x, v6.y, v6.z);
		glVertex3d(v7.x, v7.y, v7.z);
		glVertex3d(v3.x, v3.y, v3.z);
		glVertex3d(v2.x, v2.y, v2.z);

		glVertex3d(v0.x, v0.y, v0.z);
		glVertex3d(v3.x, v3.y, v3.z);
		glVertex3d(v7.x, v7.y, v7.z);
		glVertex3d(v4.x, v4.y, v4.z);

		glVertex3d(v4.x, v4.y, v4.z);
		glVertex3d(v5.x, v5.y, v5.z);
		glVertex3d(v1.x, v1.y, v1.z);
		glVertex3d(v0.x, v0.y, v0.z);
		glEnd();
	}
}

void positionCamera() {
	point p = points[currentLocation];
	point t = tangents[currentLocation];
	point n = normals[currentLocation];
	gluLookAt(p.x + n.x / 2, p.y + n.y / 2, p.z + n.z / 2, p.x + currentLocation * t.x, p.y + currentLocation * t.y, p.z + currentLocation * t.z, n.x, n.y, n.z);
}

void display()
{
	// clear buffers
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glLoadIdentity();

	positionCamera();

	// handles any rotations from mouse click + drag
	glRotatef(g_vLandRotate[0], 1.0, 0, 0);
	glRotatef(g_vLandRotate[1], 0, 1.0, 0);
	glRotatef(g_vLandRotate[2], 0, 0, 1.0);

	// handles any translations from ctrl + mouse click + drag
	glTranslatef(g_vLandTranslate[0], g_vLandTranslate[1], g_vLandTranslate[2]);

	// handles any scalings from shift + mouse click + drag
	glScalef(g_vLandScale[0], g_vLandScale[1], g_vLandScale[2]);

	displayTextureMaps();
	displaySplines();

	if (currentLocation < g_iNumOfPoints && updateEveryXDisplays == displayNumber) {
		currentLocation++;
		displayNumber = 0;
	}
	displayNumber++;
	glutSwapBuffers(); // double buffer flush
}

void reshape(int w, int h)
{
	glViewport(0, 0, (GLsizei)w, (GLsizei)h);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(60.0, 1.3, 0.01, 300.0);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}

void menufunc(int value)
{
	switch (value)
	{
	case 0:
		exit(0);
		break;
	}
}

void doIdle()
{
	/* make the screen update */
	glutPostRedisplay();
}

/* converts mouse drags into information about
rotation/translation/scaling */
void mousedrag(int x, int y)
{
	int vMouseDelta[2] = { x - g_vMousePos[0], y - g_vMousePos[1] };

	switch (g_ControlState)
	{
	case TRANSLATE:
		if (g_iLeftMouseButton)
		{
			g_vLandTranslate[0] += vMouseDelta[0] * 0.01;
			g_vLandTranslate[1] -= vMouseDelta[1] * 0.01;
		}
		if (g_iMiddleMouseButton)
		{
			g_vLandTranslate[2] += vMouseDelta[1] * 0.01;
		}
		break;
	case ROTATE:
		if (g_iLeftMouseButton)
		{
			g_vLandRotate[0] += vMouseDelta[1];
			g_vLandRotate[1] += vMouseDelta[0];
		}
		if (g_iMiddleMouseButton)
		{
			g_vLandRotate[2] += vMouseDelta[1];
		}
		break;
	case SCALE:
		if (g_iLeftMouseButton)
		{
			g_vLandScale[0] *= 1.0 + vMouseDelta[0] * 0.01;
			g_vLandScale[1] *= 1.0 - vMouseDelta[1] * 0.01;
		}
		if (g_iMiddleMouseButton)
		{
			g_vLandScale[2] *= 1.0 - vMouseDelta[1] * 0.01;
		}
		break;
	}
	g_vMousePos[0] = x;
	g_vMousePos[1] = y;
}

void mouseidle(int x, int y)
{
	g_vMousePos[0] = x;
	g_vMousePos[1] = y;
}

void mousebutton(int button, int state, int x, int y)
{
	switch (button)
	{
	case GLUT_LEFT_BUTTON:
		g_iLeftMouseButton = (state == GLUT_DOWN);
		break;
	case GLUT_MIDDLE_BUTTON:
		g_iMiddleMouseButton = (state == GLUT_DOWN);
		break;
	case GLUT_RIGHT_BUTTON:
		g_iRightMouseButton = (state == GLUT_DOWN);
		break;
	}

	switch (glutGetModifiers())
	{
	case GLUT_ACTIVE_CTRL:
		g_ControlState = TRANSLATE;
		break;
	case GLUT_ACTIVE_SHIFT:
		g_ControlState = SCALE;
		break;
	default:
		g_ControlState = ROTATE;
		break;
	}

	g_vMousePos[0] = x;
	g_vMousePos[1] = y;
}

int loadSplines(char *argv) {
	char *cName = (char *)malloc(128 * sizeof(char));
	FILE *fileList;
	FILE *fileSpline;
	int iType, i = 0, j, iLength;

	/* load the track file */
	fileList = fopen(argv, "r");
	if (fileList == NULL) {
		printf ("can't open file\n");
		exit(1);
	}
  
	/* stores the number of splines in a global variable */
	fscanf(fileList, "%d", &g_iNumOfSplines);
	printf("%d\n", g_iNumOfSplines);
	g_Splines = (struct spline *)malloc(g_iNumOfSplines * sizeof(struct spline));

	/* reads through the spline files */
	for (j = 0; j < g_iNumOfSplines; j++) {
		i = 0;
		fscanf(fileList, "%s", cName);
		fileSpline = fopen(cName, "r");

		if (fileSpline == NULL) {
			printf ("can't open file\n");
			exit(1);
		}

		/* gets length for spline file */
		fscanf(fileSpline, "%d %d", &iLength, &iType);

		/* allocate memory for all the points */
		g_Splines[j].points = (struct point *)malloc(iLength * sizeof(struct point));
		g_Splines[j].numControlPoints = iLength;

		/* saves the data to the struct */
		while (fscanf(fileSpline, "%lf %lf %lf", 
			&g_Splines[j].points[i].x, 
			&g_Splines[j].points[i].y, 
			&g_Splines[j].points[i].z) != EOF) {
			i++;
		}
	}

	free(cName);

	return 0;
}

/* Write a screenshot to the specified filename */
void saveScreenshot(char *filename)
{
	if (filename == NULL)
		return;

	// Allocate a picture buffer // 
	cv::Mat3b bufferRGB = cv::Mat::zeros(480, 640, CV_8UC3); //rows, cols, 3-channel 8-bit.
	printf("File to save to: %s\n", filename);

	//use fast 4-byte alignment (default anyway) if possible
	glPixelStorei(GL_PACK_ALIGNMENT, (bufferRGB.step & 3) ? 1 : 4);
	//set length of one complete row in destination data (doesn't need to equal img.cols)
	glPixelStorei(GL_PACK_ROW_LENGTH, bufferRGB.step / bufferRGB.elemSize());
	glReadPixels(0, 0, bufferRGB.cols, bufferRGB.rows, GL_RGB, GL_UNSIGNED_BYTE, bufferRGB.data);
	//flip to account for GL 0,0 at lower left
	cv::flip(bufferRGB, bufferRGB, 0);
	//convert RGB to BGR
	cv::Mat3b bufferBGR(bufferRGB.rows, bufferRGB.cols, CV_8UC3);
	cv::Mat3b out[] = { bufferBGR };
	// rgb[0] -> bgr[2], rgba[1] -> bgr[1], rgb[2] -> bgr[0]
	int from_to[] = { 0,2, 1,1, 2,0 };
	mixChannels(&bufferRGB, 1, out, 1, from_to, 3);

	if (cv::imwrite(filename, bufferBGR)) {
		printf("File saved Successfully\n");
	}
	else {
		printf("Error in Saving\n");
	}
}

/* Function to get a pixel value. Use like PIC_PIXEL macro. 
Note: OpenCV images are in channel order BGR. 
This means that:
chan = 0 returns BLUE, 
chan = 1 returns GREEN, 
chan = 2 returns RED. */
unsigned char getPixelValue(cv::Mat3b& image, int x, int y, int chan)
{
	return image.at<cv::Vec3b>(y, x)[chan];
}

/* Function that does nothing but demonstrates looping through image coordinates.*/
void loopImage(cv::Mat3b& image)
{
	for (int r = 0; r < image.rows; r++) { // y-coordinate
		for (int c = 0; c < image.cols; c++) { // x-coordinate
			for (int channel = 0; channel < 3; channel++) {
				// DO SOMETHING... example usage
				// unsigned char blue = getPixelValue(image, c, r, 0);
				// unsigned char green = getPixelValue(image, c, r, 1); 
				// unsigned char red = getPixelValue(image, c, r, 2); 
			}
		}
	}
}

void myinit()
{
	glGenTextures(6, texture);
	texload(0, "ground.bmp");
	texload(1, "skytop.bmp");
	texload(2, "skyrender0001.bmp");
	texload(3, "skyrender0002.bmp");
	texload(4, "skyrender0004.bmp");
	texload(5, "skyrender0005.bmp");
	splineCalc();
}

/* OpenCV help:
Access number of rows of image (height): image.rows; 
Access number of columns of image (width): image.cols;
Pixel 0,0 is the upper left corner. Byte order for 3-channel images is BGR. 
*/

int _tmain(int argc, _TCHAR* argv[])
{
	// I've set the argv[1] to track.txt.
	// To change it, on the "Solution Explorer",
	// right click "assign1", choose "Properties",
	// go to "Configuration Properties", click "Debugging",
	// then type your track file name for the "Command Arguments"
	if (argc<2)
	{  
		printf ("usage: %s <trackfile>\n", argv[0]);
		exit(0);
	}

	// request double buffer
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_DEPTH | GLUT_RGBA);

	// set window size
	glutInitWindowSize(640, 480);
	// set window position
	glutInitWindowPosition(200, 100);
	// creates a window
	glutCreateWindow("Rollercoaster");

	// tells glut to use a particular display function to redraw
	glutDisplayFunc(display);

	glutReshapeFunc(reshape);

	// allow the user to quit using the right mouse button menu 
	int g_iMenuId = glutCreateMenu(menufunc);
	glutSetMenu(g_iMenuId);
	glutAddMenuEntry("Quit", 0);
	glutAttachMenu(GLUT_RIGHT_BUTTON);

	// replace with any animate code
	glutIdleFunc(doIdle);

	// callback for mouse drags 
	glutMotionFunc(mousedrag);
	// callback for idle mouse movement 
	glutPassiveMotionFunc(mouseidle);
	// callback for mouse button changes 
	glutMouseFunc(mousebutton);

	loadSplines(argv[1]);
	myinit();

	glutMainLoop();
	// If you need to load textures use below instead of pic library:
	// readImage("spiral.jpg", imageBGR, true);

	// Demonstrates to loop across image and access pixel values:
	// Note this function doesn't do anything, but you may find it useful:
	// loopImage(imageBGR);

	// Rebuilt save screenshot function, but will seg-fault unless you have
	// OpenGL framebuffers initialized. You can use this new function as before:
	// saveScreenshot("test_screenshot.jpg");

	return 0;
}
